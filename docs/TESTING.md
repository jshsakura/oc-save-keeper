# oc-save-keeper Test Guide

## Build & Test

```bash
# Device build (produces .nro)
make

# Host build for unit tests
make test
```

## Test Coverage

| Module | Tests | Status |
|--------|-------|--------|
| Dropbox util | 17 | ✅ |
| Metadata logic | 4 | ✅ |
| Metadata file | 6 | ✅ |
| Sync logic | 3 | ✅ |
| **Total** | **30** | ✅ |

## Install (Device)

1. Extract release zip to SD card root
2. Verify: `/switch/oc-save-keeper/oc-save-keeper.nro`
3. Launch from Homebrew Menu

## Dropbox Setup

1. Create Dropbox app with `App folder` access at https://www.dropbox.com/developers/apps
2. Set `DROPBOX_APP_KEY` in `.env` or GitHub secret
3. In app: scan QR code → approve → paste code

## Test Checklist

### Basic
- [ ] App launches with correct language (ko/en)
- [ ] Game list renders properly
- [ ] Local backup creates successfully
- [ ] Backup appears in list with correct metadata

### Cloud Sync
- [ ] Dropbox OAuth flow completes
- [ ] Upload to cloud succeeds
- [ ] Download from cloud shows sync decision
- [ ] Restore updates save correctly

### Safety
- [ ] Pre-restore snapshot created
- [ ] Cross-user restore blocked
- [ ] ZIP validation before restore

## Troubleshooting

- Log file: `/switch/oc-save-keeper/logs/oc-save-keeper.log`
- Report: game title, device label, action performed, expected vs actual
