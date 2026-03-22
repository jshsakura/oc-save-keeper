# Dropbox OAuth Bridge (Python + Docker)

This service provides a callback-and-poll bridge for `oc-save-keeper`.

It exists because Dropbox does not support OAuth Device Code Grant, so the device app cannot finish pure polling-only auth without an external callback receiver.

## Domain and Redirect URI

If your production domain is `example.yourdomain.com`, register this exact redirect URI in Dropbox App Console:

```text
https://example.yourdomain.com/oauth/dropbox/callback
```

`DROPBOX_BRIDGE_BASE` must match the registered domain.

## Flow (Fully Automatic - No Manual Code Entry)

1. Device calls `POST /v1/sessions/start`.
2. Bridge generates PKCE `code_verifier` + `code_challenge`, returns `authorize_url`, `session_id`, and `poll_token`.
3. User opens `authorize_url` on phone/PC and approves Dropbox.
4. Dropbox redirects to bridge callback `/oauth/dropbox/callback` with authorization code.
5. Device polls `POST /v1/sessions/{session_id}/status` automatically.
6. On `approved`, device calls `POST /v1/sessions/{session_id}/consume`.
7. Bridge returns `authorization_code` + `code_verifier` (one-time).
8. **Device exchanges code directly with Dropbox token endpoint** → refresh token is created and stored on device.

**Key Points:**
- No manual code copy/paste required - automatic polling completes the flow
- PKCE key exchange is handled automatically by the bridge
- **Refresh token is generated on and stored only on device**
- **Bridge never stores access/refresh tokens** - only transient session data

## Privacy and Trust Boundary

This bridge applies minimum protections such as OAuth `state` validation, HMAC-protected poll tokens, TTL-based session expiry, one-time consume, and rate limiting.

That still does **not** make a public/shared bridge fully trustworthy.

- A third-party operator can still observe connection metadata, timing, and transient OAuth session activity.
- The authorization code passes through the bridge callback before the device exchanges it directly with Dropbox.
- Refresh tokens stay on the device, but trusting a public/shared bridge is still the user's responsibility.

If privacy matters, self-host the bridge on your own domain and keep request/security-event logging disabled unless you explicitly need it for incident response.

## Quick Start

```bash
cp .env.example .env
docker compose up -d --build
```

Health check:

```bash
curl http://localhost:8080/healthz
```

## Environment Variables

- `DROPBOX_APP_KEY`: Dropbox app key.
- `DROPBOX_BRIDGE_BASE`: external HTTPS URL, example `https://example.yourdomain.com`.
- `POLL_TOKEN_SECRET`: random secret for poll token HMAC hashing.
- `REDIS_URL`: redis connection string.
- `STATE_TTL_SECONDS`: state mapping TTL.
- `SESSION_TTL_SECONDS`: auth session TTL.
- `BRIDGE_LOG_LEVEL`: Python/uvicorn application log level. Default: `WARNING`.
- `ENABLE_ACCESS_LOGS`: Enables HTTP access logs when set to `1`. Default: `0`.
- `ENABLE_SECURITY_EVENT_LOGS`: Enables Redis-backed suspicious activity logs when set to `1`. Default: `0`.

## Logging Defaults

Privacy-sensitive defaults are intentional:

- HTTP access logs are disabled by default.
- Suspicious-activity event logs in Redis are disabled by default.
- General application logging defaults to `WARNING`.

Only enable these flags when you explicitly need operational visibility and understand the privacy tradeoff for your users.

## Endpoint Summary

- `GET /healthz`
- `POST /v1/sessions/start`
- `POST /v1/sessions/{session_id}/status`
- `POST /v1/sessions/{session_id}/consume`
- `GET /oauth/dropbox/callback`
