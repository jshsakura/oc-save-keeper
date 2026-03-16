# Hotfix: Shorten Restore Success Message

## TL;DR

> **Quick Summary**: 복원 성공 메시지를 짧게 변경하여 화면에서 잘리지 않도록 수정
> 
> **Deliverables**:
> - SaveManager.cpp 1줄 수정 (메시지 단축)
> 
> **Estimated Effort**: Quick (30초)

---

## Context

### Problem
복원 성공 메시지가 너무 길어서 Switch 화면에서 잘림:
- 현재: "Metadata precheck accepted incoming backup"
- 화면에서 잘려서 사용자가 성공 여부를 알 수 없음

### Fix
메시지를 짧게 변경:
- 영문: "Restore success"
- 한글: "복원 성공" (언어 시스템 사용)

---

## TODOs

- [x] 1. **Shorten success message**

  **What to do**:
  - `source/core/SaveManager.cpp` line 972
  - 변경: `"Metadata precheck accepted incoming backup"` → `"Restore success"`

  **Commit**:
  - Message: `fix(ui): shorten restore success message`
  - Files: `source/core/SaveManager.cpp`
