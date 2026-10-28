# thod2-native

### [최신 버전 파일 다운받기 (thod2-native.zip)](https://github.com/BangJeongHyeok/thod2-native/releases/latest/download/thod2-native.zip)
(선택) [고해상도 UI 텍스처 팩 (thod2-native-hd-ui.zip, 약 400MB)](https://github.com/BangJeongHyeok/thod2-native/releases/download/hd-ui-1/thod2-native-hd-ui.zip) · 이전 버전과 변경 내역은 [Releases](https://github.com/BangJeongHyeok/thod2-native/releases)에 있습니다.

PC판 **The House of the Dead 2**(SEGA, 2001)를 현대 Windows에서 잘 돌게 하는 모드입니다.  
원본 게임 위에 얹어서 동작하는 방식이며, **게임 파일은 포함되어 있지 않습니다.**  
본인이 가진 게임에 해당 파일을 덮어씌워 사용합니다.

## 무엇이 달라지나요

| 항목 | 원본 | 적용 후 |
|---|---|---|
| 화면 | 모니터를 640×480으로 바꾸는 전체 화면, 4:3 | 테두리 없는 전체 화면 창. 모니터 해상도와 비율에 맞게 |
| 3D 해상도 | 640×480 | 화면에 맞는 최대 해상도 (1080p 모니터에서 1440×1080), 안티에일리어싱 |
| 고주사율 (선택) | 초당 60장 | 144Hz·240Hz 모니터에서 주사율만큼 부드럽게 (게임 속도는 그대로) |
| 메뉴·HUD·글자 (선택) | 640×480용 저해상도 그림 | 고해상도 UI 텍스처 팩을 넣으면 4배 해상도로 선명하게 |
| CPU 사용 | 코어 1개 100% | 약 25% |
| Alt+Enter | 반응 없음 | 창 ↔ 전체 화면 전환 |
| 마우스 조준 | 창 모드에서 화면 왼쪽 위만 조준됨 | 화면 전체로 조준 |
| 세이브 | 게임 폴더에 쓸 수 없으면 저장 실패 | `%APPDATA%\thod2-native\save\`에 저장 |
| 시작 확인창 | 매번 뜸 | 뜨지 않음 |

## 원본과 비교
![타이틀 비교](docs/images/compare-title.jpg)
![타이틀 메뉴 비교](docs/images/compare-menu.jpg)
![게임 설명 화면 비교](docs/images/compare-tutorial.jpg)
![인트로 컷신 비교](docs/images/compare-cutscene.jpg)
![게임 장면 비교](docs/images/compare-gameplay.jpg)
![데모 장면 비교](docs/images/compare-alley.jpg)

## 준비물

1. **The House of the Dead 2 PC판 게임 폴더**
   - 지금 지원하는 `Hod2.exe`는 한 가지입니다: SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69`
   - 다른 판본이면 게임은 원래대로 실행되고 thod2-native 기능만 켜지지 않습니다. 이때는 아래 "문제 신고하기"로 알려 주세요. 지원 판본을 늘리는 데 쓰입니다.
2. [**dgVoodoo2** 공식 릴리스](https://github.com/dege-diosg/dgVoodoo2/releases) (확인한 버전: 2.87.5)
   - 오래된 DirectX 7 게임을 현대 그래픽 카드에서 돌려 주는 도구입니다.  
   라이선스 문제로 해당 프로젝트에 포함하지 않으니, 따로 다운로드가 필요합니다.

## 설치

1. 위의 **최신 버전 받기**로 `thod2-native.zip`을 받아, 안의 파일을 모두 게임 폴더(`Hod2.exe` 옆)에 풀어 넣습니다.
2. dgVoodoo2 zip을 받아, 안의 파일 3개를 게임 폴더에 복사합니다.
   - `MS\x86\DDraw.dll`
   - `MS\x86\D3DImm.dll`
   - `dgVoodoo.conf`
3. `Hod2.exe`를 실행합니다. 게임 폴더에 `thod2-native.log`가 생기면 동작한 것입니다.

### 고해상도 UI 텍스처 적용 (선택)

메뉴, HUD, 안내 글자, 자막 같은 2D 그림을 4배 해상도로 바꿉니다. 위 비교 사진의 오른쪽이 이 팩을 넣은 모습입니다.

1. 맨 위의 [**고해상도 UI 텍스처 팩**](https://github.com/BangJeongHyeok/thod2-native/releases/download/hd-ui-1/thod2-native-hd-ui.zip)(`thod2-native-hd-ui.zip`)을 받습니다.
2. 게임 폴더(`Hod2.exe`가 있는 곳)에 압축을 풉니다. 아래처럼 `data\tex` 폴더가 `Hod2.exe` 옆에 오면 됩니다.
   Windows의 "압축 풀기"는 기본으로 zip 이름의 새 폴더를 만드니, 풀 위치를 게임 폴더로 바꿔 주세요.
   ```text
   THOTD2\
     Hod2.exe
     thod2_core.dll
     data\
       tex\
         common\
         scr_common\
         ... (폴더 119개, 그림 3,110장)
   ```
3. 게임을 켜면 바로 적용됩니다. 따로 바꿀 설정은 없습니다.

- thod2-native를 설치한 상태에서만 동작합니다. 원본 게임만으로는 이 폴더를 읽지 않습니다.
- 적용됐는지는 `thod2-native.log`에서 볼 수 있습니다. `replace_textures: texture 10B1 512x32 -> 2048x128`처럼 바뀐 텍스처가 한 줄씩 남습니다.
- 원래 그림으로 돌아가려면 `data` 폴더를 지우거나, `thod2-native.ini`의 `[assets]` 아래 `replace_textures = 1`을 `0`으로 바꿉니다.
- thod2-native를 업데이트해도 텍스처 팩은 다시 받을 필요가 없습니다.

## 설정

게임 폴더의 `thod2-native.ini`를 메모장으로 열어 바꾸고 저장한 뒤 게임을 다시 켭니다. 자주 쓰는 것:

| 하고 싶은 것 | 바꿀 줄 |
|---|---|
| 와이드스크린 | `[display]` 아래 `widescreen = 1` |
| 고주사율 모니터에서 부드럽게 | `[display]` 아래 `interpolate = 1` (60Hz 모니터에서는 켜지 마세요) |
| 안티에일리어싱 / 끄기 | `[dgvoodoo]` 아래 `antialiasing = 8x` / `appdriven` |
| 텍스처를 부드럽게 | `[dgvoodoo]` 아래 `filtering = 16` |
| dgVoodoo2를 직접 설정 | `[dgvoodoo]` 아래 `preset = 0` |

모든 설정은 [docs/INSTALL.md](docs/INSTALL.md)의 "설정"에 있습니다.

## 업데이트와 되돌리기

- **업데이트:** 새 `thod2-native.zip`을 같은 방법으로 풀어 넣습니다. `thod2-native.ini`를 덮어쓸지 물으면, 바꾼 설정을 지키려면 "아니요"를 고릅니다.
- **되돌리기:** 게임 폴더에서 `dinput.dll`, `thod2_core.dll`, `DDraw.dll`, `D3DImm.dll`(텍스처 팩을 넣었다면 `data` 폴더도)을 지웁니다. 세이브는 `%APPDATA%\thod2-native\save\`에 남습니다. 원본으로 이어서 하려면 그 안의 파일을 게임 폴더의 같은 위치에 덮어씁니다.

## 아직 확인하지 못한 것

아래는 개발 중에 직접 확인하지 못한 부분입니다. 이상한 점을 보시면 꼭 알려 주세요.

- 게임을 처음부터 끝까지 이어서 플레이하는 것 (특히 보스전, 스테이지 결과 화면)
- 개발한 PC가 아닌 다른 PC, 다른 그래픽 카드, Windows 화면 배율(125%, 150% 등)
- 60Hz·144Hz 모니터에서의 고주사율 설정 (240Hz에서만 확인)
- 와이드스크린에서 적이 화면 양옆에서 많이 나오는 장면, 점수·보스 게이지 같은 일부 HUD 위치
- 라이트건, 게임패드

## 문제 신고하기

실행 문제 혹은 건의사항은 GitHub의 **이슈(Issue)**로 알려 주세요.
   - [**문제 신고**](https://github.com/BangJeongHyeok/thod2-native/issues/new?template=bug_report.yml): 실행이 안 되거나, 화면·조작이 이상하거나, 게임이 꺼질 때
   - [**내 Hod2.exe가 지원되지 않음**](https://github.com/BangJeongHyeok/thod2-native/issues/new?template=unsupported_exe.yml): 로그에 `unknown host executable`이 나올 때
**게임을 끈 뒤 게임 폴더의 `thod2-native.log` 파일을 "thod2-native.log" 칸에 끌어다 놓아** 주세요. 화면 캡처도 끌어다 놓으면 올라갑니다.  
**게임 파일(exe, bin 등)은 올리지 마세요.**

## 개발

- 프로젝트 목표와 단계: [docs/ROADMAP.md](docs/ROADMAP.md)
- 빌드 방법과 개발용 설정: [docs/work/build.md](docs/work/build.md)
- 분석 결과와 작업 기록: [docs/work/](docs/work/README.md), [AGENTS.md](AGENTS.md)
- 직접 빌드해서 설치하는 방법: [docs/INSTALL.md](docs/INSTALL.md)

## 라이선스

[CC BY-NC-SA 4.0](LICENSE) (저작자 표시 - 비영리 - 동일조건 변경허락).

- 고치고 다시 배포해도 됩니다. 이 프로젝트를 출처로 밝히고, 같은 라이선스로 공개해야 합니다.
- **상업적으로 이용할 수 없습니다.** 이 프로젝트도 원작을 바탕으로 한 2차 창작물이기 때문입니다.

The House of the Dead는 SEGA의 상표이며, 이 프로젝트는 SEGA와 관련이 없습니다.
