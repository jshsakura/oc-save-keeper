## 2026-03-16: Cloud Restore Path Fix

### Change
- Fixed `source/ui/saves/RevisionMenuScreen.cpp:104`
- Changed `entry.id` to `entry.path` for cloud download

### Context
- `entry.id` contains `.meta` path (used as unique identifier)
- `entry.path` contains `.zip` path (actual archive to download)
- Bug caused app to download metadata files instead of ZIP archives

### Build Note
- RevisionMenuScreen.cpp compiles successfully
- Pre-existing error in SaveMenuScreen.cpp:142 (`SDL_Delay` undeclared) blocks full build

