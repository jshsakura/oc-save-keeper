# oc-save-keeper

Safe save backup and cross-device sync for Nintendo Switch homebrew.

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

- Repository: `https://github.com/jshsakura/oc-save-keeper`
- Releases: `https://github.com/jshsakura/oc-save-keeper/releases`

Recommended for most users:

- download the release `.zip`
- extract it directly to the SD card

Advanced/manual option:

- download the standalone `.nro`
- place it inside `/switch/oc-save-keeper/`

## Features

- Local save backups with version history
- Dropbox upload and download
- Device-aware and user-aware revision metadata
- Priority-based conflict handling across multiple devices
- Korean UI for `ko`, English fallback for everything else
- Bundled `NotoSansCJK` fonts for readable Korean and English text

## Install

Extract the release zip to the root of the SD card.

The SD layout should look like this:

```text
/switch/oc-save-keeper/oc-save-keeper.nro
```

Fonts are embedded in the NRO through `romfs`, so there is no separate font copy step.

Runtime data is stored here:

```text
/switch/oc-save-keeper/backups/
/switch/oc-save-keeper/logs/
/switch/oc-save-keeper/temp/
/switch/oc-save-keeper/device_id.txt
/switch/oc-save-keeper/device_label.txt
/switch/oc-save-keeper/device_priority.txt
/switch/oc-save-keeper/dropbox_token.txt
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

### 2. Generate an access token

In the Dropbox app settings:

1. open the app page
2. find the access token section
3. click `Generate`
4. copy the generated token

### 3. Enter the token on Switch

Launch `oc-save-keeper`, open the Dropbox setup screen, and paste the token.

The token is stored locally at:

```text
/switch/oc-save-keeper/dropbox_token.txt
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
- package the SD card layout into a zip
- upload artifacts on workflow runs
- attach assets on tagged releases
