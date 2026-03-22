# oc-save-keeper

Save backup manager with cloud sync support.

Language: **English** | [한국어](README.ko.md)

![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-devkitPro-1f7a8c?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-2ea043?style=for-the-badge)

## Overview

Local save backups with version history and Dropbox cloud sync. Device-aware metadata tracking enables safe cross-device restore decisions.

## Build

Requires devkitPro toolchain.

**Docker (recommended):**

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```

**Local devkitPro:**

```bash
make
```

### Dropbox Integration

Create a `.env` file in the project root:

```bash
DROPBOX_APP_KEY=your_dropbox_app_key
```

Then build:

```bash
make
```

For GitHub Actions, set the `DROPBOX_APP_KEY` repository secret.

### Build Output

```text
oc-save-keeper.nro
```

## Install

Copy `oc-save-keeper.nro` to:

```text
/switch/oc-save-keeper.nro
```

The app creates its data folder at `/switch/oc-save-keeper/` on first run:

```text
/switch/oc-save-keeper/
├── backups/    # Local save backups
├── logs/       # Log files
├── temp/       # Temporary files
└── config/     # Dropbox auth, device settings
```

## Repository Layout

```text
source/                   # App source code
include/                  # Header files
tests/                    # Host unit tests
romfs/lang/               # Language JSON files
romfs/gfx/                # Graphics resources
backend/dropbox-bridge/   # Optional OAuth bridge service
docs/                     # Documentation
```

## Test

```bash
make test
```

## Documentation

| Document | Description |
|----------|-------------|
| [docs/TESTING.md](docs/TESTING.md) | Test guide |
| [docs/TDD.md](docs/TDD.md) | TDD workflow guide |
| [docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md](docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md) | Bridge architecture (Korean) |
| [docs/frontend/SAVE_UI_MIGRATION.md](docs/frontend/SAVE_UI_MIGRATION.md) | UI migration notes |

## License

MIT License - see [LICENSE](LICENSE) for details.
