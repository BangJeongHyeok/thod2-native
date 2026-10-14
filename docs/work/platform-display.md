# 플랫폼: 창·디스플레이

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 1-3, 2-2
- **관련 코드:** `src/platform/` (예정), `src/core/patches/device_dialog.cpp`, `src/core/patches/ddraw_trace.cpp`
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

### G2. 창 모드와 Alt+Enter — 완료 (dgVoodoo2로 해결)

- [x] G2-1 원본이 화면 모드를 바꾸는 시점과 방법 분석 (아래 "화면 모드 분석 결과")
- [x] G2-2 비교 자료: dgVoodoo2의 Alt+Enter 옵션 시험
- [x] G2-3 구현 방식 결정: **B안 (지금은 dgVoodoo2, 직접 구현은 자체 렌더러 단계)**
- [x] G2-4 테두리 없는 창 모드 → dgVoodoo2 시험 C 설정
- [x] G2-5 Alt+Enter 창/전체 화면 전환 → dgVoodoo2 `DisableAltEnterToToggleScreenMode = false`

### G3. dgVoodoo2 권장 구성 제공 — 완료

- [x] G3-1 dgVoodoo2 재배포 조건(라이선스) 확인 → 명시된 라이선스 없음, 함께 배포하지 않음
- [x] G3-2 권장 설정(시험 C + Alt+Enter)을 저장소에 프리셋으로 두고, 설치 도구에서 한 번에 적용 → `config/dgvoodoo-recommended.ini`, `tools/dgvoodoo-setup.py preset`
- [x] G3-3 사용자용 설치 안내 문서 → [docs/INSTALL.md](../INSTALL.md)

## 화면 모드 분석 결과 (2026-10-14)

`[debug] trace_ddraw = 1`로 DirectDraw 호출을 기록했다. dgVoodoo2를 뺀 Windows 기본 DirectDraw 환경, 테스트 사본(`DWM8And16BitMitigation` 호환 모드가 자동으로 걸린 상태, 아래 참고).

### 시작 순서

| 순서 | 호출 | 값 | 호출 위치 (복귀 주소) |
|---|---|---|---|
| 1 | `DirectDrawCreateEx(NULL)` ×1, `(장치 GUID)` ×2 | 장치 열거. 매번 `SetCooperativeLevel(NULL, NORMAL)`, `EnumDisplayModes`, `QueryInterface(IID_IDirect3D7)` | `0x4BD209`, `0x4BD219`, `0x4BD2DE`, `0x4BD2F3` |
| 2 | 장치 확인창 (`MessageBoxA`) | `skip_device_dialog`로 자동 처리 | `0x4A4758` |
| 3 | `RestoreDisplayMode` ×3 | | |
| 4 | `SetCooperativeLevel(hwnd, FULLSCREEN \| ALLOWREBOOT \| EXCLUSIVE)` | 전체 화면 독점 | `0x4BC704` |
| 5 | **`SetDisplayMode(640, 480, 16bpp, refresh 0)`** | **16비트 색상** | `0x4BC7EC` |
| 6 | `CreateSurface(PRIMARY \| FLIP \| COMPLEX \| 3DDEVICE, backbuffers=1)` | 앞뒤 2장 플립 체인 | `0x4BC94F` |
| 7 | `CreateSurface(ZBUFFER \| VIDEOMEMORY, 640×480, 16bit)` | Z 버퍼 | `0x4BACE7` |
| 8 | D3D 내부 버퍼 생성 (`D3DIM700.DLL`이 호출) | | |
| 9 | 텍스처 약 700개 (`TEXTURE`, 16bit, 일부 `ALPHAPIXELS`) | | |
| 10 | 매 프레임 **`Flip(NULL, DDFLIP_WAIT)`** on primary | **초당 59.7~59.8회** (60Hz 모니터) | `0x4B9896` |

- `Blt`, `BltFast`는 기록되지 않았다 (게임이 화면 버퍼에 직접 복사하지 않음).
- 백버퍼를 가져오는 `GetAttachedSurface`(vtable 12)는 기록되지 않았다. `EnumAttachedSurfaces` 등 다른 방법을 쓰는지는 미확인.
- 화면 설정 API(`EnumDisplaySettings`)로는 모드가 `640×480 32bpp`로 보였다. 게임은 16bpp를 요청했으므로, 실제 화면은 32bpp이고 16비트는 Windows 호환 모드가 흉내 내는 것으로 보인다.

### Alt+Tab 때

1. 나가면 `Flip`이 `DDERR_SURFACELOST`(`0x887601C2`)를 돌려준다.
2. 게임이 `TestCooperativeLevel`(호출 위치 `0x4A4B69`)을 부르고 `DDERR_NOEXCLUSIVEMODE`(`0x887600E1`)를 받는다. 이 상태로 기다린다.
3. 돌아오면 `IsLost` → primary, Z 버퍼 등 `Restore` → `TestCooperativeLevel` OK → 텍스처 일부를 다시 만든다.

### Windows 호환 모드가 DirectDraw를 가로채고 있음

- **Windows PCA(프로그램 호환성 관리자)가 `Hod2.exe`에 `DWM8And16BitMitigation` 호환 모드를 자동으로 등록했다** (HKCU와 HKLM `AppCompatFlags\Layers`, 경로별). 테스트 사본은 처음 실행했을 때는 등록이 없었고, 그 뒤에 생겼다. 원본 폴더와 처음 풀었던 `unpack` 폴더에도 등록되어 있다.
- 이 호환 모드는 `apphelp.dll`이 DirectDraw 함수 표를 덮어써서 동작한다.
  - `IDirectDraw7`: `QueryInterface`, `Release`, `CreateSurface`, `GetCaps`, `GetDisplayMode`, `Initialize`, `RestoreDisplayMode`, `SetCooperativeLevel`, `SetDisplayMode` (9칸)
  - `IDirectDrawSurface7`: `Blt`, `BltFast`, `Flip` (3칸)
- **DirectDraw 객체가 만들어질 때마다 다시 덮어쓴다.** 그래서 우리 후킹을 한 번만 설치하면 지워진다. `ddraw_trace`는 객체·버퍼가 만들어질 때마다 다시 설치하고, apphelp 함수를 원본으로 삼아 이어서 부른다.
- 다른 사용자의 PC에는 이 등록이 있을 수도 없을 수도 있다. 우리 코드는 두 경우 모두에서 동작해야 한다.

### Alt+Enter 비교

| 환경 | Alt+Enter 결과 |
|---|---|
| 원본 (dgVoodoo2 없음) | 아무 변화 없음 |
| dgVoodoo2 시험 C + `DisableAltEnterToToggleScreenMode = false` | 누를 때마다 테두리 없는 창(`WS_POPUP\|WS_BORDER`, 1920×1080) ↔ 전체 화면(최상위 창, 1920×1080)으로 바뀐다. 모니터 해상도는 바뀌지 않는다 |

## 구현 방식 후보 (B안으로 결정)

| 방식 | 내용 | 장점 | 단점 |
|---|---|---|---|
| A. DX7 위에서 직접 창 모드 | `SetCooperativeLevel`을 `NORMAL`로 바꾸고 `SetDisplayMode`를 건너뛴다. 플립 체인 대신 화면 밖 버퍼 + 클리퍼로 만들고 `Flip`을 창으로의 `Blt`로 바꾼다 | dgVoodoo2 없이 동작 | 16bpp 렌더 버퍼를 32bpp 화면에 옮기는 변환, apphelp 호환 모드와의 충돌, 백버퍼 획득 방식 등 해결할 것이 많다. **2단계 자체 렌더러가 생기면 버리는 코드** |
| B. 지금은 dgVoodoo2로 화면 처리 | dgVoodoo2 시험 C + Alt+Enter 옵션을 권장 구성으로 정하고, 설치·설정을 도구로 제공한다. 창 모드 직접 구현은 2단계 렌더러(ROADMAP 2-3)에서 한다 | 이미 검증됨 (테두리 없는 창, 고해상도, Alt+Tab, Alt+Enter) | 외부 도구 의존. 재배포 조건(라이선스) 확인 필요 |

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-14 | 장치 확인창은 호출 위치(`0x4A4758`)와 제목(`Attention`)이 둘 다 맞을 때만 가로챈다 | 같은 `MessageBoxA`로 띄우는 다른 창(종료 확인, CD 요청, 장치 미지원)을 건드리지 않기 위해. 대안: 제목·본문 문자열만으로 판별(다른 빌드에서도 동작하지만 오판 가능) |
| 2026-10-14 | 확인창 처리 결과는 "예"(`IDYES`)로 고정 | 사용자가 항상 "예"를 눌러 왔고, "아니요"의 동작은 아직 분석하지 않음 |
| 2026-10-14 | **창 모드·Alt+Enter·고해상도는 지금은 dgVoodoo2로 처리한다 (B안, 사용자 결정).** DX7 수준의 창 모드 직접 구현(A안)은 하지 않고, 자체 렌더러(ROADMAP 2-3)에서 구현한다 | A안은 16bpp→32bpp 변환, apphelp 호환 모드와의 충돌, 백버퍼 획득 방식 미확인 등 공수가 크고, 자체 렌더러가 생기면 버리는 코드다. dgVoodoo2 시험 C + Alt+Enter 옵션은 이미 검증됨. 위험: 외부 도구 의존, 재배포 조건 확인 필요 |
| 2026-10-14 | **dgVoodoo2는 배포물에 넣지 않는다.** 사용자가 공식 릴리스에서 받고, 우리는 설정 프리셋과 설치 도구만 제공한다 | GitHub 저장소(`dege-diosg/dgVoodoo2`)에 명시된 라이선스가 없다. README의 "freely ship" 문구는 64비트 개발자용 API DLL에 대한 것이다 |
| 2026-10-14 | `skip_device_dialog` 기본값은 켜짐(1)으로 유지 (사용자 결정) | 검토한 위험: 잘못된 장치(예: 내장 그래픽)가 골라져도 화면에서 알 수 없고 로그에만 남는다. 대안: 기본 꺼짐 |

## 미해결 질문

- 백버퍼를 어떻게 가져오는지 (`GetAttachedSurface`는 불리지 않음)
- PCA 호환 모드가 없는 환경에서의 동작 (HKLM 등록 삭제는 관리자 권한 필요)
- 렌더러 도메인과의 경계 (백버퍼 크기는 어느 쪽이 정하는지)
- 원본 해상도는 640×480 고정이고, 이를 바꾸는 설정이 없다 (`hglCore.ini`의 `FullScreen` 키는 쓰이지 않음)

## 작업 기록

### 2026-10-15 — 권장 프리셋에 MSAA 4x 추가 (렌더러에서 넘어온 내용)

- **결과:** dgVoodoo2 권장 프리셋(`config/dgvoodoo-recommended.ini`)에 `Antialiasing = 4x`가 들어갔다. 텍스처 필터링은 원본 그대로.
- **상세:** [renderer.md](renderer.md) "dgVoodoo2 화질 옵션 비교"

### 2026-10-14 — dgVoodoo2 권장 구성 제공 (관련: G3)

- **한 일:**
  - dgVoodoo2 GitHub 저장소의 라이선스를 확인했다 (GitHub API `license: null`, README 확인).
  - 권장 설정 프리셋 `config/dgvoodoo-recommended.ini`를 만들었다 (설정마다 이유를 주석으로 적음).
  - 설치 도구를 사용자용으로 `tools/compat/dgvoodoo-setup.py` → `tools/dgvoodoo-setup.py`로 옮기고 `preset` 명령을 추가했다.
  - 값을 바꿀 때 줄 끝의 `\r`이 지워지던 버그를 고쳤다 (원본 `dgVoodoo.conf`는 CRLF).
  - 사용자용 설치 안내 [docs/INSTALL.md](../INSTALL.md)를 썼다.
- **결과:**
  - 테스트 사본에서 `uninstall` → `install` → `preset`으로 다시 설치한 `dgVoodoo.conf`가 손으로 맞춘 이전 설정과 내용이 같다. 줄 끝은 원본처럼 전부 CRLF(365줄).
  - 실행 결과: 테두리 없는 1920×1080 창, 모니터 해상도 변화 없음, 시작 확인창 없음.
- **다음:** 타이밍(CPU 100%) 분석.

### 2026-10-14 — 화면 모드 분석 (관련: G2-1, G2-2)

- **한 일:**
  - 개발용 패치 `ddraw_trace`(`[debug] trace_ddraw = 1`)를 만들었다. `DDRAW!DirectDrawCreateEx` IAT 후킹 → `IDirectDraw7`·`IDirectDrawSurface7` 함수 표 후킹으로 호출을 기록한다. 매 프레임 불리는 함수는 처음 3번만 자세히, 이후 5초마다 횟수를 요약한다.
  - 함수 표 후킹 도우미 `thod2::interop::HookVtable`을 추가했다.
  - dgVoodoo2를 뺀 테스트 사본에서 실행하고 Alt+Tab을 시험했다 (`tools/compat/alttab-run.ps1`).
  - dgVoodoo2를 다시 설치하고 Alt+Enter를 시험했다 (`tools/compat/altenter-run.ps1`).
- **결과:**
  - 위 "화면 모드 분석 결과".
  - 처음 기록에서 전체 화면 전환 호출이 빠졌다. 함수 표를 확인해 보니 `apphelp.dll`이 우리 후킹을 덮어쓰고 있었다. 게임이 DirectDraw 객체를 3번 만들 때마다 다시 덮어쓴다. 만들어질 때마다 다시 설치하도록 고쳐서 전체 흐름을 기록했다.
  - 이 과정에서 Windows PCA가 테스트 사본에 호환 모드를 자동으로 등록한 것을 발견했다. 첫 실행 뒤의 측정은 모두 이 호환 모드가 켜진 상태였다.
- **다음:** 구현 방식 A/B 결정. → 사용자가 B안으로 결정 (결정 사항 표).

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
  - 이후 사용자가 기본값 켜짐 유지를 결정했다 (결정 사항 표).
- **다음:** G2 창 모드와 Alt+Enter.

### 2026-10-14 — dgVoodoo2 테두리 없는 창 모드 결과

- **결과:** dgVoodoo2 시험 C 설정으로 테두리 없는 전체 화면 크기 창이 되고, 모니터 해상도와 배치가 바뀌지 않는다. Alt+Tab은 주입 방식과 사용자 실제 키보드 모두 정상이다. 사용자 플레이에서 화면과 게임 속도도 정상이었다.
- **상세:** [compat-survey.md](compat-survey.md) "dgVoodoo2 시험 결과"

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:** 원본은 주 모니터를 **640×480 32bpp 60Hz 전용 전체 화면**으로 바꾼다 (창: `WS_POPUP` + `WS_EX_TOPMOST`). 해상도가 바뀌는 동안 보조 모니터 위치가 이동한다. 시작할 때마다 Direct3D 장치 확인창("Attention")이 뜬다. Alt+Tab은 정상이다. **Alt+Enter(창/전체 화면 전환)는 원본에서 아무 동작도 하지 않는다** (원본에 창 모드 없음).
  - 정정 (화면 모드 분석): 게임이 요청하는 모드는 16bpp다. 32bpp로 보인 것은 Windows 호환 모드가 16비트를 흉내 내기 때문으로 보인다.
- **상세:** [compat-survey.md](compat-survey.md), [reverse-engineering.md](reverse-engineering.md)
