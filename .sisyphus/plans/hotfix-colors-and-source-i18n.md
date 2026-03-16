# Hotfix: Local/Cloud Colors and Source Label i18n

## TL;DR

> **Quick Summary**: 
> 1. 메인 화면에서 Local/Cloud 칩 색상 교환 (Local=녹색, Cloud=파랑)
> 2. "cloud" → "드롭박스" 다국어 변환 추가
> 
> **Deliverables**:
> - SaveShell.cpp 4줄 수정 (색상 교환)
> - SaveBackendAdapter.cpp 1줄 수정 (조건 추가)

---

## Context

### Problem 1: 색상 반대
메인 화면에서:
- Local 칩 = 파랑 ❌ (녹색이어야 함)
- Cloud 칩 = 녹색 ❌ (파랑이어야 함)

### Problem 2: "cloud" 미번역
메타데이터에 `source = "cloud"`로 저장됨
코드에서 `src == "Dropbox"`만 체크해서 "cloud"는 번역 안됨

---

## TODOs

- [ ] 1. **Swap Local/Cloud colors on main screen**

  **File**: `source/ui/saves/SaveShell.cpp` lines 512-515
  
  **Change**:
  ```cpp
  // Before (colors swapped):
  fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(8, 47, 73) : color(39, 39, 42));
  fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(20, 83, 45) : color(39, 39, 42));
  strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(56, 189, 248) : color(82, 82, 91));
  strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(74, 222, 128) : color(82, 82, 91));
  
  // After:
  fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(20, 83, 45) : color(39, 39, 42));  // green
  fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(8, 47, 73) : color(39, 39, 42));   // blue
  strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(74, 222, 128) : color(82, 82, 91));  // green
  strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(56, 189, 248) : color(82, 82, 91));   // blue
  ```

- [ ] 2. **Fix "cloud" → "드롭박스" translation**

  **File**: `source/ui/saves/SaveBackendAdapter.cpp` line 220-221
  
  **Change**:
  ```cpp
  // Before:
  std::string src = incomingMeta.source.empty() ? "Dropbox" : incomingMeta.source;
  entry.sourceLabel = (src == "Dropbox") ? lang.get("history.source_dropbox") : src;
  
  // After:
  std::string src = incomingMeta.source.empty() ? "Dropbox" : incomingMeta.source;
  entry.sourceLabel = (src == "Dropbox" || src == "cloud") ? lang.get("history.source_dropbox") : src;
  ```

---

## Verification

```bash
make  # Must exit 0
```

---

## Commit

```
fix(ui): correct Local/Cloud colors and add "cloud" i18n
```
