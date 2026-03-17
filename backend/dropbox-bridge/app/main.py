import base64
import hashlib
import hmac
import os
import secrets
import time
from typing import Literal
from urllib.parse import urlencode

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from redis.asyncio import Redis


DROPBOX_AUTHORIZE_URL = "https://www.dropbox.com/oauth2/authorize"
DROPBOX_TOKEN_URL = "https://api.dropboxapi.com/oauth2/token"
STATE_TTL_SECONDS = int(os.getenv("STATE_TTL_SECONDS", "900"))
SESSION_TTL_SECONDS = int(os.getenv("SESSION_TTL_SECONDS", "900"))
RICKROLL_URL = "https://www.youtube.com/watch?v=dQw4w9WgXcQ&list=RDdQw4w9WgXcQ&start_radio=1"


def get_env(name: str, default: str = "") -> str:
    value = os.getenv(name, default).strip()
    return value


APP_KEY = get_env("DROPBOX_APP_KEY")
REDIRECT_BASE_URL = get_env("REDIRECT_BASE_URL", "https://example.yourdomain.com")
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

# 정적 파일 서빙 (아이콘 등)
app.mount("/static", StaticFiles(directory=os.path.join(os.path.dirname(__file__), "static")), name="static")


@app.middleware("http")
async def rickroll_unsecured_http(request: Request, call_next):
    """
    보안 가드: HTTPS가 아닌 생(plain) HTTP로 접근하면 릭롤 시전
    """
    # X-Forwarded-Proto 헤더를 확인하여 리버스 프록시(Nginx 등) 뒤에서도 판별 가능하게 함
    proto = request.headers.get("x-forwarded-proto", request.url.scheme)
    if proto == "http":
        return RedirectResponse(url=RICKROLL_URL)
    return await call_next(request)


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

    # 언어 감지 (Accept-Language 헤더)
    accept_language = request.headers.get("accept-language", "")
    is_korean = "ko" in accept_language.lower()

    # 성공/실패에 따른 다국어 텍스트
    is_success = bool(code)
    
    if is_korean:
        if is_success:
            title = "연동 완료!"
            message = "Dropbox 연동이 성공적으로 완료되었습니다."
            security_note = "인증 정보는 기기에만 안전하게 저장되며, 서버에는 어떠한 데이터도 남지 않습니다."
            wait_message = "이제 이 브라우저 창을 닫으셔도 좋습니다."
            status_text = "연결됨"
        else:
            title = "연동 실패"
            message = "인증 과정에서 오류가 발생했습니다."
            security_note = error_description or "사용자가 인증을 거절했거나 세션이 만료되었습니다."
            wait_message = "Switch 앱으로 돌아가 다시 시도해 주세요."
            status_text = "오류 발생"
    else:
        if is_success:
            title = "Connected!"
            message = "Dropbox has been successfully linked."
            security_note = "Credentials are stored securely on your device only. No data remains on this server."
            wait_message = "You can safely close this browser tab now."
            status_text = "Connected"
        else:
            title = "Connection Failed"
            message = "An error occurred during authentication."
            security_note = error_description or "Authorization was denied or the session expired."
            wait_message = "Please return to the Switch app and try again."
            status_text = "Failed"

    status_color = "#a6e3a1" if is_success else "#f38ba8"
    status_bg = "rgba(166, 227, 161, 0.15)" if is_success else "rgba(243, 139, 168, 0.15)"
    
    html = f"""
<!doctype html>
<html lang="{'ko' if is_korean else 'en'}">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
    <meta name="theme-color" content="#1e1e2e" />
    <title>oc-save-keeper | Dropbox</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
      :root {{
        --base: #1e1e2e;
        --mantle: #181825;
        --crust: #11111b;
        --text: #cdd6f4;
        --subtext0: #a6adc8;
        --subtext1: #bac2de;
        --overlay0: #6c7086;
        --lavender: #b4befe;
        --green: #a6e3a1;
        --red: #f38ba8;
        --blue: #89b4fa;
        --sapphire: #74c3ec;
        --surface0: #313244;
      }}
      
      * {{
        box-sizing: border-box;
        margin: 0;
        padding: 0;
      }}
      
      body {{
        font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background-color: var(--crust);
        background-image: 
            radial-gradient(at 0% 0%, rgba(137, 180, 250, 0.05) 0px, transparent 50%),
            radial-gradient(at 100% 100%, rgba(203, 166, 247, 0.05) 0px, transparent 50%);
        color: var(--text);
        min-height: 100vh;
        min-height: 100dvh;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        padding: 24px;
        -webkit-font-smoothing: antialiased;
      }}
      
      .container {{
        width: 100%;
        max-width: 400px;
        animation: fadeIn 0.6s ease-out;
      }}
      
      @keyframes fadeIn {{
        from {{ opacity: 0; transform: translateY(10px); }}
        to {{ opacity: 1; transform: translateY(0); }}
      }}
      
      .card {{
        background: var(--mantle);
        border: 1px solid var(--surface0);
        border-radius: 32px;
        padding: 48px 32px;
        text-align: center;
        box-shadow: 0 20px 50px rgba(0, 0, 0, 0.3);
        position: relative;
        overflow: hidden;
      }}
      
      .success-glow {{
        position: absolute;
        top: -50%;
        left: -50%;
        width: 200%;
        height: 200%;
        background: radial-gradient(circle, {status_bg} 0%, transparent 70%);
        pointer-events: none;
      }}
      
      .icon-wrapper {{
        position: relative;
        width: 120px;
        height: 120px;
        margin: 0 auto 32px;
        animation: iconPop 0.6s cubic-bezier(0.34, 1.56, 0.64, 1);
      }}

      @keyframes iconPop {{
        from {{ transform: scale(0.6); opacity: 0; }}
        to {{ transform: scale(1); opacity: 1; }}
      }}
      
      .app-icon {{
        width: 100%;
        height: 100%;
        border-radius: 28px;
        overflow: hidden;
        box-shadow: 0 12px 30px rgba(0, 0, 0, 0.5);
        border: 1px solid var(--surface0);
      }}

      .app-icon img {{
        width: 100%;
        height: 100%;
        object-fit: cover;
      }}

      .status-badge-overlay {{
        position: absolute;
        bottom: -10px;
        right: -10px;
        width: 48px;
        height: 48px;
        border-radius: 50%;
        background: var(--mantle);
        border: 3px solid var(--mantle);
        display: flex;
        align-items: center;
        justify-content: center;
        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
      }}

      .status-badge-inner {{
        width: 100%;
        height: 100%;
        border-radius: 50%;
        background: {status_bg};
        display: flex;
        align-items: center;
        justify-content: center;
        color: {status_color};
      }}
      
      .badge {{
        display: inline-block;
        padding: 6px 12px;
        background: var(--surface0);
        border-radius: 100px;
        font-size: 12px;
        font-weight: 700;
        text-transform: uppercase;
        letter-spacing: 0.05em;
        color: {status_color};
        margin-bottom: 16px;
      }}
      
      h1 {{
        font-size: 26px;
        font-weight: 700;
        margin-bottom: 12px;
        letter-spacing: -0.02em;
      }}
      
      .description {{
        font-size: 16px;
        color: var(--subtext1);
        line-height: 1.6;
        margin-bottom: 32px;
      }}
      
      .info-box {{
        background: rgba(49, 50, 68, 0.4);
        border: 1px solid var(--surface0);
        border-radius: 20px;
        padding: 20px;
        text-align: left;
        margin-bottom: 32px;
      }}
      
      .info-box-header {{
        display: flex;
        align-items: center;
        gap: 8px;
        margin-bottom: 8px;
        color: var(--sapphire);
        font-size: 13px;
        font-weight: 600;
      }}
      
      .info-box-header svg {{
        width: 16px;
        height: 16px;
      }}
      
      .info-box p {{
        font-size: 14px;
        color: var(--subtext0);
        line-height: 1.5;
      }}
      
      .action-hint {{
        background: {status_color};
        color: var(--crust);
        padding: 16px;
        border-radius: 16px;
        font-weight: 700;
        font-size: 15px;
        box-shadow: 0 10px 20px {status_bg};
      }}
      
      .footer {{
        margin-top: 40px;
        text-align: center;
      }}
      
      .footer p {{
        font-size: 13px;
        color: var(--overlay0);
      }}
      
      .footer b {{
        color: var(--lavender);
      }}

      /* Checkmark Animation */
      .checkmark {{
        width: 28px;
        height: 28px;
        stroke-width: 4;
        stroke: {status_color};
        stroke-miterlimit: 10;
        stroke-dasharray: 48;
        stroke-dashoffset: 48;
        animation: stroke 0.3s cubic-bezier(0.65, 0, 0.45, 1) 0.5s forwards;
      }}

      @keyframes stroke {{
        100% {{ stroke-dashoffset: 0; }}
      }}
    </style>
  </head>
  <body>
    <div class="container">
      <div class="card">
        <div class="success-glow"></div>
        
        <div class="icon-wrapper">
          <div class="app-icon">
            <img src="/static/icon.png" alt="oc-save-keeper" />
          </div>
          <div class="status-badge-overlay">
            <div class="status-badge-inner">
              {f'''<svg class="checkmark" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 52 52">
                  <path fill="none" d="M14.1 27.2l7.1 7.2 16.7-16.8"/>
                </svg>''' if is_success else '''<svg style="width:24px;height:24px" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">
                  <line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>
                </svg>'''}
            </div>
          </div>
        </div>
        
        <div class="badge">{status_text}</div>
        <h1>{title}</h1>
        <p class="description">{message}</p>
        
        <div class="info-box">
          <div class="info-box-header">
            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>
            {'보안 정보' if is_korean else 'Privacy Info'}
          </div>
          <p>{security_note}</p>
        </div>
        
        <div class="action-hint">
          {wait_message}
        </div>
      </div>
      
      <div class="footer">
        <p><b>oc-save-keeper</b> bridge service</p>
      </div>
    </div>
  </body>
</html>
"""
    return HTMLResponse(html, status_code=200)"""
    return HTMLResponse(html, status_code=200)
