# 리버스 엔지니어링

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 2-1
- **관련 코드:** `re/`
- **마지막 갱신:** 2026-10-14

## 주요 목표

원본 게임의 동작(함수, 전역 변수, 구조체, 호출 관계)을 여러 사람이 공유할 수 있는 텍스트 형태로 기록한다. 다른 도메인이 함수를 재구현할 때 필요한 근거를 제공한다. 원본 코드를 포함하는 Ghidra 프로젝트 파일이나 디컴파일 코드는 커밋하지 않는다.

**완료 기준:** `re/functions.csv`와 Ghidra 적용 스크립트로 누구나 같은 분석 상태를 재현할 수 있고, 각 도메인이 필요로 하는 핵심 함수의 위치가 기록되어 있다.

## 중목표 / 소목표

### G1. 실행 파일 기본 구조 파악 — 진행 중

- [x] G1-1 PE 헤더, 섹션, 임포트 테이블 정리
- [x] G1-2 `hglCore.ini` 처리 코드 분석
- [x] G1-3 CD 확인 코드와 수정 흔적 확인
- [ ] G1-4 `Toddebug.ini` 처리 코드 분석 (참조 위치 `0x40E4D9`)
- [ ] G1-5 디스플레이 모드(640×480) 설정 위치 찾기

### G2. 분석 도구 환경 — 대기

- [ ] G2-1 Ghidra(+ JDK 21), x64dbg 설치 (사용자 확인 후)
- [ ] G2-2 분석 결과 내보내기 형식 결정, `re/functions.csv` 시작

## 확인된 주소

| 주소 (VA) | 종류 | 이름 (임시) | 설명 | 근거 | 상태 |
|---|---|---|---|---|---|
| `0x4A5850`~ | 함수 내부 | (WinMain 계열 초기화) | `CreateWindowExA` 호출 후 `0x4A60A0` 호출 | 바이트 확인 | 추정 |
| `0x7DC8AC` | 전역 변수 | `g_hWnd` | 게임 메인 창 핸들. `0x4A588D`에서 `CreateWindowExA` 결과를 저장. 창 클래스와 제목은 모두 `"The House of the Dead 2"` | 바이트 확인, 실행 중 창 확인 | 확인 |
| `0x41EBA3` | 호출 | — | DirectInput 장치 `SetCooperativeLevel(g_hWnd, 9)` (vtable `+0x34`). 9 = `DISCL_EXCLUSIVE \| DISCL_BACKGROUND`. 장치 포인터는 배열 `0x7DAE18[i]`에서 꺼냄 → 컨트롤러·라이트건 장치로 추정 (키보드는 이 조합 불가) | 바이트 패턴 검색 | 확인 (장치 종류는 추정) |
| `0x7DAE18` | 전역 변수 | `g_diDevices[]` | DirectInput 장치 포인터 배열로 추정 | `0x41EBA3` 참조 | 추정 |
| `0x4A5899` | 패치 흔적 | — | 5바이트 `NOP`. 원래 CD 확인 함수 호출(`call`)이 있던 자리로 추정 | `0x4A5C50`을 호출하는 곳이 전혀 없음 | 추정 (강함) |
| `0x4A5C50` | 함수 | `CheckCdRom` | `[0x7DC890]==0`이면 통과, `CDCheckPass` 비트(0x40)면 통과, 아니면 `GetLogicalDriveStringsA` + `GetDriveTypeA()==5`(CD-ROM)로 드라이브 검사 | 바이트 확인, 호출자 없음 | 확인 |
| `0x4A6070` | 함수 | `GetHglCoreIniPath` | 인자가 NULL이면 기본값 `"hglCore.ini"`(`[0x5976C4]`)를 `GetFullPathNameA`로 절대 경로로 바꿔 `0x7DD660`에 저장. **현재 작업 폴더 기준** | 바이트 확인 | 확인 |
| `0x4A60A0` | 함수 | `LoadHglCoreIni` | `[HGLCORE]` 키를 `GetPrivateProfileStringA`로 읽고 첫 글자가 `t`/`T`면 `0x7DCE00`에 비트 OR. `SmallTexure`는 `GetPrivateProfileIntA` (1 → 0x08, 2 → 0x10) | 바이트 확인 | 확인 |
| `0x7DCE00` | 전역 변수 | `g_hglCoreFlags` | 아래 비트 표 | 쓰기/읽기 위치 확인 | 확인 |
| `0x4AC861` | 호출 위치 | — | CRT 시작 코드의 첫 `GetCommandLineA` 호출 직후 주소 (엔트리 `0x4AC7D5` 안). 프로세스에서 가장 이른 로더 락 밖 시점 | dinput_probe 실행 기록 | 확인 |
| `0x4A588B` | 호출 위치 | — | 메인 창 `CreateWindowExA` 호출 직후 주소. 바로 뒤 `0x4A588D`에서 `g_hWnd`에 저장 | dinput_probe 실행 기록 | 확인 |
| `0x4BD209` | 호출 위치 | — | `DirectDrawCreateEx` 호출 직후 주소. 시작할 때 3번 호출됨 (장치 열거) | dinput_probe 실행 기록 | 확인 |
| `0x4A4758` | 호출 위치 | — | 시작 확인창 `MessageBoxA("...will be used.", "Attention", MB_YESNO \| MB_ICONEXCLAMATION)` 호출 직후 주소 | dinput_probe 실행 기록. 코드: `hod2::kDeviceDialogMessageBoxReturn` (skip_device_dialog에서 사용) | 확인 |
| `0x4BD209` 등 | 호출 위치 | — | 장치 열거: `DirectDrawCreateEx`(`0x4BD209`), `SetCooperativeLevel(NULL, NORMAL)`(`0x4BD219`, `0x4BD2DE`), `QueryInterface(IID_IDirect3D7)`(`0x4BD2F3`) | ddraw_trace 기록 | 확인 |
| `0x4BC704` | 호출 위치 | — | `SetCooperativeLevel(hwnd, FULLSCREEN \| ALLOWREBOOT \| EXCLUSIVE)` 직후 주소 | ddraw_trace 기록 | 확인 |
| `0x4BC7EC` | 호출 위치 | — | `SetDisplayMode(640, 480, 16, 0, 0)` 직후 주소 | ddraw_trace 기록 | 확인 |
| `0x4BC94F` | 호출 위치 | — | primary 플립 체인 `CreateSurface(PRIMARY \| FLIP \| COMPLEX \| 3DDEVICE, 백버퍼 1)` 직후 주소 | ddraw_trace 기록 | 확인 |
| `0x4BACE7` | 호출 위치 | — | Z 버퍼 `CreateSurface(ZBUFFER, 640×480, 16bit)` 직후 주소 | ddraw_trace 기록 | 확인 |
| `0x4B9896` | 호출 위치 | — | 매 프레임 `Flip(NULL, DDFLIP_WAIT)` 직후 주소. 메인 루프 안으로 추정 | ddraw_trace 기록 | 확인 |
| `0x4A4B69` | 호출 위치 | — | `TestCooperativeLevel` 직후 주소. 화면을 잃었을 때(Alt+Tab) 반복 호출 | ddraw_trace 기록 | 확인 |
| `0x4A4B30` | 함수 | (프레임 끝 대기) | 프레임마다 목표 시간(16.67ms + 보정)까지 `QueryPerformanceCounter`로 바쁜 대기. 호출하는 곳 `0x4A59DF`, `0x4AB2F0`. D3DX `UpdateFrame`일 가능성 | 바이트 해석, profiler 핫스팟 | 확인 (정체는 추정) |
| `0x4A4D0F`~`0x4A4D63` | 루프 | — | 위 함수의 대기 루프. 콜백 호출 → QPC(`call edi`, 복귀 `0x4A4D30`) → 경과 ms 계산 → 비교 | 바이트 해석 | 확인 |
| `0x597444` | 전역 변수 (float) | `g_frameTimeMs` | 목표 프레임 시간 16.6667 (= 1000/60) | 파일 초기값, `0x4A4B8F`에서 읽음 | 확인 (초기값) |
| `0x59744C` | 전역 변수 (int) | — | 60 (목표 fps로 추정) | 파일 초기값 | 추정 |
| `0x597440`, `0x59743C` | 전역 변수 (float) | — | 10.0 (보정 상한 ms), 0.2 (보정 단계) | 바이트 해석 | 추정 |
| `0x597450`, `0x597454` | 전역 변수 (int) | — | 20, 333 (보정 판단 기준과 주기 ms로 추정) | 파일 초기값 | 추정 |
| `0x7DCE30` | 전역 변수 (float) | — | 프레임 시간 보정 누적값 (±10ms로 제한) | 바이트 해석 | 추정 |
| `0x7DCE38` | 전역 변수 | — | 직전 프레임의 QPC 값 (하위 32비트) | 바이트 해석 | 확인 |
| `0x7DCE3C` | 전역 변수 (float) | — | QPC 주파수 | 바이트 해석 | 확인 |
| `0x7DCDFC` | 전역 변수 (함수 포인터) | `g_frameWaitCallback` | 대기 루프에서 남은 ms를 인자로 부르는 콜백. `0x49DF0B`에서 `0x49E0C0` 저장 (즉시값 위치 `0x49DF11`). 코드: `hod2::kFrameWaitCallbackSlot`, `kFrameWaitCallbackImmediate` (frame_wait_sleep이 즉시값을 바꿈) | 바이트 해석 | 확인 |
| `0x49E0C0` | 함수 | (대기 콜백) | `cdecl(float 남은 ms)`. 1.0ms보다 크면 `0x41D5D0`으로 점프 | 바이트 해석 | 확인 |
| `0x41D620` | 함수 | (작업 대기열 처리) | 64칸 순환 대기열(`0x7D8EB0`, 항목 16바이트)에서 하나를 꺼내 `0x41D670`으로 처리. 읽기 위치 `[0x7D8AA8]`, 쓰기 위치 `[0x7D8AAC]`. 처리 함수 표 `0x587C30` | 바이트 해석 | 확인 (작업 종류는 미확인) |
| `0x41E812` | 호출 위치 | — | `DirectInputCreateEx(version 0x700)` 호출 직후 주소. 확인창 뒤에 불림 | dinput_probe 실행 기록 | 확인 |
| `0x7DC890` | 전역 변수 | — | `CheckCdRom`이 처음에 확인. 0이면 CD 확인 없이 통과 | 바이트 확인 | 확인 (의미는 미확인) |
| `0x7DD660` | 전역 변수 | `g_hglCoreIniPath` | `hglCore.ini` 전체 경로 (256바이트) | 바이트 확인 | 확인 |

### `g_hglCoreFlags` (`0x7DCE00`) 비트

| 비트 | 키 | 읽는 위치 | 비고 |
|---|---|---|---|
| 0x01 | `AllDevicePass` | `0x4A41E4` | 장치 검사 관련으로 추정 |
| 0x02 | `DispFps` | 없음 | **쓰이지 않음** |
| 0x04 | `FullScreen` | 없음 | **쓰이지 않음** |
| 0x08 | `SmallTexure=1` | `0x4A5171` | |
| 0x10 | `SmallTexure=2` | (미확인) | |
| 0x20 | `DispVersion` | 없음 | **쓰이지 않음** |
| 0x40 | `CDCheckPass` | `0x4A5C72` | `CheckCdRom` 안에서 확인 |
| 0x80 | `UseZBuffer` | `0x4A448D` | |
| 0x100 | `UseHEL` | `0x4A4672` | HEL = DirectDraw 소프트웨어 에뮬레이션 |

"읽는 위치 없음"은 `0x7DCE00`을 직접 참조하는 명령을 기준으로 한 결과다. 값을 다른 변수로 복사해서 쓰는 경우까지 확인한 것은 아니다.

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-14 | 정밀 분석 도구가 갖춰지기 전까지는 Python으로 바이트를 직접 읽어서 분석하고, 결과에 "바이트 확인"이라고 근거를 남긴다 | 디스어셈블러(Ghidra, capstone)가 아직 없음. 외부 도구 설치는 사용자 확인 필요 |

## 미해결 질문

- Ghidra 분석 결과를 어떤 형식으로 내보낼지 (CSV, JSON, Ghidra 스크립트)
- `Toddebug.ini` 키가 실제로 동작하는지
- `0x4A5899`의 NOP 자리에 원래 무엇이 있었는지 (수정 안 된 원본 `Hod2.exe`가 있으면 비교 가능)
- 원래 CD 확인 실패 시 동작 (메시지 `"Please insert the 'The House of The Dead 2' CD."` 출력 위치)
- 실행 파일에서 수정된 곳이 `0x4A5899` 말고도 있는지
- 플래그를 레지스터로 넘기는 다른 `SetCooperativeLevel` 호출이 있는지 (상수 push 패턴으로는 `0x41EBA3` 한 곳만 발견)

## 작업 기록

### 2026-10-14 — 프레임 대기 루프와 작업 대기열 (관련: G1)

- **한 일:** 개발용 `profiler`로 찾은 핫스팟 주소의 바이트를 직접 해석했다 ([platform-timing.md](platform-timing.md) "CPU 100% 분석 결과").
- **결과:** "확인된 주소" 표에 프레임 대기 함수, 목표 프레임 시간 변수, 대기 콜백, 작업 대기열 관련 주소 13개를 추가했다.

### 2026-10-14 — DirectDraw 호출 위치 (관련: G1)

- **한 일:** `ddraw_trace`로 DirectDraw 호출과 복귀 주소를 기록했다 ([platform-display.md](platform-display.md) "화면 모드 분석 결과").
- **결과:** "확인된 주소" 표에 호출 위치 7종을 추가했다. 매 프레임 `Flip` 호출 위치 `0x4B9896`은 메인 루프를 찾는 출발점이 된다.

### 2026-10-14 — 시작 순서와 호출 위치 (관련: G1)

- **한 일:** 측정용 `dinput.dll` 프록시로 시작 과정의 API 호출 순서와 복귀 주소를 기록했다 (loader.md "측정 결과: 초기화 시점").
- **결과:** "확인된 주소" 표에 호출 위치 5개를 추가했다. 복귀 주소는 `call` 명령 바로 다음 주소다. 시작 확인창은 `MessageBoxA`로 띄운다 (`type 0x34`).

### 2026-10-14 — DirectInput 협력 수준과 창 핸들 (관련: G1)

- **한 일:** `call [reg+0x34]` 앞에 작은 상수 push가 있는 패턴을 찾아 DirectInput `SetCooperativeLevel` 호출을 찾았다.
- **결과:** `0x41EBA3` 한 곳. 위 "확인된 주소" 표에 `g_hWnd`(`0x7DC8AC`), `g_diDevices[]`(`0x7DAE18`)와 함께 정리했다. 원래 Alt+Tab 문제 원인으로 의심했으나 사용자가 Alt+Enter와 혼동한 것으로 확인되어 그 가설은 폐기했다.

### 2026-10-14 — `hglCore.ini`와 CD 확인 코드 분석 (관련: G1-2, G1-3)

- **한 일:** Python으로 문자열의 VA를 구하고, 그 VA를 참조하는 명령과 임포트 주소 테이블(IAT) 호출을 찾아서 바이트를 직접 해석했다. `hglCore.ini`(`DispFps=true`, `DispVersion=true`)를 만들어서 실제로 실행해 봤다.
- **결과:**
  - 위 "확인된 주소" 표와 비트 표에 정리했다.
  - `hglCore.ini`는 게임 폴더(작업 폴더)에서 읽는다. 값은 `true`/`True`로 시작하면 켜진다.
  - `DispFps`, `FullScreen`, `DispVersion`은 읽기만 하고 쓰이지 않는다. 실제 실행에서도 FPS나 버전이 표시되지 않았다.
  - CD 확인 함수 `0x4A5C50`은 실행 파일 안에 호출하는 곳이 없고, 초기화 코드 `0x4A5899`에 `call` 크기(5바이트)의 `NOP`이 있다. **현재 실행 파일은 CD 확인을 지운 수정본으로 판단한다.** 파일 수정일(2010-07-04)이 링크 날짜(2001-03-22)와 다른 이유로 보인다.
  - `Config.exe`(MFC 앱 `Hod2_Tool`)는 컨트롤러, "MC PC Light Gun" 속성, 화면 밝기, 번쩍임 시간·색상만 설정한다. 해상도 설정은 없다. `Hod2.exe`와 `Config.exe` 둘 다 `DeviceIoControl`을 임포트한다 (라이트건 드라이버와 통신하는 것으로 추정).
- **다음:** loader와 platform-filesystem에 "수정본 실행 파일" 문제를 넘긴다. 디스플레이 모드 설정 위치(G1-5)를 찾는다.

### 2026-10-13 — 실행 파일 초기 정적 분석

- **한 일:** `Hod2.exe`, `Config.exe`의 PE 헤더, 섹션, 임포트 테이블, 문자열을 확인했다.
- **결과:**
  - 패킹 없음, 재배치 정보 없음, 이미지 베이스 `0x400000` 고정, 엔트리 포인트 VA `0x4AC7D5`
  - 임포트: `DDRAW`(DirectDrawCreateEx), `DINPUT`(DirectInputCreateEx), `DSOUND`(#1), `ole32`, `IMM32`, 일반 Win32
  - D3DX(DX7) 정적 링크 흔적 (`CD3duContext::*` 문자열)
  - 숨은 설정 `hglCore.ini`(`FullScreen`, `DispFps`, `CDCheckPass` 등)와 `Toddebug.ini` 문자열 발견
  - 상세 내용은 [AGENTS.md 3절](../../AGENTS.md)에 정리
- **다음:** Ghidra 프로젝트 생성, 분석 결과 내보내기 형식 결정
