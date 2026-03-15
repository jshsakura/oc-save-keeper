
## 2026-03-16: Header Subtitle Y Position Adjustment

**Change**: SaveShell.cpp line 401
- Subtitle Y coordinate: 54 → 74 (+20px)
- Purpose: Increase gap between App Name (bottom ~52px) and Subtitle (was y=54, only ~2px gap)
- New gap: ~22px (comfortable spacing)

**File**: `source/ui/saves/SaveShell.cpp`
**Function**: `renderHeader()`
**Line**: 401


## ZIP File Validation for Cloud Restore (2026-03-16)

### Implementation Details
- Added file existence and size validation in `SaveManager::importBackupArchive()` before calling `unzipToDirectory()`
- Uses `struct stat` to check file exists and has non-zero size
- `<sys/stat.h>` already included in SaveManager.cpp, added to Dropbox.cpp

### Error Messages
- "Archive file not found" - when stat() fails (file doesn't exist)
- "Archive file is empty" - when st.st_size == 0
- "Failed to extract downloaded archive" - existing message for ZIP extraction failure (corrupted ZIP)

### Debug Logging
- `Dropbox::downloadFile()` logs successful downloads with: dropbox path, local path, HTTP status code, file size in bytes
- `SaveManager::importBackupArchive()` logs successful validation with file path and size
- LOG_ERROR for validation failures, LOG_DEBUG for successful operations

### Code Pattern
```cpp
struct stat st;
if (stat(path.c_str(), &st) != 0) {
    LOG_ERROR("...");
    return false;
}
if (st.st_size == 0) {
    LOG_ERROR("...");
    return false;
}
LOG_DEBUG("... (%ld bytes)", st.st_size);
```

## Cloud Upload Progress Notifications (2026-03-16)

**Change**: Added step-by-step progress notifications during cloud upload

**Files Modified**:
1. `source/ui/saves/SaveBackendAdapter.cpp`:
   - Added `#include "ui/saves/Runtime.hpp"`
   - Line 274: `Runtime::instance().notify("Creating local backup...");` (before createVersionedBackup)
   - Line 290: `Runtime::instance().notify("Uploading to cloud...");` (before Dropbox uploads)

2. `romfs/lang/ko.json` (lines 49-51):
   - `"sync.creating_local": "로컬 세이브 생성중"`
   - `"sync.uploading_dropbox": "세이브 업로드 중"`
   - `"sync.upload_complete": "드롭박스 업로드 완료"`

3. `romfs/lang/en.json` (lines 49-51):
   - `"sync.creating_local": "Creating local backup..."`
   - `"sync.uploading_dropbox": "Uploading to cloud..."`
   - `"sync.upload_complete": "Upload complete"`

**Pattern**: `Runtime::instance().notify()` for UI notifications

