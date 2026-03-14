# oc-save-keeper

Nintendo Switch 홈브류 환경에서 세이브 백업과 기기 간 동기화를 더 안전하게 관리하기 위한 도구입니다.

Language: [English](README.md) | **한국어**

![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012?style=for-the-badge&logo=nintendo-switch&logoColor=white)
![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-devkitPro-1f7a8c?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-2ea043?style=for-the-badge)

## 프로젝트 목적

`oc-save-keeper`는 아래 3가지를 핵심으로 합니다.

- 로컬 백업을 먼저 만들고
- Dropbox로 업로드/다운로드를 지원하며
- 복원 전에 메타데이터를 비교해 더 안전한 결정을 돕습니다.

## 크레딧

- 원본 프로젝트 표기: [JKSV](https://github.com/J-D-K/JKSV)
- 본 프로젝트는 Switch 세이브 매니저 생태계의 경험을 바탕으로, 기기/사용자 기반 메타데이터와 Dropbox 워크플로우를 강화한 파생 구현입니다.

## 빠른 이동

- [빠른 시작](#빠른-시작)
- [다운로드](#다운로드)
- [설치](#설치)
- [Dropbox 설정](#dropbox-설정)
- [Dropbox 브리지(선택)](#dropbox-브리지선택)
- [동기화 판단 방식](#동기화-판단-방식)
- [빌드](#빌드)
- [릴리즈 자동화](#릴리즈-자동화)

## 빠른 시작

1. GitHub Releases에서 최신 zip을 받습니다.
2. SD 카드 루트에 압축을 풉니다.
3. Homebrew Menu에서 `oc-save-keeper`를 실행합니다.
4. 먼저 로컬 백업을 1회 생성합니다.
5. Dropbox를 연결하고 업로드를 진행합니다.

## 다운로드

- 저장소: https://github.com/jshsakura/oc-save-keeper
- 릴리즈: https://github.com/jshsakura/oc-save-keeper/releases

권장 방식:

- 릴리즈 `.zip` 다운로드
- SD 루트에 압축 해제 후 아래 경로 확인

```text
/switch/oc-save-keeper/oc-save-keeper.nro
```

수동 방식:

- `.nro` 단일 파일 다운로드
- `/switch/oc-save-keeper/oc-save-keeper.nro`로 배치

## 저장소 경로 안내

```text
.github/workflows/release.yml    # CI 빌드/릴리즈 워크플로우
source/                          # 앱 소스 코드
include/                         # 헤더
tests/                           # 호스트 단위 테스트
romfs/lang/                      # 런타임 언어 JSON
RELEASE_NOTES_v0.1.0-alpha.1.md  # 현재 릴리즈 노트 문서
TESTING_v0.1.0-alpha.1.md        # 현재 테스트 체크리스트 문서
```

## 주요 기능

- 로컬 세이브 버전 히스토리
- Dropbox 업로드/다운로드
- 기기/사용자 메타데이터 기반 판단
- 다중 기기 우선순위 충돌 처리
- 한국어(`ko`) UI + 영어 폴백

## 설치

zip을 SD 카드 루트에 풀고, 아래 파일이 존재하는지 확인하세요.

```text
/switch/oc-save-keeper/oc-save-keeper.nro
```

런타임 데이터 경로:

```text
/switch/oc-save-keeper/backups/
/switch/oc-save-keeper/logs/
/switch/oc-save-keeper/temp/
/switch/oc-save-keeper/device_id.txt
/switch/oc-save-keeper/device_label.txt
/switch/oc-save-keeper/device_priority.txt
/switch/oc-save-keeper/config/dropbox_auth.json
```

## Dropbox 설정

1. https://www.dropbox.com/developers/apps 에서 앱 생성
2. `Dropbox API`, `App folder` 권한으로 설정
3. 프로젝트 루트 `.env`에 Dropbox App Key 입력
4. Switch에서 로그인 화면을 열고 코드 교환

`.env` 예시:

```bash
DROPBOX_APP_KEY=your_dropbox_app_key
```

그 다음 빌드:

```bash
make
```

GitHub Actions로 빌드할 때는 저장소 시크릿 `DROPBOX_APP_KEY`를 설정하면 됩니다.

앱 내부 연결 순서:

1. `로그인 열기` 버튼 누르기
2. 표시된 QR을 스캔하거나 생성된 인증 링크를 휴대폰 또는 PC에서 열기
3. Dropbox 승인 완료
4. 완료 화면의 인증 코드 또는 돌아온 전체 URL 복사
5. Switch에 붙여넣고 `Dropbox 연결` 누르기

Switch 빌드는 이제 Dropbox 브라우저 창을 직접 띄우지 않습니다. Atmosphere 환경에서 웹 애플릿이 불안정하게 종료되는 문제를 피하기 위해, OAuth 단계는 전체 브라우저가 있는 다른 기기에서 진행하도록 고정했습니다.

인증 정보 저장 경로:

```text
/switch/oc-save-keeper/config/dropbox_auth.json
```

## Dropbox 브리지(선택)

수동 코드 붙여넣기 대신 콜백+폴링 UX를 원하면 `backend/dropbox-bridge`의 Python 브리지를 사용하세요.

- 서비스 실행 문서: `backend/dropbox-bridge/README.md`
- 아키텍처/부하/다중 사용자 문서: `docs/backend/DROPBOX_BRIDGE_ARCHITECTURE.ko.md`

공개 도메인이 `save.opencourse.kr`라면 Dropbox App Console Redirect URI를 아래처럼 정확히 등록해야 합니다.

```text
https://save.opencourse.kr/oauth/dropbox/callback
```

브리지 환경 변수도 동일하게 맞춥니다.

```text
REDIRECT_BASE_URL=https://save.opencourse.kr
```

## 동기화 판단 방식

백업마다 아래 메타데이터를 기록합니다.

- device
- user
- title
- revision
- timestamp
- device priority

이를 통해 같은 게임/같은 유저인지, 다른 기기에서 온 세이브인지 구분하고 복원 위험을 줄입니다.

## 기본 사용

1. 앱 실행
2. 타이틀 선택
3. 로컬 백업 생성
4. Dropbox 업로드
5. 다른 기기에서 다운로드 후 판단 메시지 확인
6. 기기/유저가 의도와 일치할 때만 복원

## 문제 해결

로그 확인:

```text
/switch/oc-save-keeper/logs/oc-save-keeper.log
```

## 빌드

```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 make
```

출력:

```text
oc-save-keeper.nro
```

## 릴리즈 자동화

워크플로우 파일:

```text
.github/workflows/release.yml
```

동작:

- `oc-save-keeper.nro` 빌드
- `dist/oc-save-keeper-<ref>.zip` 패키징
- 워크플로우 아티팩트 업로드
- `main` 푸시마다 `latest` 프리릴리즈 갱신
- `v*` 태그 푸시 시 정식 릴리즈 에셋 업로드
