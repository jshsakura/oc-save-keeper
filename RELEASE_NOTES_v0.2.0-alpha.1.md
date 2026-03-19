# oc-save-keeper v0.2.0-alpha.1

Second alpha build with major safety and UX improvements.

## New Features

### Safety Restoration Engine
- JKSV-style safety restoration with physical commits and auto-rollback
- ZIP validation before restore operations
- Current save is snapshotted before any incoming restore is applied

### Dropbox Bridge Service
- Optional Python backend service for OAuth callback-and-poll UX
- QR code-based Dropbox authentication with bridge session polling
- Retry logic for failed Dropbox login sessions
- Title-centric Dropbox path model for cleaner cloud organization

### Multi-Save Support
- Account/device save support for multiple users
- Multi-save backup and restore per title

### UI/UX Improvements
- SaveShell workflow replacing legacy MainUI
- Applet mode detection for better compatibility
- Current device labels displayed in UI
- Catppuccin Mocha theme with smooth animations
- Drifting background animation and toast notifications
- Smooth interpolated scrolling and motion transitions
- Sidebar widened to 40% for better readability
- Improved list navigation with refresh notifications
- Upload progress notifications

### Security
- Rickroll guard for unsecured HTTP requests
- SHA-512 hash upgrade for integrity verification
- Rate limiting support in backend

### CI/Deployment
- Docker multi-platform build workflow for backend
- Redis integration for bridge service
- Separate app release and backend docker builds with path filters
- Auto-publish prerelease on main pushes

## Included

- `oc-save-keeper.nro`
- Korean and English UI strings
- Nintendo Switch shared font rendering for Korean and English text
- Metadata-aware cloud sync flow
- Device priority based conflict handling
- Local version history and restore flow
- Dropbox bridge backend (optional)

## Install Layout

- `/switch/oc-save-keeper/oc-save-keeper.nro`
- `/switch/oc-save-keeper/backups/...`
- `/switch/oc-save-keeper/logs/...`
- `/switch/oc-save-keeper/temp/...`
- `/switch/oc-save-keeper/config/dropbox_auth.json`
- `/switch/oc-save-keeper/config/settings.json` (device_id, device_priority)

## Important Notes

- This is an alpha build intended for real hardware testing.
- Always keep a separate backup of important saves before testing restore paths.
- Language policy:
  - System language `ko` -> Korean UI
  - Everything else -> English UI
- Dropbox bridge is optional. Manual OAuth code entry still works.

## Known Limits

- Cloud version browsing is not implemented yet.
- Conflict handling is currently optimized for automatic decision plus clear reason text.

## What To Test

- App launch and font rendering
- Title scan and game list
- Local backup creation
- Dropbox OAuth via QR code or manual code entry
- Conflict decisions across two devices with different priority values
- Restore result inside the actual game
- Multi-save backup/restore per title
- Auto-rollback after failed restore

## Upgrade from v0.1.0-alpha.1

- Existing local backups are compatible
- Dropbox cloud paths have been restructured to title-centric model
- Old cloud backups may need to be re-uploaded for proper organization
- Device identity (device_id, device_priority in settings.json) is preserved

## Changelog Summary

### Added
- JKSV-style safety restoration engine with auto-rollback
- Dropbox bridge backend service
- QR code-based Dropbox authentication
- Multi-save support per title
- Applet mode detection
- Catppuccin Mocha theme with animations
- Toast notifications
- Upload progress notifications
- SHA-512 integrity verification

### Changed
- Dropbox paths restructured to title-centric model
- MainUI replaced with SaveShell workflow
- Sidebar width increased to 40%
- Force overwrite on explicit cloud restore
- Download .zip instead of .meta on restore

### Fixed
- Header and footer text positioning
- Local/Cloud color indicators
- List navigation and scrolling
- Texture destruction bug causing disappearing icons
