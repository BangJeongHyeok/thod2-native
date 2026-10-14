# 플랫폼: 창·디스플레이

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 1-3, 2-2
- **관련 코드:** `src/platform/` (예정), `src/core/patches/device_dialog.cpp`
- **마지막 갱신:** 2026-10-14

## 주요 목표

게임 창과 화면 모드를 현대 환경에 맞게 다룬다. 테두리 없는 창 모드, 데스크톱 해상도와 무관한 렌더링, DPI 인식, 다중 모니터, Alt+Tab 전환이 안정적으로 동작하게 한다. 최종적으로 원본의 창 생성과 메시지 루프를 우리 코드로 교체한다.

**완료 기준:** 어떤 DPI 배율과 모니터 구성에서도 테두리 없는 창으로 시작하고, Alt+Tab과 창/전체 화면 전환에서 문제가 없다.

## 중목표 / 소목표

### G1. 시작 확인창 자동 처리 — 완료

- [x] G1-1 `MessageBoxA` IAT 후킹으로 장치 확인창("Attention", 호출 위치 `0x4A4758`)에 "예"(`IDYES`)를 돌려주고 창은 띄우지 않음
- [x] G1-2 설정 `[display] skip_device_dialog` (기본 1)
- [x] G1-3 다른 메시지 창(종료 확인 등)은 그대로 통과 (판별 함수 단위 테스트로 확인. 게임에서 다른 창을 띄워 본 확인은 아직 안 함)
- [x] G1-4 게임에서 켬/끔 확인

### G2. 창 모드와 Alt+Enter — 대기

- [ ] G2-1 원본이 화면 모드를 바꾸는 시점과 방법 분석
- [ ] G2-2 dgVoodoo2 없이 테두리 없는 창 모드
- [ ] G2-3 Alt+Enter 창/전체 화면 전환

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-14 | 장치 확인창은 호출 위치(`0x4A4758`)와 제목(`Attention`)이 둘 다 맞을 때만 가로챈다 | 같은 `MessageBoxA`로 띄우는 다른 창(종료 확인, CD 요청, 장치 미지원)을 건드리지 않기 위해. 대안: 제목·본문 문자열만으로 판별(다른 빌드에서도 동작하지만 오판 가능) |
| 2026-10-14 | 확인창 처리 결과는 "예"(`IDYES`)로 고정 | 사용자가 항상 "예"를 눌러 왔고, "아니요"의 동작은 아직 분석하지 않음 |

## 미해결 질문

- 원본이 화면 모드를 바꾸는 시점과 방법 (DirectDraw `SetDisplayMode`, `SetCooperativeLevel`)
- 렌더러 도메인과의 경계 (백버퍼 크기는 어느 쪽이 정하는지)
- 원본 해상도는 640×480 고정이고, 이를 바꾸는 설정이 없다 (`hglCore.ini`의 `FullScreen` 키는 쓰이지 않음)
- Alt+Enter 창/전체 화면 전환을 어떻게 제공할지 (원본에는 창 모드가 없음)

## 작업 기록

### 2026-10-14 — 시작 확인창 자동 처리 (관련: G1)

- **한 일:**
  - `src/core/patches/device_dialog.cpp`: `Hod2.exe` IAT의 `USER32!MessageBoxA`를 가로채서, 호출 위치가 `0x4A4758`이고 제목이 `Attention`이면 창을 띄우지 않고 `IDYES`를 돌려준다.
  - 원본 주소 상수 파일 `src/interop/include/thod2/interop/hod2_addresses.h`를 만들고 `kDeviceDialogMessageBoxReturn = 0x4A4758`을 넣었다.
  - 설정 `[display] skip_device_dialog` (기본 1). 코어는 지원 빌드(또는 `allow_unknown_exe = 1`)일 때만 패치를 설치한다.
  - `core_tests`에 판별 함수 검사 4개, 설정 검사 1개 추가 (총 22개).
- **결과:**
  - Debug, Release 빌드 경고 0개, `ctest` 2개 모두 통과.
  - 테스트 사본에서 켬: 확인창이 뜨지 않고 바로 게임이 시작됨. 로그 `skip_device_dialog: installed`, 약 0.4초 뒤 `device confirmation dialog answered with Yes without showing it`.
  - 끔(`skip_device_dialog = 0`): 원래대로 확인창이 뜸. 로그 `skip_device_dialog: disabled in config`.
- **다음:** G2 창 모드와 Alt+Enter.

### 2026-10-14 — dgVoodoo2 테두리 없는 창 모드 결과

- **결과:** dgVoodoo2 시험 C 설정으로 테두리 없는 전체 화면 크기 창이 되고, 모니터 해상도와 배치가 바뀌지 않는다. Alt+Tab은 주입 방식과 사용자 실제 키보드 모두 정상이다. 사용자 플레이에서 화면과 게임 속도도 정상이었다.
- **상세:** [compat-survey.md](compat-survey.md) "dgVoodoo2 시험 결과"

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:** 원본은 주 모니터를 **640×480 32bpp 60Hz 전용 전체 화면**으로 바꾼다 (창: `WS_POPUP` + `WS_EX_TOPMOST`). 해상도가 바뀌는 동안 보조 모니터 위치가 이동한다. 시작할 때마다 Direct3D 장치 확인창("Attention")이 뜬다. Alt+Tab은 정상이다. **Alt+Enter(창/전체 화면 전환)는 원본에서 아무 동작도 하지 않는다** (원본에 창 모드 없음).
- **상세:** [compat-survey.md](compat-survey.md), [reverse-engineering.md](reverse-engineering.md)
