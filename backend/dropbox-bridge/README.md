# Dropbox OAuth Bridge (Python + Docker)

This service provides a callback-and-poll bridge for `oc-save-keeper`.

It exists because Dropbox does not support OAuth Device Code Grant, so the Switch app cannot finish pure polling-only auth without an external callback receiver.

## Domain and Redirect URI

If your production domain is `example.yourdomain.com`, register this exact redirect URI in Dropbox App Console:

```text
https://example.yourdomain.com/oauth/dropbox/callback
```

`DROPBOX_BRIDGE_BASE` must match the registered domain.

## Flow (Fully Automatic - No Manual Code Entry)

1. Switch calls `POST /v1/sessions/start`.
2. Bridge generates PKCE `code_verifier` + `code_challenge`, returns `authorize_url`, `session_id`, and `poll_token`.
3. User opens `authorize_url` on phone/PC and approves Dropbox.
4. Dropbox redirects to bridge callback `/oauth/dropbox/callback` with authorization code.
5. Switch polls `POST /v1/sessions/{session_id}/status` automatically.
6. On `approved`, Switch calls `POST /v1/sessions/{session_id}/consume`.
7. Bridge returns `authorization_code` + `code_verifier` (one-time).
8. **Switch exchanges code directly with Dropbox token endpoint** → refresh token is created and stored on Switch.

**Key Points:**
- No manual code copy/paste required - automatic polling completes the flow
- PKCE key exchange is handled automatically by the bridge
- **Refresh token is generated on and stored only on Switch**
- **Bridge never stores access/refresh tokens** - only transient session data

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

## Endpoint Summary

- `GET /healthz`
- `POST /v1/sessions/start`
- `POST /v1/sessions/{session_id}/status`
- `POST /v1/sessions/{session_id}/consume`
- `GET /oauth/dropbox/callback`
