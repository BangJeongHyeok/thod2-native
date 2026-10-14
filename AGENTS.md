# thod2-native — 프로젝트 안내 (에이전트용)

PC판 **The House of the Dead 2** (SEGA, 2001)의 **오픈 소스 재구현(소스 포트)** 프로젝트다.
OpenRCT2, RVGL처럼 원본 게임 데이터만 있으면 현대 PC에서 잘 돌아가고, 다른 개발자가 모드와 기능을 쉽게 추가할 수 있는 구조를 목표로 한다.

- 최우선: 현대 Windows에서 잘 동작하게 만들기
- 그다음: 원본 실행 파일 위에서 함수를 하나씩 재구현하는 **하이브리드 방식**으로 확장성 높은 구조 만들기
- 고해상도, 와이드스크린, 고프레임은 이 과정의 결과로 얻는다.

목표와 단계별 계획은 [docs/ROADMAP.md](docs/ROADMAP.md)를 본다.

이 문서는 작업을 시작하는 에이전트가 프로젝트를 빠르게 파악하기 위한 것이다.
여기 적힌 사실은 **직접 확인한 것**과 **추정**을 구분해서 적는다. 새로 확인한 사실은 이 문서나 `docs/` 아래에 반영한다.

---

## 1. 작업 공간 구조

```
E:\Repos\hod2-native\
├─ thod2-native\    ← 이 저장소 (git). 모든 작업 결과물은 여기에 둔다.
├─ test-game\       ← 실행·패치 테스트용 사본 (git 아님, 자유롭게 수정 가능)
├─ external\        ← 사용자 확인 후 받은 외부 도구 (git 아님. 현재 dgVoodoo2 v2.87.5)
├─ original-game\   ← 압축을 푼 게임 원본 (git 아님, 수정 금지)
│  ├─ THOTD2\       ← 게임 본체 (Hod2.exe, 리소스)
│  ├─ DxWnd\        ← 재배포자가 동봉한 DxWnd v2.04.65
│  └─ 00_Setup.bat  ← 재배포자가 만든 실행 도우미 메뉴
└─ packed-game\     ← 인터넷에서 받은 분할 압축 원본 (하우스 오브 데드2.zip + .z01~.z21)
```

- `thod2-native`만 git 저장소다. 상위 폴더 `hod2-native`는 저장소가 아니다.
- **테스트 사본 현재 상태 (2026-10-14):** 원본과 같은 파일 + dgVoodoo2(`DDraw.dll`, `D3DImm.dll`, `dgVoodoo.conf`, 권장 설정 C)가 설치되어 있다. 원본 그대로 시험하려면 `python tools/compat/dgvoodoo-setup.py uninstall <게임 폴더>`로 지운다. 상태를 바꾸면 이 줄을 고친다.
- 게임 파일 경로는 저장소 안에 하드코딩하지 말고 설정/환경 변수로 받는다 (기본값: `..\original-game\THOTD2`).

## 2. 작업 규칙

1. **`original-game`과 `packed-game`은 읽기 전용으로 취급한다.** 실행·패치 테스트는 테스트 사본 `E:\Repos\hod2-native\test-game\THOTD2\`에서 한다. 실행 조사용 스크립트는 `tools/compat/`에 있다.
2. **게임 원본 파일(exe, 리소스, 압축본)은 절대 커밋하지 않는다.** 저장소에는 소스 코드, 분석 문서, 스크립트, 패치 데이터만 둔다.
3. **모드 DLL은 반드시 32비트(x86 / Win32)로 빌드한다.** 게임이 32비트 프로세스라 64비트 DLL은 로드되지 않는다.
4. 주소는 **VA(가상 주소) 기준 16진수**로 적는다. 예: `0x4AC7D5`. 이미지 베이스가 고정이라 VA가 실행할 때마다 같다.
5. 리버싱으로 알아낸 함수·전역 변수·구조체는 `re/` 아래 문서에 기록한다. 이름, 주소, 근거(문자열 참조, 호출 관계 등), 확인 상태를 함께 적는다.
6. 외부 도구나 라이브러리(dgVoodoo2, DDrawCompat, MinHook 등)를 받기 전에 사용자에게 파일명과 출처를 알리고 확인을 받는다.
7. 문서, PR 제목·본문, **커밋 메시지, 머지 커밋 메시지는 한국어**로 쓴다. 고유명사(도구·라이브러리 이름, 파일명, 함수명, API 이름 등)는 영어 그대로 써도 되지만, **문장 구성은 한국어**로 한다. 코드, 주석, 식별자, 로그 메시지는 영어로 쓴다 ([docs/CONVENTIONS.md](docs/CONVENTIONS.md) "언어").
   - 좋은 예: `dgVoodoo2 설치 스크립트 추가`, `Thod2Core_Initialize에서 해시 계산 실패 처리`
   - 나쁜 예: `Add dgVoodoo2 setup script`
8. **모든 작업은 해당 도메인의 작업 문서에 기록한다.** 아래 "작업 문서" 규칙을 따른다.
9. 코드를 작성하기 전에 [docs/CONVENTIONS.md](docs/CONVENTIONS.md)(코딩 규칙)를 읽는다. 빌드·테스트·배포 명령은 [docs/work/build.md](docs/work/build.md)의 "빌드 방법"에 있다.
10. 작업은 브랜치에서 하고 PR로 `main`에 머지한다. PR은 GitHub CLI(`gh`)로 만든다. 아래 "브랜치와 PR" 규칙을 따른다.

### 브랜치와 PR

- **브랜치 하나 = 목적 하나.** 목적이 다른 변경은 브랜치를 따로 만들고, 비슷한 목적의 변경끼리 묶어서 머지한다.
  - 예: "저장소 규칙·문서 정비"와 "x86 빌드 체계 추가"는 목적이 다르므로 PR을 나눈다.
  - 작업 중에 다른 목적의 변경이 필요해지면 새 브랜치로 분리한다.
- **PR 제목은 PR의 목적을 한국어 명사구로 짧게 쓴다.** 내용 요약이나 "제목: 세부 사항" 형식은 쓰지 않는다.
  - 좋은 예: `프로젝트 기반 구성`, `원본 게임 호환성 조사`, `로더 방식 결정`
  - 나쁜 예: `Build skeleton: CMake x86 presets, core DLL, tests, deploy script`
- **PR 본문은 아래 형식으로 쓴다.** (사용자가 만족한 형식. PR #2 참고)

  ```markdown
  ## 요약
  <이 PR이 무엇을 왜 하는지 1~2문장. 관련 ROADMAP 단계>

  ## 추가한 것   (또는 "변경한 것", "주요 발견")
  - **<항목>:** <설명>

  ## 확인
  - <빌드·테스트·실행으로 확인한 사실, 수치>

  ## 아직 아닌 것   (또는 "남은 항목")
  - <이 PR에 포함되지 않은 것, 다음 단계>
  ```

- **커밋 메시지는 한국어로 쓴다.** 첫 줄은 무엇을 했는지 짧게, 필요하면 빈 줄 뒤에 이유와 세부 사항을 쓴다.
- **머지 커밋 메시지는 `<PR 제목> (#<번호>)`** 형식으로 쓴다. 예: `gh pr merge 3 --merge --subject "로더 방식 결정 (#3)"`
- **PR 제목은 머지하기 전에** 규칙에 맞는지 확인한다. 머지 커밋에 제목이 남기 때문이다.
- 2026-10-14 이전 커밋(PR #1, #2)은 영어로 남아 있다. 기록을 다시 쓰지 않기로 했다.

### 작업 문서

시스템 도메인(빌드, 로더, 렌더러, 입력, 자산 등)마다 `docs/work/<도메인>.md` 문서를 하나씩 둔다.
목록은 [docs/work/README.md](docs/work/README.md), 양식은 [docs/work/_TEMPLATE.md](docs/work/_TEMPLATE.md)에 있다.

- **작업을 시작하기 전에** 해당 도메인 문서를 읽고, 주요 목표, 진행 중인 중목표, 미해결 질문, 최근 작업 기록을 확인한다.
- **주요 목표**는 문서를 만들 때 맨 위에 적는다. 방향이 바뀔 때만 고치고, 고칠 때는 작업 기록에 이유를 남긴다.
- **중목표와 소목표**는 작업하면서 추가한다. 중목표는 `G1`, `G2` …, 소목표는 `G1-1`, `G1-2` … 로 번호를 붙인다. 소목표는 체크박스로 진행 상황을 표시한다.
- **작업 기록**은 최신 기록이 위로 오게 쓴다. 한 일, 결과(확인한 사실, 수치, 실패한 시도 포함), 다음 할 일을 적고, 관련 목표 번호를 붙인다.
- 선택의 이유를 남겨야 하는 결정은 **결정 사항** 표에, 아직 모르는 것은 **미해결 질문**에 적는다. 답을 찾으면 질문을 지우고 결과를 작업 기록에 남긴다.
- 여러 도메인에 걸친 작업은 주 도메인 문서에 기록하고, 다른 도메인 문서에는 짧게 링크만 남긴다.
- 기존 도메인에 맞지 않는 새 작업이 생기면 `_TEMPLATE.md`를 복사해서 새 도메인 문서를 만들고 `docs/work/README.md` 표에 추가한다.
- 문서 상단의 **상태**와 **마지막 갱신** 날짜를 바꾸면 `docs/work/README.md` 표도 같이 고친다.
- 확인된 사실 중 프로젝트 전체에 중요한 것(예: 실행 파일 구조)은 이 AGENTS.md에도 반영한다.

## 3. 게임 실행 파일 분석 결과 (확인됨)

### Hod2.exe (`original-game\THOTD2\Hod2.exe`, 1,695,744 bytes)

| 항목 | 값 |
|---|---|
| 형식 | PE32, Intel i386, GUI, Subsystem 4.00 |
| 링크 타임스탬프 | 2001-03-22 10:52:43 UTC |
| 파일 수정일 | 2010-07-04 (링크 날짜와 다름) |
| SHA-256 | `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69` |
| **수정 여부** | **CD 확인을 지운 수정본으로 판단.** CD 확인 함수 `0x4A5C50`을 호출하는 곳이 없고, 초기화 코드 `0x4A5899`에 5바이트 `NOP`이 있다 ([reverse-engineering.md](docs/work/reverse-engineering.md)) |
| 컴파일러 | MSVC (Rich 헤더, "Microsoft Visual C" 문자열. VC6로 추정) |
| 이미지 베이스 | `0x400000` 고정 |
| 재배치 정보 | **없음** → 항상 같은 주소에 로드됨. 고정 주소 패치와 후킹에 유리 |
| 엔트리 포인트 | RVA `0xAC7D5` (VA `0x4AC7D5`) |
| 패킹 | **안 되어 있음** (표준 섹션, `.text` 엔트로피 6.61) |

섹션:

| 섹션 | VA | VirtualSize | RawSize |
|---|---|---|---|
| `.text` | `0x401000` | `0xC1940` | `0xC2000` |
| `.rdata` | `0x4C3000` | `0xB0DDA` | `0xB1000` |
| `.data` | `0x574000` | `0x456280` | `0x28000` (대부분 BSS) |
| `.rsrc` | `0x9CB000` | `0x12F8` | `0x2000` |

임포트 DLL:

| DLL | 주요 함수 | 의미 |
|---|---|---|
| `DDRAW.dll` | `DirectDrawCreateEx`, `DirectDrawEnumerateExA` | DirectX 7 (DirectDraw7 + Direct3D7 IM) |
| `DINPUT.dll` | `DirectInputCreateEx` | DirectInput 7 |
| `DSOUND.dll` | 서수 #1 (`DirectSoundCreate`) | DirectSound |
| `ole32.dll` | `CoInitialize`, `CoCreateInstance` 등 | COM |
| `IMM32.dll` | `ImmSetOpenStatus` 등 | IME 끄기 |
| `KERNEL32`, `USER32`, `GDI32` | 일반 Win32 | — |

- **D3DX(DX7용)가 정적으로 링크되어 있다.** `CD3duContext::UpdateFrame`, `CD3duDeviceNode::RefreshCaps` 같은 오류 문자열이 있다. 화면 전환(Flip/Blt) 경로를 찾을 때 실마리가 된다.
- DLL 프록시 후보: `ddraw.dll`, `dinput.dll`, `dsound.dll` 모두 시스템 DLL을 이름으로 임포트한다. 게임 폴더에 같은 이름의 DLL을 두면 먼저 로드된다.

### 게임이 읽는 설정 파일

| 파일 | 섹션 / 키 | 비고 |
|---|---|---|
| `Hod2.ini` (게임 폴더에 있음) | `[Controller]` `PLAYER1_INDEX`, `PLAYER1_DEVID`, `PLAYER2_INDEX`, `PLAYER2_DEVID` | `Config.exe`가 편집. 키 의미는 미검증 |
| | `[Flush Setting]` `FLUSH_FRAME`, `FLUSH_POWER`, `SCREEN_LIGHT`, `G_ENABLE` | 사격 시 화면 번쩍임, 밝기 (`Config.exe` UI 기준) |
| `hglCore.ini` (현재 없음, **작업 폴더**에서 읽음) | `[HGLCORE]` `AllDevicePass`, `UseZBuffer`, `UseHEL`, `CDCheckPass` (값이 `t`/`T`로 시작하면 켜짐), `SmallTexure` (숫자 1/2) | **코드 분석과 실행으로 확인.** `DispFps`, `FullScreen`, `DispVersion`도 읽지만 쓰이지 않는다 (창 모드·FPS 표시 불가). 비트 표는 [reverse-engineering.md](docs/work/reverse-engineering.md) |
| `Toddebug.ini` (현재 없음) | `TOD`, `LIFE`, `RELOAD` 근처 문자열 | 디버그용으로 추정. 미검증 |

그 밖의 문자열:
- CD 확인 메시지: `"Please insert the 'The House of The Dead 2' CD."`
- 세이브 식별자: `"HOD2 SAVE FILE"`

### Config.exe

PE32 i386, 135,168 bytes, 링크 2001-03-21, SHA-256 `BFFA11376A21311994A4F3BC0E012F313753F72D8DF7D0B063DCDE409A543F07`. 패킹 없음. MFC 앱(`Hod2_Tool`)이다.
**컨트롤러 전용 설정 도구**다: Player1/Player2 컨트롤러 선택, "MC PC Light Gun" 속성, 화면 밝기, 번쩍임 시간·색상을 `Hod2.ini`에 저장한다. **해상도 설정은 없다.**
인터넷 다운로드 표시(Zone.Identifier)가 남아 있어서 실행하면 Windows 보안 경고가 뜬다. 사용자 확인 없이 실행하지 않는다.

### 원본 실행 동작 (2026-10-14 확인)

- 시작할 때마다 Direct3D 장치 확인창("Attention", 예/아니요)이 뜬다.
- 주 모니터를 **640×480 32bpp 60Hz 전용 전체 화면**으로 바꾼다. 보조 모니터 위치가 이동한다.
- 실행 중 CPU 코어 1개를 100% 쓴다.
- 상세 결과는 [docs/work/compat-survey.md](docs/work/compat-survey.md)에 있다.

## 4. 게임 리소스 구조 (`original-game\THOTD2\`)

| 폴더 | 파일 수 | 크기 | 형식 | 추정 내용 |
|---|---|---|---|---|
| `pol\` | 656 | 81MB | `.bin` | 폴리곤/모델 |
| `tex\` | 450 | 66MB | `.bin` | 텍스처 (예: `b6boss1z.bin`, `bg_adv00.bin`, `scr_blood_*.bin`) |
| `mot\` | 50 | 4.8MB | `.bin` | 모션/애니메이션 |
| `cam\` | 23 | 784KB | `.bin` | 카메라 경로 |
| `coli\` | 9 | 436KB | `.bin` | 충돌 판정 |
| `evt\` | 11 | 309KB | `.bin` | 이벤트/스크립트 |
| `sound\` | 802 | 368MB | `.wav` | `bgm\`, `SE\`, `voice\` |

`.bin` 포맷은 모두 **미분석** 상태다. 실행 파일 안에 `DXT1`~`DXT5` 문자열이 있어서 텍스처 일부가 DXT 압축일 가능성이 있다.

`tex\scr_blood_green.bin`은 `00_Setup.bat`이 피 색상(초록/빨강)을 바꿀 때 덮어쓰는 파일이다.

## 5. 동봉된 실행 환경

- **출처:** 인터넷 블로그(tistory)에 올라온 "무설치" 재배포본의 분할 압축. `original-game` 파일 2162개 중 1201개에 인터넷 다운로드 표시(Zone.Identifier, ZoneId=3)가 남아 있다 (`Config.exe`, `dxwnd.exe`, `cam\*.bin` 등). 표시가 있는 실행 파일은 실행할 때 Windows 보안 경고가 뜨므로 사용자 확인 없이 실행하지 않는다. `Hod2.exe`에는 표시가 없다.

- **DxWnd v2.04.65** (`original-game\DxWnd\`): DirectX 게임을 후킹해서 창 모드 등으로 띄우는 도구. `dxwnd.ini`에 프로필 2개(창 화면, 전체 화면, 둘 다 640×480, `maxddinterface=7`)가 들어 있다. `proxy\` 폴더에 `ddraw.dll`, `d3d8.dll`, `d3d9.dll`, `dinput.dll`, `wing32.dll` 프록시가 있지만 게임 폴더에 복사되어 있지는 않다.
- **`00_Setup.bat` 메뉴**
  - 1번: `HKCU\...\AppCompatFlags\Layers`에 `Hod2.exe = ~ WINXPSP3` 등록
  - 2번: 1번 등록 삭제
  - 3번: DxWnd 실행
  - 4·5번: 피 색상 교체

### 이 PC에서 관찰한 상태 (2026-10-13 기준)

- `Hod2.exe`를 DxWnd 없이 직접 실행한 기록이 있다 (PCA 로그).
- 레지스트리에 `Hod2.exe = DWM8And16BitMitigation` 호환성 플래그가 붙어 있다 (Windows가 자동으로 붙인 것으로 보임). 이 플래그는 **압축을 처음 풀었던 경로(`...\unpack\THOTD2\Hod2.exe`) 기준**이라 지금 경로에는 적용되지 않을 수 있다.
- `~ WINXPSP3` 플래그는 등록되어 있지 않다.

## 6. 기술 방향 요약

자세한 내용은 [docs/ROADMAP.md](docs/ROADMAP.md)에 있다.

- **진행 방식 (OpenRCT2 방식):** 런처 또는 프록시 DLL로 원본 `Hod2.exe`에 우리 DLL을 주입한다. MinHook으로 원본 함수를 가로채서 우리가 재구현한 함수로 하나씩 교체한다. 어느 시점에서든 게임은 처음부터 끝까지 플레이 가능해야 한다.
- **교체 순서:** 플랫폼 계층(창, 타이밍, 입력, 오디오, 파일) → 렌더러 → 자산 로더 → 게임 로직. 최종 목표는 원본 코드 없이 우리 실행 파일 + 원본 데이터만으로 독립 실행하는 것이다.
- **렌더링:** DX7 API가 아니라 게임의 그리기 함수 수준에서 가로채서 자체 렌더 인터페이스(1차 백엔드 D3D11)로 넘긴다. 초기 호환성 확보용으로 기존 래퍼(dgVoodoo2, DDrawCompat)를 임시로 쓸 수 있다.
- **확장성:** 플러그인 DLL, 이벤트 훅, 데이터 덮어쓰기 폴더(RVGL 방식)를 모드 API로 제공한다.
- **리버싱:** Ghidra(정적 분석)와 x32dbg(동적 분석). 분석 결과는 텍스트(함수 목록, 구조체, 포맷 문서)로 `re/`와 `docs/formats/`에 남긴다. 원본 코드를 포함하는 Ghidra 프로젝트 파일이나 디컴파일한 코드를 그대로 커밋하지 않는다.
- **고프레임:** 게임 로직은 60Hz 고정 스텝으로 추정된다 (아케이드 → 드림캐스트 → PC 이식작). 프레임 제한만 풀면 게임 속도가 빨라질 가능성이 높다. 그래서 메인 루프를 재구현해서 로직은 60Hz로 두고 렌더링만 보간한다.

## 7. 저장소 구조

`(있음)` 표시가 있는 것은 이미 만들어졌다. 나머지는 계획이다. 새로 만들면 이 절을 갱신한다.

```
thod2-native/
├─ AGENTS.md          ← 이 문서 (있음)
├─ CMakeLists.txt     ← 최상위 빌드 (있음). CMakePresets.json의 x86 프리셋 사용
├─ config/            ← 배포용 예제 설정 thod2-native.ini (있음)
├─ docs/              ← 로드맵, 코딩 규칙(CONVENTIONS.md), 설계 문서 (있음)
│  ├─ work/           ← 도메인별 작업 문서 (있음)
│  └─ formats/        ← 게임 데이터 파일 포맷 문서
├─ re/                ← 리버싱 기록 (functions.csv, 구조체, 주소 맵, Ghidra 적용 스크립트)
├─ src/
│  ├─ core/           ← thod2_core.dll: 로그, 설정, 실행 파일 식별, 공개 C API (있음)
│  ├─ launcher/       ← thod2.exe: 데이터 경로 탐색, 원본 실행 + DLL 주입
│  ├─ interop/        ← 원본 함수/전역 변수 주소 정의, 후킹·패치 도우미
│  ├─ platform/       ← 창, 타이밍·메인 루프, 입력, 오디오, 파일 입출력
│  ├─ renderer/       ← 렌더 인터페이스와 백엔드 (D3D11 등)
│  ├─ assets/         ← pol/tex/mot/cam/coli/evt 로더, 데이터 덮어쓰기
│  ├─ game/           ← 재구현한 게임 로직
│  └─ modapi/         ← 플러그인 로딩, 이벤트, 공개 헤더
├─ tests/             ← 스모크 테스트 core_tests (있음)
├─ experiments/       ← 결정을 위한 측정용 코드 (있음: dinput_probe). 제품 코드 아님
├─ mods/              ← 예제 모드
├─ tools/             ← deploy.ps1, compat/ 조사 스크립트 (있음). 이후 리소스 추출/변환
└─ third_party/       ← MinHook 등 (라이선스 확인 후)
```

하이브리드 단계에서는 원본과 같은 프로세스에서 돌아가므로 모든 코드를 x86으로 빌드한다. 독립 실행 단계(ROADMAP 2단계 M4)부터 x64를 고려한다.

## 8. 아직 모르는 것 (우선 확인 대상)

- 메인 루프, 프레임 타이머 위치. 게임 로직이 프레임 단위인지 시간 단위인지
- 640×480 디스플레이 모드를 설정하는 코드 위치 (설정으로 바꿀 방법은 없음이 확인됨)
- 투영 행렬을 설정하는 곳 (와이드스크린용)
- HUD/2D 스프라이트 그리기 경로
- `Hod2.exe`에서 CD 확인 말고 수정된 곳이 더 있는지 (수정 안 된 원본과 비교 필요)
- Alt+Enter 창/전체 화면 전환을 어떻게 제공할지 (원본에는 창 모드가 없어서 Alt+Enter가 아무 동작도 하지 않음. Alt+Tab은 정상)
- 고주사율 모니터에서 게임 속도
- 마우스/라이트건 입력 처리 방식
