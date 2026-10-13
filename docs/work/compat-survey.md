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
- [ ] G1-4 Alt+Tab 문제 재현 (사용자 보고와 자동 테스트 결과가 다름 → 조건 확인 필요)
- [ ] G1-5 고주사율(120Hz 이상) 환경에서 게임 속도 확인 (현재 모니터 60Hz뿐)
- [ ] G1-6 DPI 배율 100%가 아닌 환경에서 확인
- [ ] G1-7 세이브 파일 생성 위치 확인
- [ ] G1-8 사운드, 마우스 조준 정확도 상세 확인 (사용자 플레이: "정상")

### G2. 숨은 설정과 부속 프로그램 확인 — 완료

- [x] G2-1 `hglCore.ini` 읽는 위치와 키별 효과 확인
- [x] G2-2 `Config.exe` 기능 확인 (정적 분석)

### G3. 기존 도구와 비교 — 대기

- [ ] G3-1 `00_Setup.bat`의 `~ WINXPSP3` 호환 모드 적용 결과
- [ ] G3-2 DxWnd(동봉 v2.04.65) 창 모드 프로필 결과
- [ ] G3-3 dgVoodoo2 적용 결과 (사용자 확인 후 다운로드)
- [ ] G3-4 DDrawCompat 적용 결과 (사용자 확인 후 다운로드)

## 확인 결과 요약

| # | 항목 | 증상 / 결과 | 재현 방법 | 담당 도메인 |
|---|---|---|---|---|
| 1 | 실행 | 정상 실행. 시작할 때마다 "Attention" 확인창이 뜬다 ("NVIDIA GeForce RTX 3080 \ Microsoft Direct3D Hardware Transform and Lighting acceleration capable device will be used." 예/아니요) | 그냥 실행 | renderer (장치 선택), platform-display |
| 2 | 화면 모드 | **전용 전체 화면, 640×480 32bpp 60Hz 고정.** 주 모니터의 해상도를 바꾼다. 창은 `WS_POPUP`, `WS_EX_TOPMOST`, 크기 640×480 | 실행 후 `EnumDisplaySettings` | platform-display, renderer |
| 3 | 다중 모니터 | 게임이 해상도를 바꾸는 동안 **보조 모니터의 위치가 이동한다** (`(1920,511)` → `(640,480)`). 종료하거나 Alt+Tab으로 나가면 원래대로 돌아온다 | 모니터 2개 환경에서 실행 | platform-display |
| 4 | Alt+Tab (자동 테스트) | 나가기: 창 최소화, 해상도 복구, CPU 사용 1%로 정지. 돌아오기(Alt+Tab, 작업 표시줄 복원 둘 다): 640×480으로 다시 바뀌고 화면 갱신 계속됨 → **정상** | `keybd_event`로 Alt+Tab 전송 | platform-display |
| 5 | Alt+Tab (사용자 보고) | **실제 키보드로 Alt+Tab을 누르면 아무 반응이 없다.** 깜빡임이나 해상도 변경도 일어나지 않는다. 자동 테스트(`keybd_event` 주입)와 결과가 다름. 실행 경로와 게임 상태(메뉴/플레이 중)는 미확인 | 실제 키보드로 Alt+Tab | platform-display, platform-input |
| 6 | CPU 사용 | 실행 중 **CPU 코어 1개를 100% 사용** (대기 없이 도는 루프로 추정). 최소화하면 1% | 5초간 프로세스 CPU 시간 측정 | platform-timing |
| 7 | 고주사율 모니터 속도 | **미확인** (테스트 PC 모니터가 60Hz) | — | platform-timing |
| 8 | CD 확인 | CD 없이 실행됨. 실행 파일의 CD 확인 호출이 NOP으로 지워진 것으로 보임 (reverse-engineering 참고) | — | platform-filesystem, loader |
| 9 | 숨은 설정 | `hglCore.ini`의 `DispFps`, `FullScreen`, `DispVersion`은 읽기만 하고 쓰이지 않음. 창 모드나 FPS 표시를 설정으로 켤 수 없음 | `hglCore.ini` 작성 후 실행 | reverse-engineering |
| 10 | 해상도 설정 | 해상도를 바꾸는 설정이 없음. `Config.exe`는 컨트롤러·밝기·번쩍임 설정 전용 | 정적 분석 | renderer |
| 11 | 보안 경고 | `Config.exe`에 인터넷 다운로드 표시(Zone.Identifier)가 남아 있어서 실행할 때 "파일 열기 - 보안 경고"가 뜬다. `Hod2.exe`에는 표시가 없다 | 탐색기나 `Start-Process`로 실행 | (배포 안내) |
| 12 | 사운드, 조준, 진행 | 사용자 플레이 결과 "정상" | 사용자 플레이 | — |

## 테스트 환경

| 항목 | 값 |
|---|---|
| OS | Windows 11 Education 10.0.26200 |
| GPU | NVIDIA GeForce RTX 3080 (드라이버 32.0.16.1714) |
| 기타 디스플레이 어댑터 | Parsec Virtual Display Adapter |
| 모니터 | 2대. DISPLAY2(주) 1920×1080 60Hz `(0,0)`, DISPLAY1 1920×1080 60Hz `(1920,511)` |
| 호환성 플래그 | 테스트 사본 경로에는 없음 |
| 실행 파일 | `Hod2.exe` SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69` |

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-14 | 테스트는 `test-game\THOTD2` 사본에서만 한다 | 원본 보존. 사본은 `robocopy`로 만들고 파일 수·크기 일치 확인 |
| 2026-10-14 | 인터넷 다운로드 표시가 있는 실행 파일(`Config.exe`)은 사용자 확인 없이 실행하지 않는다 | 보안 경고는 사용자가 판단할 부분. 정적 분석으로 대체 |

## 미해결 질문

- Alt+Tab이 실제 키보드에서만 무반응인 이유. 가설:
  - DirectInput이 키보드를 독점 모드(`DISCL_EXCLUSIVE`)로 잡아서 시스템이 Alt+Tab을 처리하기 전에 입력을 가져감. `keybd_event`로 주입한 입력은 경로가 달라서 통과했을 수 있음
  - 사용자가 실행한 조건(원본 폴더 + `DWM8And16BitMitigation` 플래그, 또는 DxWnd 경유)이 테스트 조건과 다름
- 사용자가 실행한 경로와 게임 상태(메뉴/플레이 중)
- 고주사율 테스트 방법 (Parsec 가상 디스플레이로 120/144Hz를 만들 수 있는지)
- 외부 래퍼를 받을지, 받는다면 어느 버전으로 할지 (사용자 확인 필요)
- 게임이 실제로 몇 fps로 그리는지, vsync를 기다리는지

## 작업 기록

### 2026-10-14 — Alt+Tab 사용자 보고 (관련: G1-4)

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
