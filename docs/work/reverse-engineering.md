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
- [x] G2-2 분석 결과 내보내기 형식 결정, `re/functions.csv` 시작 → `re/` CSV 3종 ([re/README.md](../../re/README.md))
- [x] G2-3 CSV를 Ghidra에 적용하는 스크립트와 Ghidra에서 붙인 이름을 CSV로 내보내는 스크립트 (`tools/re/sync.ps1`)
- [x] G2-4 CSV 형식 검사 (`tools/re/check_re.py`, ctest `re_tables`)
- [ ] G2-5 x32dbg를 게임에 붙여 보기 (동적 분석 확인)

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

**확인한 함수, 전역 변수, 코드 위치는 [`re/`](../../re/README.md)의 CSV에 적는다** (2026-10-15부터, 이 문서에 있던 표를 옮김).

- [`re/functions.csv`](../../re/functions.csv): 함수 (주소, 이름, 진행 상태, 시그니처)
- [`re/globals.csv`](../../re/globals.csv): 전역 변수 (주소, 이름, 타입)
- [`re/locations.csv`](../../re/locations.csv): 호출 위치(복귀 주소), 플래그를 읽는 명령, 패치 흔적

각 행에 근거(`evidence`)와 설명 문서(`doc`)가 있다. 근거를 자세히 설명하는 글은 이 문서의 작업 기록과 각 도메인 문서에 남긴다. 형식과 Ghidra 적용 방법은 [re/README.md](../../re/README.md)를 본다.

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
| 2026-10-15 | **분석 결과의 기준 데이터는 `re/`의 CSV 3종**(`functions.csv`, `globals.csv`, `locations.csv`)이다. Ghidra 프로젝트는 적용 스크립트로 다시 만드는 사본으로 본다. 이 문서의 "확인된 주소" 표는 CSV로 옮겼다 | 텍스트라서 git으로 비교·리뷰·병합할 수 있고, 여러 사람이 같은 이름으로 작업할 수 있다. 대안: Ghidra 프로젝트 공유(바이너리, 원본 코드 포함이라 커밋 불가), Ghidra 서버(설치·운영 부담), JSON(사람이 읽고 고치기 불편) |
| 2026-10-15 | 함수·전역 변수·코드 위치를 파일 세 개로 나눈다. 코드 위치는 복귀 주소(`ret`)와 명령 주소(`insn`)를 구분한다 | 열이 다르다 (함수는 진행 상태·시그니처, 전역 변수는 타입). 기록 기능은 복귀 주소를 남기므로 그대로 적고, 주석은 적용 스크립트가 바로 앞 call에 단다 |
| 2026-10-15 | CSV의 `notes`는 영어로 쓴다 | Ghidra 주석으로 들어가서 코드처럼 외부 기여자가 읽는다 (코드·주석은 영어라는 규칙과 같음). 근거를 설명하는 글은 한국어 작업 문서에 쓴다 |
| 2026-10-15 | 적용 스크립트는 함수 주석(Plate)과 EOL 주석을 `[thod2]`로 시작하는 관리 칸으로 쓰고 매번 덮어쓴다. 내보내기는 이름(과 비어 있을 때 시그니처)만 가져온다 | 적용을 여러 번 해도 결과가 같아야 한다. 주석까지 내보내면 Ghidra와 CSV 중 무엇이 기준인지 흐려진다 |
| 2026-10-15 | Ghidra 스크립트는 Java로 쓴다 | 추가 설치 없이 Ghidra 12.1.4 기본 기능으로 돈다. PyGhidra는 별도 Python 환경 구성이 필요하다 |
| 2026-10-15 | 분석 도구는 `external\`, Ghidra 프로젝트는 `ghidra-projects\`(둘 다 저장소 밖)에 두고, 실행 스크립트만 저장소(`tools/re/`)에 둔다 | 도구 바이너리와 원본 코드가 들어간 프로젝트는 커밋하지 않는다는 원칙. 시스템 JDK 설치나 PATH 변경 대신 프로세스 단위 `JAVA_HOME`으로 다른 환경에 영향을 주지 않는다 |
| 2026-10-14 | 정밀 분석 도구가 갖춰지기 전까지는 Python으로 바이트를 직접 읽어서 분석하고, 결과에 "바이트 확인"이라고 근거를 남긴다 | 디스어셈블러(Ghidra, capstone)가 아직 없음. 외부 도구 설치는 사용자 확인 필요 |

## 미해결 질문

- `Toddebug.ini` 키가 실제로 동작하는지
- `0x4A5899`의 NOP 자리에 원래 무엇이 있었는지 (수정 안 된 원본 `Hod2.exe`가 있으면 비교 가능)
- 원래 CD 확인 실패 시 동작 (메시지 `"Please insert the 'The House of The Dead 2' CD."` 출력 위치)
- 실행 파일에서 수정된 곳이 `0x4A5899` 말고도 있는지
- 플래그를 레지스터로 넘기는 다른 `SetCooperativeLevel` 호출이 있는지 (상수 push 패턴으로는 `0x41EBA3` 한 곳만 발견)

## 작업 기록

### 2026-10-15 — `re/` CSV와 Ghidra 동기화 스크립트 (관련: G2-2, G2-3, G2-4)

- **한 일:** 이 문서의 "확인된 주소" 표를 `re/functions.csv`(10행), `re/globals.csv`(20행), `re/locations.csv`(41행)로 옮겼다. CSV → Ghidra 적용 스크립트(`ApplyThod2Re.java`), Ghidra → CSV 내보내기 스크립트(`ExportThod2Re.java`), 실행 스크립트(`tools/re/sync.ps1`), 형식 검사(`tools/re/check_re.py`, ctest `re_tables`)를 만들었다. 형식과 사용법은 [re/README.md](../../re/README.md).
- **결과:**
  - 실제 프로젝트(`ghidra-projects\hod2`)에 적용: 함수 3개 생성(`0x49DE30`, `0x49E0C0`, `0x4A5C50`. 적용 전 새 프로젝트에서 자동 분석이 놓친 함수를 따로 확인함), 경고 0. 처음에는 `0x5976C4`에 Ghidra가 자동으로 만든 `pointer`가 있어서 `char *`를 넣지 못했다. 그 자리의 데이터 하나만 지우고 바꾸게(`CLEAR_SINGLE_DATA`) 고쳤다.
  - `locations.csv` 41행 모두 검증 통과: `ret` 행은 모두 call 명령 바로 다음이고, `insn` 행은 모두 명령 안이다. `0x41EBA3`은 `CALL dword ptr [ECX + 0x34]` 명령 자체다. 41행은 명령 40개에 붙는다 (`0x49DF0B`와 즉시값 `0x49DF11`이 같은 명령).
  - 같은 CSV를 다시 적용해도 생성 0, 경고 0 (반복 적용 가능). 바로 내보내면 변경 0이고, CSV 세 파일의 SHA-256이 내보내기 전과 같다.
  - 임시 폴더에 새 프로젝트를 만드는 `-Import`: 임포트·분석·적용 78초, 경고 0.
  - 임시 프로젝트에서 이름을 바꾸고 내보내기를 시험했다: 함수 이름 변경 2건, 새 함수 1건, 새 전역 변수 1건이 반영됐다. 코드 안의 라벨은 제외됐다. 새 행은 `notes`가 비어 있어서 `check_re.py`가 오류 2개로 알려 준다. 잘못된 주소 형식, 상태, 종류 값도 오류로 잡는다.
  - **새로 확인:** 메인 창 `CreateWindowExA`(복귀 `0x4A588B`)와 지워진 CD 확인 호출 자리(`0x4A5899`)는 Ghidra 자동 분석 기준으로 함수 `0x4A5790` 안에 있다. WinMain 계열 초기화 함수로 보고 `functions.csv`에 이름 없이 넣었다.
  - Ghidra 헤드리스는 프로젝트 경로에 `..`가 들어가면 `Path element starting with '.' is not permitted`로 실패한다. `sync.ps1`이 절대 경로로 바꾼다. `analyzeHeadless.bat`는 cmd를 거치므로 `-scriptPath`에 `;`로 여러 폴더를 넘길 수 없다.
  - `check_re.py`가 알려 주는 "문서에만 있는 주소" 11개(`0x4604D5`, `0x49DF14`, `0x49E0D6`, `0x4A4B8F`, `0x4A4D63`, `0x4A588D`, `0x4AC7D5`, `0x4C3000`, `0x4C3120`, `0x4C31EC`, `0x4C31FC`)는 구간 끝, IAT 칸, 스택 값, entry라서 표에 넣지 않았다.
- **다음:** x32dbg를 게임에 붙여 본다 (G2-5). 입력 경로(DirectInput 장치 열거, 마우스 → 조준점) 분석을 시작한다 ([platform-input.md](platform-input.md)).

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
