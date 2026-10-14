# 호환성 조사

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 1-1
- **관련 코드:** 없음 (조사 문서)
- **마지막 갱신:** 2026-10-14

## 주요 목표

현대 Windows 10/11에서 원본 게임을 그대로 실행했을 때 무엇이 깨지는지 빠짐없이 찾고 기록한다. 찾은 문제는 담당 도메인 문서(platform-*, renderer 등)로 넘긴다. 기존 래퍼(dgVoodoo2, DDrawCompat)와 DxWnd를 적용한 결과도 비교 기준으로 남긴다.

**완료 기준:** 확인 항목 전체에 대해 증상, 재현 방법, 담당 도메인이 표로 정리되어 있다.

## 중목표 / 소목표

### G1. 아무 도구 없이 원본 실행 — 진행 중

- [x] G1-1 테스트용 게임 사본 만들기 (`E:\Repos\hod2-native\test-game\THOTD2`)
- [x] G1-2 실행 흐름, 화면 모드, 창 상태 기록
- [x] G1-3 Alt+Tab 나가기/돌아오기 시험
- [x] G1-4 Alt+Tab 문제 재현 → 사용자가 Alt+Enter와 혼동한 것으로 확인. Alt+Tab은 정상
- [x] G1-5 고주사율(120Hz 이상) 환경에서 게임 속도 확인 → **실제 240Hz 모니터에서 변화 없음** (60fps, 게임 속도 그대로, [platform-timing.md](platform-timing.md) G3)
- [ ] G1-6 DPI 배율 100%가 아닌 환경에서 확인
- [x] G1-7 세이브 파일 생성 위치 확인 → 게임 폴더의 데이터 파일 4개 ([platform-filesystem.md](platform-filesystem.md))
- [ ] G1-8 사운드, 마우스 조준 정확도 상세 확인 (사용자 플레이: "정상")

### G2. 숨은 설정과 부속 프로그램 확인 — 완료

- [x] G2-1 `hglCore.ini` 읽는 위치와 키별 효과 확인
- [x] G2-2 `Config.exe` 기능 확인 (정적 분석)

### G3. 기존 도구와 비교 — 대기

- [ ] G3-1 `00_Setup.bat`의 `~ WINXPSP3` 호환 모드 적용 결과 (환경 변수 방식은 시험함. 레지스트리 방식은 사용자 확인 대기)
- [ ] G3-2 DxWnd(동봉 v2.04.65) 창 모드 프로필 결과
- [x] G3-3 dgVoodoo2 적용 결과 (v2.87.5, 아래 "dgVoodoo2 시험 결과")
- [x] G3-5 dgVoodoo2 테두리 없는 창 모드에서 실제 키보드 Alt+Tab 확인 (사용자: 정상, 화면 정상, 게임 속도 원본과 같음)
- [ ] G3-4 DDrawCompat 적용 결과 (사용자 확인 후 다운로드)

## 확인 결과 요약

| # | 항목 | 증상 / 결과 | 재현 방법 | 담당 도메인 |
|---|---|---|---|---|
| 1 | 실행 | 정상 실행. 시작할 때마다 "Attention" 확인창이 뜬다 ("NVIDIA GeForce RTX 3080 \ Microsoft Direct3D Hardware Transform and Lighting acceleration capable device will be used." 예/아니요) | 그냥 실행 | renderer (장치 선택), platform-display. **해결:** thod2-native `skip_device_dialog`로 자동 처리 (platform-display G1) |
| 2 | 화면 모드 | **전용 전체 화면, 640×480 32bpp 60Hz 고정.** 주 모니터의 해상도를 바꾼다. 창은 `WS_POPUP`, `WS_EX_TOPMOST`, 크기 640×480 | 실행 후 `EnumDisplaySettings` | platform-display, renderer |
| 3 | 다중 모니터 | 게임이 해상도를 바꾸는 동안 **보조 모니터의 위치가 이동한다** (`(1920,511)` → `(640,480)`). 종료하거나 Alt+Tab으로 나가면 원래대로 돌아온다 | 모니터 2개 환경에서 실행 | platform-display |
| 4 | Alt+Tab | 나가기: 창 최소화, 해상도 복구, CPU 사용 1%로 정지. 돌아오기(Alt+Tab, 작업 표시줄 복원 둘 다): 640×480으로 다시 바뀌고 화면 갱신 계속됨 → **정상**. 사용자 실제 키보드로도 정상 (dgVoodoo2 시험 C) | `keybd_event`로 Alt+Tab 전송, 사용자 확인 | platform-display |
| 5 | **Alt+Enter** | **아무 반응이 없다** (깜빡임, 해상도 변경 없음). 원본에는 창 모드가 없다 (`hglCore.ini`의 `FullScreen` 키도 쓰이지 않음). 처음에 "Alt+Tab이 안 된다"로 보고됐으나 사용자가 Alt+Enter와 혼동한 것으로 확인 | 실제 키보드로 Alt+Enter | platform-display |
| 6 | CPU 사용 | 실행 중 **CPU 코어 1개를 100% 사용** (대기 없이 도는 루프로 추정). 최소화하면 1% | 5초간 프로세스 CPU 시간 측정 | platform-timing. **해결:** thod2-native `frame_wait_sleep`으로 약 25% (platform-timing G2) |
| 7 | 고주사율 모니터 속도 | 2026-10-14에는 미확인 (당시 모니터 60Hz). **2026-10-15 실제 240Hz 모니터에서 60fps, 게임 속도 그대로 확인** | 주 모니터 240Hz에서 실행, `refresh-test.ps1` | platform-timing |
| 8 | CD 확인 | CD 없이 실행됨. 실행 파일의 CD 확인 호출이 NOP으로 지워진 것으로 보임 (reverse-engineering 참고) | — | platform-filesystem, loader |
| 9 | 숨은 설정 | `hglCore.ini`의 `DispFps`, `FullScreen`, `DispVersion`은 읽기만 하고 쓰이지 않음. 창 모드나 FPS 표시를 설정으로 켤 수 없음 | `hglCore.ini` 작성 후 실행 | reverse-engineering |
| 10 | 해상도 설정 | 해상도를 바꾸는 설정이 없음. `Config.exe`는 컨트롤러·밝기·번쩍임 설정 전용 | 정적 분석 | renderer |
| 11 | 보안 경고 | `Config.exe`에 인터넷 다운로드 표시(Zone.Identifier)가 남아 있어서 실행할 때 "파일 열기 - 보안 경고"가 뜬다. `Hod2.exe`에는 표시가 없다 | 탐색기나 `Start-Process`로 실행 | (배포 안내) |
| 12 | 사운드, 조준, 진행 | 사용자 플레이 결과 "정상" | 사용자 플레이 | — |
| 13 | XP SP3 호환 모드 (환경 변수) | `__COMPAT_LAYER=WinXPSP3`로 실행해도 결과 동일 (640×480 전체 화면, CPU 93%). 단, 어떤 레이어를 지정해도 호환성 DLL(`AcLayers` 등)이 로드되지 않아 **실제 적용 여부가 불확실** | 32비트 PowerShell에서 환경 변수 설정 후 실행, 모듈 목록 확인 | — |
| 14 | DX7 실행 경로 | Windows 11이 DX7을 `DDRAW.dll` → `D3DIM700.DLL` → `dxgi.dll` + `nvd3dum.dll`(NVIDIA) 순서로 처리한다. 즉 DXGI 위에서 DX7을 흉내 낸다 | 32비트 PowerShell로 모듈 목록 확인 (64비트에서는 32비트 모듈이 안 보임) | renderer |

## dgVoodoo2 시험 결과

- 버전: v2.87.5 `dgVoodoo2_87_5.zip` (9,268,078 bytes, SHA-256 `5ffde692…1040850`, GitHub 공개 digest와 일치)
- 받은 위치: `E:\Repos\hod2-native\external\dgVoodoo2_87_5\` (저장소 밖)
- 설치: `MS\x86\DDraw.dll`, `MS\x86\D3DImm.dll`, `dgVoodoo.conf`를 게임 폴더에 복사 (`tools/dgvoodoo-setup.py install`, 2026-10-14에 `tools/compat/`에서 옮김)
- 적용 확인: 시작 확인창의 장치 이름이 "dgVoodoo Hardware Accelerated Transform & Light Device"로 바뀐다.
- 모든 시험에서 `DirectX.dgVoodooWatermark=false`

| 시험 | 주요 설정 | 결과 |
|---|---|---|
| A | 기본값 (전체 화면, 해상도 그대로) | 정상 동작. 그러나 **주 모니터가 아니라 보조 모니터(DISPLAY1)를 640×480으로 바꿔서** 띄웠다 (`FullScreenOutput=default`) |
| B | `FullScreenMode=false`, `Resolution=2x` | 게임이 전체 화면을 요청하면 그대로 따라서(`AppControlledScreenMode=true`) 보조 모니터가 1280×960이 됐다. **2배 렌더링은 정상** — 3D와 HUD가 선명해짐 |
| B2 | B + `AppControlledScreenMode=false` | 창 모드 성공. 모니터 해상도 변화 없음. 창 크기는 640×480 그대로 |
| **C (권장)** | B2 + `WindowedAttributes=borderless,fullscreensize`, `Resolution=max`, `ScalingMode=stretched_ar` | **테두리 없는 1920×1080 창, 4:3 영역(1440×1080)에 렌더링하고 좌우 여백.** 모니터 해상도·배치 변화 없음. Alt+Tab(주입) 나가기/돌아오기 정상, 나가 있는 동안 CPU 2% |

시험 C의 `dgVoodoo.conf` 변경 값 (기본값 대비):

```
[General]     FullScreenMode = false
              ScalingMode = stretched_ar
[GeneralExt]  WindowedAttributes = borderless,fullscreensize
[DirectX]     Resolution = max
              AppControlledScreenMode = false
              dgVoodooWatermark = false
```

남은 문제: CPU 코어 1개 100% 사용은 그대로다 (렌더러 문제가 아니라 메인 루프 문제로 보임 → platform-timing).

## 테스트 환경

| 항목 | 값 |
|---|---|
| OS | Windows 11 Education 10.0.26200 |
| GPU | NVIDIA GeForce RTX 3080 (드라이버 32.0.16.1714) |
| 기타 디스플레이 어댑터 | Parsec Virtual Display Adapter |
| 모니터 | 2대. DISPLAY2(주) 1920×1080 60Hz `(0,0)`, DISPLAY1 1920×1080 60Hz `(1920,511)` (2026-10-14 기준). **2026-10-15 0시 무렵 사용자가 DISPLAY2를 239.76Hz로 변경** |
| 호환성 플래그 | 첫 실행 때는 없었음. **이후 Windows PCA가 `DWM8And16BitMitigation`을 자동 등록** (HKCU, HKLM) |
| 실행 파일 | `Hod2.exe` SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69` |

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-14 | 테스트는 `test-game\THOTD2` 사본에서만 한다 | 원본 보존. 사본은 `robocopy`로 만들고 파일 수·크기 일치 확인 |
| 2026-10-14 | 인터넷 다운로드 표시가 있는 실행 파일(`Config.exe`)은 사용자 확인 없이 실행하지 않는다 | 보안 경고는 사용자가 판단할 부분. 정적 분석으로 대체 |

## 미해결 질문

- Alt+Enter로 창 모드 ↔ 전체 화면을 전환하는 기능을 어디서 제공할지 (dgVoodoo2는 `DisableAltEnterToToggleScreenMode` 옵션이 있음, 최종적으로는 platform-display에서 직접 구현)
- 고주사율 테스트 방법 (Parsec 가상 디스플레이로 120/144Hz를 만들 수 있는지)
- 외부 래퍼를 받을지 (사용자 확인 필요). 2026-10-14 기준 최신 배포:
  - dgVoodoo2 v2.87.5 — `dgVoodoo2_87_5.zip` (9,268,078 bytes), github.com/dege-diosg/dgVoodoo2 releases
  - DDrawCompat v0.7.1 (프리릴리스) — `DDrawCompat-v0.7.1.zip` (1,067,481 bytes), github.com/narzoul/DDrawCompat releases
- 동봉 `dxwnd.exe`에 인터넷 다운로드 표시가 있어서 실행 전 사용자 확인 필요
- 게임이 실제로 몇 fps로 그리는지, vsync를 기다리는지

## 작업 기록

### 2026-10-15 — 세이브 위치와 원본 비정상 종료 기록 (파일 접근 분석에서 넘어온 내용)

- **결과:**
  - 세이브는 게임 폴더의 데이터 파일 4개(`pol/bg_adv19.bin` 등, 각 987 bytes)에 정상 종료 때 쓴다. 게임 폴더에 쓰기 권한이 없으면 저장되지 않는다. → G1-7 세이브 파일 생성 위치 확인 완료.
  - `C:\WINDOWS\d3dx.dat`도 쓴다. 관리자 권한이 없으면 UAC 가상화로 `VirtualStore`에 써진다.
  - 원본 폴더에 `exception.log`가 있다 (2026-10-14 00:15:40, `0x4A0899` 접근 위반). 원본도 비정상 종료할 수 있다.
- **상세:** [platform-filesystem.md](platform-filesystem.md) "파일 접근 분석 결과"

### 2026-10-14 — Windows 자동 호환 모드 발견 (화면 모드 분석에서 넘어온 내용)

- **결과:**
  - Windows PCA가 테스트 사본 `Hod2.exe`에 `DWM8And16BitMitigation` 호환 모드를 자동으로 등록했다 (HKCU, HKLM). 첫 조사 때(2026-10-14 00시)는 없었다. **그 이후의 측정(dgVoodoo2 시험 포함)은 이 호환 모드가 켜진 상태였다.**
  - 이 호환 모드는 `apphelp.dll`이 DirectDraw 함수 표를 덮어쓰는 방식으로 동작한다.
  - Windows 기본 호환성 데이터베이스(`sysmain.sdb`)에는 HOTD 1편(`THOTD.exe`)만 있고 2편은 없다.
  - Alt+Enter: dgVoodoo2 시험 C에 `DisableAltEnterToToggleScreenMode = false`를 더하면 테두리 없는 창 ↔ 전체 화면 전환이 된다.
- **상세:** [platform-display.md](platform-display.md) "화면 모드 분석 결과"

### 2026-10-14 — Alt+Tab 보고 정정, dgVoodoo2 사용자 확인 (관련: G1-4, G3-5)

- **한 일:** 사용자가 dgVoodoo2 시험 C 설정으로 직접 플레이했다.
- **결과:**
  - 앞서 "Alt+Tab이 안 된다"는 보고는 **Alt+Enter와 혼동한 것**이었다. Alt+Tab은 정상이고, 문제는 Alt+Enter(창/전체 화면 전환)가 원본에서 아무 동작도 하지 않는 것이다.
  - dgVoodoo2 시험 C: 실제 키보드 Alt+Tab 정상, 화면 정상, **게임 속도가 원본과 같다**.
- **다음:** Alt+Enter 전환 기능은 platform-display의 요구 사항으로 넘긴다. 호환성 조사의 남은 항목(G1-5 고주사율, G1-6 DPI, G1-7 세이브 위치)은 환경이 준비되면 이어서 한다.

### 2026-10-14 — dgVoodoo2 시험 (관련: G3-3)

- **한 일:** 사용자 확인 후 dgVoodoo2 v2.87.5를 공식 GitHub 릴리스에서 받아 해시를 확인했다. 테스트 사본에 설치하고 설정을 바꿔 가며 A, B, B2, C 네 가지를 시험했다. 설치·설정용 스크립트 `tools/compat/dgvoodoo-setup.py`를 만들었다. `runtest.ps1`이 게임 창 위치를 기준으로 캡처하도록 고쳤다.
- **결과:** 위 "dgVoodoo2 시험 결과" 표. 시험 C(테두리 없는 창 + 최대 해상도 + 4:3 유지)로 1단계 완료 기준의 "테두리 없는 창으로 시작"과 고해상도 렌더링을 기존 도구만으로 얻을 수 있음을 확인했다.
- **다음:** 사용자가 시험 C 설정에서 실제 키보드로 Alt+Tab을 확인한다 (테스트 사본에 설치된 상태로 둠). DDrawCompat 비교는 사용자 판단에 따라 보류.

### 2026-10-14 — XP 호환 모드 시험, DirectInput 협력 수준 확인 (관련: G1-4, G3-1)

- **한 일:** `__COMPAT_LAYER` 환경 변수로 XP SP3 호환 모드를 이번 실행에만 적용해서 시험했다. 32비트 PowerShell로 로드된 모듈을 확인했다. 실행 파일에서 DirectInput `SetCooperativeLevel`(vtable `+0x34`) 호출 패턴을 찾았다.
- **결과:** 표 13, 14번과 "미해결 질문"의 Alt+Tab 가설에 정리했다. 호환 모드는 겉으로 달라진 점이 없었고 실제 적용 여부도 확인하지 못했다.
- **다음:** 레지스트리 방식 호환 모드, DxWnd, dgVoodoo2, DDrawCompat 시험은 사용자 확인 후 진행한다.

### 2026-10-14 — Alt+Tab 사용자 보고 (관련: G1-4) — *이후 Alt+Enter 혼동으로 정정됨*

- **한 일:** 사용자에게 Alt+Tab 증상을 확인했다.
- **결과:** 실제 키보드로 Alt+Tab을 누르면 아무 반응이 없다 (깜빡임, 해상도 변경 없음). 자동 테스트와 다르다. 가설은 "미해결 질문"에 적었다.
- **다음:** DirectInput 키보드 협력 수준(`SetCooperativeLevel` 플래그)을 코드에서 확인한다. 실제 키보드 입력으로 재현할 방법을 찾는다.

### 2026-10-14 — 원본 첫 실행 조사 (관련: G1-1~G1-3, G2-1, G2-2)

- **한 일:**
  - `original-game\THOTD2`를 `test-game\THOTD2`로 복사했다 (2004개 파일, 542,336,749 bytes 일치).
  - 테스트 사본의 `Hod2.exe`를 도구 없이 실행하고, Win32 API(`EnumWindows`, `EnumDisplaySettings`)로 창과 디스플레이 상태를 기록하고 `CopyFromScreen`으로 화면을 캡처했다. GDI 캡처로 게임 화면이 정상적으로 찍힌다.
  - `keybd_event`로 Alt+Tab을 보내서 나가기와 돌아오기를 시험했다.
  - `hglCore.ini`(`DispFps=true`, `DispVersion=true`)를 만들어서 실행했다.
  - `Config.exe`는 실행하려 했으나 보안 경고가 떠서 취소하고 리소스와 문자열을 정적 분석했다.
- **결과:** 위 "확인 결과 요약" 표 1~11번.
  - 컴퓨터 제어 도구(computer-use)는 설치된 앱 목록에 없는 `Hod2.exe`에 접근 권한을 줄 수 없었다. PowerShell 기반 조사 스크립트로 대체했다.
- **다음:** 사용자에게 Alt+Tab 증상을 확인하고, G3(기존 도구 비교)으로 넘어간다.
