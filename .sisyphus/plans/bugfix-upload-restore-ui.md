# Bug Fixes: ZIP Restore, Header Spacing, Upload Progress

## TL;DR

> **Quick Summary**: Fix three bugs in oc-save-keeper: (1) Add ZIP file validation + debug logging for cloud restore failures, (2) Increase header subtitle spacing from ~2px to ~22px, (3) Add step-by-step progress notifications during cloud upload.
> 
> **Deliverables**:
> - ZIP validation with file existence, size checks, and debug logging
> - Header subtitle repositioned from Y=54 to Y=74
> - Upload progress notifications in Korean and English
> 
> **Estimated Effort**: Short (3 focused tasks)
> **Parallel Execution**: NO - sequential (different files per task)
> **Critical Path**: Task 1 → Task 2 → Task 3 (can be parallel but different concerns)

---

## Context

### Original Request
User reported three issues:
1. **ZIP extraction failure**: `Failed to open ZIP: /switch/oc-save-keeper/temp/frontend_010086A022444000_1773601484.zip` - temp folder appears empty
2. **Header spacing**: Subtitle too close to title, needs ~20px more spacing
3. **Upload progress missing**: No status shown during upload - wants "로컬 세이브 생성중", "세이브 업로드 중", "드롭박스 업로드 완료"

### Interview Summary
**Key Discussions**:
- **ZIP Fix Scope**: User chose "Validation + Logging" - add file checks AND debug logs for network/file errors
- **Upload Progress**: User chose "Step-by-step Notifications" - simple notifications between phases, NOT full async refactor
- **Testing**: User will test on actual Switch hardware

**Research Findings**:
- ZIP issue: No file validation before extraction, no size check after download
- Header spacing: Subtitle at Y=54, title bottom edge at ~52px = ~2px gap
- Upload flow: 3 phases (local backup → ZIP export → Dropbox upload), only shows one notification

### Metis Review
**Identified Gaps** (addressed):
- **Language files**: Found both `en.json` AND `ko.json` - must update BOTH
- **Guardrails**: Do NOT add retry logic, progress bars, or refactor surrounding code
- **Edge cases**: Handle missing file, empty file, corrupted ZIP, partial extraction

---

## Work Objectives

### Core Objective
Fix three specific bugs without introducing regressions or scope creep.

### Concrete Deliverables
1. ZIP file validation + debug logging in `SaveManager::importBackupArchive()` and `Dropbox::downloadFile()`
2. Subtitle Y position change from 54 to 74 in `SaveShell.cpp:401`
3. Progress notification keys in `en.json` and `ko.json`, notification calls in `SaveBackendAdapter::upload()`

### Definition of Done
- [x] Cloud restore shows detailed error when ZIP is missing/empty/corrupted
- [x] Header subtitle has ~20px more spacing from title
- [x] Upload shows step-by-step status: "Creating local backup..." → "Uploading to cloud..." → "Upload complete"
- [x] All existing functionality preserved (no regressions)

### Must Have
- File existence check before ZIP extraction
- File size > 0 validation before ZIP extraction
- Debug logging for file validation failures
- Header subtitle repositioned to Y=74
- Progress notifications in BOTH en.json and ko.json

### Must NOT Have (Guardrails from Metis)
- ❌ DO NOT add retry logic for network failures
- ❌ DO NOT refactor surrounding code in any file
- ❌ DO NOT add progress bars - only text notifications
- ❌ DO NOT fix other UI spacing issues
- ❌ DO NOT change the upload algorithm - only add notification calls
- ❌ DO NOT add new dependencies

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed.

### Test Decision
- **Infrastructure exists**: NO dedicated test infrastructure for Switch hardware
- **Automated tests**: None - manual hardware testing
- **Framework**: N/A
- **Agent-Executed QA**: Build verification + manual test scenarios documented

### QA Policy
Every task MUST include agent-executed QA scenarios:
- **Build Verification**: `make` compiles successfully
- **Manual Test Scenarios**: Documented steps for hardware testing

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Sequential - different concerns):
├── Task 1: ZIP validation + debug logging [quick]
├── Task 2: Header subtitle spacing [quick]
└── Task 3: Upload progress notifications [quick]

Wave FINAL (After ALL tasks — verification):
├── Task F1: Build verification [quick]
└── Task F2: Scope fidelity check [quick]

Critical Path: Task 1 → Task 2 → Task 3 → F1 → F2
```

### Dependency Matrix
- **1**: — — 2, 3, 1 (independent)
- **2**: — — 3, 1 (independent)
- **3**: — — F1, 1 (independent)

### Agent Dispatch Summary
- **1**: **1** — T1 → `quick`
- **2**: **1** — T2 → `quick`
- **3**: **1** — T3 → `quick`
- **FINAL**: **2** — F1 → `quick`, F2 → `quick`

---

## TODOs

> Implementation + Test = ONE Task. Never separate.
> EVERY task MUST have: Recommended Agent Profile + Parallelization info + QA Scenarios.

- [x] 1. **ZIP File Validation + Debug Logging**

  **What to do**:
  - Add file existence check in `SaveManager::importBackupArchive()` before calling `unzipToDirectory()`
  - Add file size validation (> 0 bytes) before extraction
  - Add debug logging in `Dropbox::downloadFile()` after download completes (log file size, HTTP status)
  - Add debug logging in `SaveManager::importBackupArchive()` for validation failures
  - Improve error messages to distinguish: "file not found" vs "empty file" vs "corrupted ZIP"

  **Must NOT do**:
  - Do NOT add retry logic for network failures
  - Do NOT change minizip library or its configuration
  - Do NOT refactor surrounding code

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple validation logic additions, well-defined scope
  - **Skills**: []
    - No special skills needed for this C++ validation logic

  **Parallelization**:
  - **Can Run In Parallel**: YES (independent of Task 2 and 3)
  - **Parallel Group**: Wave 1 (with Tasks 2, 3)
  - **Blocks**: None
  - **Blocked By**: None

  **References** (CRITICAL - Be Exhaustive):

  **Pattern References** (existing code to follow):
  - `source/core/SaveManager.cpp:930-979` - `importBackupArchive()` function - add validation at line 938 BEFORE calling `unzipToDirectory()`
  - `source/network/Dropbox.cpp:551-600` - `downloadFile()` function - add logging after line 584 (after fclose)

  **API/Type References** (contracts to implement against):
  - `include/utils/Logger.hpp:LOG_ERROR`, `LOG_DEBUG` macros - use these for logging

  **External References** (libraries and frameworks):
  - C++ `<sys/stat.h>` - use `stat()` for file existence and size check

  **WHY Each Reference Matters**:
  - `SaveManager.cpp:930-979`: This is where ZIP extraction happens - add validation BEFORE line 938
  - `Dropbox.cpp:551-600`: This is where download happens - add size logging after fclose at line 584
  - `Logger.hpp`: Use existing logging macros for consistency

  **Acceptance Criteria**:
  - [ ] File existence check added before `unzipToDirectory()` call
  - [ ] File size validation (> 0) added before extraction
  - [ ] Debug log shows file path and size after download
  - [ ] Debug log shows specific failure reason (not found/empty/corrupted)
  - [ ] Existing behavior preserved for valid ZIP files

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Missing ZIP file shows specific error
    Tool: Bash (build verification)
    Preconditions: Code changes applied
    Steps:
      1. Run `make` to compile
      2. Verify build succeeds with no errors
    Expected Result: Build completes successfully
    Failure Indicators: Compilation errors
    Evidence: .sisyphus/evidence/task-1-build.log

  Scenario: Hardware test - restore with missing file
    Tool: Manual (documented for hardware testing)
    Preconditions: App running on Switch, cloud restore triggered but file missing
    Steps:
      1. Trigger cloud restore for a save
      2. Observe debug log output (via serial/log file)
      3. Verify specific error message shown
    Expected Result: Debug log shows "File not found" or similar specific message
    Evidence: .sisyphus/evidence/task-1-missing-file.txt
  ```

  **Commit**: YES
  - Message: `fix(restore): add ZIP file validation and debug logging`
  - Files: `source/core/SaveManager.cpp`, `source/network/Dropbox.cpp`

- [x] 2. **Header Subtitle Spacing Fix**

  **What to do**:
  - Change subtitle Y coordinate from 54 to 74 in `SaveShell.cpp` line 401
  - This adds ~20px spacing between app name (bottom edge ~52px) and subtitle (now starts at 74px)

  **Must NOT do**:
  - Do NOT fix other UI spacing issues "while we're here"
  - Do NOT change any other values in `renderHeader()`
  - Do NOT change font sizes or other positioning

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single line change, trivial fix
  - **Skills**: []
    - No special skills needed

  **Parallelization**:
  - **Can Run In Parallel**: YES (independent of Task 1 and 3)
  - **Parallel Group**: Wave 1 (with Tasks 1, 3)
  - **Blocks**: None
  - **Blocked By**: None

  **References** (CRITICAL):

  **Pattern References**:
  - `source/ui/saves/SaveShell.cpp:385-422` - `renderHeader()` function
  - Line 401: `renderText(subtitle, 46, 54, m_fontSmall, color(148, 163, 184));`
  - Change to: `renderText(subtitle, 46, 74, m_fontSmall, color(148, 163, 184));`

  **WHY Each Reference Matters**:
  - This is the exact line where subtitle Y position is set - single value change

  **Acceptance Criteria**:
  - [ ] Subtitle Y position changed from 54 to 74
  - [ ] No other changes in `renderHeader()` function
  - [ ] Build succeeds

  **QA Scenarios:**

  ```
  Scenario: Build verification
    Tool: Bash
    Preconditions: Code change applied
    Steps:
      1. Run `make`
      2. Verify build succeeds
    Expected Result: Build completes with no errors
    Evidence: .sisyphus/evidence/task-2-build.log

  Scenario: Hardware test - visual spacing
    Tool: Manual (hardware testing)
    Preconditions: App running on Switch
    Steps:
      1. Navigate to any screen with header
      2. Observe spacing between "OC Save Keeper" title and subtitle
      3. Verify ~20px additional gap compared to before
    Expected Result: Subtitle clearly separated from title with visible gap
    Evidence: .sisyphus/evidence/task-2-spacing.png (screenshot)
  ```

  **Commit**: YES
  - Message: `fix(ui): increase header subtitle spacing`
  - Files: `source/ui/saves/SaveShell.cpp`

- [x] 3. **Upload Progress Notifications**

  **What to do**:
  - Add new translation keys to BOTH `romfs/lang/en.json` AND `romfs/lang/ko.json`:
    - `sync.creating_local`: "Creating local backup..." / "로컬 세이브 생성중"
    - `sync.uploading_dropbox`: "Uploading to cloud..." / "세이브 업로드 중"
    - `sync.upload_complete`: "Upload complete" / "드롭박스 업로드 완료"
  - Add `Runtime::notify()` calls in `SaveBackendAdapter::upload()` between each phase:
    - Before `createVersionedBackup()`: notify "sync.creating_local"
    - Before Dropbox uploads: notify "sync.uploading_dropbox"
    - After success: notify "sync.upload_complete" (already exists as "sync.upload_completed")

  **Must NOT do**:
  - Do NOT add progress bars - only text notifications
  - Do NOT change the upload algorithm
  - Do NOT make upload async
  - Do NOT add retry logic

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple notification additions, well-defined scope
  - **Skills**: []
    - No special skills needed

  **Parallelization**:
  - **Can Run In Parallel**: YES (independent of Task 1 and 2)
  - **Parallel Group**: Wave 1 (with Tasks 1, 2)
  - **Blocks**: None
  - **Blocked By**: None

  **References** (CRITICAL):

  **Pattern References**:
  - `source/ui/saves/SaveBackendAdapter.cpp:264-306` - `upload()` function - add notifications between phases
  - `romfs/lang/ko.json` - existing translation keys like `sync.uploading` - follow same pattern
  - `romfs/lang/en.json` - add corresponding English translations

  **API/Type References**:
  - `include/ui/saves/Runtime.hpp:notify()` - use this for status display
  - Existing pattern in `SaveMenuScreen.cpp:139`: `Runtime::instance().notify(lang.get("sync.uploading"))`

  **WHY Each Reference Matters**:
  - `SaveBackendAdapter.cpp:264-306`: This is the upload flow - add notifications between lines 274, 278, and 293
  - `ko.json`/`en.json`: Must add keys to BOTH language files
  - `Runtime::notify()`: Existing notification mechanism - use this

  **Acceptance Criteria**:
  - [ ] Translation keys added to both `en.json` and `ko.json`
  - [ ] `Runtime::notify()` called before local backup creation
  - [ ] `Runtime::notify()` called before Dropbox upload
  - [ ] Existing "sync.upload_completed" notification still shown after success
  - [ ] Build succeeds

  **QA Scenarios:**

  ```
  Scenario: Build verification
    Tool: Bash
    Preconditions: Code changes applied
    Steps:
      1. Run `make`
      2. Verify build succeeds
    Expected Result: Build completes with no errors
    Evidence: .sisyphus/evidence/task-3-build.log

  Scenario: Hardware test - upload progress
    Tool: Manual (hardware testing)
    Preconditions: App running on Switch, valid save selected
    Steps:
      1. Press upload button
      2. Observe notifications in sequence:
         - "로컬 세이브 생성중" (Creating local backup...)
         - "세이브 업로드 중" (Uploading to cloud...)
         - "드롭박스 업로드 완료" (Upload complete)
    Expected Result: All three notifications appear during upload process
    Evidence: .sisyphus/evidence/task-3-upload-progress.txt
  ```

  **Commit**: YES
  - Message: `feat(upload): add step-by-step progress notifications`
  - Files: `source/ui/saves/SaveBackendAdapter.cpp`, `romfs/lang/en.json`, `romfs/lang/ko.json`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

- [x] F1. **Build Verification** — `quick`
  Run `make` to verify all changes compile without errors. Check for warnings.
  Output: `Build [PASS/FAIL]`

- [x] F2. **Scope Fidelity Check** — `quick`
  For each task: verify only specified files were modified, no scope creep.
  Output: `Tasks [N/N compliant]`

---

## Commit Strategy

- **1**: `fix(restore): add ZIP file validation and debug logging` — SaveManager.cpp, Dropbox.cpp
- **2**: `fix(ui): increase header subtitle spacing` — SaveShell.cpp
- **3**: `feat(upload): add step-by-step progress notifications` — SaveBackendAdapter.cpp, en.json, ko.json

---

## Success Criteria

### Verification Commands
```bash
make  # Expected: successful build with no errors
```

### Final Checklist
- [x] ZIP validation logs show detailed error messages
- [x] Header subtitle has proper spacing
- [x] Upload shows progress notifications in both languages
- [x] All existing functionality preserved
