# 자산(리소스)

- **상태:** 진행 중
- **관련 로드맵:** ROADMAP 2-4
- **관련 코드:** `src/assets/`, `tools/assets/`, `docs/formats/`
- **마지막 갱신:** 2026-10-15

## 주요 목표

게임 데이터 파일(`pol`, `tex`, `mot`, `cam`, `coli`, `evt`의 `.bin`, `sound`의 `.wav`) 포맷을 분석해서 문서화하고, 우리 로더로 읽는다. 같은 경로의 파일을 `data/`나 `mods/<이름>/data/`에 두면 원본 대신 로드하는 데이터 덮어쓰기 기능(RVGL 방식)을 제공한다. 추출·변환 도구를 만든다.

**완료 기준:** 모든 자산을 우리 로더로 읽고, 각 포맷이 `docs/formats/`에 문서화되어 있으며, 텍스처 하나를 교체하는 예제 모드가 동작한다.

## 중목표 / 소목표

### G1. UI 스프라이트 고해상도 교체

현재 집중 2순위 (ROADMAP "현재 집중"). HUD, 메뉴, 타이틀의 2D 스프라이트를 더 큰 이미지로 바꿔서 고해상도 화면에서 선명하게 보이게 한다.

- [x] G1-1. 텍스처 뱅크 포맷 분석 (LZSS, 실행 파일 안의 항목 표, PVR 픽셀, 팔레트) → [docs/formats/tex.md](../formats/tex.md)
- [x] G1-2. 추출 도구 `tools/assets/tex_extract.py` (UI 뱅크 `scr_common.bin`, `scr_title.bin` 165개 전부 PNG로 추출)
- [x] G1-3. 교체 가능성 분석 (그리기 크기와 UV가 텍스처 실제 크기와 무관함을 확인)
- [x] G1-4. 실행 중 교체: `CreateTextureFromPvr` 호출을 가로채서 교체 이미지가 있으면 큰 32비트 표면을 만든다 (`src/core/patches/texture_replace.cpp`, 시험 이미지 7개로 확인)
- [x] G1-5. 교체 이미지 폴더 규칙 (`<게임 폴더>\data\tex\<16진수 텍스처 번호>.png`)과 PNG 읽기 (Windows 내장 WIC, 외부 라이브러리 없음)
- [ ] G1-7. 타이틀 메뉴 글자(`Network`/`Arcade mode`/`Original mode`)가 어느 텍스처인지 찾기. `scr_title`의 `Arcade mode`(`DF9`)를 바꿔도 메뉴 화면은 그대로였다
- [ ] G1-6. 고해상도 이미지 만들기 방법 정하기 (원본 업스케일 / 다시 그리기). 결과물은 저장소에 넣지 않는다

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|
| 2026-10-15 | 교체는 텍스처를 **만드는 곳**(`CreateTextureFromPvr`)에서 한다. `g_textureSizes`의 `{w, h}`는 원본 값을 그대로 둔다 | 2D 사각형 크기는 `g_textureSizes`로 정해지고 UV는 항상 0~1이라, 텍스처만 크게 만들면 화면 위치·크기는 그대로이고 해상도만 오른다. 그리기 함수(`DrawScreenQuad`)를 바꿀 필요가 없다 |
| 2026-10-15 | 교체 파일 이름은 **텍스처 번호**(16진수)로 한다 (`data\tex\10B1.png`) | 텍스처 번호는 게임 전체에서 하나뿐이고 `CreateTextureFromPvr`가 받는 항목에 들어 있다. 뱅크 이름은 이 함수에서 알 수 없다. 추출 도구의 파일 이름 끝 `_tex<번호>`와 같다 |
| 2026-10-15 | PNG는 Windows 내장 WIC로 읽는다 | 외부 라이브러리(libpng, stb_image 등)를 받지 않아도 된다. PNG 말고 BMP 등도 읽힌다 |
| 2026-10-15 | 교체 텍스처는 `g_textureFormats` 7번 슬롯(A8R8G8B8)으로 만들고, 없으면 6번(A4R4G4B4) | 원본 16비트 형식은 고해상도 그림의 색과 투명도 경계를 망가뜨린다 |
| 2026-10-15 | 추출 결과(PNG)는 저장소 밖 `E:\Repos\hod2-native\screenshots\tex\`에 둔다 | 게임 자산이라 커밋하지 않는다 |

## 미해결 질문

- 같은 텍스처 번호를 다른 팔레트로 만드는 경우가 있는지 (있다면 교체 파일 이름에 팔레트를 넣어야 함)
- `0x41CCE0`의 다른 뱅크 읽기 경로가 무엇인지
- VQ 배치(`0x03`) 풀기 (UI 뱅크에는 없음)
- `tex/scr_blood_*.bin` 피 색상 교체가 어떤 원리인지 (같은 뱅크 번호의 파일을 바꿔 끼우는 것으로 추정)

- 3D 모델 텍스처(`LoadModelTextures` 경로)도 같은 방식으로 바뀌는지 (훅은 걸려 있지만 시험하지 않음)
- 2048보다 큰 이미지를 넣으면 `ClampTextureSize`가 줄이는지, 실패하는지

## 작업 기록

### 2026-10-15 — 실행 중 텍스처 교체 시험 (G1-4, G1-5)

- **한 일:** `[assets] replace_textures` (기본 켬)를 만들었다. 시작할 때 `<게임 폴더>\data\tex\`의 `<16진수 번호>.png` 목록을 읽고, `CreateTextureFromPvr`의 호출 3곳(`0x4ABFD1`, `0x4ABE1D`, `0x4ABE42`)을 우리 함수로 돌린다. 목록에 있는 번호면 WIC로 PNG를 읽어 `CreateTextureSurface(w, h, 7)`로 표면을 만들고, Lock해서 **줄 순서를 뒤집어** 채운 다음 `UploadTextureSurface`에 넘긴다. `g_textureSizes`는 원본 크기를 그대로 둔다. 실패하면 원본 함수로 만든다.
- **실행 환경:** 테스트 사본, dgVoodoo2 2.87.5 (`[DirectX] Resolution = max`, `ScalingMode = stretched`), `[display] widescreen = 1`, 1920×1080 주 모니터.
- **결과:**
  - **`g_textureFormats` (실행 중 값):** 0 비어 있음, 1 팔레트 8비트, 2 RGB565, 3 X1R5G5B5, 4 비어 있음, 5 A1R5G5B5, 6 A4R4G4B4, **7 A8R8G8B8** (마스크 `00FF0000 0000FF00 000000FF FF000000`). 알파 있는 32비트 형식이 바로 쓸 수 있다.
  - **최대 텍스처 크기:** 2048×2048 (`g_textureMaxWidth` / `g_textureMaxHeight`).
  - 시험 이미지 7개(원본 글자를 다른 글꼴·노란색으로 4배 크기로 다시 그린 것, `tools/assets/make-test-textures.ps1`로 생성)가 모두 32비트로 만들어졌다: `BC3` 32×32 → 128×128, `10B0` 128×16 → 512×64, `10B1` 512×32 → 2048×128, `11E2` 128×64 → 512×256, `11E4` 256×64 → 1024×256, `78B` 512×32 → 2048×128, `DF9` 256×32 → 1024×128.
  - **화면 확인 (데모 화면 캡처):** `RELOAD`, `SHOOT OUTSIDE OF THE SCREEN!`, `1P`, `PRESS START BUTTON`이 교체 이미지로 그려졌다. 위치·크기·방향이 원본과 같고, 원본(128×64 텍스처의 계단 현상)보다 선명하다. 캡처는 `E:\Repos\hod2-native\screenshots\texrep3\` (비교: `compare_reload.png`, 교체 전: `texrep2\`).
  - 타이틀 메뉴의 `Arcade mode`는 바뀌지 않았다. 메뉴 화면은 `DF9`가 아닌 다른 텍스처를 쓴다 (G1-7).
  - 같은 뱅크를 다시 읽으면 (타이틀로 돌아올 때) 교체도 다시 된다 (로그에 `78B`, `DF9` 두 번).
  - 교체 한 건에 약 3 ms (로그 시각 기준, 2048×128 PNG).
- **테스트 사본에 남긴 것:** `data\tex\`에 시험 이미지 7개. 원본 글자로 되돌리려면 이 폴더를 지우거나 `replace_textures = 0`.
- **다음:** 고해상도 이미지를 어떻게 만들지 정하기 (G1-6), 메뉴 텍스처 찾기 (G1-7).

### 2026-10-15 — 텍스처 뱅크 분석과 UI 스프라이트 추출 (G1-1 ~ G1-3)

- **한 일:** 텍스처를 읽는 코드를 Ghidra로 따라가서 포맷을 정리하고 ([tex.md](../formats/tex.md)), 같은 방식으로 푸는 `tools/assets/tex_extract.py`를 만들었다. `hotd2-decomp`는 함수 역할을 찾는 힌트로만 봤고, 모든 값은 우리 실행 파일과 실제 파일로 확인했다.
- **결과:**
  - `scr_common.bin` (뱅크 `0x147`): 풀린 크기 171,776 / 171,776바이트, 텍스처 76/76개. 숫자, `CONTINUE?`, `GAME OVER`, `PRESS START BUTTON`, `RELOAD`, 총알, 조준점, 목숨 아이콘, `1P PAUSE` 등.
  - `scr_title.bin` (뱅크 `0x16D`): 1,482,752 / 1,482,752바이트, 89/89개. 타이틀 로고, 메뉴 글자(`Arcade mode`, `Original mode`, `Training mode`, `Boss mode`, `Rankings`, `Options`), 배경 조각, 저작권 표시.
  - 처음에는 이미지가 위아래로 뒤집혀 나왔다. 줄이 아래에서 위로 저장되어 있어서, 쓰는 순서를 뒤집어 고쳤다.
  - 4비트 팔레트 텍스처도 색이 맞게 나온다 (숫자 흰색, 조준점 빨강·파랑, `OBTAINED` 노랑 바탕).
  - **교체 가능성:** `SubmitSprite`(`0x4AC140`)가 UV를 항상 (0, 0)–(1, 1)로 넣고 (`g_constOne` = 1.0), `DrawScreenQuad`는 사각형 크기를 `g_textureSizes`에서 구한다 (`0x4C33AC` = 0.5를 곱함). 그래서 같은 텍스처 번호에 더 큰 텍스처를 만들어 넣으면 화면 위치·크기는 그대로이고 해상도만 오른다. 원본 경로의 크기 제한(`ClampTextureSize`)은 `SmallTexure`를 켤 때만 256/128이고, 기본은 장치 최대값이다.
  - 32비트 텍스처 형식 슬롯(7번)이 이미 있어서 (`EnumTextureFormat`이 채움) 교체 이미지는 채널당 8비트로 넣을 수 있다. 원본은 모두 16비트다.
  - 함수 13개, 전역 12개를 `re/`에 추가했다.
- **실행 확인은 아직 안 했다.** 위 결론은 코드 분석 기준이다.
- **다음:** G1-4 실행 중 교체를 시험 구현해서 한 텍스처(예: `PRESS START BUTTON`)를 2배로 바꿔 보고 화면에서 확인한다.
