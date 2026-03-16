# Hotfix: Cloud Restore Path Bug

## TL;DR

> **Quick Summary**: 클라우드 복원 시 `.meta` 파일 대신 `.zip` 파일을 다운로드하도록 수정
> 
> **Deliverables**:
> - RevisionMenuScreen.cpp 1줄 수정
> 
> **Estimated Effort**: Quick (1분)
> **Parallel Execution**: NO - 단일 파일 수정

---

## Context

### Original Request
로그에서 클라우드 복원 실패 발견:
```
[05:28:21] [DEBUG] Download complete: /titles/.../20260316_052804.meta -> .../temp/frontend_010086A022444000_1773606500.zip (HTTP 200, 300 bytes)
[05:28:21] [ERROR] Failed to open ZIP: .../temp/frontend_010086A022444000_1773606500.zip
```

### Root Cause
`RevisionMenuScreen.cpp:104`에서 `entry.id` (`.meta` 경로)를 사용하여 다운로드하지만,
실제로는 `entry.path` (`.zip` 경로)를 사용해야 함.

**SaveBackendAdapter.cpp에서 데이터 구조:**
```cpp
// line 222: entry.id = .meta 경로 (식별자용)
entry.id = file.path;  // /titles/.../xxx.meta

// line 224: entry.path = .zip 경로 (다운로드용)  
entry.path = file.path.substr(0, file.path.size() - 5) + ".zip";  // /titles/.../xxx.zip
```

### Metis Review
- N/A (단순 1줄 수정)

---

## Work Objectives

### Core Objective
클라우드 복원이 올바른 `.zip` 파일을 다운로드하도록 수정

### Concrete Deliverables
- `source/ui/saves/RevisionMenuScreen.cpp` 1줄 수정

### Definition of Done
- [x] `entry.id` → `entry.path` 변경
- [x] `make` 빌드 성공

### Must Have
- 클라우드 복원이 `.zip` 파일을 다운로드

### Must NOT Have (Guardrails)
- 다른 코드 수정 금지
- 로컬 복원 로직 변경 금지

---

## Verification Strategy

### QA Policy
- 빌드 성공 확인
- 사용자가 Switch에서 직접 테스트

---

## Execution Strategy

### Single Task (즉시 실행)

```
Task 1: Fix download path [quick]
└── RevisionMenuScreen.cpp:104 수정
```

---

## TODOs

- [x] 1. **Fix Cloud Restore Download Path**

  **What to do**:
  - `source/ui/saves/RevisionMenuScreen.cpp` line 104
  - `entry.id` → `entry.path` 로 변경

  **Must NOT do**:
  - 다른 코드 수정
  - 로컬 복원 로직 (`restore()` 호출) 변경

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 없음

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: 없음
  - **Blocked By**: 없음

  **References**:
  - `source/ui/saves/RevisionMenuScreen.cpp:94-114` - restoreSelected() 함수
  - `source/ui/saves/SaveBackendAdapter.cpp:221-224` - entry.id/entry.path 설정

  **Acceptance Criteria**:
  - [ ] line 104: `entry.id` → `entry.path` 변경
  - [ ] `make` 빌드 성공

  **QA Scenarios**:
  ```
  Scenario: Cloud restore downloads .zip file
    Tool: Manual (Switch hardware)
    Steps:
      1. 클라우드 리비전 선택
      2. 복원 버튼 클릭
      3. 로그에서 .zip 파일 다운로드 확인
    Expected Result: .zip 확장자 파일 다운로드, ZIP 추출 성공
  ```

  **Commit**: YES
  - Message: `fix(cloud): download .zip instead of .meta on restore`
  - Files: `source/ui/saves/RevisionMenuScreen.cpp`

---

## Final Verification Wave

- [x] F1. **Build Verification** — `make` exits 0
- [x] F2. **Scope Fidelity Check** — Only RevisionMenuScreen.cpp modified

---

## Commit Strategy

- **1**: `fix(cloud): download .zip instead of .meta on restore` — RevisionMenuScreen.cpp

---

## Success Criteria

### Verification Commands
```bash
make  # Expected: exit 0
```

### Final Checklist
- [x] 버그 원인 파악: entry.id 대신 entry.path 사용
- [x] 수정 완료
- [x] 빌드 성공
