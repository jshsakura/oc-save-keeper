# Hotfix: Cloud Restore Force Overwrite

## TL;DR

> **Quick Summary**: 클라우드 복원 시 충돌 체크를 건너뛰고 무조건 덮어쓰도록 수정
> 
> **Deliverables**:
> - SaveBackendAdapter.cpp 1줄 수정 (skipConflictCheck=true 추가)
> 
> **Estimated Effort**: Quick (1분)
> **Parallel Execution**: NO - 단일 파일 수정

---

## Context

### Original Request
클라우드 복원 시 동일한 타임스탬프/우선순위인 경우 "keeping local backup" 메시지와 함께 복원이 실제로 적용되지 않음.

로그:
```
[INFO] Extracted ZIP: ... -> /switch/oc-save-keeper/temp/import_...
[INFO] ui runtime notify: Priority and timestamp are tied; keeping local backup
```

### Root Cause
`SaveBackendAdapter::download()` line 329에서 `importBackupArchive()` 호출 시 `skipConflictCheck` 파라미터를 생략 (기본값=false)

**현재 코드:**
```cpp
const bool ok = m_saveManager.importBackupArchive(title, tempArchive, &reason);
```

**수정 코드:**
```cpp
const bool ok = m_saveManager.importBackupArchive(title, tempArchive, &reason, true);
```

### Why This Fix Is Correct
- 사용자가 명시적으로 클라우드 리비전을 선택하고 "복원" 버튼을 눌렀음
- 이것은 명확한 사용자 의도이므로 충돌 체크 없이 무조건 적용해야 함
- `skipConflictCheck=true`는 이미 구현되어 있음 (SaveManager.cpp line 972)

---

## Work Objectives

### Core Objective
클라우드 복원 선택 시 무조건 덮어쓰기

### Concrete Deliverables
- `source/ui/saves/SaveBackendAdapter.cpp` 1줄 수정

### Definition of Done
- [x] `skipConflictCheck=true` 파라미터 추가
- [x] `make` 빌드 성공

### Must Have
- 클라우드 복원이 무조건 로컬에 적용

### Must NOT Have (Guardrails)
- 다른 코드 수정 금지
- 로컬 복원 로직 변경 금지

---

## Verification Strategy

### QA Policy
- 빌드 성공 확인
- Switch에서 클라우드 복원 테스트

---

## Execution Strategy

### Single Task (즉시 실행)

```
Task 1: Add skipConflictCheck parameter [quick]
└── SaveBackendAdapter.cpp:329 수정
```

---

## TODOs

- [x] 1. **Add skipConflictCheck to Cloud Restore**

  **What to do**:
  - `source/ui/saves/SaveBackendAdapter.cpp` line 329
  - `importBackupArchive()` 호출에 `true` 파라미터 추가

  **Must NOT do**:
  - 다른 코드 수정
  - 로컬 복원 로직 변경

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 없음

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: 없음
  - **Blocked By**: 없음

  **References**:
  - `source/ui/saves/SaveBackendAdapter.cpp:329` - download() 함수
  - `source/core/SaveManager.cpp:972` - skipConflictCheck 사용 방식

  **Acceptance Criteria**:
  - [x] line 329: 4번째 파라미터 `true` 추가
  - [x] `make` 빌드 성공

  **Commit**: YES
  - Message: `fix(cloud): force overwrite on explicit cloud restore`
  - Files: `source/ui/saves/SaveBackendAdapter.cpp`

---

## Final Verification Wave

- [x] F1. **Build Verification** — `make` exits 0
- [x] F2. **Scope Fidelity Check** — Only SaveBackendAdapter.cpp modified

---

## Commit Strategy

- **1**: `fix(cloud): force overwrite on explicit cloud restore` — SaveBackendAdapter.cpp

---

## Success Criteria

### Verification Commands
```bash
make  # Expected: exit 0
```

### Final Checklist
- [x] 수정 완료
- [x] 빌드 성공
- [ ] 클라우드 복원이 무조건 적용됨
