# oc-save-keeper

Nintendo Switch 홈브류 환경에서 세이브 백업과 기기 간 동기화를 더 안전하게 관리하기 위한 도구입니다.

Language: [English](README.md) | **한국어**

![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012?style=for-the-badge&logo=nintendo-switch&logoColor=white)
![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-devkitPro-1f7a8c?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-2ea043?style=for-the-badge)

## 프로젝트 소개

`oc-save-keeper`는 여러 기기와 사용자 간에 **안전한** 백업과 복원을 목표로 하는 Nintendo Switch 홈브류 세이브 매니저입니다.

- 🗂️ **로컬 우선**: 위험한 복원 전에 버전 관리되는 로컬 백업 생성
- ☁️ **클라우드 동기화**: Dropbox를 통한 업로드/다운로드
- 🛡️ **안전한 결정**: 복원 전 메타데이터(기기/사용자/타이틀/리비전) 비교
- 🌏 **이중 언어 UI**: 한국어(`ko`) + 영어 폴백
- 🔄 **자동 롤백**: JKSV 스타일 안전 복원 엔진 (실패 시 자동 롤백)

## 크레딧 및 참조

본 프로젝트는 J-D-K의 [JKSV](https://github.com/J-D-K/JKSV)에서 영감을 받아 제작되었습니다.

`oc-save-keeper`는 Nintendo Switch 세이브 관리 생태계를 확장하여 다음을 추가합니다:

- 백업 메타데이터 추적 (기기, 사용자, 타이틀, 리비전, 타임스탬프)
- 기기 인식 동기화 결정 및 우선순위 기반 충돌 처리
- Dropbox 중심 클라우드 워크플로우 (선택적 브리지 서비스 포함)
- JKSV 스타일 안전 복원 엔진 (물리적 커밋 및 자동 롤백)

이 프로젝트가 유용하다면 원작인 [JKSV](https://github.com/J-D-K/JKSV) 프로젝트도 확인해 보세요.

## 빠른 이동

- [빠른 시작](#빠른-시작)
- [다운로드](#다운로드)
- [설치](#설치)
- [Dropbox 설정](#dropbox-설정)
- [Dropbox 브리지(선택)](#dropbox-브리지선택)
- [동기화 판단 방식](#동기화-판단-방식)
- [빌드](#빌드)
- [테스트](#테스트)
- [릴리즈 자동화](#릴리즈-자동화)
- [문서](#문서)

## 주요 기능

### 핵심 기능

- 로컬 세이브 버전 히스토리
- Dropbox 업로드/다운로드
- 기기/사용자 메타데이터 기반 판단
- 다중 기기 우선순위 충돌 처리
- 한국어(`ko`) UI + 영어 폴백
- Nintendo Switch 공유 폰트 렌더링 (한국어/영어)

### 안전 기능

- **JKSV 스타일 복원 엔진**: 물리적 커밋 및 실패 시 자동 롤백
- **ZIP 검증**: 복원 전 아카이브 무결성 확인
- **복원 전 스냅샷**: 모든 복원 전 현재 세이브 자동 백업
- **사용자 간 보호**: 다른 사용자의 자동 덮어쓰기 차단

### 클라우드 기능

- **QR 코드 인증**: 휴대폰/PC에서 QR 스캔으로 Dropbox 인증
- **Dropbox 브리지 서비스**: 선택적 Python 백엔드 (콜백+폴링 OAuth UX)
- **타이틀 중심 클라우드 경로**: 게임 타이틀별 깔끔한 정리
- **멀티 세이브 지원**: 타이틀당 여러 세이브 백업/복원

### UI/UX

- **SaveShell 워크플로우**: 현대적이고 간소화된 인터페이스
- **Catppuccin Mocha 테마**: 부드러운 애니메이션과 큰 버튼
- **토스트 알림**: 업로드 진행률 및 작업 피드백
- **애플릿 모드 감지**: 다양한 환경에서 향상된 호환성

### 보안

- **SHA-512 무결성 검증**: 백업 검증을 위한 강력한 해시
- **릭롤 가드**: 보안되지 않은 HTTP 요청 차단
- **속도 제한**: 브리지 서비스 백엔드 보호

## 빠른 시작

1. [GitHub Releases](https://github.com/jshsakura/oc-save-keeper/releases)에서 `oc-save-keeper.nro` 다운로드.
2. SD 카드의 `/switch/` 폴더에 복사.
3. Homebrew Menu에서 `oc-save-keeper` 실행.
4. Dropbox 연결 후 클라우드에 세이브 업로드.

> **참고**: "클라우드에 업로드"를 누르면 자동으로 로컬 백업이 먼저 생성됩니다. 수동으로 백업할 필요가 없습니다.

안전하게 테스트만 원한다면 타이틀 목록을 둘러보며 UI가 정상 작동하는지 확인하세요.

## 다운로드

- 저장소: https://github.com/jshsakura/oc-save-keeper
- 릴리즈: https://github.com/jshsakura/oc-save-keeper/releases

`oc-save-keeper.nro`를 다운로드 후 아래 경로에 복사:

```text
/switch/oc-save-keeper.nro
```

앱은 첫 실행 시 자동으로 데이터 폴더(`/switch/oc-save-keeper/`)를 생성합니다.

## 저장소 구조

```text
.github/workflows/        # CI 빌드/릴리즈 파이프라인
source/                   # 앱 소스 코드
include/                  # 헤더 파일
tests/                    # 호스트 단위 테스트
romfs/lang/               # 런타임 언어 JSON
romfs/gfx/                # 그래픽 리소스
backend/dropbox-bridge/   # 선택적 OAuth 브리지 서비스
docs/                     # 문서
  ├── backend/            # 백엔드 아키텍처 문서
  └── frontend/           # 프론트엔드 UI 문서
RELEASE_NOTES_*.md        # 릴리즈 노트
```

## 설치

`oc-save-keeper.nro`를 SD 카드에 복사:

```text
/switch/oc-save-keeper.nro
```

앱은 첫 실행 시 자동으로 데이터 폴더를 생성합니다:

```text
/switch/oc-save-keeper/
├── backups/           # 로컬 세이브 백업
├── logs/              # 로그 파일
├── temp/              # 임시 파일
└── config/            # Dropbox 인증, 기기 설정 (settings.json)
```

## Dropbox 설정

### 1. Dropbox 앱 생성

아래 주소로 이동:

```text
https://www.dropbox.com/developers/apps
```

다음 설정으로 앱 생성:

- API: `Dropbox API`
- Access: `App folder`
- App name: `OCSaveKeeper-Backup` 등

### 2. Dropbox App Key를 `.env`에 입력

프로젝트 루트에 `.env` 파일 생성:

```bash
DROPBOX_APP_KEY=your_dropbox_app_key
```

그 다음 빌드:

```bash
make
```

GitHub Actions로 빌드할 때는 저장소 시크릿 `DROPBOX_APP_KEY`를 설정하세요.

### 3. Switch에서 연결

`oc-save-keeper` 실행 후 Dropbox 설정 화면 열기:

1. `로그인 열기` 버튼 누르기
2. 휴대폰으로 QR 스캔 (또는 인증 링크를 PC로 복사)
3. Dropbox에서 앱 승인
4. 반환된 인증 코드 또는 전체 리디렉션 URL 복사
5. Switch에 붙여넣고 `Dropbox 연결` 누르기

인증 정보 저장 경로:

```text
/switch/oc-save-keeper/config/dropbox_auth.json
```

## Dropbox 브리지(선택)

자동 콜백+폴링 인증(수동 코드 입력 없음)을 원하면 Python 브리지 서비스를 사용하세요.

**특징:**
- 수동 코드 복사/붙여넣기 없음 - 자동 폴링으로 완료
- PKCE 키 교환이 자동으로 처리됨
- 리프래시 토큰은 Switch에만 저장, 브리지에는 저장되지 않음

*Dropbox가 OAuth Device Code Grant를 공식 지원하지 않아, Switch에서 폴링 기반 인증을 완료하려면 이 브릿지가 필요합니다. 원한다면 직접 호스팅하여 운용할 수 있습니다.*

- 서비스 문서: `backend/dropbox-bridge/README.md`
- 아키텍처 문서: `docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md`

공개 도메인이 `example.yourdomain.com`라면 Dropbox App Console에 다음 리디렉션 URI 등록:

```text
https://example.yourdomain.com/oauth/dropbox/callback
```

그 다음 설정:

```text
DROPBOX_BRIDGE_BASE=https://example.yourdomain.com
```

## 동기화 판단 방식

모든 백업에는 다음 메타데이터가 포함됩니다:

- device (기기)
- user (사용자)
- title (타이틀)
- revision (리비전)
- timestamp (타임스탬프)
- device priority (기기 우선순위)

이를 통해 다음을 구분할 수 있습니다:

- 같은 게임의 같은 사용자
- 다른 기기의 같은 사용자
- 완전히 다른 사용자의 세이브

현재 동작:

- 같은 `title`과 같은 `user`: 자동 비교 가능
- 다른 `device`: 다른 세이브 소스로 표시
- 다른 `user`: 자동 덮어쓰기 차단
- 다른 기기에서의 수동 복원은 여전히 허용
- 복원 적용 전 현재 세이브 스냅샷 생성

## 다중 기기 사용

두 대 이상의 Switch를 사용한다면 각 기기마다 고유 ID를 유지하세요:

- `device_id`: 안정적인 내부 식별자 (자동 생성)
- `device_priority`: 큰 숫자가 충돌 시 우선

둘 다 `config/settings.json`에 저장됩니다. 예시:

```json
{
  "device_id": "abc123",
  "device_priority": 200
}
```

다른 기기는 `device_priority: 100` 등으로 설정할 수 있습니다.

## 기본 사용

1. Homebrew Menu에서 앱 실행
2. 타이틀 선택
3. Dropbox 연결 후 클라우드에 업로드 (로컬 백업 자동 생성)
4. 다른 기기에서 다운로드 후 동기화 결정 확인
5. 기기와 사용자가 의도와 일치할 때만 복원

### 조작법

- `A`: 선택 또는 확인
- `B`: 뒤로 가기
- `X`: 보이는 모든 타이틀 동기화
- `+`: 앱 종료

### 첫 실행 가이드

처음 사용 시:

1. `oc-save-keeper` 실행
2. 텍스트가 올바르게 렌더링되는지 확인
3. 이미 세이브가 있는 게임 하나 열기
4. Dropbox 연결
5. `클라우드에 업로드` 선택 (로컬 백업 자동 생성)

이후 다른 기기에서 같은 타이틀로 `클라우드에서 다운로드` 가능.

### 권장 워크플로우

안전한 테스트를 위한 순서:

1. Dropbox 연결
2. 클라우드에 업로드 (로컬 백업 자동 생성)
3. 기기 전환
4. 클라우드에서 다운로드
5. 결정 사유 확인
6. 기기와 사용자가 의도와 일치할 때만 복원

### 중요 안전 주의사항

- 중요 세이브의 유일한 복사본으로 복원 로직을 테스트하지 마세요.
- 기기 간 복원 시도 전 최소 하나의 알려진 양호한 로컬 백업을 유지하세요.
- 앱이 백업이 다른 기기에서 왔다고 표시하면 복원 전 기기 라벨을 읽으세요.
- 앱이 복원 전 스냅샷을 자동 생성하지만 여전히 주의해야 합니다.

## 문제 해결

문제 발생 시 로그 확인:

```text
/switch/oc-save-keeper/logs/oc-save-keeper.log
```

이슈 리포트 시 포함할 유용한 정보:

- 게임 타이틀
- 로컬 기기 라벨
- 선택한 사용자
- 수행한 정확한 작업
- 앱이 로컬 또는 클라우드 우선이라고 표시했는지 여부

## 알려진 문제

- 클라우드 버전 브라우징은 아직 구현되지 않음 (현재 최신 동기화 백업에 집중)
- 기기 간 복원은 지원되지만 복원 전 소스 기기 라벨을 확인해야 함
- 이 프로젝트는 아직 알파 단계이므로 중요하지 않은 세이브로 복원 동작을 주의 깊게 테스트해야 함

## 소스에서 빌드

### 빌드 환경

이 프로젝트는 Nintendo Switch 타겟으로 devkitPro가 필요합니다. 로컬에 devkitPro가 없으면 Docker 이미지를 사용하세요.

**Docker (권장):**

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```

**로컬 devkitPro:**

```bash
make
```

### Dropbox App Key로 빌드

방법 1: `.env` 파일 (로컬 개발 권장):

```bash
echo "DROPBOX_APP_KEY=your_app_key" > .env
make
```

방법 2: 명령줄 인자:

```bash
make DROPBOX_APP_KEY="your_app_key"
```

GitHub Actions에서는 저장소 시크릿 `DROPBOX_APP_KEY`를 설정하세요.

### 빌드 결과물

```text
oc-save-keeper.nro
```

### 개발 노트

- **Include 의존성**: `SaveShell.hpp` 수정 시 `network/Dropbox.hpp`를 포함해야 `DropboxBridgeSession` 타입을 인식할 수 있습니다.
- **다국어**: 새 UI 문구 추가 시 `romfs/lang/ko.json`과 `romfs/lang/en.json`을 모두 업데이트하세요.

## 테스트

로컬에서 호스트 유닛 테스트 실행:

```bash
make test
```

### 테스트 커버리지

| 모듈 | 테스트 | 상태 |
|------|--------|------|
| Dropbox util | 17 | ✅ |
| Metadata logic | 4 | ✅ |
| Metadata file | 6 | ✅ |
| Sync logic | 3 | ✅ |
| **합계** | **30** | ✅ |

상세 테스트 체크리스트는 [docs/TESTING.md](docs/TESTING.md) 참조.

## 릴리즈 자동화

GitHub Actions가 자동으로 릴리즈 패키지를 빌드합니다.

워크플로우 파일:

```text
.github/workflows/release.yml
```

기능:

- `oc-save-keeper.nro` 빌드
- `dist/oc-save-keeper-<ref>.zip` 패키징
- 워크플로우 실행 시 아티팩트 업로드
- `main` 푸시마다 `latest` 프리릴리즈 갱신
- 태그 릴리즈(`v*`) 시 에셋 첨부

## 문서

| 문서 | 설명 |
|------|------|
| [docs/TESTING.md](docs/TESTING.md) | 테스트 가이드 및 체크리스트 |
| [docs/TDD.md](docs/TDD.md) | TDD 워크플로우 가이드 |
| [docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md](docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md) | Dropbox 브리지 아키텍처 (한국어) |
| [docs/frontend/SAVE_UI_MIGRATION.md](docs/frontend/SAVE_UI_MIGRATION.md) | UI 마이그레이션 노트 |

## 라이선스

MIT License - 자세한 내용은 [LICENSE](LICENSE) 참조.

## 감사의 말

- [JKSV](https://github.com/J-D-K/JKSV) by J-D-K - 이 프로젝트에 영감을 준 원작 세이브 매니저
- [devkitPro](https://devkitpro.org/) - Nintendo Switch 홈브류 개발 툴체인
- [libnx](https://github.com/switchbrew/libnx) - Nintendo Switch 홈브류 라이브러리
