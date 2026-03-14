# oc-save-keeper 프로젝트 가이드

이 파일은 `oc-save-keeper` 프로젝트의 구조와 빌드 방식, 주요 로직에 대한 지침을 담고 있습니다.

## 1. 빌드 환경 (Build Environment)

본 프로젝트는 Nintendo Switch용 애플리케이션으로, 로컬 환경에 `devkitPro`가 설치되어 있지 않은 경우 **Docker**를 사용하여 빌드하도록 설정되어 있습니다.

- **이미지**: `devkitpro/devkita64`
- **빌드 명령어**: 
  ```bash
  make DROPBOX_APP_KEY="your_app_key"
  ```
- **결과물**: 루트 디렉토리에 `oc-save-keeper.nro` 파일이 생성됩니다.
- **호스트 테스트**: 로컬 개발 환경에서 유닛 테스트를 실행하려면 `make test`를 사용합니다 (g++, json-c, libcurl 필요).

## 2. 드롭박스 인증 흐름 (Dropbox Auth Flow)

기존의 복잡한 수동 입력 방식에서 **QR 코드 기반의 자동 인증**으로 개편되었습니다.

- **백엔드 브리지**: `https://save.opencourse.kr` (FastAPI + Redis 기반)
- **인증 과정**:
  1. 앱에서 브리지 세션 시작 (`/v1/sessions/start`)
  2. 반환된 URL을 QR 코드로 표시
  3. 사용자가 휴대폰으로 스캔 및 승인
  4. 앱에서 브리지 상태를 폴링 (`/v1/sessions/{id}/status`)
  5. 승인 완료 시 토큰 교환 (`/v1/sessions/{id}/consume`)
- **주요 파일**:
  - `include/network/Dropbox.hpp`, `source/network/Dropbox.cpp`: 브리지 통신 로직
  - `source/ui/saves/SaveShell.cpp`: QR UI 및 폴링 상태 머신 관리

## 3. 주의 사항

- **인클루드**: `SaveShell.hpp` 수정 시 `network/Dropbox.hpp`가 반드시 포함되어야 `DropboxBridgeSession` 타입을 인식할 수 있습니다.
- **번역**: 새로운 UI 문구 추가 시 `romfs/lang/ko.json` 및 `en.json`을 동시에 업데이트해야 합니다.
