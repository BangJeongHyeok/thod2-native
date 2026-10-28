# 설치 안내

PC판 The House of the Dead 2에 thod2-native와 권장 화면 설정(dgVoodoo2)을 적용하는 방법이다.
게임 파일은 이 프로젝트에 포함되어 있지 않다. 자기가 가진 게임 폴더(`Hod2.exe`가 있는 `THOTD2` 폴더)에 적용한다.

**배포판(`thod2-native.zip`)으로 설치하는 방법은 [README](../README.md)의 "설치"에 있다.** 이 문서는 저장소에서 직접 빌드해서 설치하는 방법과 전체 설정을 다룬다.

## 적용하면 달라지는 것

| 항목 | 원본 | 적용 후 |
|---|---|---|
| 시작 확인창("Attention") | 매번 뜸 | 뜨지 않음 (자동으로 "예") |
| 화면 | 모니터를 640×480으로 바꾸는 전체 화면 | 테두리 없는 전체 화면 크기 창. 모니터 해상도는 그대로 |
| 3D 해상도 | 640×480 | 화면에 맞는 최대 4:3 해상도 (1080p 모니터에서 1440×1080) |
| 테두리 계단 현상 | 있음 | 안티에일리어싱(MSAA 4x)으로 매끄럽게 |
| Alt+Enter | 반응 없음 | 창 ↔ 전체 화면 전환 |
| CPU 사용 | 코어 1개 100% | 약 25% (프레임 끝 바쁜 대기를 잠자기로 바꿈) |
| 마우스 조준 | dgVoodoo2 창(화면 크기)에서는 화면 왼쪽 위 640×480만 조준됨 | 화면에 그려진 게임 영역 전체로 조준 (`[input] map_cursor`) |
| 와이드스크린 (선택) | 4:3 | `[display] widescreen = 1`이면 모니터 비율로 넓게 보임 (아래 "와이드스크린") |
| 세이브 위치 | 게임 폴더의 데이터 파일 4개. 폴더에 쓸 수 없으면 조용히 저장 실패 | `%APPDATA%\thod2-native\save\`. 게임 폴더가 읽기 전용이어도 저장됨. 처음 실행할 때 기존 세이브를 옮겨 옴 |

## 준비물

- 원본 게임 폴더 (`Hod2.exe` 지원 빌드: SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69`)
- thod2-native 빌드 결과물: `dinput.dll`, `thod2_core.dll` ([work/build.md](work/build.md)의 "빌드 방법"대로 빌드한다)
- [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2/releases) 공식 릴리스 (확인한 버전: v2.87.5, `dgVoodoo2_87_5.zip`)
- (선택) Python 3: `tools\dgvoodoo-setup.py`로 dgVoodoo2 파일을 복사할 때. 손으로 복사해도 된다

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

**Python 없이 하려면:** 위 세 파일을 직접 복사하기만 하면 된다. thod2-native가 게임을 켤 때마다 `dgVoodoo.conf`에 권장값을 넣는다 (`[dgvoodoo] preset = 1`, 기본값). `preset` 명령은 필요 없다.

### 화질을 더 바꾸고 싶을 때 (선택)

권장 설정은 원본의 분위기를 유지하면서 테두리만 매끄럽게 한다. 취향에 따라 `thod2-native.ini`의 `[dgvoodoo]`에서 바꿀 수 있다 (thod2-native가 게임을 켤 때 `dgVoodoo.conf`에 넣는다).

| 원하는 것 | `thod2-native.ini` | 비고 |
|---|---|---|
| 테두리를 더 매끄럽게 | `[dgvoodoo] antialiasing = 8x` | 4x와 차이는 작고 그래픽 카드 부담이 조금 늘어난다 |
| 텍스처를 부드럽게 | `[dgvoodoo] filtering = 16` | 원본은 텍스처를 각진 픽셀로 그린다. 부드러워지지만 분위기가 달라진다 |
| 테두리를 가장 매끄럽게 | `[dgvoodoo] antialiasing = 16x` | 그래픽 카드가 지원할 때만. 시험하지 않았다 |
| 안티에일리어싱 끄기 | `[dgvoodoo] antialiasing = off` | 원본과 같은 테두리 |
| dgVoodoo2를 직접 설정 | `[dgvoodoo] preset = 0` | thod2-native가 `dgVoodoo.conf`를 건드리지 않는다 (와이드스크린의 `ScalingMode` 맞춤만 한다). 그다음은 `dgVoodooCpl.exe`나 `python tools\dgvoodoo-setup.py set`으로 바꾼다 |

비교 결과는 [work/renderer.md](work/renderer.md)의 "dgVoodoo2 화질 옵션 비교"에 있다.

## 3. 실행

`Hod2.exe`를 그냥 실행한다. 게임 폴더에 `thod2-native.log`가 생기면 thod2-native가 동작한 것이다.

## 설정 (`thod2-native.ini`)

| 섹션 / 키 | 기본값 | 설명 |
|---|---|---|
| `[core] log_level` | `info` | `debug`, `info`, `warn`, `error` |
| `[core] allow_unknown_exe` | `0` | `1`이면 지원하지 않는 `Hod2.exe`에서도 패치를 적용 (충돌 가능) |
| `[display] skip_device_dialog` | `1` | 시작 확인창을 자동으로 "예" 처리 |
| `[display] widescreen` | `0` | `1`이면 와이드스크린 (아래 "와이드스크린") |
| `[display] widescreen_aspect` | `auto` | 화면 비율. `auto`는 주 모니터 해상도로 계산. `16:9`, `21:9`처럼 직접 줄 수 있음 |
| `[display] widescreen_mirror` | `1` | 와이드스크린에서 전체 화면 2D 그림(타이틀·로딩·결과 화면 배경)의 양옆을 그림 끝을 거울처럼 뒤집어 채움. `0`이면 양옆에 뒤의 3D 장면이 보임 |
| `[display] interpolate` | `0` | `1`이면 고주사율 모니터에서 게임 틱(초당 60회) 사이 프레임을 보간해서 더 그림 (아래 "고주사율") |
| `[display] max_fps` | `0` | `interpolate`와 함께 쓰는 프레임 제한. `0`이면 모니터 주사율(수직 동기화)까지 |
| `[dgvoodoo] preset` | `1` | 게임을 켤 때마다 `dgVoodoo.conf`에 권장 설정을 넣음 (테두리 없는 전체 화면 창, 최대 해상도, Alt+Enter, 워터마크 끔). `0`이면 건드리지 않음 |
| `[dgvoodoo] antialiasing` | `4x` | `preset`과 함께: dgVoodoo2 안티에일리어싱. `off`(끔), `appdriven`(게임에 맡김, 이 게임은 쓰지 않아 끈 것과 같음), `2x`, `4x`, `8x`, `16x`(그래픽 카드가 지원할 때). 값 목록은 dgVoodoo2 2.87.5 `dgVoodoo.conf`의 설명 줄 기준이고, 직접 비교한 것은 `appdriven`·`4x`·`8x`다 |
| `[dgvoodoo] filtering` | (비어 있음) | `preset`과 함께: dgVoodoo2 텍스처 필터. 비어 있으면 바꾸지 않음. `16`이면 부드럽게(비등방성) |
| `[assets] replace_textures` | `1` | `data\tex\` 아래 PNG를 원본 텍스처 대신 씀 (아래 "텍스처 교체"). 폴더가 없으면 아무 일도 하지 않음 |
| `[input] map_cursor` | `1` | 마우스 커서를 dgVoodoo2가 그리는 게임 영역 기준으로 바꿈. `0`이면 원본처럼 창 픽셀 그대로 |
| `[files] redirect_user_files` | `1` | 세이브·`d3dx.dat`·`exception.log`를 사용자 폴더(`%APPDATA%`, `%LOCALAPPDATA%`의 `thod2-native`)에 둠 |
| `[timing] frame_wait_sleep` | `1` | 프레임 끝 대기를 바쁜 대기 대신 잠자기로 (CPU 사용 감소) |
| `[timing] frame_wait_margin_ms` | `1.0` | 마감 직전 이 시간(ms)만큼은 원래처럼 바쁜 대기 |

개발할 때 쓰는 `[debug]` 설정(호출 기록, 프로파일러 등)은 [work/build.md](work/build.md)의 "개발용 설정"에 있다.

## 와이드스크린

`thod2-native.ini`에 넣는다:

```ini
[display]
widescreen = 1
```

- 세로 시야는 그대로 두고 양옆을 더 보여 준다 (Hor+). 화면 비율은 주 모니터에 맞춘다 (`widescreen_aspect`).
- HUD(총알, 생명, 플레이어별 표시)는 화면 양 끝으로 붙고, 타이틀 문구·자막 같은 나머지 2D는 원래 비율로 가운데에 그려진다. 영화 장면의 위아래 띠도 화면 전체 폭을 덮는다.
- 조준은 넓어진 화면 끝까지 커서를 따라간다.
- 타이틀·로딩·결과 화면처럼 화면 전체를 덮는 2D 그림은 4:3이라, 양옆을 그림 끝을 거울처럼 뒤집어 채운다 (`widescreen_mirror`). 16:9까지는 그림 조각 한 줄로 채워진다. 21:9처럼 더 넓으면 조각이 그 폭에 맞게 놓인 그림만 채워지고, 아니면 끝에 빈 곳이 남을 수 있다.
- dgVoodoo2의 `ScalingMode`는 thod2-native가 맞춘다: 켜면 `stretched`, 끄면 `stretched_ar` (`[dgvoodoo] preset = 0`이면 `stretched`일 때만 되돌린다). 따로 고칠 필요 없다.
- 아직 확인하지 않은 것: 화면 양옆에서 나오는 적이 많은 장면에서의 동작, 이번 확인에서 나오지 않은 HUD(점수, 보스 게이지 등). 이런 HUD는 가운데 4:3 영역에 그려진다.
- 자세한 분석은 [work/renderer.md](work/renderer.md)의 "화면 비율 분석".

## 고주사율 (시험 단계)

`thod2-native.ini`에 넣는다:

```ini
[display]
interpolate = 1
```

- 게임 속도는 원래대로 초당 60틱이다. 그 사이에 모니터 주사율만큼 프레임을 더 그리고, 3D 모델과 카메라는 앞뒤 틱 사이 위치로 보간해서 움직임이 부드러워진다. 240Hz 모니터에서 초당 약 240장을 그린다.
- 화면이 보간 때문에 1틱(약 17ms) 늦게 따라간다. 조준점, HUD 같은 2D도 같은 방식으로 보간한다 (2026-10-29).
- 60Hz 모니터에서는 얻는 것이 없고, 시험하지 않았다. 켜지 않는다.
- 문제가 있으면 `interpolate = 0`으로 끈다. 자세한 것은 [work/platform-timing.md](work/platform-timing.md) G4.

## 텍스처 교체 (시험 단계)

게임 폴더에 `data\tex\<뱅크>\` 폴더를 만들고, 바꿀 텍스처를 **16진수 텍스처 번호**를 이름으로 한 PNG로 넣는다. `<뱅크>`는 `tex\` 안의 파일 이름에서 `.bin`을 뺀 것이다 (예: `PRESS START BUTTON`은 `data\tex\scr_common\10B1.png`).

- 텍스처 번호는 뱅크마다 겹친다(예: 챕터마다 같은 번호에 다른 제목). 그래서 뱅크 폴더를 쓴다. `data\tex\` 바로 아래의 `<번호>.png`는 모든 뱅크에 적용된다.
- 텍스처 번호와 원본 이미지는 `python tools\assets\tex_extract.py <Hod2.exe> <게임 폴더>\tex <출력 폴더> scr_common.bin`으로 뽑은 파일 이름의 `_tex<번호>` 부분이다. 원본은 위아래가 바로 된 PNG로 나온다.
- UI 전체를 Real-ESRGAN으로 4배로 만드는 도구가 있다: `tools\assets\upscale-ui.ps1` ([work/assets.md](work/assets.md)). 대상은 `scr_*.bin`, 타이틀 메뉴 `komono_title.bin`, 게임 중 UI·효과 `common.bin`을 `tex_extract.py`로 뽑은 것이다. 결과 폴더가 그대로 `data\tex\` 구조다.
- 이미지는 원본보다 크게 만들어도 된다. **가로세로 비율은 원본과 같게** 한다 (다르면 원본 크기로 늘어남). 크기는 가로·세로 2048 이하 (dgVoodoo2 2.87.5 기준).
- 투명도(알파)를 채널당 8비트로 쓴다.
- 고해상도로 보이려면 dgVoodoo2 `[DirectX] Resolution`이 `max` 같은 높은 값이어야 한다 (권장 프리셋은 이미 `max`).
- 바꾼 텍스처는 `thod2-native.log`에 `replace_textures: texture 10B1 512x32 -> 2048x128` 처럼 남는다.

## 되돌리기

```powershell
.\tools\deploy.ps1 -GameDir "<게임 폴더>" -Remove
python tools\dgvoodoo-setup.py uninstall "<게임 폴더>"
```

`thod2-native.ini`는 남는다. 필요 없으면 직접 지운다.

세이브는 `%APPDATA%\thod2-native\save\`에 남는다. thod2-native 없이 원본으로 돌아가면 게임은 다시 게임 폴더의 세이브를 쓴다. 최신 기록을 이어 쓰려면 `save\` 안의 파일 4개를 게임 폴더의 같은 위치(`pol\`, `tex\`)에 덮어쓴다.

## 문제 해결

문제를 알리는 방법(GitHub 이슈)은 [README](../README.md)의 "문제 신고하기"에 있다.

- **Network 메뉴에 들어가면 DirectPlay 설치 창이 뜬다:** 게임의 네트워크 기능이 Windows의 DirectPlay(기본으로 꺼진 선택적 기능)를 쓴다. 설치하면 Network 화면이 뜬다. 네트워크 기능을 쓰지 않으면 취소해도 된다.

- **thod2-native.log가 생기지 않는다:** `dinput.dll`과 `thod2_core.dll`이 `Hod2.exe`와 같은 폴더에 있는지 확인한다. 게임 폴더에 쓸 수 없으면 로그는 `%LOCALAPPDATA%\thod2-native\thod2-native.log`에 생긴다.
- **로그에 `unknown host executable`:** 지원하지 않는 `Hod2.exe`다. 패치는 적용되지 않고 게임은 원래대로 실행된다.
- **실행할 때 "보안 경고"가 뜬다:** 인터넷에서 받은 파일에 붙는 Windows 표시 때문이다. 파일 속성에서 "차단 해제"를 하면 사라진다.
- **세이브가 저장되지 않는다:** 게임은 **정상 종료할 때**("Are you sure you want to quit?"에서 예) 저장한다. 강제 종료하면 저장되지 않는다. 원본 게임은 세이브를 게임 폴더 안의 데이터 파일 4개(`pol/bg_adv19.bin`, `tex/scr_tod_itm_itamidome2.bin`, `pol/komono_0.bin`, `pol/tv2.bin`)에 쓰고, 폴더에 쓸 수 없으면 아무 표시 없이 실패한다. thod2-native는 이 파일들을 `%APPDATA%\thod2-native\save\`에 쓰므로 게임 폴더 권한과 상관없이 저장된다 (`redirect_user_files = 0`이면 원본처럼 동작).
- **Windows가 자동으로 호환 모드를 건다:** Windows는 `Hod2.exe`를 처음 실행한 뒤 `DWM8And16BitMitigation` 호환 모드를 자동으로 등록하는 경우가 있다. thod2-native와 dgVoodoo2는 이 상태에서 시험했다.

## 왜 dgVoodoo2를 같이 배포하지 않나

dgVoodoo2 저장소에 명시된 라이선스가 없어서 재배포 조건을 확인할 수 없다. 그래서 사용자가 공식 릴리스에서 직접 받고, thod2-native는 설정만 적용한다. 화면 처리는 나중에 자체 렌더러(ROADMAP 2-3)로 대체할 예정이다.
