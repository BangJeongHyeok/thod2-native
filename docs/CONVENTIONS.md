# 코딩 규칙

이 저장소에서 코드를 작성할 때 지키는 규칙이다. 문서 작성 규칙은 [../AGENTS.md](../AGENTS.md)를 본다.

## 빌드

- CMake + MSVC, **x86(Win32) 전용**. 원본 `Hod2.exe`가 32비트라서 같은 프로세스에 들어가는 코드는 모두 x86이어야 한다. 최상위 `CMakeLists.txt`가 x86이 아니면 구성 단계에서 멈춘다.
- C++20. 경고 수준 `/W4`, `/permissive-`, 소스는 UTF-8(`/utf-8`).
- CRT는 정적 링크(`/MT`, `/MTd`). 게임 폴더에 VC++ 재배포 패키지가 필요 없게 한다.
- 빌드 결과물: DLL과 EXE는 `build/x86/bin/<Config>/`, 정적 라이브러리는 `build/x86/lib/<Config>/`.
- 빌드 방법은 [work/build.md](work/build.md)를 본다.

## 언어

- 문서(`docs/`, `AGENTS.md`)는 한국어.
- **코드, 주석, 식별자, 커밋 메시지, 로그 메시지는 영어.** 외부 기여자가 읽을 수 있게 한다.
- PowerShell 스크립트는 ASCII만 쓴다. Windows PowerShell 5.1은 BOM 없는 UTF-8을 시스템 코드 페이지로 읽어서 한글이 깨진다.

## 이름

| 대상 | 규칙 | 예 |
|---|---|---|
| 네임스페이스 | `thod2::<서브시스템>` | `thod2::core` |
| 타입, 함수 | PascalCase | `LoadConfig`, `CoreConfig` |
| 변수, 멤버 | snake_case, 멤버는 끝에 `_` | `log_level`, `hash_` |
| 상수 | `k` + PascalCase | `kKnownExes` |
| 전역 변수 | `g_` + snake_case, 익명 네임스페이스 안 | `g_initialized` |
| 공개 C API | `Thod2<서브시스템>_<동작>`, `__cdecl` | `Thod2Core_Initialize` |
| 파일 | snake_case | `exe_id.cpp` |

## 원본 주소 표기

- 원본 `Hod2.exe`의 주소는 **VA, 16진수 대문자**로 쓴다: `0x4A5C50`. 이미지 베이스가 `0x400000`으로 고정이고 재배치 정보가 없어서 VA가 항상 같다.
- 코드에서 원본 주소를 쓸 때는 (로더 단계에서 만들 `src/interop/`에) 이름 붙인 상수로 한 곳에 모으고, 근거를 주석으로 남긴다. 같은 주소를 `docs/work/reverse-engineering.md`의 "확인된 주소" 표에도 적는다.

  ```cpp
  // CheckCdRom: no callers in the supported build (call removed at 0x4A5899).
  // See docs/work/reverse-engineering.md.
  constexpr uintptr_t kCheckCdRom = 0x4A5C50;
  ```

- 원본 주소에 의존하는 코드는 `FindKnownExe`로 지원 빌드인지 확인된 뒤에만 실행한다.

## 재구현 함수 표시

원본 함수를 우리 코드로 교체할 때는 함수 바로 위에 아래 형식의 주석을 단다. 나중에 `grep "Reimplements:"`로 진행 상황을 모을 수 있게 한다.

```cpp
// Reimplements: 0x4A5C50 CheckCdRom
// Status: verified  (implemented | verified)
// Notes: always reports success; the supported build never calls the original.
bool CheckCdRom();
```

- `implemented`: 우리 코드로 교체했고 게임이 돌아간다.
- `verified`: 원본과 결과가 같은지 확인했다 (확인 방법을 Notes나 작업 문서에 적는다).
- 같은 내용을 `re/functions.csv`의 상태 열에도 반영한다 (리버싱 인프라를 만들 때 형식 확정).

## 구조

- 서브시스템마다 내부 구현은 정적 라이브러리(`<이름>_lib`)로 만들고, DLL은 그 위에 공개 API만 얹는다. 테스트는 정적 라이브러리를 직접 링크한다.
- 공개 헤더는 `src/<서브시스템>/include/thod2/`에 둔다. 내부 헤더는 서브시스템 폴더 바로 아래에 둔다.
- `DllMain`에서는 모듈 핸들 저장 같은 최소한의 일만 한다. 파일 읽기, 해시 계산, 후킹 설치는 로더 락 밖에서 호출되는 초기화 함수에서 한다.

## 로그

- `thod2::core::Log(LogLevel, format, args...)`를 쓴다 (`std::format` 문법).
- 로그 파일은 `thod2_core.dll` 옆의 `thod2-native.log`. 실행할 때마다 새로 쓴다.
- 줄마다 바로 `WriteFile`로 쓰므로 게임이 비정상 종료해도 그 직전까지 남는다.

## 테스트

- `tests/`에 둔다. 외부 프레임워크 없이 `Check(condition, "설명")`으로 쓰고, 종료 코드가 실패 개수다.
- 게임 파일이 필요한 테스트는 저장소 테스트에 넣지 않는다 (CI에서 게임 파일을 쓸 수 없음). 게임으로 하는 확인은 `tools/`의 스크립트와 작업 문서로 남긴다.

## 서드파티

- `third_party/`에 둔다. 넣기 전에 라이선스를 확인하고, 받기 전에 사용자 확인을 받는다 (AGENTS.md 규칙 6).
