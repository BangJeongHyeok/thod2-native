# re/ — 리버스 엔지니어링 기록

원본 `Hod2.exe`(지원 빌드 SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69`)를 분석한 결과를 텍스트로 남긴다. **이 폴더의 CSV가 분석 결과의 기준 데이터다.** Ghidra 프로젝트는 여기서 다시 만들 수 있는 사본이다. 원본 코드가 들어가는 Ghidra 프로젝트 파일과 디컴파일한 코드는 커밋하지 않는다.

진행 기록과 근거 설명은 [../docs/work/reverse-engineering.md](../docs/work/reverse-engineering.md)에 있다.

## 파일

| 파일 | 내용 |
|---|---|
| `functions.csv` | 함수 시작 주소, 이름, 진행 상태, 시그니처 |
| `globals.csv` | 전역 변수(데이터) 주소, 이름, 타입 |
| `locations.csv` | 코드 위치: API 호출 위치(복귀 주소), 플래그를 읽는 명령, 패치 흔적, 패치하는 즉시값 |
| `ghidra_scripts/` | Ghidra 스크립트: `ApplyThod2Re.java`(CSV → Ghidra), `ExportThod2Re.java`(Ghidra → CSV), `Thod2ReCsv.java`(공용 도우미), `Scope*.java`(규모 측정. `tools/re/scope.ps1`이 프로젝트 임시 사본에서 실행) |

## 형식

- UTF-8, 쉼표 구분, 첫 줄은 열 이름. 쉼표나 따옴표가 들어간 값만 큰따옴표로 감싼다 (RFC 4180, Python `csv` 기본 형식).
- 행은 주소 순서로 정렬한다. 한 파일 안에서 주소는 한 번만 나온다.
- **`address`:** VA, `0x` + 대문자 16진수 6자리 (`0x4A5C50`). 이미지 범위는 `0x400000`~`0x9CCFFF`.
- **`name`:** C 식별자. 함수는 PascalCase(`CheckCdRom`), 전역 변수는 `g_` + camelCase(`g_hWnd`). 두 파일 통틀어 같은 이름은 한 번만 쓴다. 의미를 모르면 비워 둔다 (Ghidra 기본 이름 `FUN_...`이 그대로 남는다).
- **`confidence`:** `confirmed`(동작을 확인함) 또는 `guess`(추정). 이름이 아니라 **설명한 동작**에 대한 확신이다.
- **`evidence`:** 근거. `;`로 여러 개를 쓴다.

  | 값 | 뜻 |
  |---|---|
  | `bytes` | 바이트를 직접 해석함 |
  | `pattern` | 바이트 패턴 검색으로 찾음 |
  | `ghidra` | Ghidra 분석 결과 (자동 분석, 디컴파일) |
  | `profiler` | 개발용 샘플링 프로파일러 |
  | `runtime` | 게임을 실행해서 동작을 확인함 |
  | `trace:<도구>` | thod2-native 기록 기능의 실행 기록 (`trace:dinput_probe`, `trace:ddraw_trace`, `trace:file_trace`) |
  | `exception.log` | 원본 게임이 남긴 오류 기록 |

- **`doc`:** 근거를 설명한 `docs/work/` 문서 파일 이름. `;`로 여러 개를 쓸 수 있다.
- **`notes`:** **영어**로 쓴 짧은 설명 (Ghidra 주석으로 들어가서 외부 기여자도 읽는다. 코드·주석은 영어라는 규칙과 같음). 비워 둘 수 없다.

### `functions.csv`

`address,name,status,confidence,signature,evidence,doc,notes`

- **`status`:** 로드맵의 함수 진행 상태.

  | 값 | 뜻 |
  |---|---|
  | `unnamed` | 미확인. 함수 시작은 알지만 이름이 없다 |
  | `named` | 이름 붙임 |
  | `implemented` | 재구현됨. 우리 코드로 교체했고 게임이 돌아간다 |
  | `verified` | 검증됨. 원본과 결과가 같은지 확인했다 |

  `implemented`·`verified`는 코드의 `// Reimplements: <주소> <이름>` / `// Status:` 주석([../docs/CONVENTIONS.md](../docs/CONVENTIONS.md) "재구현 함수 표시")과 맞아야 한다.
- **`signature`:** 선택. Ghidra가 읽을 수 있는 C 원형 (`void __cdecl FrameWaitCallback(float remaining_ms)`). 이름이 `name`과 같아야 한다.

### `globals.csv`

`address,name,type,confidence,evidence,doc,notes`

- **`type`:** 선택. Ghidra 데이터 타입 이름 (`float`, `int`, `uint`, `HWND`, `void *`, `char *`, `char[256]`). 구조체를 모르면 비워 둔다.

### `locations.csv`

`address,kind,confidence,evidence,doc,notes`

- **`kind`:** `ret` 또는 `insn`.
  - `ret`: `address`는 **복귀 주소**(호출 명령 바로 다음 주소)다. 기록 기능이 남기는 주소가 이것이다. 주석은 바로 앞의 call 명령에 단다. 바로 앞 명령이 call이 아니면 적용 스크립트가 경고한다.
  - `insn`: `address`가 들어 있는 명령에 주석을 단다. 명령 중간 주소(즉시값 위치 등)도 된다.

## 작업 흐름

명령은 저장소 폴더의 PowerShell에서 실행한다. 도구 설치는 [../docs/work/reverse-engineering.md](../docs/work/reverse-engineering.md) "분석 도구"를 본다.

```powershell
# 처음 한 번: Ghidra 프로젝트 만들기 (임포트 + 자동 분석 + CSV 적용, 약 80초)
.\tools\re\sync.ps1 -Import ..\test-game\THOTD2\Hod2.exe

# CSV를 고치거나 다른 사람의 변경을 받은 뒤: Ghidra 프로젝트에 적용
.\tools\re\sync.ps1 -Apply

# Ghidra GUI에서 이름을 붙인 뒤: CSV로 내보내기
.\tools\re\sync.ps1 -Export

# 형식 검사 (ctest의 re_tables 테스트와 같음)
python tools/re/check_re.py
```

- Ghidra 프로젝트 기본 위치는 `..\ghidra-projects\hod2.gpr`(저장소 밖)다. `-ProjectDir`, `-ProjectName`으로 바꿀 수 있다.
- **헤드리스 명령은 Ghidra GUI에서 프로젝트를 닫은 뒤에 실행한다** (프로젝트 잠금).
- 적용 스크립트는 지원 빌드가 아니면(SHA-256이 다르면) 아무것도 바꾸지 않는다. 여러 번 실행해도 결과가 같다.
- `-Export`는 Ghidra에서 사용자가 붙인 함수 이름과, 코드 밖의 사용자 라벨만 가져온다. 새 행은 `status=named`, `confidence=guess`, `evidence=ghidra`로 들어가고 `notes`가 비어 있다. `notes`와 `doc`을 손으로 채워야 검사를 통과한다. 기존 행은 지우지 않고 이름만 바꾼다. `locations.csv`와 주석은 내보내지 않는다.
- GUI에서는 Script Manager의 스크립트 폴더에 `re\ghidra_scripts`를 추가하면 `thod2` 분류에서 같은 스크립트를 실행할 수 있다.

### 적용 스크립트가 관리하는 주석

적용 스크립트가 쓰는 주석은 `[thod2]`로 시작하고, 실행할 때마다 **덮어쓴다.**

| 대상 | 주석 칸 |
|---|---|
| `functions.csv`의 함수 | 함수 주석 (Plate) |
| `globals.csv`의 주소 | EOL 주석 |
| `locations.csv`가 가리키는 명령 | EOL 주석 (같은 명령을 가리키는 행은 한 주석으로 합침) |

자기 메모는 Pre/Post 주석에 쓴다. 공유할 내용은 CSV의 `notes`에 쓴다.
