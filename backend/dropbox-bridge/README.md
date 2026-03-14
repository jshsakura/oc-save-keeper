# Dropbox OAuth Bridge (Python + Docker)

This service provides a callback-and-poll bridge for `oc-save-keeper`.

It exists because Dropbox does not support OAuth Device Code Grant, so the Switch app cannot finish pure polling-only auth without an external callback receiver.

## Domain and Redirect URI

If your production domain is `save.opencourse.kr`, register this exact redirect URI in Dropbox App Console:

```text
https://save.opencourse.kr/oauth/dropbox/callback
```

`REDIRECT_BASE_URL` must match the registered domain.

## Flow

1. Switch calls `POST /v1/sessions/start`.
2. Bridge returns `authorize_url`, `session_id`, and `poll_token`.
3. User opens `authorize_url` on phone/PC and approves Dropbox.
4. Dropbox redirects to bridge callback `/oauth/dropbox/callback`.
5. Switch polls `POST /v1/sessions/{session_id}/status`.
6. On `approved`, Switch calls `POST /v1/sessions/{session_id}/consume`.
7. Bridge returns `authorization_code` + `code_verifier`.
8. Switch exchanges code directly with Dropbox token endpoint.

In this model, the bridge does not store Dropbox access/refresh tokens.

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
- `REDIRECT_BASE_URL`: external HTTPS URL, example `https://save.opencourse.kr`.
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
