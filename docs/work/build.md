# 빌드·배포

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 0
- **관련 코드:** `CMakeLists.txt`, `CMakePresets.json`, `src/core/`, `tests/`, `tools/deploy.ps1`, `tools/package.ps1`, `.github/`, `config/`
- **마지막 갱신:** 2026-10-29

## 주요 목표

누구나 같은 절차로 x86(Win32) 빌드를 만들고, 테스트용 게임 폴더에 배포해서 바로 실행해 볼 수 있는 빌드 체계를 만든다. 이후 오픈 소스 기여자도 같은 절차로 빌드할 수 있어야 한다.

**완료 기준:** 깨끗한 개발 환경에서 문서에 적힌 명령만으로 빌드와 테스트 폴더 배포가 끝난다.

## 중목표 / 소목표

### G1. 빌드 골격 (ROADMAP 0) — 완료

- [x] G1-1 CMake 프로젝트와 x86 프리셋 (`CMakePresets.json`: `x86`, `x86-debug`, `x86-release`)
- [x] G1-2 코어 DLL `thod2_core.dll`: 로그, 설정 파일, 실행 파일 해시 식별
- [x] G1-3 스모크 테스트 `core_tests` (CTest)
- [x] G1-4 테스트 사본 배포 스크립트 `tools/deploy.ps1`
- [x] G1-5 코딩 규칙 문서 [docs/CONVENTIONS.md](../CONVENTIONS.md)

### G2. 빌드 편의 — 진행 중

- [ ] G2-1 CMake를 PATH 없이 부르는 래퍼 스크립트 (또는 Developer PowerShell 안내만으로 충분한지 결정)
- [x] G2-2 CI (GitHub Actions, windows 러너에서 x86 빌드 + 테스트) — 2026-10-29 워크플로 추가 (`.github/workflows/build.yml`). GitHub에서 처음 돌리는 것은 머지 뒤

### G3. 배포판 (ROADMAP 4단계) — 진행 중

- [x] G3-1 배포 zip 스크립트 `tools/package.ps1` (zip 안의 파일을 게임 폴더에 그대로 풂)
- [x] G3-2 v* 태그를 올리면 GitHub Actions가 zip을 Release에 올림. README의 받기 링크는 `releases/latest/download/thod2-native.zip`
- [x] G3-3 배포 zip + dgVoodoo2 파일 3개만으로 깨끗한 게임 사본에 설치되는지 확인 (Python 없이)
- [ ] G3-4 저장소 공개, 첫 Release(v0.1.0)

## 빌드 방법

"Developer PowerShell for VS 2022"에서 저장소 폴더로 이동한 뒤 실행한다. 일반 PowerShell에서는 `cmake`가 PATH에 없으므로 아래 "개발 환경" 표의 전체 경로로 부른다.

```powershell
cmake --preset x86                  # 구성 (build/x86, Visual Studio 2022 Win32)
cmake --build --preset x86-release  # 빌드 (x86-debug도 있음)
ctest --preset x86-release          # 테스트 (core_tests, loader_tests, platform_tests, re_tables)
.\tools\deploy.ps1                  # ..\test-game\THOTD2 에 배포 (-Config Debug, -Remove 가능)
```

결과물:

| 파일 | 위치 | 설명 |
|---|---|---|
| `dinput.dll` | `build/x86/bin/<Config>/` | **로더** (`src/loader`). 게임 폴더에 넣으면 `thod2_core.dll`을 불러옴 |
| `thod2_core.dll` | 같은 폴더 | 게임에 들어갈 코어 DLL. 의존 DLL은 `bcrypt.dll`, `KERNEL32.dll`뿐 (정적 CRT) |
| `core_tests.exe`, `loader_tests.exe`, `platform_tests.exe` | 같은 폴더 | 테스트 |
| `dinput.dll` | `build/x86/bin/experiments/<Config>/` | 측정용 프록시 `experiments/dinput_probe` (배포 대상 아님) |
| `thod2_core_lib.lib`, `thod2_interop_lib.lib`, `thod2_platform_lib.lib` | `build/x86/lib/<Config>/` | 코어·후킹 도우미·재구현 플랫폼 계층 내부 구현 (DLL과 테스트가 링크) |

`deploy.ps1`은 `dinput.dll`(로더)과 `thod2_core.dll`을 복사하고, 게임 폴더에 `thod2-native.ini`가 없을 때만 `config/thod2-native.ini`를 복사한다. `-Remove`는 DLL과 로그를 지우고 ini는 남긴다 (로컬 설정 보존). 로더 동작은 [loader.md](loader.md) "로더 동작"을 본다.

## 배포판 만들기

```powershell
cmake --build --preset x86-release
.\tools\package.ps1                 # build\package\thod2-native.zip
```

zip 안의 파일 (모두 게임 폴더에 그대로 푼다):

| 파일 | 원본 |
|---|---|
| `dinput.dll`, `thod2_core.dll` | Release 빌드 |
| `thod2-native.ini` | `config/thod2-native.ini` |
| `thod2-native-README.txt` | `config/package-readme.txt` (짧은 설치 안내, 한국어) |
| `thod2-native-LICENSE.txt` | `LICENSE` (CC BY-NC-SA 4.0 원문) |

dgVoodoo2는 라이선스를 확인할 수 없어서 넣지 않는다 ([INSTALL.md](../INSTALL.md) 끝). 사용자가 받은 dgVoodoo2의 `dgVoodoo.conf`에는 thod2-native가 게임을 켤 때 권장값을 넣는다 (`[dgvoodoo] preset`, [platform-display.md](platform-display.md)).

**릴리스 절차:**

1. `main`에서 `CMakeLists.txt`의 `project(... VERSION x.y.z)`를 올린다 (로그 첫 줄과 `Thod2Core_Version()`에 나온다). 필요하면 `docs/release-notes.md`를 고친다.
2. `git tag vx.y.z` → `git push origin vx.y.z`.
3. `.github/workflows/build.yml`이 빌드·테스트·`package.ps1`을 돌리고, `gh release create`로 Release를 만들어 `thod2-native.zip`을 올린다. README의 링크 `https://github.com/BangJeongHyeok/thod2-native/releases/latest/download/thod2-native.zip`은 늘 최신 Release의 zip을 가리킨다.

워크플로는 `main` push와 PR마다 빌드·테스트도 하고, zip을 실행 결과(artifact)로 남긴다. 러너는 `windows-2022`(Visual Studio 2022, Python 포함)다.

## 코어 DLL 동작 (0.1.0, 로더 추가 후)

- 공개 API (`src/core/include/thod2/core.h`): `Thod2Core_Version()`, `Thod2Core_Initialize(const char* loader_info)` (둘 다 `__cdecl`, 꾸밈 없는 이름으로 내보냄). `loader_info`는 누가 언제 불렀는지 설명하는 문자열로 로그에 남는다
- `Thod2Core_Initialize()`가 하는 일:
  1. DLL 옆의 `thod2-native.ini`를 읽는다 (`[core] log_level`, `allow_unknown_exe`).
  2. DLL 옆에 `thod2-native.log`를 새로 만든다. 초기화 주체(`loader_info`)와 그 시점에 게임 메인 창이 있는지 기록한다.
  3. 호스트 실행 파일의 SHA-256을 계산해서 지원 빌드 목록(`src/core/exe_id.cpp`)과 비교한다.
  4. 지원 빌드(또는 `allow_unknown_exe = 1`)이면 설정에서 켜진 패치를 설치한다. 지금 있는 패치: `skip_device_dialog` ([platform-display.md](platform-display.md) G1), `frame_wait_sleep` ([platform-timing.md](platform-timing.md) G2), `redirect_user_files` ([platform-filesystem.md](platform-filesystem.md) G2), 개발용 `trace_ddraw` (`[debug]`, 기본 꺼짐, DirectDraw 호출 기록), 개발용 `profile` (`[debug]`, 기본 꺼짐, 메인 스레드 샘플링 프로파일러), 개발용 `trace_files` (`[debug]`, 기본 꺼짐, 파일 접근 기록).
  5. 결과 코드: `0` OK, `1` 알 수 없는 실행 파일, `2` 이미 초기화됨, `3` 실패.
- `DllMain`에서는 모듈 핸들만 저장한다 (로더 락 안에서 파일·해시 작업을 하지 않기 위해).

## 개발용 설정 (`[debug]`)

`thod2-native.ini`에 직접 넣어 쓴다. 예제 설정(`config/thod2-native.ini`)과 사용자 안내(`docs/INSTALL.md`)에는 넣지 않는다. 시험 스크립트(`tools/compat/*.ps1`)는 필요한 값을 스스로 넣었다가 되돌린다.

```ini
[debug]
trace_ddraw = 0
profile = 0
trace_files = 0
flip_override = 0
trace_interp = 0
interp_snap = 2.0
interp_2d = 1
```

| 키 | 기본값 | 설명 | 관련 문서 |
|---|---|---|---|
| `trace_ddraw` | `0` | DirectDraw 호출(화면 모드, 버퍼, Flip)과 프레임 간격 통계를 로그에 기록 | [platform-display.md](platform-display.md) |
| `profile` | `0` | 메인 스레드를 약 1ms마다 샘플링해서 CPU를 어디에 쓰는지 5초마다 기록 | [platform-timing.md](platform-timing.md) |
| `trace_files` | `0` | 게임의 파일 접근(경로, 읽기·쓰기, 결과, 호출 위치)을 기록 | [platform-filesystem.md](platform-filesystem.md) |
| `input_compare` | `0` | 입력 갱신을 원본과 재구현으로 함께 돌려 결과를 비교·기록 | [platform-input.md](platform-input.md) |
| `widescreen_2d` | `1` | `0`이면 와이드스크린에서 투영만 넓히고 2D는 압축하지 않음 (비교용) | [renderer.md](renderer.md) |
| `trace_screen_quads` | `0` | 넓거나 화면 가장자리에 걸친 2D 사각형, 모델 행렬, 띠 모델 호출 경로, 스프라이트 호출 위치별 좌표를 기록 | [renderer.md](renderer.md) |
| `trace_textures` | `0` | `replace_textures`가 켜져 있을 때, 교체 이미지 없이 만들어진 텍스처를 한 번씩 기록 (뱅크, 번호, 크기, PVR 형식). 업스케일이 빠진 텍스처를 찾는 데 쓴다 | [assets.md](assets.md) |
| `flip_override` | `0` | 실험. `trace_ddraw = 1`과 함께 쓰면 primary `Flip`에 `1` = `DDFLIP_NOVSYNC`, `2` = `DDFLIP_INTERVAL2`를 더함 | [platform-timing.md](platform-timing.md) G3 |
| `trace_interp` | `0` | `[display] interpolate`가 켜져 있을 때, 5초마다 초당 틱·화면 수, 건너뛴 틱, 보간한 행렬 수, 가까운 쌍으로 짝지은 수(`reordered`), 버린 짝(`replaced`), 시작·끝 장치 상태가 다른 틱 수, 행렬 이동량 분포, 2D 사각형 수(`quads`, 다시 그릴 때마다 셈)·짝·snap·버린 짝을 기록 | [platform-timing.md](platform-timing.md) G4 |
| `interp_snap` | `2.0` | 보간하지 않고 바로 바꾸는 기준. 한 틱에 이 값 + (카메라에서의 거리의 1/4)보다 많이 움직인 행렬 | [platform-timing.md](platform-timing.md) G4 |
| `interp_2d` | `1` | `[display] interpolate`가 켜져 있을 때 2D 사각형(조준점, HUD, 메뉴)도 보간. `0`이면 3D만 보간하고 2D는 초당 60번 움직임 (비교용) | [platform-timing.md](platform-timing.md) G4-5 |

## 개발 환경 (현재 PC, 2026-10-14 확인)

| 도구 | 버전 / 위치 | 비고 |
|---|---|---|
| Visual Studio | Community 2022 17.14.41 | |
| MSVC | 14.44.35207 | x86 컴파일러(`bin\Hostx64\x86\cl.exe`) 있음 |
| Windows SDK | 10.0.22621.0, 10.0.26100.0 | 사용자가 VS Installer로 추가 설치 |
| CMake | 3.31.6-msvc6 | VS 내장 (`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`, 같은 폴더에 `ctest.exe`). **PATH에 없음** |
| Ninja | 1.12.1 | VS 내장 (`Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`). 현재 프리셋은 Visual Studio 생성기를 써서 필요 없음 |
| Git | 2.55.0.windows.5 | |
| GitHub CLI | 2.102.0 | `C:\Program Files\GitHub CLI\gh.exe`. 설치 전에 열린 터미널에서는 PATH에 없으므로 새 터미널에서 쓰거나 전체 경로로 호출. `BangJeongHyeok` 계정으로 로그인됨 |
| Python | 3.14.7 | 조사 스크립트와 ctest `re_tables`(`tools/re/check_re.py`)용. 구성 단계에서 찾지 못하면 그 테스트만 빠진다 |
| Ghidra, JDK, x64dbg, capstone | Ghidra 12.1.4, Temurin JDK 21.0.12.1, x64dbg 2026.05.27, capstone 5.0.9 | 2026-10-15 설치 (저장소 밖 `external\`). 빌드에는 필요 없음. 상세는 [reverse-engineering.md](reverse-engineering.md) "분석 도구" |
| Real-ESRGAN (ncnn-vulkan) | 20220424 (`v0.2.5.0` 릴리스) | 2026-10-15 사용자 확인 후 설치 (`external\realesrgan-ncnn-vulkan\`). UI 텍스처 업스케일(`tools/assets/upscale-ui.ps1`)용. 빌드에는 필요 없음. 상세는 [assets.md](assets.md) |

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-14 | CMake 생성기는 "Visual Studio 17 2022" + `-A Win32` | vcvars 환경 없이도 x86 빌드가 된다. Ninja는 x86 개발자 환경이 필요해서 보류 |
| 2026-10-14 | CRT 정적 링크 (`/MT`) | 게임 폴더에 VC++ 재배포 패키지가 필요 없게 하기 위해 |
| 2026-10-14 | 테스트는 외부 프레임워크 없이 작성 | 서드파티 추가는 사용자 확인이 필요하고, 지금 규모에서는 충분함. 테스트가 늘면 재검토 |
| 2026-10-14 | 코어 내부는 정적 라이브러리 + DLL은 공개 API만 | 테스트가 내부 함수를 직접 부를 수 있게 하기 위해 |
| 2026-10-29 | 라이선스는 CC BY-NC-SA 4.0 | 사용자 결정: 고치고 다시 배포하는 것은 허용하되, 원작의 2차 창작물이므로 상업적 이용은 막는다. 대안: PolyForm Noncommercial 1.0.0(소프트웨어용이지만 덜 알려졌고 동일조건 강제가 없음). 둘 다 OSI 오픈 소스 라이선스는 아니다 |
| 2026-10-29 | 배포는 GitHub Release의 zip 하나, README에서 `releases/latest/download/` 링크로 받는다 | 저장소 전체를 받지 않고 파일만 받을 수 있다. 바이너리를 저장소에 커밋하지 않는다(`.gitignore`가 `*.dll`, `*.zip` 제외). 대안: 저장소 안에 배포 폴더를 두기(바이너리가 기록에 쌓임) |
| 2026-10-29 | 배포 zip은 게임 폴더에 바로 푸는 평평한 구조, 안내·라이선스 파일 이름에 `thod2-native-`를 붙인다 | 폴더째 풀면 사용자가 파일을 다시 옮겨야 한다. 게임 폴더의 다른 파일과 이름이 겹치지 않게 한다 |

## 미해결 질문

- GitHub Actions `windows-2022` 러너에서 `loader_tests`(시스템 `dinput.dll`을 불러옴)가 통과하는지 (공개 뒤 첫 실행에서 확인)
- 지원 빌드 목록에 수정 안 된 원본 `Hod2.exe`를 추가할지 (해시를 구할 방법이 필요)

## 작업 기록

### 2026-10-29 — 배포판, CI, README, 라이선스 (관련: G2-2, G3)

- **한 일:**
  - `tools/package.ps1`(배포 zip), `config/package-readme.txt`, `.github/workflows/build.yml`(빌드·테스트·zip, v* 태그면 Release), `docs/release-notes.md`(Release 본문).
  - `README.md`(사용자용: 받기 링크, 설치, 설정, 아직 확인하지 못한 것, 이슈 올리는 방법, 라이선스), `LICENSE`(CC BY-NC-SA 4.0 원문을 creativecommons.org `legalcode.txt`에서 받음, 20,850 bytes, SHA-256 `e66c269d…2b7bbd`), `.github/ISSUE_TEMPLATE/`(문제 신고, 지원되지 않는 `Hod2.exe`).
  - `[dgvoodoo] preset`(기본 1): 사용자가 dgVoodoo2 파일 3개만 복사하면 thod2-native가 `dgVoodoo.conf`에 권장값을 넣는다. 배포판 설치에 Python이 필요 없다 ([platform-display.md](platform-display.md)).
- **결과:**
  - 깨끗한 게임 사본(`original-game\THOTD2`를 `test-game\THOTD2-clean`에 복사)에 zip을 풀고, dgVoodoo2 2.87.5 zip의 원래 `DDraw.dll`, `D3DImm.dll`, `dgVoodoo.conf`를 복사했다. 실행하자 로그에 `dgvoodoo_preset` 8줄(FullScreenMode true → false, ScalingMode unspecified → stretched_ar, WindowedAttributes, Resolution unforced → max, Antialiasing appdriven → 4x, AppControlledScreenMode, DisableAltEnterToToggleScreenMode, dgVoodooWatermark true → false)이 남고, 화면은 테두리 없는 4:3, 워터마크 없이 나왔다. 경고·오류 없음. 측정 중 주 모니터 240Hz.
  - 그 전에 INSTALL.md 순서(`deploy.ps1` + `dgvoodoo-setup.py`)대로 같은 사본에 설치한 것도 정상 실행됐다 (초당 60장, 경고·오류 없음. 측정 중 사용자가 PC를 쓰고 있었음).
  - zip 348,149 bytes. Release 빌드 경고 0개, `ctest` 4개 통과.
- **다음:** 저장소 공개와 첫 Release(G3-4). CI 첫 실행 결과 확인.

### 2026-10-15 — ctest에 `re_tables` 추가

- **결과:** `tests/CMakeLists.txt`가 Python 3를 찾으면 `re/` CSV 검사(`tools/re/check_re.py`)를 테스트 `re_tables`로 등록한다. Release 구성·빌드·테스트 3개 통과. 게임 파일이 필요 없어서 CI에서도 돌 수 있다. 상세는 [reverse-engineering.md](reverse-engineering.md).
- 개발 환경 표의 "Ghidra, x64dbg 없음"을 설치 결과로 고쳤다 (2026-10-15 설치 때 빠뜨림).

### 2026-10-15 — 개발용 설정을 사용자 문서에서 분리

- **한 일:** `[debug]` 설정(`trace_ddraw`, `profile`, `trace_files`, `flip_override`)을 `docs/INSTALL.md`와 예제 설정 `config/thod2-native.ini`에서 빼고, 이 문서의 "개발용 설정"으로 옮겼다.
- **결과:** 사용자 문서와 예제 설정에는 사용자가 바꿀 설정(`[core]`, `[display]`, `[files]`, `[timing]`)만 남았다.

### 2026-10-14 — 빌드 골격 (관련: G1)

- **한 일:** CMake 프로젝트와 x86 프리셋, 코어 DLL, 스모크 테스트, 배포 스크립트, 예제 설정 파일, 코딩 규칙 문서를 만들었다. `tools/compat`의 한국어 주석을 영어로 바꿨다 (코딩 규칙: 코드 주석은 영어, PowerShell은 ASCII).
- **결과:**
  - `cmake --preset x86` → Debug, Release 빌드 모두 경고 0개.
  - `ctest` Debug, Release 모두 통과 (검사 16개: SHA-256, 설정 파일, 실행 파일 식별, DLL 로드·내보내기·초기화·로그).
  - `dumpbin`: `thod2_core.dll`은 x86(machine 14C), 내보내기 `Thod2Core_Initialize`, `Thod2Core_Version`, 의존 DLL `bcrypt.dll`, `KERNEL32.dll`. Release 크기 343,040 bytes.
  - `deploy.ps1`로 테스트 사본에 배포하고 `-Remove`로 지우는 것까지 확인했다.
- **다음:** 로더(ROADMAP 1-2). 런처 주입과 프록시 DLL 중 방식을 정한다.

### 2026-10-14 — 개발 환경 확인

- **한 일:** 설치된 빌드 도구를 확인했다. 사용자가 Windows SDK와 "C++ CMake tools for Windows"를 추가 설치했다.
- **결과:** 위 "개발 환경" 표. 테스트용 게임 사본은 `E:\Repos\hod2-native\test-game\THOTD2`에 `robocopy original-game\THOTD2 test-game\THOTD2 /E`로 만들었다 (파일 2004개, 542,336,749 bytes).
- **다음:** 0단계 빌드 골격 (CMake x86 DLL 프로젝트).
