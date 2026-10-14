# 플랫폼: 파일·경로·세이브

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 1-3, 2-2
- **관련 코드:** `src/platform/` (예정), `src/core/patches/file_redirect.cpp`, `src/core/patches/file_trace.cpp` (개발용)
- **마지막 갱신:** 2026-10-15

## 주요 목표

게임 데이터, 설정, 세이브 파일의 경로를 추상화한다. 원본 데이터 폴더를 찾고, 세이브와 설정을 사용자 폴더(`%APPDATA%` 등)에 저장해서 관리자 권한 없이 동작하게 한다. CD 확인을 원본 실행 파일 수정 없이 처리한다. 자산 도메인의 데이터 덮어쓰기 폴더 기능의 기반이 된다.

**완료 기준:** 게임 폴더에 쓰기 권한이 없어도 정상 동작하고, CD 없이 수정 안 된 원본 `Hod2.exe`로 실행되며, 모든 파일 접근이 우리 경로 계층을 거친다.

## 중목표 / 소목표

### G1. 게임이 쓰고 읽는 파일 분석 — 완료

- [x] G1-1 파일 접근 기록 기능 (`[debug] trace_files`)
- [x] G1-2 시작·타이틀·정상 종료 때의 파일 접근 기록
- [x] G1-3 세이브 파일 찾기

### G2. 쓰는 파일을 사용자 폴더로 옮기기 — 완료

- [x] G2-1 세이브 파일 4개를 사용자 폴더로 리디렉션 (처음 한 번 게임 폴더에서 복사)
- [x] G2-2 `C:\WINDOWS\d3dx.dat`를 사용자 폴더로 리디렉션 (UAC 가상화에 의존하지 않게)
- [x] G2-3 `exception.log` 리디렉션, `thod2-native.log`는 게임 폴더에 쓸 수 없으면 사용자 폴더로
- [x] G2-4 게임 폴더 쓰기 실패 상황 확인 (세이브·로그 파일에 읽기 전용 속성)

## 파일 접근 분석 결과 (2026-10-15)

`[debug] trace_files = 1`로 `Hod2.exe`의 IAT를 거치는 파일 API를 기록했다 (테스트 사본, 타이틀·데모 화면, 정상 종료). 게임은 정적 링크 CRT를 쓰므로 CRT 파일 함수도 같은 IAT의 `CreateFileA`를 거친다.

### 쓰는 파일

| 파일 | 언제 | 방식 | 호출 위치 | 비고 |
|---|---|---|---|---|
| **`pol/bg_adv19.bin`, `tex/scr_tod_itm_itamidome2.bin`, `pol/komono_0.bin`, `pol/tv2.bin`** | 시작 때 읽고, **정상 종료 때 다시 씀** | 읽기 `OPEN_EXISTING` (`0x49F990`), 쓰기 `CREATE_ALWAYS` (`0x49FA17`) → `OPEN_EXISTING` (`0x49FA8C`) | 위 | **세이브 파일.** 아래 참고 |
| `C:\WINDOWS\d3dx.dat` | 시작 때 7번 | 읽기·쓰기 `OPEN_ALWAYS` | `0x4BBF28` (경로는 `GetWindowsDirectoryA`, `0x4C0E13`) | D3DX 장치 정보 캐시 (`D3DX\r\n\x1A\n`로 시작). 관리자 권한이 없으면 Windows가 `%LOCALAPPDATA%\VirtualStore\Windows\d3dx.dat`로 돌려서 쓴다 (UAC 가상화, 4,096 bytes 확인) |
| `exception.log` | 게임이 비정상 종료할 때 | — | 게임 내장 오류 처리기 | 작업 폴더(게임 폴더)에 씀. `original-game`에 2026-10-14 00:15 기록 1건 있음 (아래 참고) |

### 세이브 파일

- 데이터 파일처럼 보이는 이름의 파일 **4개가 세이브**다. 4개 모두 **987 bytes**이고, 내용은 암호화된 것처럼 무작위다. 진짜 모델 파일(`pol/b6boss1z.bin` 등)은 `a0 92 00 00 03 a0 ...` 같은 형식이 있는데 이 4개는 없다.
- 테스트 사본의 `pol/tv2.bin`은 원본 폴더와 내용이 다르다 (사용자 플레이가 저장됨). 나머지 3개는 같다. 4개가 같은 내용의 사본인지, 역할이 다른지는 미확인.
- 다시 쓴 뒤에도 파일 날짜가 **2001-02-07 13:45**로 남아 있다. `Hod2.exe`가 `GetFileTime`/`SetFileTime`을 임포트하므로, 쓰기 전 날짜를 되돌려 놓는 것으로 추정한다 (`SetFileTime` 호출은 아직 기록하지 않음).
- 실행 파일 안의 `HOD2 SAVE FILE` 문자열이 이 파일과 관련된 것으로 보인다 (미확인).
- **게임 폴더에 쓰기 권한이 없으면 저장이 안 된다.** 또 이 파일들은 게임 데이터 폴더 안에 있어서, 데이터 폴더를 공유하거나 읽기 전용으로 두면 문제가 된다.

### 읽기만 하는 설정 파일

| 파일 | 위치 | 호출 위치 |
|---|---|---|
| `hglCore.ini` | 작업 폴더 (`GetFullPathNameA`) | `0x4A6093` |
| `Toddebug.ini` (`[TOD] LIFE`, `RELOAD`, 기본값 `"FAILED"`) | 작업 폴더 | `0x40E4EB` |
| `Hod2.ini` (`[Controller]`, `[Flush Setting]`) | 작업 폴더 | `0x41DF88`, `0x49DF2F` |
| `Hod2.exe` 자신 | 작업 폴더 | `0x49DA02` |

- 게임 데이터(`pol/`, `tex/`, `mot/`, `cam/`, `evt/`, `Sound/`)는 상대 경로로 읽는다. 작업 폴더가 게임 폴더여야 한다.

### 그 밖에

- `\\.\SEGAJOY.VXD`를 열려다 실패한다 (호출 위치 `0x41DF23`, 오류 2). 당시 세가 라이트건 드라이버로 보인다 → [platform-input.md](platform-input.md).
- 정상 종료 순서: `WM_CLOSE` → "END" / "Are you sure you want to quit?" 확인창(예/아니요) → 세이브 파일 쓰기 → 종료 코드 0. 이번 시험에서는 `exception.log`가 생기지 않았다.

### 원본 폴더의 `exception.log` (2026-10-14 00:15:40)

- 접근 위반 `c0000005` at `0x4A0899`, `Eax = 0`, `Esi = 0` (null 포인터로 보임), 스택에 `0x4604D5` 등.
- 사용자가 원본과 비교 플레이하던 시점이다. 사용자는 당시 상황(플레이 중/종료할 때)을 기억하지 못한다 (2026-10-15 확인).

## 수정 방향 (G2)

- `Hod2.exe` IAT의 `CreateFileA`를 가로채서, 경로가 아래에 해당하면 사용자 폴더로 바꾼다.
  - 세이브 4개 → `%APPDATA%\thod2-native\save\<원래 상대 경로>` (처음 한 번 게임 폴더에서 복사해 기존 기록을 옮김)
  - `<Windows 폴더>\d3dx.dat` → `%LOCALAPPDATA%\thod2-native\d3dx.dat`
  - `exception.log` → `%LOCALAPPDATA%\thod2-native\exception.log`
- 세이브 파일의 날짜 되돌리기(`SetFileTime`)는 리디렉션된 핸들에 그대로 적용되므로 따로 처리하지 않아도 된다 (확인 필요).
- 우리 로그 `thod2-native.log`도 게임 폴더에 쓸 수 없으면 `%LOCALAPPDATA%\thod2-native\`에 쓴다.

## 사용자 폴더 리디렉션 결과 (2026-10-15)

`[files] redirect_user_files` 패치 (`src/core/patches/file_redirect.cpp`, 기본 켬).

| 게임이 여는 파일 | 실제 위치 |
|---|---|
| 세이브 4개 (`pol/bg_adv19.bin` 등) | `%APPDATA%\thod2-native\save\<같은 상대 경로>` |
| `<Windows>\d3dx.dat` | `%LOCALAPPDATA%\thod2-native\d3dx.dat` |
| `<게임 폴더>\exception.log` | `%LOCALAPPDATA%\thod2-native\exception.log` |
| `thod2-native.log` (우리 로그) | 게임 폴더. 쓸 수 없으면 `%LOCALAPPDATA%\thod2-native\thod2-native.log` |

- 방법: `Hod2.exe` IAT의 `CreateFileA`를 가로채 전체 경로(대소문자 무시, `/`와 `..` 정리)를 비교하고, 해당하면 `CreateFileW`로 바뀐 경로를 연다. 사용자 이름에 ANSI로 표현할 수 없는 문자가 있어도 동작하도록 유니코드 API를 쓴다.
- 처음 켤 때 게임 폴더의 세이브 4개를 사용자 폴더로 복사한다 (`CopyFileW`, 파일 시각 유지). 사용자 폴더에 이미 있으면 복사하지 않는다.

시험 (테스트 사본, `tools/compat/graceful-exit-run.ps1`):

| 시험 | 결과 |
|---|---|
| 처음 실행 | 세이브 4개 복사 (사용자 플레이가 담긴 `pol/tv2.bin` 포함). 시작 때 읽기 4건, 종료 때 쓰기 8건, `d3dx.dat` 7건 모두 사용자 폴더로. 게임 폴더 세이브 해시는 실행 전후 같음 |
| 다시 쓴 세이브의 날짜 | 2001-02-07 13:45 그대로 → 게임이 날짜를 되돌리는 동작(`SetFileTime`으로 추정)이 바뀐 경로에서도 동작 |
| 게임 폴더 세이브·로그를 읽기 전용으로, 리디렉션 켬 | 로그는 `%LOCALAPPDATA%`로, 세이브 읽기·쓰기 성공, 종료 코드 0 |
| 같은 조건, 리디렉션 끔 (원본 동작) | 종료 때 `pol/bg_adv19.bin` `CREATE_ALWAYS`가 **오류 5(접근 거부)로 실패**하고 나머지 쓰기는 하지 않음. 아무 표시 없이 종료 코드 0 → **원본은 조용히 저장에 실패한다** |

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-15 | **세이브는 `%APPDATA%`(로밍), 캐시·크래시 로그는 `%LOCALAPPDATA%`** | 세이브는 사용자 데이터라 로밍 폴더, `d3dx.dat`(장치 캐시)와 `exception.log`는 기기별 데이터라 로컬 폴더. 대안: 게임 폴더 옆 `save\` 폴더(쓰기 권한 문제가 그대로) |
| 2026-10-15 | `redirect_user_files` 기본값 켬, 처음 한 번 게임 폴더 세이브를 복사 | 켜는 순간 기존 기록이 사라지면 안 되므로. 이후에는 사용자 폴더만 쓴다 (게임 폴더 세이브는 그대로 남음) |
| 2026-10-15 | `CreateFileA`를 함께 가로채는 패치는 리디렉션 → 파일 기록 순서로 설치 | 나중에 설치한 후킹이 게임 호출을 먼저 받는다. 파일 기록이 게임의 호출 위치와 원래 경로를 남기도록 기록을 바깥에 둔다 |

## 미해결 질문

- 세이브 4개 파일의 역할 (같은 내용의 사본인지, 항목별로 나뉘는지), 암호화 방식, 언제 쓰는지 (정상 종료 때 말고도 쓰는지)
- `SetFileTime`으로 날짜를 되돌리는지
- 원본 폴더 `exception.log`의 비정상 종료 상황 (`0x4A0899`). 사용자도 당시 상황을 기억하지 못함 (2026-10-15). 재현되면 `%LOCALAPPDATA%	hod2-native\exception.log`에 남으므로 그때 조사
- 원본이 CD를 확인하는 방법 (드라이브 종류 검사, 특정 파일 확인 등)
- 수정 안 된 원본 실행 파일을 지원할 때 CD 확인을 어떻게 처리할지 (`CDCheckPass` 비트를 켜는 방법이 가장 단순)

## 작업 기록

### 2026-10-15 — 사용자 폴더 리디렉션 (관련: G2)

- **한 일:**
  - `src/core/patches/file_redirect.cpp`: 경로 대응 함수 `MapUserFile`과 `CreateFileA` 후킹, 처음 한 번 세이브 복사.
  - `util`: `KnownFolderPath`, `LocalDataDir` 추가. 코어가 게임 폴더에 로그를 만들 수 없으면 `%LOCALAPPDATA%\thod2-native\`에 쓴다.
  - 설정 `[files] redirect_user_files` (기본 1).
  - 테스트: `MapUserFile` 7개 (대소문자, `/`와 `..`, 다른 폴더의 같은 이름 제외 포함), 설정 2개.
- **결과:** 위 "사용자 폴더 리디렉션 결과". Debug, Release 빌드 경고 0개, `ctest` 2개 모두 통과 (core_tests 37개 검사).
  - 처음에는 리디렉션 다음에 파일 기록을 설치해서, 파일 기록의 호출 위치가 우리 DLL 주소로 찍혔다. 설치 순서를 바꿔 해결했다 (결정 사항).
- **다음:** 원본 비정상 종료(`0x4A0899`)는 재현되면 조사.

### 2026-10-15 — 파일 접근 분석 (관련: G1)

- **한 일:**
  - 개발용 패치 `file_trace`(`[debug] trace_files = 1`)를 만들었다. `Hod2.exe` IAT의 `CreateFileA`, `DeleteFileA`, `GetPrivateProfileStringA`, `GetPrivateProfileIntA`, `GetWindowsDirectoryA`, `GetFullPathNameA`를 가로채 경로, 읽기·쓰기 모드, 결과, 호출 위치를 기록한다. 읽기 전용 열기는 폴더별로 처음 3개만 자세히 기록한다.
  - 정상 종료 스크립트 `tools/compat/graceful-exit-run.ps1`을 만들었다 (`WM_CLOSE` → 종료 확인 "예" → 종료 코드와 `exception.log` 확인).
  - 원본·테스트 사본의 세이브 파일 해시, 크기, 날짜, 앞부분 바이트를 비교했다. `VirtualStore`를 확인했다.
- **결과:** 위 "파일 접근 분석 결과". 세이브는 데이터 파일로 위장한 987바이트 파일 4개이고, 정상 종료 때 게임 폴더에 다시 쓴다.
  - 이전까지의 시험 스크립트(`runtest.ps1` 등)는 프로세스를 강제 종료해서 세이브를 쓰지 않았다.
- **다음:** G2 리디렉션 구현을 별도 브랜치에서 한다.

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:** 현재 보유한 `Hod2.exe`는 CD 확인 호출이 NOP으로 지워진 수정본으로 판단된다 (CD 확인 함수 `0x4A5C50`은 호출하는 곳이 없음). 원래 CD 확인은 `GetLogicalDriveStringsA`로 드라이브를 돌면서 `GetDriveTypeA()==5`(CD-ROM)를 찾는다. `hglCore.ini`의 `CDCheckPass=true`로도 통과시킬 수 있다. `hglCore.ini`는 작업 폴더에서 읽는다.
- **상세:** [compat-survey.md](compat-survey.md), [reverse-engineering.md](reverse-engineering.md)
