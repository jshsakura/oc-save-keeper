import base64
import hashlib
import hmac
import logging
import os
import secrets
import time
from contextlib import asynccontextmanager
from typing import Literal
from urllib.parse import urlencode

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse, RedirectResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from redis.asyncio import Redis
from redis.exceptions import RedisError


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
RELEASE_URL = get_env("RELEASE_URL", "https://github.com/jshsakura/oc-save-keeper/releases/tag/latest")
GITHUB_URL = get_env("GITHUB_URL", "https://github.com/jshsakura/oc-save-keeper")

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


async def detect_attack(redis: Redis, client_ip: str) -> bool:
    """
    공격 탐지: 의심스러운 패턴 감지
    - 1분 내 100회 이상 요청 = DDoS 의심
    - 1시간 내 차단 횟수 누적
    """
    # 분당 요청 수 체크
    minute_key = f"attack:minute:{client_ip}"
    minute_count = await redis.incr(minute_key)
    if minute_count == 1:
        await redis.expire(minute_key, 60)
    
    # 1분에 100회 이상 = 공격 의심
    if minute_count > 100:
        block_key = f"attack:blocked:{client_ip}"
        await redis.setex(block_key, 3600, "1")  # 1시간 차단
        return True
    
    # 이미 차단된 IP인지 확인
    block_key = f"attack:blocked:{client_ip}"
    if await redis.exists(block_key):
        return True
    
    return False


async def log_suspicious(redis: Redis, client_ip: str, action: str, reason: str):
    """의심스러운 활동 로깅"""
    import json
    log_entry = json.dumps({
        "ip": client_ip,
        "action": action,
        "reason": reason,
        "timestamp": int(time.time())
    })
    await redis.lpush("security:logs", log_entry)
    await redis.ltrim("security:logs", 0, 999)  # 최근 1000개만 유지


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
    status: Literal["ok", "degraded"]
    redis: Literal["ok", "unavailable"]


# 로깅 설정
logger = logging.getLogger("bridge")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")


async def safe_redis_call(coro, fallback=None, error_msg="Redis error"):
    """Redis 호출을 안전하게 래핑"""
    try:
        return await coro
    except RedisError as e:
        logger.error(f"{error_msg}: {e}")
        if fallback is not None:
            return fallback
        raise HTTPException(status_code=503, detail="Service temporarily unavailable")


app = FastAPI(title="OC Save Keeper Bridge", version="0.1.0")
redis_client = Redis.from_url(
    REDIS_URL,
    decode_responses=True,
    socket_timeout=5,
    socket_connect_timeout=5,
    retry_on_timeout=True,
    max_connections=50
)

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


@app.get("/", response_class=HTMLResponse)
async def root(request: Request) -> str:
    """서비스 가동 중 페이지 (다국어 지원)"""
    # Accept-Language 헤더로 언어 감지
    accept_lang = request.headers.get("accept-language", "")
    is_korean = "ko" in accept_lang.lower()
    
    # 다국어 텍스트
    if is_korean:
        lang, title = "ko", "OC Save Keeper Bridge"
        desc = "세이브 백업 앱 <strong>OC Save Keeper</strong>의 OAuth 브릿지 서비스입니다.<br><span style='color: var(--subtext0); font-size: 14px;'>Dropbox가 OAuth Device Code Grant를 공식 지원하지 않아, Switch에서 폴링 기반 인증을 완료하려면 이 브릿지가 필요합니다. 원한다면 직접 호스팅하여 운용할 수 있습니다.</span>"
        install_title = "설치 방법"
        install_icon = "download"
        install_steps = [
            "Release에서 <code>.nro</code> 파일 다운로드",
            "SD카드 <code>/switch/</code> 폴더에 복사",
            "Homebrew Menu에서 실행"
        ]
        dropbox_title = "Dropbox 연동"
        dropbox_icon = "link"
        dropbox_steps = [
            "앱에서 <strong>클라우드 동기화</strong> → <strong>Dropbox 로그인</strong> 선택",
            "화면에 표시된 QR코드를 스마트폰으로 스캔",
            "브라우저에서 Dropbox 계정 로그인 및 권한 승인",
            "자동으로 인증 완료! 별도 코드 입력 불필요",
            "클라우드 동기화 활성화 완료!"
        ]
        api_title = "API Endpoints"
        api_icon = "terminal"
        footer = 'Created for Switch gamers by <a href="mailto:support@opencourse.kr" style="color: var(--lavender); text-decoration: none; font-weight: 600;">Husband of Rebekah</a>'
        status_text = "브릿지 서버 정상 작동 중"
    else:
        lang, title = "en", "OC Save Keeper Bridge"
        desc = "OAuth bridge service for <strong>OC Save Keeper</strong>, a save backup app.<br><span style='color: var(--subtext0); font-size: 14px;'>Dropbox does not officially support OAuth Device Code Grant, so this bridge is required for polling-based auth on Switch. You can also self-host this service.</span>"
        install_title = "Installation"
        install_icon = "download"
        install_steps = [
            "Download <code>.nro</code> file from Release",
            "Copy to SD card <code>/switch/</code> folder",
            "Launch from Homebrew Menu"
        ]
        dropbox_title = "Dropbox Setup"
        dropbox_icon = "link"
        dropbox_steps = [
            "Select <strong>Cloud Sync</strong> → <strong>Dropbox Login</strong> in app",
            "Scan QR code with your smartphone",
            "Login to Dropbox and authorize in browser",
            "Auth completes automatically - no code entry needed",
            "Cloud sync activated!"
        ]
        api_title = "API Endpoints"
        api_icon = "terminal"
        footer = 'Created for Switch gamers by <a href="mailto:support@opencourse.kr" style="color: var(--lavender); text-decoration: none; font-weight: 600;">Husband of Rebekah</a>'
        status_text = "Bridge server is running"
    
    install_list = "\n          ".join(f"<li>{step}</li>" for step in install_steps)
    dropbox_list = "\n          ".join(f"<li>{step}</li>" for step in dropbox_steps)
    
    return f"""<!DOCTYPE html>
<html lang="{lang}">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
    <meta name="theme-color" content="#1e1e2e" />
    <title>{title}</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <script src="https://unpkg.com/feather-icons"></script>
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
        --surface1: #45475a;
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
            radial-gradient(circle at 0% 0%, rgba(137, 180, 250, 0.08) 0%, transparent 40%),
            radial-gradient(circle at 100% 100%, rgba(203, 166, 247, 0.08) 0%, transparent 40%),
            radial-gradient(circle at 50% 50%, rgba(180, 190, 254, 0.03) 0%, transparent 60%);
        color: var(--text);
        min-height: 100vh;
        min-height: 100dvh;
        display: flex;
        flex-direction: column;
        align-items: center;
        padding: 60px 24px;
        -webkit-font-smoothing: antialiased;
      }}
      
      .container {{
        width: 100%;
        max-width: 640px;
        animation: slideUp 0.8s cubic-bezier(0.16, 1, 0.3, 1);
      }}
      
      @keyframes slideUp {{
        from {{ opacity: 0; transform: translateY(30px); }}
        to {{ opacity: 1; transform: translateY(0); }}
      }}
      
      .card {{
        background: rgba(24, 24, 37, 0.7);
        backdrop-filter: blur(16px);
        -webkit-backdrop-filter: blur(16px);
        border: 1px solid rgba(255, 255, 255, 0.08);
        border-radius: 40px;
        padding: 60px 40px;
        box-shadow: 0 30px 60px rgba(0, 0, 0, 0.4), inset 0 0 0 1px rgba(255, 255, 255, 0.02);
        position: relative;
        overflow: hidden;
      }}

      .card::before {{
        content: "";
        position: absolute;
        top: 0;
        left: 0;
        right: 0;
        height: 1px;
        background: linear-gradient(90deg, transparent, rgba(180, 190, 254, 0.3), transparent);
      }}
      
      .header {{
        text-align: center;
        margin-bottom: 32px;
      }}
      
      .icon-wrapper {{
        width: 120px;
        height: 120px;
        margin: 0 auto 32px;
        position: relative;
        animation: float 6s ease-in-out infinite;
      }}

      @keyframes float {{
        0%, 100% {{ transform: translateY(0); }}
        50% {{ transform: translateY(-10px); }}
      }}

      .app-icon {{
        width: 100%;
        height: 100%;
        border-radius: 28px;
        overflow: hidden;
        box-shadow: 0 12px 30px rgba(0, 0, 0, 0.5);
        border: 1px solid var(--surface1);
        background: var(--mantle);
      }}

      .app-icon img {{
        width: 100%;
        height: 100%;
        object-fit: cover;
      }}

      .badge {{
        display: inline-flex;
        align-items: center;
        gap: 8px;
        padding: 8px 16px;
        background: rgba(166, 227, 161, 0.1);
        border: 1px solid rgba(166, 227, 161, 0.2);
        border-radius: 100px;
        font-size: 13px;
        font-weight: 600;
        color: var(--green);
        margin-bottom: 20px;
        letter-spacing: 0.02em;
      }}

      .badge-dot {{
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background-color: var(--green);
        box-shadow: 0 0 12px var(--green);
        animation: pulse 2.5s infinite;
      }}

      @keyframes pulse {{
        0% {{ transform: scale(0.95); box-shadow: 0 0 0 0 rgba(166, 227, 161, 0.5); }}
        70% {{ transform: scale(1); box-shadow: 0 0 0 10px rgba(166, 227, 161, 0); }}
        100% {{ transform: scale(0.95); box-shadow: 0 0 0 0 rgba(166, 227, 161, 0); }}
      }}
      
      h1 {{
        font-size: 32px;
        font-weight: 700;
        margin-bottom: 16px;
        color: var(--text);
        letter-spacing: -0.03em;
        background: linear-gradient(180deg, #fff 0%, var(--subtext1) 100%);
        -webkit-background-clip: text;
        -webkit-text-fill-color: transparent;
      }}
      
      .description {{
        font-size: 17px;
        color: var(--subtext1);
        line-height: 1.6;
        font-weight: 400;
        max-width: 480px;
        margin: 0 auto;
      }}
      
      .github-link-container {{
        text-align: center;
        margin-bottom: 48px;
      }}
      
      .btn-github {{
        display: inline-flex;
        align-items: center;
        gap: 10px;
        padding: 12px 24px;
        border-radius: 16px;
        font-weight: 600;
        font-size: 15px;
        text-decoration: none;
        background: rgba(255, 255, 255, 0.05);
        color: var(--text);
        border: 1px solid rgba(255, 255, 255, 0.1);
        backdrop-filter: blur(8px);
        transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
      }}
      
      .btn-github:hover {{
        background: rgba(255, 255, 255, 0.1);
        border-color: rgba(255, 255, 255, 0.2);
      }}

      .btn-github i {{ width: 18px; height: 18px; margin-right: 8px; }}
      
      h2 {{
        font-size: 19px;
        font-weight: 600;
        color: var(--lavender);
        margin: 40px 0 20px;
        display: flex;
        align-items: center; gap: 12px;
      }}

      h2 i {{ width: 20px; height: 20px; stroke-width: 2.5; vertical-align: middle; margin-right: 8px; }}
      
      h2::after {{
        content: "";
        flex: 1;
        height: 1px;
        background: linear-gradient(90deg, var(--surface1), transparent);
        margin-left: 8px;
      }}
      
      ol {{
        list-style: none;
        counter-reset: steps;
      }}
      
      li {{
        position: relative;
        padding-left: 44px;
        margin-bottom: 20px;
        color: var(--subtext1);
        font-size: 15px;
        line-height: 1.6;
        transition: color 0.2s ease;
      }}

      li:hover {{
        color: var(--text);
      }}
      
      li::before {{
        counter-increment: steps;
        content: counter(steps);
        position: absolute;
        left: 0;
        top: -1px;
        width: 28px;
        height: 28px;
        background: var(--surface0);
        color: var(--lavender);
        border: 1px solid rgba(180, 190, 254, 0.2);
        border-radius: 8px;
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 13px;
        font-weight: 700;
        box-shadow: 0 4px 10px rgba(0, 0, 0, 0.2);
      }}

      code {{
        font-family: 'JetBrains Mono', ui-monospace, SFMono-Regular, monospace;
        background: rgba(180, 190, 254, 0.1);
        color: var(--lavender);
        padding: 2px 8px;
        border-radius: 6px;
        font-size: 13.5px;
        border: 1px solid rgba(180, 190, 254, 0.1);
      }}
      
      .endpoints {{
        display: flex;
        flex-direction: column;
        gap: 10px;
      }}
      
      .endpoint {{
        display: flex;
        align-items: center;
        gap: 16px;
        padding: 14px 20px;
        background: rgba(49, 50, 68, 0.3);
        border: 1px solid rgba(255, 255, 255, 0.05);
        border-radius: 16px;
        font-family: ui-monospace, SFMono-Regular, monospace;
        font-size: 13px;
        color: var(--subtext0);
        transition: all 0.3s ease;
      }}

      .endpoint:hover {{
        background: rgba(49, 50, 68, 0.6);
        border-color: rgba(180, 190, 254, 0.2);
        color: var(--text);
      }}
      
      .method {{
        color: var(--sapphire);
        font-weight: 700;
        min-width: 50px;
        font-size: 12px;
        letter-spacing: 0.05em;
      }}
      
      .method-get {{
        color: var(--green);
      }}

      .footer {{
        margin-top: 50px;
        text-align: center;
      }}
      
      .footer p {{
        font-size: 14px;
        color: var(--overlay0);
        letter-spacing: 0.01em;
      }}

      @media (max-width: 480px) {{
        .card {{ padding: 40px 24px; }}
        h1 {{ font-size: 26px; }}
        .btn-github {{ width: 100%; justify-content: center; }}
      }}
    </style>
  </head>
  <body>
    <div class="container">
      <div class="card">
        
        <div class="header">
          <div class="icon-wrapper">
            <div class="app-icon">
              <img src="/static/icon.png" alt="Icon" />
            </div>
          </div>
          <div class="badge">
            <div class="badge-dot"></div>
            {status_text}
          </div>
          <h1>{title}</h1>
          <p class="description">{desc}</p>
        </div>
        
        <div class="github-link-container">
          <a href="{GITHUB_URL}" class="btn-github">
            <i data-feather="github"></i>
            GitHub
          </a>
        </div>
        
        <h2><i data-feather="{install_icon}"></i> {install_title}</h2>
        <ol>
          {install_list}
        </ol>
        
        <h2><i data-feather="{dropbox_icon}"></i> {dropbox_title}</h2>
        <ol>
          {dropbox_list}
        </ol>
        
        <h2><i data-feather="{api_icon}"></i> {api_title}</h2>
        <div class="endpoints">
          <div class="endpoint"><span class="method">POST</span> /v1/sessions/start</div>
          <div class="endpoint"><span class="method">POST</span> /v1/sessions/{{id}}/status</div>
          <div class="endpoint"><span class="method">POST</span> /v1/sessions/{{id}}/consume</div>
          <div class="endpoint"><span class="method method-get">GET</span> /oauth/dropbox/callback</div>
          <div class="endpoint"><span class="method method-get">GET</span> /healthz</div>
        </div>
        
      </div>
      
      <div class="footer">
        <p>{footer}</p>
      </div>
    </div>
    <script>feather.replace();</script>
  </body>
</html>"""


@app.get("/robots.txt")
async def robots() -> str:
    """봇 차단"""
    return "User-agent: *\nDisallow: /"


@app.get("/favicon.ico")
async def favicon():
    """favicon은 static/icon.png로 리다이렉트"""
    return RedirectResponse(url="/static/icon.png")


@app.post("/v1/sessions/start", response_model=StartSessionResponse)
async def start_session(payload: StartSessionRequest, request: Request) -> StartSessionResponse:
    client_ip = request.client.host
    
    # 공격 탐지
    if await detect_attack(redis_client, client_ip):
        await log_suspicious(redis_client, client_ip, "start_session", "blocked_by_attack_detection")
        raise HTTPException(status_code=403, detail="Access denied")
    
    # 세션 시작 시에도 너무 잦은 요청은 차단
    await check_rate_limit(redis_client, client_ip, "start_session", limit=5, window=60)
    
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
    client_ip = request.client.host
    
    # 공격 탐지
    if await detect_attack(redis_client, client_ip):
        await log_suspicious(redis_client, client_ip, "status", "blocked_by_attack_detection")
        raise HTTPException(status_code=403, detail="Access denied")
    
    # 무차별 대입 공격 방지
    await check_rate_limit(redis_client, client_ip, "status")
    
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
    client_ip = request.client.host
    
    # 공격 탐지
    if await detect_attack(redis_client, client_ip):
        await log_suspicious(redis_client, client_ip, "consume", "blocked_by_attack_detection")
        raise HTTPException(status_code=403, detail="Access denied")
    
    # 가장 중요한 지점: 엄격한 Rate Limit (5분 내 5회 실패 시 차단)
    await check_rate_limit(redis_client, client_ip, "consume", limit=5, window=300)
    
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
    <title>OC Save Keeper | Dropbox</title>
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
            <img src="/static/icon.png" alt="OC Save Keeper" />
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
            {'정보' if is_korean else 'Info'}
          </div>
          <p>{security_note}</p>
        </div>
        
        <div class="action-hint">
          {wait_message}
        </div>
      </div>
      
      <div class="footer">
        <p><b>OC Save Keeper</b> bridge service</p>
      </div>
    </div>
  </body>
</html>
"""
    return HTMLResponse(html, status_code=200)
