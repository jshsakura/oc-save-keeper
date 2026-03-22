# oc-save-keeper

클라우드 동기화를 지원하는 세이브 백업 매니저입니다.

Language: [English](README.md) | **한국어**

![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-devkitPro-1f7a8c?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-2ea043?style=for-the-badge)

## 개요

버전 히스토리가 있는 로컬 세이브 백업과 Dropbox 클라우드 동기화. 기기 인식 메타데이터 추적으로 안전한 기기 간 복원 결정을 지원합니다.

## 빌드

devkitPro 툴체인이 필요합니다.

**Docker (권장):**

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```

**로컬 devkitPro:**

```bash
make
```

### Dropbox 연동

프로젝트 루트에 `.env` 파일 생성:

```bash
DROPBOX_APP_KEY=your_dropbox_app_key
```

그 다음 빌드:

```bash
make
```

GitHub Actions에서는 저장소 시크릿 `DROPBOX_APP_KEY`를 설정하세요.

### 빌드 결과물

```text
oc-save-keeper.nro
```

## 설치

`oc-save-keeper.nro`를 다음 경로에 복사:

```text
/switch/oc-save-keeper.nro
```

앱은 첫 실행 시 데이터 폴더를 자동 생성합니다:

```text
/switch/oc-save-keeper/
├── backups/    # 로컬 세이브 백업
├── logs/       # 로그 파일
├── temp/       # 임시 파일
└── config/     # Dropbox 인증, 기기 설정
```

## 저장소 구조

```text
source/                   # 앱 소스 코드
include/                  # 헤더 파일
tests/                    # 호스트 유닛 테스트
romfs/lang/               # 언어 JSON 파일
romfs/gfx/                # 그래픽 리소스
backend/dropbox-bridge/   # 선택적 OAuth 브리지 서비스
docs/                     # 문서
```

## Dropbox 설정

### 1. Dropbox 앱 생성

https://www.dropbox.com/developers/apps 에서 다음 설정으로 앱 생성:

- API: `Dropbox API`
- Access: `App folder`
- App name: `OCSaveKeeper-Backup` 등

### 2. 앱 키 설정

`.env` 파일에 키 입력 후 빌드하거나, GitHub Actions 시크릿 `DROPBOX_APP_KEY` 설정.

### 3. 연결

앱 실행 후 Dropbox 설정 화면에서:

1. `로그인 열기` 선택
2. QR 스캔 또는 인증 링크 복사
3. Dropbox에서 앱 승인
4. 인증 코드 또는 리디렉션 URL 붙여넣기
5. `Dropbox 연결` 선택

## 테스트

```bash
make test
```

## 문서

| 문서 | 설명 |
|------|------|
| [docs/TESTING.md](docs/TESTING.md) | 테스트 가이드 |
| [docs/TDD.md](docs/TDD.md) | TDD 워크플로우 가이드 |
| [docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md](docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md) | 브리지 아키텍처 |
| [docs/frontend/SAVE_UI_MIGRATION.md](docs/frontend/SAVE_UI_MIGRATION.md) | UI 마이그레이션 노트 |

## 라이선스

MIT License - 자세한 내용은 [LICENSE](LICENSE) 참조.
