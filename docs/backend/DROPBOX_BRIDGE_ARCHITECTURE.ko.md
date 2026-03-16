# Dropbox 브리지 아키텍처 (example.yourdomain.com 기준)

## 목적

Switch에서 Dropbox 로그인 후 콜백 URL/코드를 수동 복사하지 않고, 브리지 서버가 콜백을 받아 앱이 폴링으로 완료할 수 있게 하는 구조를 정의합니다.

## 왜 도메인이 필요한가

- Dropbox App Console에 Redirect URI를 사전 등록해야 합니다.
- 운영에서는 `https://example.yourdomain.com/oauth/dropbox/callback` 같이 공개 HTTPS 도메인이 필요합니다.
- Redirect URI는 문자열이 정확히 일치해야 합니다.

## 구성 요소

- `bridge-api` (FastAPI): 세션 생성, 콜백 수신, 상태 조회, 1회 consume 제공
- `redis`: 짧은 TTL 세션 저장소
- `reverse proxy` (예: Nginx/Caddy/Cloudflare Tunnel): TLS 종료와 도메인 라우팅

## OAuth 처리 흐름

1. Switch -> `POST /v1/sessions/start`
2. 브리지 -> `session_id`, `poll_token`, `authorize_url` 반환
3. 사용자 -> 휴대폰/PC 브라우저에서 `authorize_url` 승인
4. Dropbox -> `GET /oauth/dropbox/callback?code=...&state=...` 호출
5. Switch -> `POST /v1/sessions/{id}/status` 폴링
6. 승인 상태면 -> `POST /v1/sessions/{id}/consume`
7. 브리지 -> `authorization_code` + `code_verifier` 1회 반환
8. Switch가 Dropbox 토큰 엔드포인트로 직접 교환

핵심은 **토큰 교환을 Switch에서 수행**해 브리지의 민감정보 보관 범위를 줄이는 것입니다.

## 보안 설계

- `state` 검증 필수 (CSRF 방지)
- `poll_token` 원문 저장 금지: HMAC 해시 저장
- 세션 TTL 기본 900초, consume 후 60초 이내 만료
- `consume`는 1회만 허용
- 로그에 `code`, `poll_token`, `authorization` 헤더를 남기지 않음
- HTTPS 강제, HTTP -> HTTPS 리다이렉트

## 부하/다중 사용자 관점

이 브리지는 I/O 중심 워크로드입니다.

- 요청 대부분이 Redis 읽기/쓰기 + 짧은 JSON 응답
- Dropbox API 직접 호출은 콜백 단계에서 없음(토큰 교환을 Switch가 수행)
- 세션 1건당 저장 데이터는 대략 0.8~1.5KB 수준

예시 계산:

- 동시 인증 세션 10,000개면 Redis 메모리 약 10~20MB + 오버헤드
- 폴링 주기 2초, 활성 세션 2,000개면 상태 조회 약 1,000 RPS
- 2 vCPU + 1~2GB 메모리 + Redis 단일 노드로도 시작 가능

권장 운영 단계:

1. 초기: 단일 bridge + 단일 redis
2. 성장: bridge 수평 확장(2~4 replicas), redis는 managed 또는 sentinel/cluster
3. 대규모: 전역 LB + 지역별 bridge + 중앙 redis/replica 전략

## 장애/예외 처리

- 콜백 지연/누락: 세션 만료 후 재시작
- state 불일치: 즉시 실패 처리(보안 이벤트)
- Redis 장애: `healthz` 실패로 트래픽 차단, 복구 후 신규 세션만 허용

## 도메인 example.yourdomain.com 적용 체크리스트

1. DNS A/AAAA 또는 CNAME 설정
2. TLS 인증서 발급 (Let's Encrypt 또는 CDN managed)
3. Dropbox App Console Redirect URI 등록:

```text
https://example.yourdomain.com/oauth/dropbox/callback
```

4. 브리지 환경 변수 설정:

```text
REDIRECT_BASE_URL=https://example.yourdomain.com
DROPBOX_APP_KEY=<your_app_key>
POLL_TOKEN_SECRET=<long_random_secret>
```

5. 방화벽/보안 그룹: 443만 외부 공개

## API 요약

- `POST /v1/sessions/start`
- `POST /v1/sessions/{session_id}/status`
- `POST /v1/sessions/{session_id}/consume`
- `GET /oauth/dropbox/callback`
- `GET /healthz`

## 향후 확장 포인트

- rate limit (IP, device_id, session_id)
- observability (Prometheus metrics, structured logs)
- abuse 방지 (WAF, bot detection)
- 다중 테넌트 분리 키 네이밍
