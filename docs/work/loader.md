# 로더·후킹 (launcher, interop)

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 0, 1-2
- **관련 코드:** `src/launcher/`, `src/interop/`, `experiments/dinput_probe/` (측정용)
- **마지막 갱신:** 2026-10-14

## 주요 목표

원본 `Hod2.exe`를 수정하지 않고 우리 코드를 게임 프로세스에 올린다. 원본 함수와 전역 변수를 안전하게 가로채고 교체할 수 있는 공통 기반(주소 정의, 후킹·패치 도우미, 실행 파일 버전 확인)을 만든다.

**완료 기준:** 런처(또는 프록시 DLL)로 실행하면 우리 DLL이 로드되고, 예제 후킹 하나가 동작하고, 지원하지 않는 실행 파일이면 경고 후 중단한다.

## 중목표 / 소목표

### G1. 로더 방식 결정 — 진행 중

- [x] G1-1 프록시 후보 DLL의 내보내기 함수 수와 `KnownDLLs` 여부 확인
- [x] G1-2 측정용 `dinput.dll` 프록시로 초기화 시점 측정
- [ ] G1-3 방식 결정 (사용자 확인 대기)

## 측정 결과: 프록시 후보

| DLL | 내보내기 | 게임이 쓰는 함수 | 비고 |
|---|---|---|---|
| `dinput.dll` | 7개 (`DirectInputCreateA/W/Ex`, `DllCanUnloadNow`, `DllGetClassObject`, `DllRegisterServer`, `DllUnregisterServer`) | `DirectInputCreateEx` | **추천** |
| `dsound.dll` | 약 12개 | 서수 #1 (`DirectSoundCreate`) | |
| `ddraw.dll` | 약 22개 | `DirectDrawCreateEx`, `DirectDrawEnumerateExA` | dgVoodoo2가 이 이름을 사용 → 제외 |
| `winmm.dll` | 200개 | 없음 | 연결할 함수가 너무 많음 |

- 위 DLL은 모두 `KnownDLLs`(`HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs`)에 없다. 게임 폴더에 같은 이름의 DLL을 두면 시스템 DLL보다 먼저 로드된다.

## 측정 결과: 초기화 시점 (`dinput_probe`, 2026-10-14)

`experiments/dinput_probe`: 모든 내보내기를 시스템 `dinput.dll`로 넘기고, `DllMain`에서 `Hod2.exe`의 IAT 4곳(`GetCommandLineA`, `CreateWindowExA`, `MessageBoxA`, `DirectDrawCreateEx`)을 가로채서 호출 순서를 기록한다. 테스트 사본(dgVoodoo2 시험 C 설정)에서 실행했다.

| 시각 (DllMain 기준) | 사건 | 호출 위치 (복귀 주소) |
|---|---|---|
| 0.16 ms | 프록시 `DllMain` (`PROCESS_ATTACH`). 창 없음 | — |
| 0.5~3.5 ms | `DllMain`에서 IAT 후킹 4개 설치 성공 | IAT 슬롯 `0x4C3120`, `0x4C31EC`, `0x4C31FC`, `0x4C3000` |
| **4.6 ms** | **첫 `GetCommandLineA`** (CRT 시작 코드. 로더 락 밖) | `0x4AC861` |
| 12 ms | `CreateWindowExA` 메인 창 생성 | `0x4A588B` |
| 75 ms, 505 ms ×2 | `DirectDrawCreateEx` (장치 열거) | `0x4BD209` |
| 517 ms | `MessageBoxA` "Attention" (type `0x34` = `MB_YESNO \| MB_ICONEXCLAMATION`) | `0x4A4758` |
| 965 ms | 확인창 응답 6 (`IDYES`) | — |
| **1051 ms** | **`DirectInputCreateEx`** (version `0x700`). 메인 창이 이미 보이는 상태 | `0x41E812` |

결론:
- `DirectInputCreateEx`는 **창 생성과 시작 확인창 뒤**에 처음 불린다. 여기서 초기화하면 확인창 넘기기 같은 이른 패치는 늦다.
- 대신 **프록시 `DllMain`에서 IAT 후킹을 바로 설치할 수 있다.** 그리고 **첫 `GetCommandLineA` 호출**(4.6 ms, CRT 시작 코드, 로더 락 밖, 창 생성 전)에서 `Thod2Core_Initialize()`를 부르면 가장 이른 시점에 안전하게 초기화할 수 있다.
- 프록시를 넣은 상태에서도 게임은 타이틀 화면까지 정상 동작했다 (`DirectInputCreateEx` 결과 `S_OK`).

## 추천안 (사용자 결정 대기)

- **`dinput.dll` 프록시**로 로드한다. `Hod2.exe`를 그냥 실행해도 적용되고, 내보내기 7개만 넘기면 된다.
- 초기화 순서:
  1. `DllMain`: 모듈 핸들 저장, `Hod2.exe` IAT의 `GetCommandLineA` 하나만 가로챈다 (메모리 쓰기만, 파일·라이브러리 로드 없음).
  2. 첫 `GetCommandLineA` 호출: 시스템 `dinput.dll` 로드, `thod2_core.dll` 로드, `Thod2Core_Initialize()` 호출. 지원 빌드면 후킹 설치.
  3. 안전장치: 어떤 이유로 2가 안 불렸으면 `DirectInputCreateEx`에서 초기화한다.
- 런처 방식은 지금은 만들지 않는다. 나중에 "런처로 실행"이 필요해지면(예: 설정 UI) 같은 코어를 쓰는 런처를 추가할 수 있다.

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|

## 미해결 질문

- 런처 주입 방식과 프록시 DLL 방식 중 무엇을 기본으로 할지 → 측정 결과로 `dinput.dll` 프록시 추천, 사용자 결정 대기
- DxWnd 등 다른 도구가 `dinput.dll` 프록시를 쓸 때의 충돌 처리 (지금 구성에서는 충돌 없음)
- 지원할 `Hod2.exe` 버전 목록 (현재 보유본의 해시, 다른 배포본 존재 여부)
- 수정본만 지원할지, 수정 안 된 원본도 지원할지. 둘 다 지원하려면 해시별로 패치 위치가 같은지 확인 필요

## 작업 기록

### 2026-10-14 — 로더 방식 측정 (관련: G1-1, G1-2)

- **한 일:** 시스템 32비트 DLL(`SysWOW64`)의 내보내기 함수를 `dumpbin`으로 세고 `KnownDLLs`를 확인했다. 측정용 `dinput.dll` 프록시(`experiments/dinput_probe`)를 만들어 테스트 사본에서 실행하고 호출 순서를 기록했다. 측정 후 테스트 사본에서 프록시를 지웠다.
- **결과:** 위 "측정 결과" 두 표. `DirectInputCreateEx`는 확인창 뒤(1051 ms)에 불리지만, `DllMain` IAT 후킹 + 첫 `GetCommandLineA`(4.6 ms) 초기화로 창 생성 전에 초기화할 수 있다.
  - 참고: 이 실행에서 2초 동안 잰 CPU 사용률이 35%로, 이전 측정(90~100%)보다 낮았다. 타이틀 화면이었다는 차이 말고는 원인을 확인하지 않았다.
- **다음:** 사용자 결정 후 정식 로더(`src/loader/`)를 만든다. 첫 예제는 시작 확인창 넘기기 (`MessageBoxA` "Attention", `0x4A4758`).

### 2026-10-14 — 코어 DLL 준비됨 (빌드 도메인에서 넘어온 내용)

- **결과:** `thod2_core.dll`이 `Thod2Core_Initialize()`(`__cdecl`, 꾸밈 없는 이름)를 내보낸다. 로더는 DLL을 로드한 뒤 로더 락 밖에서 이 함수를 한 번 호출하면 된다. 반환값이 `1`(알 수 없는 실행 파일)이면 후킹을 설치하지 않아야 한다. 지원 빌드 목록은 `src/core/exe_id.cpp`.
- **상세:** [build.md](build.md) "코어 DLL 동작"

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:** 현재 보유한 `Hod2.exe` SHA-256은 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69` (MD5 `258F350774317F785B430BE32D2ABE9A`)이다. 이 파일은 CD 확인을 지운 수정본으로 판단된다 (`0x4A5899`에 5바이트 NOP). 지원 버전 목록과 실행 파일 식별 방식을 정할 때 고려해야 한다.
- **상세:** [compat-survey.md](compat-survey.md), [reverse-engineering.md](reverse-engineering.md)
