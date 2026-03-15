# Dropbox 클라우드 구조 재설계

## TL;DR

> **Quick Summary**: Dropbox 클라우드 저장 경로를 유저 기반에서 타이틀 중심 구조로 변경하고, 중복된 `oc-save-keeper` prefix를 제거합니다.
>
> **Deliverables**:
> - 수정된 클라우드 경로 함수들 (`getCloudTitlePath`, `getCloudPath`, etc.)
> - 수정된 SaveBackendAdapter 경로 로직
> - 빌드 성공 + 수동 QA 검증
>
> **Estimated Effort**: Medium
> **Parallel Execution**: YES - 2 waves
> **Critical Path**: Task 1 → Task 3 → Task 6 → Task 8

---

## Context

### Original Request
Dropbox 업로드 시 폴더 경로가 `/oc-save-keeper/oc-save-keeper/users/...`로 중복 생성됨. 또한 유저 기반 구조에서 타이틀 중심 구조로 변경 필요.

### Interview Summary
**Key Discussions**:
- Dropbox 앱 폴더가 이미 `oc-save-keeper`로 설정되어 코드에서 중복 prefix 추가됨
- 타이틀 기준 공유 구조로 변경 (유저/기기는 메타데이터로만 유지)
- 리비전 히스토리 유지
- 출처 식별은 메타데이터의 `deviceLabel`, `userName` 사용

**Research Findings**:
- `BackupMetadata`에 이미 `deviceId`, `deviceLabel`, `userId`, `userName` 포함됨
- UI에서 이 값을 표시하므로 경로에서 분리해도 출처 식별 가능

### Metis Review
**Identified Gaps** (addressed):
- `listTitles()` 경로 불일치 문제 → 수정 범위에 포함
- 리비전 목록 조회 경로도 함께 수정 필요 → Task 7로 추가

---

## Work Objectives

### Core Objective
클라우드 저장 경로를 타이틀 중심 구조로 변경하고 중복 prefix 제거

### Concrete Deliverables
- `source/core/SaveManager.cpp` - 클라우드 경로 함수 수정
- `source/ui/saves/SaveBackendAdapter.cpp` - 어댑터 경로 로직 수정
- 빌드 성공 + 수동 QA 통과

### Definition of Done
- [ ] `make` 빌드 성공
- [ ] Dropbox에 `/titles/{titleId}/...` 경로로 업로드됨
- [ ] 리비전 목록에서 기기/유저 정보 표시됨

### Must Have
- 타이틀 중심 경로 구조: `titles/{titleId}/latest.zip`
- 리비전 히스토리 유지: `titles/{titleId}/revisions/{timestamp}.zip`
- 메타데이터에 출처 정보 유지

### Must NOT Have (Guardrails)
- 로컬 백업 경로 변경 금지
- Dropbox 인증 코드 수정 금지
- 기존 데이터 마이그레이션 코드 추가 금지
- 새로운 기능 추가 금지

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO (C++ Switch 홈브류, 테스트 프레임워크 없음)
- **Automated tests**: None
- **Framework**: N/A
- **Agent-Executed QA**: 빌드 검증 + 코드 리뷰

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **C++ Build**: Use Bash (make) — Build command, check exit code
- **Code Review**: Use Grep — Search for old patterns, verify new patterns

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Core Path Functions — sequential due to tight coupling):
├── Task 1: getCloudUserPath() prefix 제거 [quick]
├── Task 2: getCloudTitlePath() 타이틀 중심으로 재설계 [quick]
├── Task 3: getCloudPath() 및 관련 함수 수정 [quick]
└── Task 4: getCloudRevisionDirectory() 수정 [quick]

Wave 2 (Adapter & UI — parallel after Wave 1):
├── Task 5: listTitles() 경로 수정 [quick]
├── Task 6: upload() 경로 검증 [quick]
└── Task 7: listRemoteRevisionMetadata() 경로 수정 [quick]

Wave 3 (Verification):
├── Task 8: 빌드 검증 [quick]
└── Task 9: 코드 패턴 검증 [quick]
```

### Dependency Matrix

- **1**: — — 2, 3
- **2**: 1 — 3
- **3**: 1, 2 — 5, 6, 7
- **4**: 1 — 7
- **5**: 3 — 8
- **6**: 3 — 8
- **7**: 3, 4 — 8
- **8**: 5, 6, 7 — 9
- **9**: 8 — —

### Agent Dispatch Summary

- **1-4**: Sequential — `quick` (C++ path logic)
- **5-7**: Parallel — `quick` (adapter modifications)
- **8-9**: Sequential — `quick` (verification)

---

## TODOs

- [x] 1. getCloudUserPath() prefix 제거

  **What to do**:
  - `source/core/SaveManager.cpp` line 683-686 수정
  - `"oc-save-keeper/users/"` → `"users/"`로 변경
  - Dropbox 앱 폴더가 이미 `oc-save-keeper`이므로 prefix 불필요

  **Must NOT do**:
  - 로컬 경로 함수 수정 금지
  - 메타데이터 필드 변경 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 단순 문자열 변경, 1-2줄 수정
  - **Skills**: []
    - git-master: 불필요 (나중에 한 번에 커밋)

  **Parallelization**:
  - **Can Run In Parallel**: NO (다른 task의 기준이 됨)
  - **Parallel Group**: Wave 1 (Sequential)
  - **Blocks**: Task 2, 3
  - **Blocked By**: None

  **References**:
  - `source/core/SaveManager.cpp:683-686` - 현재 `getCloudUserPath()` 구현

  **Acceptance Criteria**:
  - [ ] `getCloudUserPath()`가 `"users/" + userId` 반환
  - [ ] `"oc-save-keeper/"` 문자열이 함수에서 제거됨

  **QA Scenarios**:
  ```
  Scenario: 코드 패턴 검증
    Tool: Bash (grep)
    Steps:
      1. grep -n "oc-save-keeper/users" source/core/SaveManager.cpp
    Expected Result: getCloudUserPath() 함수 내에서 매치 없음
    Evidence: .sisyphus/evidence/task-01-pattern-check.txt
  ```

  **Commit**: NO (groups with others)

- [x] 2. getCloudTitlePath() 타이틀 중심으로 재설계

  **What to do**:
  - `source/core/SaveManager.cpp` line 697-702 수정
  - 현재: `getCloudUserPath() + "/titles/" + titleComponent`
  - 목표: `"titles/" + titleComponent` (유저 경로 제거)

  **Must NOT do**:
  - titleId 포맷 변경 금지 (016X 유지)
  - 다른 함수 수정 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 경로 구조 단순화
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (Task 1 완료 후)
  - **Parallel Group**: Wave 1 (Sequential)
  - **Blocks**: Task 3
  - **Blocked By**: Task 1

  **References**:
  - `source/core/SaveManager.cpp:697-702` - 현재 `getCloudTitlePath()` 구현

  **Acceptance Criteria**:
  - [ ] `getCloudTitlePath()`가 `"titles/{titleId}"` 반환
  - [ ] `getCloudUserPath()` 호출 제거됨

  **QA Scenarios**:
  ```
  Scenario: 새 경로 구조 검증
    Tool: Bash (grep)
    Steps:
      1. grep -A5 "getCloudTitlePath" source/core/SaveManager.cpp
    Expected Result: "titles/" + titleComponent 패턴 확인
    Evidence: .sisyphus/evidence/task-02-title-path.txt
  ```

  **Commit**: NO (groups with others)

- [x] 3. getCloudPath() 및 getCloudDevicesPath() 수정

  **What to do**:
  - `source/core/SaveManager.cpp` line 704-706 (`getCloudPath`) 수정
    - 현재: `getCloudTitlePath(title) + "/latest.zip"`
    - 그대로 유지 (이미 올바름, Task 2에서 자동 반영)
  - `getCloudDevicesPath()` 제거 또는 deprecated 처리
    - 새 구조에서 devices 경로 불필요

  **Must NOT do**:
  - getCloudRevisionDirectory() 수정 (Task 4에서 처리)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 함수 호출 확인 및 불필요 함수 정리
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (Task 2 완료 후)
  - **Parallel Group**: Wave 1 (Sequential)
  - **Blocks**: Task 5, 6, 7
  - **Blocked By**: Task 1, 2

  **References**:
  - `source/core/SaveManager.cpp:704-706` - `getCloudPath()` 구현
  - `source/core/SaveManager.cpp:708-713` - `getCloudDevicesPath()` 구현

  **Acceptance Criteria**:
  - [ ] `getCloudPath()`가 `"titles/{titleId}/latest.zip"` 반환
  - [ ] `getCloudDevicesPath()`가 더 이상 사용되지 않음

  **QA Scenarios**:
  ```
  Scenario: 최종 경로 검증
    Tool: Bash (grep)
    Steps:
      1. grep -A2 "getCloudPath\|getCloudMetadataPath" source/core/SaveManager.cpp
    Expected Result: titles/ 경로로 시작
    Evidence: .sisyphus/evidence/task-03-final-path.txt
  ```

  **Commit**: NO (groups with others)

- [x] 4. getCloudRevisionDirectory() 수정

  **What to do**:
  - `source/core/SaveManager.cpp` line 715-717 수정
  - 현재: `getCloudTitlePath(title) + "/revisions"`
  - 새 구조: `"titles/" + titleComponent + "/revisions"`
  - Task 2에서 getCloudTitlePath가 수정되면 자동 반영됨

  **Must NOT do**:
  - 리비전 파일명 포맷 변경 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: getCloudTitlePath() 변경으로 자동 반영 확인
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (Task 3 완료 후)
  - **Parallel Group**: Wave 1 (Sequential)
  - **Blocks**: Task 7
  - **Blocked By**: Task 1

  **References**:
  - `source/core/SaveManager.cpp:715-717` - `getCloudRevisionDirectory()` 구현

  **Acceptance Criteria**:
  - [ ] `getCloudRevisionDirectory()`가 `"titles/{titleId}/revisions"` 반환

  **QA Scenarios**:
  ```
  Scenario: 리비전 경로 검증
    Tool: Bash (grep)
    Steps:
      1. grep -A2 "getCloudRevisionDirectory" source/core/SaveManager.cpp
    Expected Result: titles/.../revisions 패턴
    Evidence: .sisyphus/evidence/task-04-revision-path.txt
  ```

  **Commit**: NO (groups with others)

- [x] 5. listTitles() 경로 수정 (SaveBackendAdapter)

  **What to do**:
  - `source/ui/saves/SaveBackendAdapter.cpp` line 127-129 수정
  - 현재: `"/users/" + userId + "/titles"` (하드코딩, prefix 없음)
  - 수정: `"/" + m_saveManager.getCloudTitlePath(nullptr)` 또는 타이틀 목록 조회 방식 재설계
  - 새 구조에서는 `/titles/` 직접 조회

  **Must NOT do**:
  - API 응답 파싱 로직 변경 금지
  - 새로운 에러 처리 추가 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 경로 문자열 수정
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 6, 7)
  - **Blocks**: Task 8
  - **Blocked By**: Task 3

  **References**:
  - `source/ui/saves/SaveBackendAdapter.cpp:127-129` - 현재 listTitles() 경로

  **Acceptance Criteria**:
  - [ ] `listTitles()`가 `/titles/` 경로 조회
  - [ ] 하드코딩된 `/users/` 경로 제거됨

  **QA Scenarios**:
  ```
  Scenario: listTitles 경로 검증
    Tool: Bash (grep)
    Steps:
      1. grep -n '"/users/"' source/ui/saves/SaveBackendAdapter.cpp
    Expected Result: 매치 없음 (모든 /users/ 하드코딩 제거)
    Evidence: .sisyphus/evidence/task-05-list-titles.txt
  ```

  **Commit**: NO (groups with others)

- [x] 6. upload() 경로 검증 (SaveBackendAdapter)

  **What to do**:
  - `source/ui/saves/SaveBackendAdapter.cpp` line 295-306 검증
  - 현재: `"/" + m_saveManager.getCloudPath(title)`
  - Task 3에서 getCloudPath가 수정되면 자동으로 `/titles/{titleId}/latest.zip` 됨
  - 경로가 올바르게 구성되는지 확인만 함

  **Must NOT do**:
  - 업로드 로직 자체 변경 금지
  - 압축 파일 생성 로직 변경 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 검증만 수행, 코드 변경 최소
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 5, 7)
  - **Blocks**: Task 8
  - **Blocked By**: Task 3

  **References**:
  - `source/ui/saves/SaveBackendAdapter.cpp:295-306` - upload() 경로 구성

  **Acceptance Criteria**:
  - [ ] upload()에서 `getCloudPath()` 사용 확인
  - [ ] 추가 수정 불필요한지 확인

  **QA Scenarios**:
  ```
  Scenario: upload 경로 확인
    Tool: Bash (grep)
    Steps:
      1. grep -B2 -A2 "uploadFile" source/ui/saves/SaveBackendAdapter.cpp | head -20
    Expected Result: getCloudPath() 사용 확인
    Evidence: .sisyphus/evidence/task-06-upload-path.txt
  ```

  **Commit**: NO (groups with others)

- [x] 7. listRemoteRevisionMetadata() 경로 수정

  **What to do**:
  - `source/ui/saves/SaveBackendAdapter.cpp`에서 리비전 메타데이터 조회 경로 수정
  - 현재: devices 기반 경로 사용
  - 수정: `titles/{titleId}/revisions/` 경로로 변경
  - Task 4의 getCloudRevisionDirectory() 사용

  **Must NOT do**:
  - 메타데이터 파싱 로직 변경 금지
  - UI 표시 데이터 구조 변경 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 경로 함수 호출로 변경
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 5, 6)
  - **Blocks**: Task 8
  - **Blocked By**: Task 3, 4

  **References**:
  - `source/ui/saves/SaveBackendAdapter.cpp` - listRemoteRevisionMetadata() 함수
  - `source/core/SaveManager.cpp:715-717` - getCloudRevisionDirectory()

  **Acceptance Criteria**:
  - [ ] 리비전 조회가 `/titles/{titleId}/revisions/` 경로 사용
  - [ ] getCloudRevisionDirectory() 호출로 경로 구성

  **QA Scenarios**:
  ```
  Scenario: 리비전 조회 경로 검증
    Tool: Bash (grep)
    Steps:
      1. grep -n "getCloudRevisionDirectory\|/devices/" source/ui/saves/SaveBackendAdapter.cpp
    Expected Result: getCloudRevisionDirectory 사용, /devices/ 미사용
    Evidence: .sisyphus/evidence/task-07-revision-lookup.txt
  ```

  **Commit**: NO (groups with others)

- [x] 8. 빌드 검증

  **What to do**:
  - `make` 명령으로 프로젝트 빌드
  - 빌드 에러 없이 성공 확인
  - 필요시 devkitpro Docker 컨테이너 사용

  **Must NOT do**:
  - 코드 수정 금지 (에러 발생 시 이전 task로 rollback)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 빌드 명령 실행만
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3 (Sequential)
  - **Blocks**: Task 9
  - **Blocked By**: Task 5, 6, 7

  **References**:
  - `Makefile` - 빌드 설정

  **Acceptance Criteria**:
  - [ ] `make` 실행 성공 (exit code 0)
  - [ ] .nro 파일 생성 확인

  **QA Scenarios**:
  ```
  Scenario: 빌드 성공
    Tool: Bash (make)
    Steps:
      1. docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make clean
      2. docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
    Expected Result: Exit code 0, *.nro 파일 생성
    Evidence: .sisyphus/evidence/task-08-build.log
  ```

  **Commit**: NO (task 9 이후)

- [x] 9. 코드 패턴 최종 검증 + 커밋

  **What to do**:
  - grep으로 모든 변경 사항 검증
  -旧 패턴 (`oc-save-keeper/users`, `/users/` 하드코딩) 잔존 여부 확인
  - 새 패턴 (`titles/`) 적용 확인
  - git commit 수행

  **Must NOT do**:
  - 추가 리팩토링 금지

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 검증 + 커밋
  - **Skills**: [`git-master`]
    - git-master: 원자적 커밋 생성

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3 (Sequential)
  - **Blocks**: None
  - **Blocked By**: Task 8

  **References**:
  - 모든 수정된 파일

  **Acceptance Criteria**:
  - [ ] `grep -r "oc-save-keeper/users" source/` 결과 없음 (클라우드 경로)
  - [ ] `grep -r '"/users/"' source/` 결과 없음 (하드코딩)
  - [ ] `grep -r "titles/" source/core/SaveManager.cpp` 결과 있음
  - [ ] git commit 완료

  **QA Scenarios**:
  ```
  Scenario: 패턴 검증
    Tool: Bash (grep)
    Steps:
      1. grep -r "oc-save-keeper/users" source/core/ source/ui/
      2. grep -r '"/users/"' source/ui/saves/SaveBackendAdapter.cpp
      3. grep -r "titles/" source/core/SaveManager.cpp
    Expected Result: 1, 2는 매치 없음, 3은 매치 있음
    Evidence: .sisyphus/evidence/task-09-pattern-final.txt
  ```

  **Commit**: YES
  - Message: `fix(cloud): restructure Dropbox paths to title-centric model`
  - Files: `source/core/SaveManager.cpp`, `source/ui/saves/SaveBackendAdapter.cpp`
  - Pre-commit: `make` (빌드 성공 확인)

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists. For each "Must NOT Have": search codebase for forbidden patterns.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `make` build. Review all changed files for: old path patterns remaining, typos, missing semicolons.
  Output: `Build [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Pattern Verification** — `quick`
  Grep for old patterns: `oc-save-keeper/users`, `/users/`. Verify none remain in cloud path logic.
  Output: `Old Patterns [0 found] | New Patterns [N found] | VERDICT`

---

## Commit Strategy

- **1**: `fix(cloud): restructure Dropbox paths to title-centric model`

---

## Success Criteria

### Verification Commands
```bash
make  # Expected: Build successful (exit code 0)
grep -r "oc-save-keeper/users" source/  # Expected: No matches in cloud path code
grep -r "titles/" source/core/SaveManager.cpp  # Expected: Matches found
```

### Final Checklist
- [x] All "Must Have" present
- [x] All "Must NOT Have" absent
- [x] Build passes
