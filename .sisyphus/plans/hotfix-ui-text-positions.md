# Hotfix: UI Text Position Issues

## TL;DR

> **Quick Summary**: 두 가지 UI 텍스트 위치 문제 수정
> 1. 우측 하단 메시지 잘림 → 올바른 우측 정렬
> 2. "세이브 브라우저" 타이틀 겹침 → X 위치 이동
> 
> **Deliverables**:
> - SaveShell.cpp renderFooter() 수정
> - SaveShell.cpp renderHeader() 수정
> 
> **Estimated Effort**: Quick (5분)
> **Parallel Execution**: NO - 단일 파일 수정

---

## Context

### Problem 1: 우측 하단 메시지 잘림

**현재 코드 (SaveShell.cpp:431-439):**
```cpp
const int maxW = 800;
const std::string fitted = fitText(m_fontSmall, rightHint, maxW);
const int estimatedW = static_cast<int>(fitted.size() * 8);  // BUG: 한글은 더 넓음
const int x = std::max(400, 1280 - 24 - estimatedW);
renderText(fitted, x, footer.y + 10, m_fontSmall, color(100, 116, 139));
```

**문제점:**
- `estimatedW = fitted.size() * 8`은 영문 기준, 한글은 더 넓음
- 긴 메시지는 화면 밖으로 잘림
- "세이브 성공" 같은 한글 메시지가 특히 문제

**해결책:**
- `fitText()`가 이미 텍스트를 잘라서 반환
- X 위치를 화면 우측에서 고정 위치로 변경 (예: `1280 - 24 - measuredWidth`)
- 또는 `maxW`를 줄이고 X를 고정값 사용

### Problem 2: "세이브 브라우저" 타이틀 겹침

**현재 코드 (SaveShell.cpp:393-397):**
```cpp
renderText(tr("app.name", "OC Save Keeper"), 46, 18, m_fontLarge, color(241, 245, 249));
// ...
if (!title.empty()) {
    renderText(title, 315, 30, m_fontSmall, color(148, 163, 184));
}
```

**문제점:**
- "OC Save Keeper" (m_fontLarge) 텍스트가 X=46에서 시작
- X=315에 "Save Browser" 텍스트가 시작
- m_fontLarge가 큰 폰트라 텍스트가 X=315까지 연장됨

**해결책:**
- title의 X 위치를 더 오른쪽으로 이동 (예: 400 또는 450)
- 또는 App Name 끝 위치 계산 후 그 다음에 title 배치

---

## Work Objectives

### Core Objective
두 가지 텍스트 위치 문제 수정

### Concrete Deliverables
- `source/ui/saves/SaveShell.cpp` 2곳 수정

### Definition of Done
- [ ] 우측 하단 메시지가 화면 내에서 올바르게 표시
- [ ] "세이브 브라우저"가 App Name과 겹치지 않음
- [ ] `make` 빌드 성공

### Must Have
- 텍스트가 화면 밖으로 잘리지 않음
- 텍스트 간 겹침 없음

### Must NOT Have (Guardrails)
- 다른 UI 요소 위치 변경 금지
- 폰트 크기 변경 금지

---

## Verification Strategy

### QA Policy
- 빌드 성공 확인
- Switch에서 시각적 확인

---

## Execution Strategy

### Single Task

```
Task 1: Fix text positions [quick]
├── renderFooter() - 우측 정렬 수정
└── renderHeader() - title X 위치 이동
```

---

## TODOs

- [ ] 1. **Fix Footer Right-Hint Position**

  **What to do**:
  - `SaveShell.cpp` line 436-437 수정
  - 한글 텍스트도 화면 내에 표시되도록 수정
  
  **Current:**
  ```cpp
  const int estimatedW = static_cast<int>(fitted.size() * 8);
  const int x = std::max(400, 1280 - 24 - estimatedW);
  ```
  
  **Fix Option A (간단):**
  ```cpp
  const int estimatedW = static_cast<int>(fitted.size() * 12);  // 한글 고려
  const int x = std::max(24, 1280 - 24 - estimatedW);
  ```
  
  **Fix Option B (더 안전):**
  ```cpp
  // fitText로 이미 잘렸으니, 우측에서 24px 위치에 텍스트 끝이 오도록
  // estimatedW를 더 보수적으로 계산
  const int estimatedW = static_cast<int>(fitted.size() * 10);
  const int x = 1280 - 24 - estimatedW;
  if (x < 24) {
      // 텍스트가 너무 길면 왼쪽 끝에서 시작
      renderText(fitted, 24, footer.y + 10, m_fontSmall, color(100, 116, 139));
  } else {
      renderText(fitted, x, footer.y + 10, m_fontSmall, color(100, 116, 139));
  }
  ```

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 없음

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: 없음
  - **Blocked By**: 없음

  **References**:
  - `source/ui/saves/SaveShell.cpp:431-439` - renderFooter()
  - `source/ui/saves/SaveShell.cpp:220-240` - fitText() 함수 (있으면)

  **Acceptance Criteria**:
  - [ ] 한글 메시지가 화면 밖으로 잘리지 않음
  - [ ] `make` 빌드 성공

- [ ] 2. **Fix Header Title Position**

  **What to do**:
  - `SaveShell.cpp` line 397 수정
  - title X 위치를 더 오른쪽으로 이동
  
  **Current:**
  ```cpp
  renderText(title, 315, 30, m_fontSmall, color(148, 163, 184));
  ```
  
  **Fix:**
  ```cpp
  renderText(title, 450, 30, m_fontSmall, color(148, 163, 184));
  ```
  
  또는 App Name 끝 위치 계산:
  ```cpp
  // App Name "OC Save Keeper"은 약 14글자, m_fontLarge (약 20px/글자)
  // X=46 + 14*20 = 326, 여유공간 50px 추가하여 X=380 정도
  renderText(title, 380, 30, m_fontSmall, color(148, 163, 184));
  ```

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: 없음

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Blocks**: 없음
  - **Blocked By**: 없음

  **References**:
  - `source/ui/saves/SaveShell.cpp:393-398` - renderHeader()

  **Acceptance Criteria**:
  - [ ] "세이브 브라우저"가 "OC Save Keeper"와 겹치지 않음
  - [ ] `make` 빌드 성공

  **Commit**: YES
  - Message: `fix(ui): correct text positions for footer and header`
  - Files: `source/ui/saves/SaveShell.cpp`

---

## Final Verification Wave

- [ ] F1. **Build Verification** — `make` exits 0
- [ ] F2. **Visual Check** — Switch에서 텍스트 위치 확인

---

## Commit Strategy

- **1**: `fix(ui): correct text positions for footer and header` — SaveShell.cpp

---

## Success Criteria

### Verification Commands
```bash
make  # Expected: exit 0
```

### Final Checklist
- [ ] 우측 하단 메시지 잘림 수정
- [ ] 헤더 타이틀 겹침 수정
- [ ] 빌드 성공
