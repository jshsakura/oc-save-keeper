import base64
import hashlib
import hmac
import os
import secrets
import time
from typing import Literal
from urllib.parse import urlencode

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse
from pydantic import BaseModel, Field
from redis.asyncio import Redis


DROPBOX_AUTHORIZE_URL = "https://www.dropbox.com/oauth2/authorize"
DROPBOX_TOKEN_URL = "https://api.dropboxapi.com/oauth2/token"
STATE_TTL_SECONDS = int(os.getenv("STATE_TTL_SECONDS", "900"))
SESSION_TTL_SECONDS = int(os.getenv("SESSION_TTL_SECONDS", "900"))


def get_env(name: str, default: str = "") -> str:
    value = os.getenv(name, default).strip()
    return value


APP_KEY = get_env("DROPBOX_APP_KEY")
REDIRECT_BASE_URL = get_env("REDIRECT_BASE_URL", "https://save.opencourse.kr")
POLL_TOKEN_SECRET = get_env("POLL_TOKEN_SECRET")

# 보안 강화: 파일에서 시크릿 읽기 지원 (Docker Secrets 스타일)
SECRET_FILE_PATH = "/run/secrets/poll_token_secret"
if os.path.exists(SECRET_FILE_PATH):
    with open(SECRET_FILE_PATH, "r") as f:
        POLL_TOKEN_SECRET = f.read().strip()

REDIS_URL = get_env("REDIS_URL", "redis://redis:6379/0")

if not APP_KEY:
    raise RuntimeError("DROPBOX_APP_KEY must be set")
if not POLL_TOKEN_SECRET:
    raise RuntimeError("POLL_TOKEN_SECRET must be set")


def code_challenge_s256(verifier: str) -> str:
    digest = hashlib.sha256(verifier.encode("utf-8")).digest()
    return base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")


def hash_poll_token(token: str) -> str:
    # 더 강력한 SHA-512 사용
    digest = hmac.new(
        POLL_TOKEN_SECRET.encode("utf-8"),
        token.encode("utf-8"),
        hashlib.sha512,
    ).digest()
    return base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")


async def check_rate_limit(redis: Redis, client_ip: str, action: str, limit: int = 10, window: int = 60):
    """
    Redis 기반 단순 Rate Limit: 60초(window) 내에 10회(limit) 초과 시 차단
    """
    key = f"ratelimit:{action}:{client_ip}"
    current = await redis.incr(key)
    if current == 1:
        await redis.expire(key, window)
    if current > limit:
        raise HTTPException(status_code=429, detail="Too many requests. Slow down.")


def session_key(session_id: str) -> str:
    return f"oauth:session:{session_id}"


def state_key(state: str) -> str:
    return f"oauth:state:{state}"


class StartSessionRequest(BaseModel):
    device_id: str | None = Field(default=None, max_length=128)


class StartSessionResponse(BaseModel):
    session_id: str
    poll_token: str
    authorize_url: str
    expires_in: int
    poll_url: str


class SessionStatusResponse(BaseModel):
    status: Literal["pending", "approved", "failed", "consumed", "expired"]
    retry_after: int = 2


class SessionStatusRequest(BaseModel):
    poll_token: str = Field(min_length=32, max_length=256)


class ConsumeSessionRequest(BaseModel):
    poll_token: str = Field(min_length=32, max_length=256)


class ConsumeSessionResponse(BaseModel):
    authorization_code: str
    code_verifier: str
    state: str
    redirect_uri: str
    token_endpoint: str


class HealthResponse(BaseModel):
    status: Literal["ok"]
    redis: Literal["ok"]


app = FastAPI(title="oc-save-keeper Dropbox Bridge", version="0.1.0")
redis_client = Redis.from_url(REDIS_URL, decode_responses=True)


@app.get("/healthz", response_model=HealthResponse)
async def healthz() -> HealthResponse:
    await redis_client.ping()
    return HealthResponse(status="ok", redis="ok")


@app.post("/v1/sessions/start", response_model=StartSessionResponse)
async def start_session(payload: StartSessionRequest, request: Request) -> StartSessionResponse:
    # 세션 시작 시에도 너무 잦은 요청은 차단
    await check_rate_limit(redis_client, request.client.host, "start_session", limit=5, window=60)
    
    session_id = secrets.token_urlsafe(18)
    state = secrets.token_urlsafe(18)
    poll_token = secrets.token_urlsafe(24)
    code_verifier = secrets.token_urlsafe(64)
    challenge = code_challenge_s256(code_verifier)
    now = int(time.time())

    key = session_key(session_id)
    poll_token_hash = hash_poll_token(poll_token)
    redirect_uri = f"{REDIRECT_BASE_URL}/oauth/dropbox/callback"

    await redis_client.hset(
        key,
        mapping={
            "status": "pending",
            "state": state,
            "poll_token_hash": poll_token_hash,
            "code_verifier": code_verifier,
            "auth_code": "",
            "error": "",
            "device_id": payload.device_id or "",
            "created_at": str(now),
            "updated_at": str(now),
            "redirect_uri": redirect_uri,
        },
    )
    await redis_client.expire(key, SESSION_TTL_SECONDS)
    await redis_client.set(state_key(state), session_id, ex=STATE_TTL_SECONDS)

    authorize_url = (
        f"{DROPBOX_AUTHORIZE_URL}?"
        + urlencode(
            {
                "client_id": APP_KEY,
                "redirect_uri": redirect_uri,
                "response_type": "code",
                "token_access_type": "offline",
                "state": state,
                "code_challenge_method": "S256",
                "code_challenge": challenge,
            }
        )
    )

    poll_url = f"{REDIRECT_BASE_URL}/v1/sessions/{session_id}/status"
    return StartSessionResponse(
        session_id=session_id,
        poll_token=poll_token,
        authorize_url=authorize_url,
        expires_in=SESSION_TTL_SECONDS,
        poll_url=poll_url,
    )


def verify_poll_token(stored_hash: str, provided_token: str) -> bool:
    provided_hash = hash_poll_token(provided_token)
    return hmac.compare_digest(stored_hash, provided_hash)


@app.post("/v1/sessions/{session_id}/status", response_model=SessionStatusResponse)
async def get_status(
    session_id: str,
    payload: SessionStatusRequest,
    request: Request,
) -> SessionStatusResponse:
    # 무차별 대입 공격 방지
    await check_rate_limit(redis_client, request.client.host, "status")
    
    key = session_key(session_id)
    if not await redis_client.exists(key):
        return SessionStatusResponse(status="expired")

    data = await redis_client.hgetall(key)
    if not data:
        return SessionStatusResponse(status="expired")
    if not verify_poll_token(data.get("poll_token_hash", ""), payload.poll_token):
        raise HTTPException(status_code=401, detail="invalid poll token")

    status = data.get("status", "expired")
    if status not in {"pending", "approved", "failed", "consumed"}:
        status = "expired"
    return SessionStatusResponse(status=status)


@app.post("/v1/sessions/{session_id}/consume", response_model=ConsumeSessionResponse)
async def consume_session(
    session_id: str,
    payload: ConsumeSessionRequest,
    request: Request,
) -> ConsumeSessionResponse:
    # 가장 중요한 지점: 엄격한 Rate Limit (5분 내 5회 실패 시 차단)
    await check_rate_limit(redis_client, request.client.host, "consume", limit=5, window=300)
    
    key = session_key(session_id)
    if not await redis_client.exists(key):
        raise HTTPException(status_code=404, detail="session expired")

    data = await redis_client.hgetall(key)
    if not data:
        raise HTTPException(status_code=404, detail="session expired")
    if not verify_poll_token(data.get("poll_token_hash", ""), payload.poll_token):
        raise HTTPException(status_code=401, detail="invalid poll token")

    status = data.get("status", "expired")
    if status == "pending":
        raise HTTPException(status_code=409, detail="authorization pending")
    if status == "failed":
        raise HTTPException(status_code=409, detail=data.get("error", "authorization failed"))
    if status == "consumed":
        raise HTTPException(status_code=409, detail="already consumed")

    auth_code = data.get("auth_code", "")
    code_verifier = data.get("code_verifier", "")
    state = data.get("state", "")
    redirect_uri = data.get("redirect_uri", "")

    if not auth_code or not code_verifier or not state or not redirect_uri:
        raise HTTPException(status_code=500, detail="incomplete session data")

    now = int(time.time())
    await redis_client.hset(
        key,
        mapping={
            "status": "consumed",
            "auth_code": "",
            "updated_at": str(now),
        },
    )
    await redis_client.expire(key, 60)

    return ConsumeSessionResponse(
        authorization_code=auth_code,
        code_verifier=code_verifier,
        state=state,
        redirect_uri=redirect_uri,
        token_endpoint=DROPBOX_TOKEN_URL,
    )


@app.get("/oauth/dropbox/callback", response_class=HTMLResponse)
async def dropbox_callback(
    request: Request,
    code: str | None = None,
    state: str | None = None,
    error_description: str | None = None,
) -> HTMLResponse:
    if not state:
        return HTMLResponse("missing state", status_code=400)

    sid = await redis_client.get(state_key(state))
    if not sid:
        return HTMLResponse("session expired", status_code=410)

    key = session_key(sid)
    if not await redis_client.exists(key):
        return HTMLResponse("session expired", status_code=410)

    current_state = await redis_client.hget(key, "state")
    if current_state != state:
        return HTMLResponse("state mismatch", status_code=400)

    now = int(time.time())
    if not code:
        await redis_client.hset(
            key,
            mapping={
                "status": "failed",
                "error": error_description or "authorization denied",
                "updated_at": str(now),
            },
        )
    else:
        await redis_client.hset(
            key,
            mapping={
                "status": "approved",
                "auth_code": code,
                "error": "",
                "updated_at": str(now),
            },
        )

    await redis_client.expire(key, SESSION_TTL_SECONDS)
    await redis_client.delete(state_key(state))

    html = """
<!doctype html>
<html>
  <head>
    <meta charset=\"utf-8\" />
    <title>oc-save-keeper Dropbox Bridge</title>
  </head>
  <body>
    <h2>Dropbox authorization completed</h2>
    <p>You can return to your Nintendo Switch app now.</p>
  </body>
</html>
"""
    return HTMLResponse(html, status_code=200)
