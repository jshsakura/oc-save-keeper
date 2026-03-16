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
            title = "Dropbox 연동 완료"
            message = "인증이 안전하게 완료되었습니다."
            security_note = "서버에는 토큰이 저장되지 않으며, 모든 인증 정보는 기기에만 남습니다."
            wait_message = "잠시 후 Nintendo Switch 화면이 자동으로 변경됩니다."
        else:
            title = "인증 실패"
            message = "인증이 거부되었거나 취소되었습니다."
            security_note = error_description or "인증이 거부되었습니다."
            wait_message = "Switch 앱으로 돌아가 다시 시도해 주세요."
    else:
        if is_success:
            title = "Dropbox Connected"
            message = "Authentication completed securely."
            security_note = "No tokens are stored on the server. All credentials remain on your device only."
            wait_message = "Your Nintendo Switch screen will update shortly."
        else:
            title = "Authorization Failed"
            message = "Authorization was denied or cancelled."
            security_note = error_description or "Authorization was denied."
            wait_message = "Please return to the Switch app and try again."

    status_color = "#a6e3a1" if is_success else "#f38ba8"
    status_bg = "rgba(166, 227, 161, 0.1)" if is_success else "rgba(243, 139, 168, 0.1)"
    
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
        --pink: #f5c2e7;
      }}
      
      * {{
        box-sizing: border-box;
        margin: 0;
        padding: 0;
      }}
      
      html, body {{
        height: 100%;
      }}
      
      body {{
        font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: linear-gradient(145deg, var(--crust) 0%, var(--base) 50%, var(--mantle) 100%);
        color: var(--text);
        min-height: 100vh;
        min-height: 100dvh;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 20px;
        padding-bottom: max(20px, env(safe-area-inset-bottom));
        -webkit-font-smoothing: antialiased;
        -moz-osx-font-smoothing: grayscale;
      }}
      
      .container {{
        width: 100%;
        max-width: 380px;
        animation: slideUp 0.5s cubic-bezier(0.16, 1, 0.3, 1);
      }}
      
      @keyframes slideUp {{
        from {{
          opacity: 0;
          transform: translateY(20px);
        }}
        to {{
          opacity: 1;
          transform: translateY(0);
        }}
      }}
      
      .card {{
        background: linear-gradient(180deg, var(--mantle) 0%, rgba(24, 24, 37, 0.95) 100%);
        border: 1px solid rgba(205, 214, 244, 0.08);
        border-radius: 24px;
        padding: 40px 28px;
        text-align: center;
        box-shadow: 
          0 4px 6px rgba(0, 0, 0, 0.1),
          0 20px 40px rgba(0, 0, 0, 0.3),
          inset 0 1px 0 rgba(255, 255, 255, 0.05);
        backdrop-filter: blur(10px);
      }}
      
      .app-icon {{
        width: 88px;
        height: 88px;
        margin: 0 auto 24px;
        border-radius: 22px;
        overflow: hidden;
        box-shadow: 
          0 8px 24px rgba(0, 0, 0, 0.4),
          0 0 0 1px rgba(255, 255, 255, 0.1);
        animation: iconPop 0.6s cubic-bezier(0.34, 1.56, 0.64, 1) 0.1s both;
      }}
      
      .app-icon img {{
        width: 100%;
        height: 100%;
        object-fit: cover;
        display: block;
      }}
      
      @keyframes iconPop {{
        from {{
          opacity: 0;
          transform: scale(0.8);
        }}
        to {{
          opacity: 1;
          transform: scale(1);
        }}
      }}
      
      .status-badge {{
        display: inline-flex;
        align-items: center;
        gap: 8px;
        padding: 8px 16px;
        border-radius: 100px;
        background: {status_bg};
        margin-bottom: 20px;
        animation: fadeIn 0.4s ease 0.2s both;
      }}
      
      .status-badge svg {{
        width: 18px;
        height: 18px;
        color: {status_color};
      }}
      
      .status-badge span {{
        font-size: 13px;
        font-weight: 600;
        color: {status_color};
        letter-spacing: 0.02em;
      }}
      
      @keyframes fadeIn {{
        from {{ opacity: 0; }}
        to {{ opacity: 1; }}
      }}
      
      h1 {{
        font-size: 22px;
        font-weight: 700;
        margin-bottom: 8px;
        color: var(--text);
        letter-spacing: -0.02em;
        animation: fadeIn 0.4s ease 0.25s both;
      }}
      
      .message {{
        font-size: 15px;
        color: var(--subtext1);
        line-height: 1.5;
        margin-bottom: 24px;
        animation: fadeIn 0.4s ease 0.3s both;
      }}
      
      .security-card {{
        background: rgba(180, 190, 254, 0.06);
        border: 1px solid rgba(180, 190, 254, 0.15);
        border-radius: 12px;
        padding: 16px;
        margin-bottom: 20px;
        animation: fadeIn 0.4s ease 0.35s both;
      }}
      
      .security-card .icon-row {{
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 8px;
        margin-bottom: 10px;
      }}
      
      .security-card svg {{
        width: 16px;
        height: 16px;
        color: var(--lavender);
      }}
      
      .security-card .label {{
        font-size: 12px;
        font-weight: 600;
        color: var(--lavender);
        text-transform: uppercase;
        letter-spacing: 0.08em;
      }}
      
      .security-card p {{
        font-size: 13px;
        color: var(--subtext0);
        line-height: 1.6;
      }}
      
      .wait-notice {{
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 10px;
        padding: 14px;
        background: rgba(137, 180, 250, 0.08);
        border-radius: 10px;
        animation: fadeIn 0.4s ease 0.4s both;
      }}
      
      .wait-notice .spinner {{
        width: 16px;
        height: 16px;
        border: 2px solid var(--overlay0);
        border-top-color: var(--blue);
        border-radius: 50%;
        animation: spin 1s linear infinite;
      }}
      
      @keyframes spin {{
        to {{ transform: rotate(360deg); }}
      }}
      
      .wait-notice p {{
        font-size: 13px;
        color: var(--blue);
        font-weight: 500;
      }}
      
      .footer {{
        margin-top: 28px;
        padding-top: 20px;
        border-top: 1px solid rgba(205, 214, 244, 0.08);
        animation: fadeIn 0.4s ease 0.45s both;
      }}
      
      .footer .brand {{
        font-size: 14px;
        font-weight: 600;
        color: var(--lavender);
        letter-spacing: 0.02em;
      }}
      
      .footer .tagline {{
        font-size: 12px;
        color: var(--overlay0);
        margin-top: 4px;
      }}
      
      @media (prefers-reduced-motion: reduce) {{
        *, *::before, *::after {{
          animation-duration: 0.01ms !important;
          animation-iteration-count: 1 !important;
          transition-duration: 0.01ms !important;
        }}
      }}
      
      @media (max-width: 400px) {{
        .card {{
          padding: 32px 20px;
          border-radius: 20px;
        }}
        
        .app-icon {{
          width: 72px;
          height: 72px;
          border-radius: 18px;
        }}
        
        h1 {{
          font-size: 20px;
        }}
        
        .message {{
          font-size: 14px;
        }}
      }}
    </style>
  </head>
  <body>
    <div class="container">
      <div class="card">
        <div class="app-icon">
          <img src="/static/icon.png" alt="oc-save-keeper" />
        </div>
        
        <div class="status-badge">
          {'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg><span>Complete</span>' if is_success else '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg><span>Failed</span>'}
        </div>
        
        <h1>{title}</h1>
        <p class="message">{message}</p>
        
        <div class="security-card">
          <div class="icon-row">
            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>
            <span class="label">{'보안 안내' if is_korean else 'Security Notice'}</span>
          </div>
          <p>{security_note}</p>
        </div>
        
        <div class="wait-notice">
          <div class="spinner"></div>
          <p>{wait_message}</p>
        </div>
        
        <div class="footer">
          <div class="brand">oc-save-keeper</div>
          <div class="tagline">{'안전한 세이브 백업 & 동기화' if is_korean else 'Safe Save Backup & Sync'}</div>
        </div>
      </div>
    </div>
  </body>
</html>
"""
    return HTMLResponse(html, status_code=200)
