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
- ✅ [Test](#test)
- ⚙️ [Release Automation](#release-automation)
- 📚 [Documentation](#documentation)

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
- **Rate limiting and attack detection**: Backend throttling for bridge abuse protection
- **OAuth state verification**: CSRF mitigation for Dropbox authorization callbacks
- **SSRF-resistant poll URL handling**: Bridge poll URLs are constrained to the configured bridge base

## Quick Start

1. Download `oc-save-keeper.nro` from [GitHub Releases](https://github.com/jshsakura/oc-save-keeper/releases).
2. Copy it to `/switch/` on your SD card.
3. Launch `oc-save-keeper` from the Homebrew Menu.
4. Connect Dropbox and upload your save to cloud.

> **Note**: "Upload to Cloud" automatically creates a local backup before uploading. No need to backup manually first.

If you only want to test the app safely, just browse your save titles and verify the UI works correctly.

## Downloads

- Repository: https://github.com/jshsakura/oc-save-keeper
- Releases: https://github.com/jshsakura/oc-save-keeper/releases

Download `oc-save-keeper.nro` and copy it to:

```text
/switch/oc-save-keeper.nro
```

The app will automatically create its data folder at `/switch/oc-save-keeper/` on first run.

## Repository Layout

```text
.github/workflows/        # CI build/release pipelines
source/                   # App source code
include/                  # Header files
tests/                    # Host unit tests
romfs/lang/               # Runtime language JSON files
romfs/gfx/                # Graphics resources
backend/dropbox-bridge/   # Optional OAuth bridge service
docs/                     # Documentation
  ├── backend/            # Backend architecture docs
  └── frontend/           # Frontend UI docs
RELEASE_NOTES_*.md        # Release notes
```

## Install

Copy `oc-save-keeper.nro` to your SD card:

```text
/switch/oc-save-keeper.nro
```

The app creates its data folder automatically on first run:

```text
/switch/oc-save-keeper/
├── backups/           # Local save backups
├── logs/              # Log files
├── temp/              # Temporary files
└── config/            # Dropbox auth, device settings (settings.json)
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

**Features:**
- No manual code copy/paste - automatic polling completes the flow
- PKCE key exchange handled automatically
- Refresh token stored only on Switch, never on the bridge

*Dropbox does not officially support OAuth Device Code Grant, so this bridge is required for polling-based auth on Switch. You can also self-host this service.*

> **Security note**: The bridge includes minimum safeguards such as OAuth `state` validation, HMAC-protected poll tokens, TTL-based session expiry, one-time consume, and rate limiting. That does **not** make a public or shared bridge fully trustworthy.
>
> If you use a bridge operated by someone else, your OAuth traffic and session metadata still pass through infrastructure you do not control. The bridge does not keep refresh tokens, but the operator can still observe connection metadata and authorization timing. Choosing to trust a public/shared bridge is your responsibility. If privacy matters, self-host the bridge on your own domain and keep logging disabled unless you explicitly need it.

- Service docs: `backend/dropbox-bridge/README.md`
- Architecture notes: `docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md`

If your public domain is `example.yourdomain.com`, register this redirect URI in Dropbox App Console:

```text
https://example.yourdomain.com/oauth/dropbox/callback
```

Then set:

```text
DROPBOX_BRIDGE_BASE=https://example.yourdomain.com
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

- `device_id`: stable internal identifier (auto-generated)
- `device_priority`: larger number wins conflicts

Both are stored in `config/settings.json`. Example:

```json
{
  "device_id": "abc123",
  "device_priority": 200
}
```

Another device might use `device_priority: 100`.

## Basic Usage

1. Launch the app from the Homebrew Menu.
2. Select a title.
3. Connect Dropbox and upload to cloud (local backup is created automatically).
4. On another device, download from cloud and review the sync decision.
5. Restore only after confirming the source device and user are correct.

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
4. Connect Dropbox
5. Choose `Upload to Cloud` (local backup is created automatically)

After that, the same title on another device can use `Download from Cloud`.

### Recommended Workflow

For safer testing, use this order:

1. Connect Dropbox
2. Upload to cloud (local backup is created automatically)
3. Switch devices
4. Download from cloud
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

## Building from Source

### Build Environment

This project targets Nintendo Switch and requires devkitPro. If devkitPro is not installed locally, use the Docker image.

**Docker (recommended):**

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```

**Local devkitPro:**

```bash
make
```

### Build with Dropbox App Key

Option 1: `.env` file (recommended for local development):

```bash
echo "DROPBOX_APP_KEY=your_app_key" > .env
make
```

Option 2: Command-line argument:

```bash
make DROPBOX_APP_KEY="your_app_key"
```

For GitHub Actions, set the `DROPBOX_APP_KEY` repository secret.

### Build Output

```text
oc-save-keeper.nro
```

### Development Notes

- **Include dependency**: When modifying `SaveShell.hpp`, ensure `network/Dropbox.hpp` is included to recognize `DropboxBridgeSession` type.
- **Localization**: When adding new UI strings, update both `romfs/lang/ko.json` and `romfs/lang/en.json`.

## Test

Run host unit tests locally:

```bash
make test
```

### Test Coverage

| Module | Tests | Status |
|--------|-------|--------|
| Dropbox util | 17 | ✅ |
| Metadata logic | 4 | ✅ |
| Metadata file | 6 | ✅ |
| Sync logic | 3 | ✅ |
| **Total** | **30** | ✅ |

See [docs/TESTING.md](docs/TESTING.md) for detailed test checklist.

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

## Documentation

| Document | Description |
|----------|-------------|
| [docs/TESTING.md](docs/TESTING.md) | Test guide and checklist |
| [docs/TDD.md](docs/TDD.md) | TDD workflow guide |
| [docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md](docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md) | Dropbox bridge architecture (Korean) |
| [docs/frontend/SAVE_UI_MIGRATION.md](docs/frontend/SAVE_UI_MIGRATION.md) | UI migration notes |

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K - Original save manager that inspired this project
- [devkitPro](https://devkitpro.org/) - Toolchain for Nintendo Switch homebrew development
- [libnx](https://github.com/switchbrew/libnx) - Nintendo Switch homebrew library
