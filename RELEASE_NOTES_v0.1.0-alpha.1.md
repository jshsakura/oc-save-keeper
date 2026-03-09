# oc-save-keeper v0.1.0-alpha.1

First public alpha build for on-device testing.

## Included

- `oc-save-keeper.nro`
- Korean and English UI strings
- Nintendo Switch shared font rendering for Korean and English text
- Metadata-aware cloud sync flow
- Device priority based conflict handling
- Local version history and restore flow

## Install Layout

- `/switch/oc-save-keeper/oc-save-keeper.nro`
- `/switch/oc-save-keeper/backups/...`

## Important Notes

- This is an alpha build intended for real hardware testing.
- Always keep a separate backup of important saves before testing restore paths.
- Language policy is intentionally simple:
  - System language `ko` -> Korean UI
  - Everything else -> English UI

## Known Limits

- Dropbox token entry flow still needs more polishing.
- Cloud version browsing is not implemented yet.
- Conflict handling is currently optimized for automatic decision plus clear reason text.

## What To Test

- App launch and font rendering
- Title scan and game list
- Local backup creation
- Dropbox upload and download
- Conflict decisions across two devices with different priority values
- Restore result inside the actual game
