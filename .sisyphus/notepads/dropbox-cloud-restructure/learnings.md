
## 2026-03-16: Title-centric Cloud Path Restructure

### Changes Made
- **File**: `source/core/SaveManager.cpp`
- **Lines**: 683-717

### New Path Structure
| Function | Old Path | New Path |
|----------|----------|----------|
| `getCloudUserPath()` | `oc-save-keeper/users/{userId}` | `users/{userId}` |
| `getCloudTitlePath()` | `{userPath}/titles/{titleId}` | `titles/{titleId}` |
| `getCloudPath()` | `{userPath}/titles/{titleId}/latest.zip` | `titles/{titleId}/latest.zip` |
| `getCloudMetadataPath()` | `{userPath}/titles/{titleId}/latest.meta` | `titles/{titleId}/latest.meta` |
| `getCloudRevisionDirectory()` | `{devicePath}/titles/{titleId}/revisions` | `titles/{titleId}/revisions` |

### Key Decisions
1. **Removed "oc-save-keeper/" prefix** - Dropbox app folder is already scoped to `oc-save-keeper`
2. **Title-centric paths** - userId/deviceId stored in metadata, not path
3. **Kept deprecated functions** - `getCloudDevicesPath()`, `getCloudDevicePath()` retained but unused

### Why This Matters
- Old path: `/oc-save-keeper/oc-save-keeper/users/{userId}/titles/{titleId}/latest.zip` (duplication)
- New path: `/oc-save-keeper/titles/{titleId}/latest.zip` (clean)
- Cross-device restore works via metadata comparison, not path structure

### Wave 2 Dependencies
- `SaveBackendAdapter` uses these functions (needs update)
- Dropbox operations will auto-use new paths via `getCloudTitlePath()`

---

## Task 4: upload() Path Verification (2026-03-16)

### File: `source/ui/saves/SaveBackendAdapter.cpp`

### Verification Result: ✅ NO CHANGES NEEDED

**Code Review (lines 295-306):**
```cpp
const std::string latestArchivePath = "/" + m_saveManager.getCloudPath(title);
const std::string latestMetaPath = "/" + m_saveManager.getCloudMetadataPath(title);
const std::string revisionDir = "/" + m_saveManager.getCloudRevisionDirectory(title);
```

### Why No Changes Needed
1. `upload()` already calls `getCloudPath()` - not hardcoded
2. Task 3 changed `getCloudPath()` to return `"titles/{titleId}/latest.zip"`
3. This change automatically propagates to `upload()`
4. Upload flow: revision files → revisionDir, latest files → getCloudPath()

### Upload Logic (unchanged, correct)
- Line 303: `m_dropbox.uploadFile(localMetaPath, revisionMetaPath)`
- Line 304: `m_dropbox.uploadFile(archivePath, revisionArchivePath)`
- Line 305: `m_dropbox.uploadFile(localMetaPath, latestMetaPath)`
- Line 306: `m_dropbox.uploadFile(archivePath, latestArchivePath)`

---

## Task 5: listTitles() Path Update (2026-03-16)

### File: `source/ui/saves/SaveBackendAdapter.cpp`

### Change: ✅ COMPLETED

**Before (lines 127-128):**
```cpp
const std::string userId = m_saveManager.getDeviceId();
const std::string remotePath = "/users/" + userId + "/titles";
```

**After (line 127):**
```cpp
const std::string remotePath = "/titles";
```

### Why This Change
- Wave 1 changed `getCloudTitlePath()` to return `"titles/{titleId}"` (no user prefix)
- `listTitles()` must query the same path structure
- Removed unused `getDeviceId()` call

### Verification
- Grep confirms no remaining `/users/.*userId.*titles` pattern
