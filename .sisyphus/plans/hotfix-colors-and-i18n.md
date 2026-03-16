# Hotfix: Local/Cloud Colors and Source Label i18n

## TL;DR

> **Quick Summary**: 
> 1. 메인 화면 Local/Cloud 칩 색상 교환 (Local=녹색, Cloud=파랑)
> 2. "cloud" → "드롭박스" 다국어 변환 추가
> 
> **Estimated Effort**: Quick (2분)

---

## TODOs

- [ ] 1. **Swap Local/Cloud colors on main screen**

  **File**: `source/ui/saves/SaveShell.cpp` lines 512-515
  
  **Current (Local=파랑, Cloud=녹색):**
  ```cpp
  fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(8, 47, 73) : color(39, 39, 42));
  fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(20, 83, 45) : color(39, 39, 42));
  strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(56, 189, 248) : color(82, 82, 91));
  strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(74, 222, 128) : color(82, 82, 91));
  ```
  
  **Change to (Local=녹색, Cloud=파랑):**
  ```cpp
  fillRect(m_renderer, localChip, entry.hasLocalBackup ? color(20, 83, 45) : color(39, 39, 42));
  fillRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(8, 47, 73) : color(39, 39, 42));
  strokeRect(m_renderer, localChip, entry.hasLocalBackup ? color(74, 222, 128) : color(82, 82, 91));
  strokeRect(m_renderer, cloudChip, entry.hasCloudBackup ? color(56, 189, 248) : color(82, 82, 91));
  ```

- [ ] 2. **Fix "cloud" → "드롭박스" translation**

  **File**: `source/ui/saves/SaveBackendAdapter.cpp` line 221
  
  **Current:**
  ```cpp
  entry.sourceLabel = (src == "Dropbox") ? lang.get("history.source_dropbox") : src;
  ```
  
  **Change to:**
  ```cpp
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
