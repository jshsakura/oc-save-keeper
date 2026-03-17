# oc-save-keeper

Safe save backup and cross-device sync for Nintendo Switch homebrew.

Language: **English** | [한국어](README.ko.md)

![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012?style=for-the-badge&logo=nintendo-switch&logoColor=white)
![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-devkitPro-1f7a8c?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-2ea043?style=for-the-badge)

## What This Project Does

`oc-save-keeper` is a save manager for Nintendo Switch homebrew focused on **safe** backup and restore across multiple devices and users.

- 🗂️ **Local first**: versioned local backups before any risky restore
- ☁️ **Cloud sync**: upload and download through Dropbox
- 🛡️ **Safer decisions**: compare metadata (device/user/title/revision) before restore
- 🌏 **Bilingual UI**: Korean (`ko`) + English fallback
- 🔄 **Auto-rollback**: JKSV-style safety restoration with automatic rollback on failure

## Credits & Attribution

This project is **inspired by and references** [JKSV](https://github.com/J-D-K/JKSV) by J-D-K.

`oc-save-keeper` extends the Nintendo Switch save-management ecosystem with:

- Backup metadata tracking (device, user, title, revision, timestamp)
- Device-aware sync decisions with priority-based conflict handling
- Dropbox-focused cloud workflows with optional bridge service
- JKSV-style safety restoration engine with physical commits and auto-rollback

If you find this project useful, please also check out the original [JKSV](https://github.com/J-D-K/JKSV) project.

## Quick Navigation

- 🚀 [Quick Start](#quick-start)
- 📦 [Install](#install)
- ☁️ [Dropbox Setup](#dropbox-setup)
- 🌉 [Dropbox Bridge (Optional)](#dropbox-bridge-optional)
- 🔄 [How Sync Decisions Work](#how-sync-decisions-work)
- 🧪 [Build](#build)
- ⚙️ [Release Automation](#release-automation)

## Features

### Core Features

- Local save backups with version history
- Dropbox upload and download
- Device-aware and user-aware revision metadata
- Priority-based conflict handling across multiple devices
- Korean UI for `ko`, English fallback for everything else
- Nintendo Switch shared font rendering for Korean and English text

### Safety Features

- **JKSV-style restoration engine**: Physical commits with auto-rollback on failure
- **ZIP validation**: Verify archive integrity before restore
- **Pre-restore snapshots**: Current save is automatically backed up before any restore
- **Cross-user protection**: Automatic overwrite is blocked for different users

### Cloud Features

- **QR code authentication**: Scan QR to authorize Dropbox on your phone/PC
- **Dropbox bridge service**: Optional Python backend for callback-and-poll OAuth UX
- **Title-centric cloud paths**: Clean organization by game title
- **Multi-save support**: Backup and restore multiple saves per title

### UI/UX

- **SaveShell workflow**: Modern, streamlined interface
- **Catppuccin Mocha theme**: Smooth animations and large buttons
- **Toast notifications**: Upload progress and operation feedback
- **Applet mode detection**: Better compatibility across environments

### Security

- **SHA-512 integrity verification**: Strong hash for backup validation
- **Rickroll guard**: Protection against unsecured HTTP requests
- **Rate limiting**: Backend protection for bridge service

## Quick Start

1. Download the latest release zip from GitHub Releases.
2. Extract it to the root of the SD card.
3. Launch `oc-save-keeper` from the Homebrew Menu.
4. Create a local backup first.
5. Connect Dropbox and upload the backup.

If you only want to test the app safely, stop after the first local backup and verify that the backup folder and UI look correct.

## Downloads

- Repository: https://github.com/jshsakura/oc-save-keeper
- Releases: https://github.com/jshsakura/oc-save-keeper/releases

Recommended for most users:

- download the release `.zip` from Releases
- extract it to the SD root so this exact path exists:

```text
/switch/oc-save-keeper/oc-save-keeper.nro
```

## Repository Layout

```text
.github/workflows/        # CI build/release pipelines
source/                   # App source code
include/                  # Header files
tests/                    # Host unit tests
romfs/lang/               # Runtime language JSON files
backend/dropbox-bridge/   # Optional OAuth bridge service
docs/                     # Documentation
RELEASE_NOTES_*.md        # Release notes
```

## Install

Extract the release zip to the root of the SD card.

After extraction, verify this exact runtime file path:

```text
/switch/oc-save-keeper/oc-save-keeper.nro
```

Runtime data is stored here:

```text
/switch/oc-save-keeper/backups/
/switch/oc-save-keeper/logs/
/switch/oc-save-keeper/temp/
/switch/oc-save-keeper/device_id.txt
/switch/oc-save-keeper/device_label.txt
/switch/oc-save-keeper/device_priority.txt
/switch/oc-save-keeper/config/dropbox_auth.json
```

## Dropbox Setup

### 1. Create a Dropbox app

Open:

```text
https://www.dropbox.com/developers/apps
```

Create an app with:

- API: `Dropbox API`
- Access: `App folder`
- App name: something like `OCSaveKeeper-Backup`

### 2. Put your Dropbox app key in `.env`

Create a file named `.env` in the project root:

```bash
DROPBOX_APP_KEY=your_dropbox_app_key
```

Then build normally:

```bash
make
```

For GitHub Actions builds, set repository secret `DROPBOX_APP_KEY`.

### 3. Connect on Switch

Launch `oc-save-keeper`, open the Dropbox setup screen, then:

1. Press `Open Sign-In`
2. Scan the QR code with your phone (or copy the authorization link to your PC)
3. Approve the app on Dropbox
4. Copy the returned authorization code or full redirected URL
5. Paste it into the Switch and press `Connect Dropbox`

The app stores the OAuth session here:

```text
/switch/oc-save-keeper/config/dropbox_auth.json
```

## Dropbox Bridge (Optional)

For automatic callback-and-poll auth (no manual code entry), use the Python bridge service.

**Benefits:**
- No manual code copy/paste - automatic polling completes the flow
- PKCE key exchange handled automatically
- Refresh token stored only on Switch, never on the bridge

- Service docs: `backend/dropbox-bridge/README.md`
- Architecture notes: `docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md`

If your public domain is `example.yourdomain.com`, register this redirect URI in Dropbox App Console:

```text
https://example.yourdomain.com/oauth/dropbox/callback
```

Then set:

```text
REDIRECT_BASE_URL=https://example.yourdomain.com
```

## How Sync Decisions Work

Every backup carries metadata for:

- device
- user
- title
- revision
- timestamp
- device priority

This lets the app tell the difference between:

- the same game on the same user
- the same user on a different device
- a completely different user save

Current behavior:

- same `title` and same `user`: can be compared automatically
- different `device`: shown as a different save source
- different `user`: automatic overwrite is blocked
- manual restore from another device is still allowed
- current save is snapshotted before an incoming restore is applied

## Multiple Devices

If you use more than one Switch, each device should keep its own identity:

- `device_id.txt`: stable internal identifier
- `device_label.txt`: human-readable device name
- `device_priority.txt`: larger number wins conflicts

Example:

```text
device_label.txt     -> OLED Main
device_priority.txt  -> 200
```

Another device might use:

```text
device_label.txt     -> Lite Backup
device_priority.txt  -> 100
```

## Basic Usage

1. Launch the app from the Homebrew Menu.
2. Select a title.
3. Create a local backup first.
4. Upload the backup to Dropbox.
5. On another device, download and review the sync decision.
6. Restore only after confirming the source device and user are correct.

### Controls

- `A`: select or confirm
- `B`: go back
- `X`: sync all visible titles
- `+`: exit the app

### Typical First Run

If this is your first time using the app:

1. Launch `oc-save-keeper`
2. Confirm text renders correctly
3. Open one game that already has a save
4. Choose `Local Backup`
5. Connect Dropbox
6. Choose `Upload to Cloud`

After that, the same title on another device can use `Download from Cloud`.

### Recommended Workflow

For safer testing, use this order:

1. Create a local backup
2. Upload to Dropbox
3. Switch devices
4. Download from Dropbox
5. Review the decision reason
6. Restore only if the device and user match your intent

### Important Safety Notes

- Do not test restore logic with your only copy of an important save.
- Keep at least one known-good local backup before trying cross-device restores.
- If the app says a backup came from another device, stop and read the device label before restoring.
- The app creates a pre-restore snapshot automatically, but you should still be cautious.

## Troubleshooting

If something fails, check:

```text
/switch/oc-save-keeper/logs/oc-save-keeper.log
```

Useful things to record when reporting an issue:

- Game title
- Local device label
- Selected user
- Exact action you performed
- Whether the app said local or cloud was preferred

## Known Issues

- Cloud version browsing is not implemented yet; the current flow focuses on the latest synchronized backup.
- Cross-device restore is supported, but users should still verify the source device label before restoring.
- This project is still in alpha, so restore behavior should be tested carefully with non-critical saves first.

## Build

Build with the devkitPro container:

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```

The build output is:

```text
oc-save-keeper.nro
```

## Release Automation

GitHub Actions builds the release package automatically.

The workflow is located at:

```text
.github/workflows/release.yml
```

It is designed to:

- Build `oc-save-keeper.nro`
- Package `dist/oc-save-keeper-<ref>.zip`
- Upload artifacts on workflow runs
- Publish a `latest` prerelease on every `main` push
- Attach assets on tagged releases (`v*`)

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K - Original save manager that inspired this project
- [devkitPro](https://devkitpro.org/) - Toolchain for Nintendo Switch homebrew development
- [libnx](https://github.com/switchbrew/libnx) - Nintendo Switch homebrew library
