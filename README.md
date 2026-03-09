# oc-save-keeper

Safe save backup and cross-device sync for Nintendo Switch homebrew.

## Features

- Local save backups with version history
- Cloud sync with metadata-aware conflict checks
- Device and user aware revision tracking
- Korean UI for `ko`, English fallback for everything else

## Install

Extract the package so the SD card contains:

```text
/switch/OpenCourse/oc-save-keeper/oc-save-keeper.nro
/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Regular.ttc
/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Bold.ttc
```

## Dropbox Setup

1. Open `dropbox.com/developers/apps`
2. Create an app with `App folder` access
3. Generate an access token
4. Launch `oc-save-keeper` and paste the token

## Build

Use the devkitPro container:

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```
