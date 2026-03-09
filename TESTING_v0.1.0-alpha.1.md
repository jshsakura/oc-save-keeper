# oc-save-keeper Alpha Test Guide

## Install

1. Extract the release zip to the root of the SD card.
2. Confirm these files exist:
   - `/switch/OpenCourse/oc-save-keeper/oc-save-keeper.nro`
   - `/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Regular.ttc`
   - `/switch/OpenCourse/oc-save-keeper/fonts/NotoSansCJK-Bold.ttc`
3. Launch from the Homebrew Menu.

## Dropbox Setup

1. Create a Dropbox app with `App folder` access.
2. Generate an access token.
3. Enter the token in oc-save-keeper.

## Suggested Test Pass

1. Launch the app and confirm Korean or English text renders correctly.
2. Select one game with a known save.
3. Create a local backup.
4. Upload to Dropbox.
5. Change data on another device or another backup source.
6. Download from Dropbox and verify:
   - It explains why it accepted or rejected the cloud save.
   - The current save is snapshotted before restore.
7. Launch the actual game and confirm the save is valid.

## If Something Fails

- Check `/switch/OpenCourse/oc-save-keeper/logs/oc-save-keeper.log`
- Note the game title, time, and the action you performed
- Keep the backup folders if restore behavior looks wrong
