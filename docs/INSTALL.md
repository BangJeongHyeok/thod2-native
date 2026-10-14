# 설치 안내

PC판 The House of the Dead 2에 thod2-native와 권장 화면 설정(dgVoodoo2)을 적용하는 방법이다.
게임 파일은 이 프로젝트에 포함되어 있지 않다. 자기가 가진 게임 폴더(`Hod2.exe`가 있는 `THOTD2` 폴더)에 적용한다.

## 적용하면 달라지는 것

| 항목 | 원본 | 적용 후 |
|---|---|---|
| 시작 확인창("Attention") | 매번 뜸 | 뜨지 않음 (자동으로 "예") |
| 화면 | 모니터를 640×480으로 바꾸는 전체 화면 | 테두리 없는 전체 화면 크기 창. 모니터 해상도는 그대로 |
| 3D 해상도 | 640×480 | 화면에 맞는 최대 4:3 해상도 (1080p 모니터에서 1440×1080) |
| Alt+Enter | 반응 없음 | 창 ↔ 전체 화면 전환 |
| CPU 사용 | 코어 1개 100% | 약 25% (프레임 끝 바쁜 대기를 잠자기로 바꿈) |
| 세이브 위치 | 게임 폴더의 데이터 파일 4개. 폴더에 쓸 수 없으면 조용히 저장 실패 | `%APPDATA%\thod2-native\save\`. 게임 폴더가 읽기 전용이어도 저장됨. 처음 실행할 때 기존 세이브를 옮겨 옴 |

## 준비물

- 원본 게임 폴더 (`Hod2.exe` 지원 빌드: SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69`)
- thod2-native 빌드 결과물: `dinput.dll`, `thod2_core.dll` (아직 배포판이 없으므로 [work/build.md](work/build.md)의 "빌드 방법"대로 빌드한다)
- [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2/releases) 공식 릴리스 (확인한 버전: v2.87.5, `dgVoodoo2_87_5.zip`)
- Python 3 (dgVoodoo2 설정 도구용. 손으로 설정해도 된다)

원본 게임 폴더를 그대로 두고 싶으면 먼저 복사본을 만들어 거기에 적용한다.

## 1. thod2-native 넣기

저장소 폴더에서 PowerShell로:

```powershell
.\tools\deploy.ps1 -GameDir "<게임 폴더>"
```

게임 폴더에 다음 파일이 생긴다.

| 파일 | 역할 |
|---|---|
| `dinput.dll` | 로더. 게임이 시작될 때 `thod2_core.dll`을 불러온다 |
| `thod2_core.dll` | thod2-native 본체 |
| `thod2-native.ini` | 설정 파일 (이미 있으면 덮어쓰지 않음) |

## 2. dgVoodoo2 넣고 권장 설정 적용

1. dgVoodoo2 zip을 받아 아무 폴더에 푼다.
2. 저장소 폴더에서:

```powershell
python tools\dgvoodoo-setup.py install "<dgVoodoo2를 푼 폴더>" "<게임 폴더>"
python tools\dgvoodoo-setup.py preset "<게임 폴더>"
```

`install`은 `MS\x86\DDraw.dll`, `MS\x86\D3DImm.dll`, `dgVoodoo.conf`를 게임 폴더에 복사한다. `preset`은 [config/dgvoodoo-recommended.ini](../config/dgvoodoo-recommended.ini)의 값을 `dgVoodoo.conf`에 적용한다.

**Python 없이 하려면:** 위 세 파일을 직접 복사하고, 메모장으로 `dgVoodoo.conf`를 열어 `dgvoodoo-recommended.ini`에 적힌 값을 같은 섹션의 같은 키에 써 넣는다. `Resolution`은 `[Glide]`와 `[DirectX]` 두 곳에 있으니 `[DirectX]` 쪽을 고친다.

## 3. 실행

`Hod2.exe`를 그냥 실행한다. 게임 폴더에 `thod2-native.log`가 생기면 thod2-native가 동작한 것이다.

## 설정 (`thod2-native.ini`)

| 섹션 / 키 | 기본값 | 설명 |
|---|---|---|
| `[core] log_level` | `info` | `debug`, `info`, `warn`, `error` |
| `[core] allow_unknown_exe` | `0` | `1`이면 지원하지 않는 `Hod2.exe`에서도 패치를 적용 (충돌 가능) |
| `[display] skip_device_dialog` | `1` | 시작 확인창을 자동으로 "예" 처리 |
| `[files] redirect_user_files` | `1` | 세이브·`d3dx.dat`·`exception.log`를 사용자 폴더(`%APPDATA%`, `%LOCALAPPDATA%`의 `thod2-native`)에 둠 |
| `[timing] frame_wait_sleep` | `1` | 프레임 끝 대기를 바쁜 대기 대신 잠자기로 (CPU 사용 감소) |
| `[timing] frame_wait_margin_ms` | `1.0` | 마감 직전 이 시간(ms)만큼은 원래처럼 바쁜 대기 |

개발할 때 쓰는 `[debug]` 설정(호출 기록, 프로파일러 등)은 [work/build.md](work/build.md)의 "개발용 설정"에 있다.

## 되돌리기

```powershell
.\tools\deploy.ps1 -GameDir "<게임 폴더>" -Remove
python tools\dgvoodoo-setup.py uninstall "<게임 폴더>"
```

`thod2-native.ini`는 남는다. 필요 없으면 직접 지운다.

세이브는 `%APPDATA%\thod2-native\save\`에 남는다. thod2-native 없이 원본으로 돌아가면 게임은 다시 게임 폴더의 세이브를 쓴다. 최신 기록을 이어 쓰려면 `save\` 안의 파일 4개를 게임 폴더의 같은 위치(`pol\`, `tex\`)에 덮어쓴다.

## 문제 해결

- **thod2-native.log가 생기지 않는다:** `dinput.dll`과 `thod2_core.dll`이 `Hod2.exe`와 같은 폴더에 있는지 확인한다. 게임 폴더에 쓸 수 없으면 로그는 `%LOCALAPPDATA%\thod2-native\thod2-native.log`에 생긴다.
- **로그에 `unknown host executable`:** 지원하지 않는 `Hod2.exe`다. 패치는 적용되지 않고 게임은 원래대로 실행된다.
- **실행할 때 "보안 경고"가 뜬다:** 인터넷에서 받은 파일에 붙는 Windows 표시 때문이다. 파일 속성에서 "차단 해제"를 하면 사라진다.
- **세이브가 저장되지 않는다:** 게임은 **정상 종료할 때**("Are you sure you want to quit?"에서 예) 저장한다. 강제 종료하면 저장되지 않는다. 원본 게임은 세이브를 게임 폴더 안의 데이터 파일 4개(`pol/bg_adv19.bin`, `tex/scr_tod_itm_itamidome2.bin`, `pol/komono_0.bin`, `pol/tv2.bin`)에 쓰고, 폴더에 쓸 수 없으면 아무 표시 없이 실패한다. thod2-native는 이 파일들을 `%APPDATA%\thod2-native\save\`에 쓰므로 게임 폴더 권한과 상관없이 저장된다 (`redirect_user_files = 0`이면 원본처럼 동작).
- **Windows가 자동으로 호환 모드를 건다:** Windows는 `Hod2.exe`를 처음 실행한 뒤 `DWM8And16BitMitigation` 호환 모드를 자동으로 등록하는 경우가 있다. thod2-native와 dgVoodoo2는 이 상태에서 시험했다.

## 왜 dgVoodoo2를 같이 배포하지 않나

dgVoodoo2 저장소에 명시된 라이선스가 없어서 재배포 조건을 확인할 수 없다. 그래서 사용자가 공식 릴리스에서 직접 받고, thod2-native는 설정만 적용한다. 화면 처리는 나중에 자체 렌더러(ROADMAP 2-3)로 대체할 예정이다.
