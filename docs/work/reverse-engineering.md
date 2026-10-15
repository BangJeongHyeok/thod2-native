# 리버스 엔지니어링

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 2-1
- **관련 코드:** `re/`
- **마지막 갱신:** 2026-10-15

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

### G2. 분석 도구 환경 — 진행 중

- [x] G2-1 Ghidra(+ JDK 21), x64dbg, capstone 설치 (2026-10-15 사용자 확인 후. 아래 "분석 도구")
- [ ] G2-2 분석 결과 내보내기 형식 결정, `re/functions.csv` 시작

## 분석 도구

2026-10-15에 사용자 확인 후 받았다. 모두 저장소 밖(`E:\Repos\hod2-native\external\`)에 있고, 시스템 PATH나 환경 변수는 바꾸지 않았다.

| 도구 | 버전 | 받은 파일 (출처) | SHA-256 | 설치 위치 |
|---|---|---|---|---|
| Ghidra | 12.1.4 (PUBLIC, 2026-09-21) | `ghidra_12.1.4_PUBLIC_20260921.zip`, 569,649,598 bytes (GitHub `NationalSecurityAgency/ghidra` 릴리스 `Ghidra_12.1.4_build`) | `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` (릴리스 노트 값과 일치) | `external\ghidra\ghidra_12.1.4_PUBLIC\` |
| Eclipse Temurin JDK | 21.0.12.1+1 LTS (x64) | `OpenJDK21U-jdk_x64_windows_hotspot_21.0.12.1_1.zip`, 205,073,461 bytes (GitHub `adoptium/temurin21-binaries`, Adoptium API의 최신 안정판) | `f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e` (Adoptium API 값과 일치) | `external\jdk\jdk-21.0.12.1+1\` |
| x64dbg (x32dbg 포함) | 2026.05.27 스냅샷 (commit `9c8ca1cae0b6d56cc44f31fddcb10e3b02ffbb87`) | `snapshot_2026-05-27_12-11.zip`, 31,526,833 bytes (GitHub `x64dbg/x64dbg` 릴리스 `2026.05.27`) | `d41966dfc5b435a372798245300ca0ab7bb8e48bdbf48512c6fb20fcca427697` (GitHub asset digest와 일치) | `external\x64dbg\release\x32\x32dbg.exe` |
| capstone (Python) | 5.0.9 (pip 패키지. `capstone.__version__`은 `5.0.7`로 표시) | PyPI, `pip install --user capstone` (Python 3.14.7) | — | 사용자 site-packages |

- 받은 압축 파일은 `external\downloads\`에 남겨 두었다. 세 도구를 합친 설치 크기는 약 1,273MB다.
- Ghidra 12.1.4는 JDK 21(64비트)을 요구한다 (`GettingStarted.md`). Python 3.9~3.14는 디버거와 PyGhidra를 쓸 때만 필요하다.
- **Ghidra 실행:** `.\tools\re\ghidra.ps1` (GUI), `.\tools\re\ghidra.ps1 -Headless <analyzeHeadless 인자>`. Ghidra의 `launch.bat`는 `launch.properties`를 읽기 **전에** PATH나 `JAVA_HOME`에서 `java.exe`를 찾고, 없으면 바로 실패한다. 그래서 스크립트가 그 프로세스에만 `JAVA_HOME`을 넣는다. 위치는 `THOD2_GHIDRA_DIR`, `THOD2_JDK_DIR`로 바꿀 수 있다.
- `external\ghidra\ghidra_12.1.4_PUBLIC\support\launch.properties`의 `JAVA_HOME_OVERRIDE`도 위 JDK로 지정해 두었다 (Ghidra 설치본 수정, 저장소 밖).
- **Ghidra 프로젝트:** `E:\Repos\hod2-native\ghidra-projects\hod2.gpr`. 원본 코드가 들어가므로 저장소 밖에 두고 커밋하지 않는다. 테스트 사본의 `Hod2.exe`(SHA-256은 AGENTS.md 3절과 같음)를 임포트했다.

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
| `0x49F990` | 호출 위치 | — | 세이브 파일 4개 읽기 `CreateFileA` 직후 주소 (시작 때) | file_trace 기록 | 확인 |
| `0x49FA17`, `0x49FA8C` | 호출 위치 | — | 세이브 파일 4개 쓰기: `CREATE_ALWAYS` 직후, 이어서 `OPEN_EXISTING` 쓰기 직후 주소 (정상 종료 때) | file_trace 기록 | 확인 |
| `0x4BBF28` | 호출 위치 | — | `<Windows>\d3dx.dat`를 `OPEN_ALWAYS` 읽기·쓰기로 여는 `CreateFileA` 직후 주소. 경로는 `GetWindowsDirectoryA`(`0x4C0E13`)로 만듦 | file_trace 기록 | 확인 |
| `0x41DF23` | 호출 위치 | — | `\.\SEGAJOY.VXD` 열기 시도 직후 주소 (라이트건 드라이버) | file_trace 기록 | 확인 |
| `0x40E4EB`, `0x41DF88`, `0x49DF2F`, `0x4A6093` | 호출 위치 | — | `Toddebug.ini`, `Hod2.ini`(×2), `hglCore.ini`의 `GetFullPathNameA` 직후 주소 | file_trace 기록 | 확인 |
| `0x4A0899` | 명령 | — | 원본 폴더 `exception.log`의 접근 위반 주소 (`Eax = 0`, `Esi = 0`) | exception.log | 확인 (원인 미확인) |
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
| 2026-10-15 | 분석 도구는 `external\`, Ghidra 프로젝트는 `ghidra-projects\`(둘 다 저장소 밖)에 두고, 실행 스크립트만 저장소(`tools/re/`)에 둔다 | 도구 바이너리와 원본 코드가 들어간 프로젝트는 커밋하지 않는다는 원칙. 시스템 JDK 설치나 PATH 변경 대신 프로세스 단위 `JAVA_HOME`으로 다른 환경에 영향을 주지 않는다 |
| 2026-10-14 | 정밀 분석 도구가 갖춰지기 전까지는 Python으로 바이트를 직접 읽어서 분석하고, 결과에 "바이트 확인"이라고 근거를 남긴다 | 디스어셈블러(Ghidra, capstone)가 아직 없음. 외부 도구 설치는 사용자 확인 필요 |

## 미해결 질문

- Ghidra 분석 결과를 어떤 형식으로 내보낼지 (CSV, JSON, Ghidra 스크립트)
- `Toddebug.ini` 키가 실제로 동작하는지
- `0x4A5899`의 NOP 자리에 원래 무엇이 있었는지 (수정 안 된 원본 `Hod2.exe`가 있으면 비교 가능)
- 원래 CD 확인 실패 시 동작 (메시지 `"Please insert the 'The House of The Dead 2' CD."` 출력 위치)
- 실행 파일에서 수정된 곳이 `0x4A5899` 말고도 있는지
- 플래그를 레지스터로 넘기는 다른 `SetCooperativeLevel` 호출이 있는지 (상수 push 패턴으로는 `0x41EBA3` 한 곳만 발견)

## 작업 기록

### 2026-10-15 — 분석 도구 설치와 Ghidra 첫 분석 (관련: G2-1)

- **한 일:** 사용자 확인 후 Ghidra 12.1.4, Temurin JDK 21.0.12.1, x64dbg 2026.05.27, capstone 5.0.9를 받았다 (위 "분석 도구"). 받은 압축 파일 세 개의 SHA-256을 공식 값과 대조했다. `tools/re/ghidra.ps1`로 Ghidra를 실행하게 했다. 테스트 사본의 `Hod2.exe`를 헤드리스 모드로 임포트하고 기본 자동 분석을 돌렸다.
- **결과:**
  - 처음에는 `launch.properties`의 `JAVA_HOME_OVERRIDE`만 지정해서 실패했다 (`ERROR: The 'java' command could not be found in your PATH or with JAVA_HOME.`). `launch.bat`가 그 설정을 읽기 전에 `java.exe`를 먼저 찾기 때문이다. 프로세스에 `JAVA_HOME`을 넣으니 동작했다.
  - 임포트와 분석 성공. 언어 `x86:LE:32:default`, 컴파일러 사양 `windows`. 자동 분석으로 **함수 2,030개**를 찾았다. 분석 70초 (그중 `Decompiler Switch Analysis` 48.9초), 전체 87초. PDB는 없다.
  - 이미 확인한 주소와 대조: `0x4AC7D5`(entry), `0x4A6070`, `0x4A60A0`, `0x4A4B30`, `0x41D620`은 함수 시작으로 잡혔다. **`0x4A5C50`(`CheckCdRom`)과 `0x49E0C0`(대기 콜백)은 함수로 잡히지 않았다.** 앞의 것은 호출하는 곳이 없고, 뒤의 것은 함수 포인터로 저장만 되기 때문이다. 자동 분석이 놓치는 이런 함수는 적용 스크립트에서 직접 만들어야 한다.
  - `tools/re/ghidra.ps1 -Headless`를 Git Bash에서 `powershell -File`로 부르면 출력이 보이지 않았다. PowerShell에서 직접 부르면 정상이다 (원인 미확인).
- **다음:** G2-2. `re/functions.csv` 형식을 정하고 "확인된 주소" 표를 옮긴다. CSV를 Ghidra 프로젝트에 적용하는 스크립트를 만든다 (이름, 함수 생성, 주석). 첫 분석 대상은 입력 경로다 ([platform-input.md](platform-input.md)).

### 2026-10-15 — 파일 접근 호출 위치 (관련: G1)

- **한 일:** `file_trace`로 파일 API 호출 위치를 기록했다 ([platform-filesystem.md](platform-filesystem.md) "파일 접근 분석 결과").
- **결과:** "확인된 주소" 표에 세이브 읽기·쓰기, `d3dx.dat`, 라이트건 드라이버, 설정 파일 호출 위치와 원본 비정상 종료 주소를 추가했다.

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
