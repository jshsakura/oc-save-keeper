# oc-save-keeper

Safe save backup and cross-device sync for Nintendo Switch homebrew.

Language: **English** | [한국어](README.ko.md)

![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012?style=for-the-badge&logo=nintendo-switch&logoColor=white)
![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-devkitPro-1f7a8c?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-2ea043?style=for-the-badge)

## What This Project Does

`oc-save-keeper` is built around one simple goal: make save backup and restore safer across multiple devices and users.

- 🗂️ **Local first**: create versioned local backups before any risky restore.
- ☁️ **Cloud sync**: upload and download through Dropbox.
- 🛡️ **Safer decisions**: compare metadata (device/user/title/revision) before restore.
- 🌏 **Bilingual UI**: Korean (`ko`) + English fallback.

## Credits

- Original project reference: [JKSV](https://github.com/J-D-K/JKSV)
- This project is inspired by the Nintendo Switch save-management ecosystem and adds backup metadata, device-aware sync decisions, and Dropbox-focused workflows.

## Quick Navigation

- 🚀 [Quick Start](#quick-start)
- 📦 [Install](#install)
- ☁️ [Dropbox Setup](#dropbox-setup)
- 🌉 [Dropbox Bridge (Optional)](#dropbox-bridge-optional)
- 🔄 [How Sync Decisions Work](#how-sync-decisions-work)
- 🧪 [Build](#build)
- ⚙️ [Release Automation](#release-automation)

`oc-save-keeper` is a save manager for Nintendo Switch homebrew that focuses on three things:

- simple local backups
- cloud sync with Dropbox
- safer cross-device restore decisions

The app keeps track of which device and which user created a backup, then uses that metadata to explain why a cloud save is accepted or rejected.

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

Advanced/manual option:

- download the standalone `.nro`
- place it at `/switch/oc-save-keeper/oc-save-keeper.nro`

## Repository Layout

Important project paths in this repository:

```text
.github/workflows/release.yml    # CI build/release pipeline
source/                          # app source code
include/                         # headers
tests/                           # host unit tests
romfs/lang/                      # runtime language JSON files
RELEASE_NOTES_v0.1.0-alpha.1.md  # current release notes doc
TESTING_v0.1.0-alpha.1.md        # current test checklist doc
```

## Features

- Local save backups with version history
- Dropbox upload and download
- Device-aware and user-aware revision metadata
- Priority-based conflict handling across multiple devices
- Korean UI for `ko`, English fallback for everything else
- Nintendo Switch shared font rendering for Korean and English text

### Feature Highlights

- 📚 **Version history**: keep multiple recovery points per game.
- 🧭 **Source clarity**: show which device and user created each backup.
- ⚖️ **Conflict reasoning**: explain why local/cloud was preferred.
- 🔐 **Safety by default**: block dangerous cross-user overwrite flows.

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

Command-line override still works if you want it, but `.env` is the default path now.

For GitHub Actions builds, set repository secret `DROPBOX_APP_KEY`.

### 3. Connect on Switch

Launch `oc-save-keeper`, open the Dropbox setup screen, then:

1. press `Open Sign-In`
2. scan the QR code (or copy the generated Dropbox authorization link) and open it on your phone or PC
3. approve the app
4. copy the returned authorization code or the full redirected URL
5. paste it into the Switch and press `Connect Dropbox`

The Switch build intentionally does not launch the Dropbox browser window directly anymore. This avoids web applet instability under Atmosphere and keeps the OAuth step on a device with a full browser.

The app stores the resulting OAuth session here:

```text
/switch/oc-save-keeper/config/dropbox_auth.json
```

## Dropbox Bridge (Optional)

If you want callback-and-poll UX instead of manual code paste, use the Python bridge service under `backend/dropbox-bridge`.

- Service docs: `backend/dropbox-bridge/README.md`
- Architecture and load notes: `docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md`

If your public domain is `save.opencourse.kr`, register this redirect URI in Dropbox App Console:

```text
https://save.opencourse.kr/oauth/dropbox/callback
```

Then set:

```text
REDIRECT_BASE_URL=https://save.opencourse.kr
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

If you use more than one Switch or more than one setup, each device should keep its own identity:

- `device_id.txt`: stable internal identifier
- `device_label.txt`: human-readable device name
- `device_priority.txt`: larger number means the device wins conflicts more often

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

With this setup, the UI can show:

- which device created the backup
- which user it belongs to
- why the app kept local data or accepted cloud data

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

1. launch `oc-save-keeper`
2. confirm text renders correctly
3. open one game that already has a save
4. choose `Local Backup`
5. connect Dropbox
6. choose `Upload to Cloud`

After that, the same title on another device can use `Download from Cloud`.

### What You Will See

On the title detail screen, the app shows:

- save size
- latest backup device
- latest backup user
- latest backup source
- current Dropbox connection state

In version history, each entry is labeled with:

- timestamp
- device label
- user name
- source

This is intentional. The goal is to make cross-device restores understandable even for non-technical users.

### Recommended Workflow

For safer testing, use this order:

1. create a local backup
2. upload to Dropbox
3. switch devices
4. download from Dropbox
5. review the decision reason
6. restore only if the device and user match your intent

### Important Safety Notes

- Do not test restore logic with your only copy of an important save.
- Keep at least one known-good local backup before trying cross-device restores.
- If the app says a backup came from another device, stop and read the device label before restoring.

## Troubleshooting

If something fails, check:

```text
/switch/oc-save-keeper/logs/oc-save-keeper.log
```

Useful things to record when reporting an issue:

- game title
- local device label
- selected user
- exact action you performed
- whether the app said local or cloud was preferred

## Known Issues

- Dropbox token entry still needs a more polished on-device input flow.
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

GitHub Actions can build the release package automatically.

The workflow is located at:

```text
.github/workflows/release.yml
```

It is designed to:

- build `oc-save-keeper.nro`
- package `dist/oc-save-keeper-<ref>.zip`
- upload artifacts on workflow runs
- publish a `latest` prerelease on every `main` push
- attach assets on tagged releases (`v*`)
