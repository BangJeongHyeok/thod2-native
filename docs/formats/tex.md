# tex/*.bin 텍스처 뱅크

- **확인 기준:** `Hod2.exe` (2001-03-22 빌드). 주소는 VA.
- **확인 방법:** Ghidra 디컴파일로 읽는 코드를 찾고, `tools/assets/tex_extract.py`로 같은 방식으로 풀어서 결과를 비교했다 (2026-10-15).
- **추출 도구:** `python -I tools/assets/tex_extract.py <Hod2.exe> <tex 폴더> <출력 폴더> [뱅크 파일명 ...]`. 출력은 게임 자산이므로 저장소 밖에 둔다.

## 개요

`tex/` 아래 `.bin` 파일 하나가 텍스처 **뱅크** 하나다. 파일에는 압축된 픽셀 데이터만 있다. 텍스처 목록(크기, 형식, 위치, 텍스처 번호)은 **실행 파일 안의 표**에 있다. 그래서 뱅크 파일만으로는 텍스처를 나눌 수 없고, 같은 빌드의 `Hod2.exe`가 필요하다.

| 단계 | 함수 | 하는 일 |
|---|---|---|
| 1 | `LoadTextureBank` (`0x41CE70`) | `tex\<이름>`을 통째로 읽는다. 이름은 `g_texBankNames[bank]` |
| 2 | `DecompressLzss` (`0x40ACF0`) | 앞 4바이트(풀린 크기) 뒤를 푼다 |
| 3 | `LoadBankTexture` (`0x4ABF70`) | 표의 항목마다 텍스처를 만든다 (텍스처 번호당 한 번) |
| 4 | `CreateTextureFromPvr` (`0x4AB690`) | 항목 크기의 표면을 만들고 PVR 픽셀을 변환해 넣는다 |
| 5 | `UploadTextureSurface` (`0x4A5430`) | 비디오 메모리 텍스처를 만들고 복사한다 |

## 파일

| 위치 | 크기 | 내용 |
|---|---|---|
| `+0` | uint32 | 풀린 데이터 크기 |
| `+4` | — | LZSS 압축 데이터 |

### LZSS (`DecompressLzss`)

플래그 비트를 바이트 단위로 읽고, 한 바이트 안에서는 **낮은 비트부터** 쓴다. 플래그 바이트는 필요할 때 데이터 흐름에서 바로 읽는다.

| 플래그 | 뒤따르는 데이터 | 동작 |
|---|---|---|
| `1` | 바이트 1개 | 그 바이트를 그대로 쓴다 |
| `0`, `1` | 16비트 LE 값 `w` | `w == 0`이면 끝. 거리 = `(w >> 3) - 0x2000`. 길이 = `(w & 7) + 2`, 단 `w & 7 == 0`이면 다음 바이트 + 1 |
| `0`, `0` | 플래그 2비트 `n` (먼저 읽은 비트가 상위), 바이트 `b` | 거리 = `b - 0x100`, 길이 = `n + 2` |

거리는 음수이고, 이미 쓴 출력의 끝에서부터 센다. 복사는 한 바이트씩 해서 겹쳐도 된다.

## 실행 파일 안의 표

| 주소 | 형식 | 내용 |
|---|---|---|
| `0x4D0410` | `char *[]` | 뱅크 번호 → 파일 이름. 0번은 비어 있고 443개 |
| `0x55A9B8` | `entry *[]` | 뱅크 번호 → 항목 표 |
| `0x55B0A8` | `int16[]` | 뱅크 번호 → 항목 수 |

항목 (16바이트):

| 위치 | 형식 | 내용 |
|---|---|---|
| `+0` | int16 | 너비 |
| `+2` | int16 | 높이 |
| `+4` | uint32 | PVR 형식 (아래) |
| `+8` | uint32 | 풀린 데이터 안의 픽셀 위치 |
| `+12` | int32 | 텍스처 번호 (게임 전체에서 쓰는 번호) |

예: `scr_common.bin`은 뱅크 `0x147`, 텍스처 76개, 풀린 크기 171,776바이트. `scr_title.bin`은 뱅크 `0x16D`, 89개, 1,482,752바이트.

## 픽셀 형식

드림캐스트 PVR 형식 그대로다.

- `fmt & 7`: 픽셀 형식. `0` ARGB1555, `1` RGB565, `2` ARGB4444.
- `(fmt >> 8) & 0xFF`: 배치.

| 배치 | 뜻 | 처리 |
|---|---|---|
| `0x01` | 트위들 (정사각형) | 16비트 픽셀 |
| `0x03` | VQ | 앞 `0x800`바이트가 코드북(2×2 픽셀 묶음 256개, 묶음 안 순서는 트위들: (0,0), (0,1), (1,0), (1,1)), 그 뒤가 묶음마다 1바이트 번호(묶음 좌표로 트위들). 배경·사진류 |
| `0x05` | 4비트 팔레트, 트위들 | 아래 "팔레트" |
| `0x09` | 직사각형 (선형) | 16비트 픽셀, 줄 순서 그대로 |
| `0x0D` | 트위들 직사각형 | 짧은 변 크기의 정사각형 블록을 이어 붙인 것 |

- **트위들 순서:** 위치 번호의 짝수 비트가 y, 홀수 비트가 x다. `CreateTextureFromPvr`는 이 번호를 `(i + 0x55556) & 0xAAAAA`, `(i + 0x2AAAB) & 0x55555`로 늘려 간다.
- **줄 순서:** 줄이 **아래에서 위로** 저장되어 있다. 이미지로 볼 때는 위아래를 뒤집어야 한다.
- UI 뱅크에서 본 형식: `0x500`/`0x502` (4비트 팔레트), `0x902` (직사각형 ARGB4444), `0xD00`/`0xD02` (트위들 직사각형).

### 팔레트

- `InitTextureSystem` (`0x418220`)이 `SetTexturePalettes` (`0x4AC0C0`)로 `g_paletteTable` (`0x579020`)의 16색 팔레트를 `g_texturePalettes` (`0x80E480`)에 복사한다. 표는 `+2`부터 12바이트 간격이고, 항목마다 int16 개수(16)와 uint32 색 배열 주소가 있다.
- 텍스처마다 쓸 팔레트는 `GetTexturePalette(bank, index)` (`0x41CAB0`)가 정한다. 뱅크에 따라 텍스처별 int16 표(예: `scr_common` → `0x579534`)를 보거나, 상수를 돌려주거나, 텍스처 번호 범위로 고른다 (`scr_ranking`, `scr_vmc`, `scr_endstaff`, `scr_hinichi`). 해당하지 않는 뱅크는 팔레트 0이다. 뱅크별 값은 `tools/assets/tex_extract.py`의 `palette_index()`에 그대로 옮겨 두었다.
- **팔레트 색은 텍스처의 픽셀 형식과 상관없이 항상 ARGB1555다.** `CreateTextureFromPvr`가 표면 형식에 맞게 바꾼다 (A4R4G4B4면 알파 1비트를 0/15로, 색은 상위 4비트). 그래서 4비트 텍스처의 투명도는 켜짐/꺼짐 두 단계뿐이다.
- 팔레트 색은 텍스처를 **만들 때** 적용된다. 만든 뒤 팔레트를 바꿔도 이미 만든 텍스처는 그대로다.

## 그릴 때 쓰는 값

- `LoadBankTexture`는 텍스처 번호마다 핸들(`g_textureHandles`, `0x80EC8C`), 크기 `{w, h}` (`g_textureSizes`, `0x80EC80`), 참조 수(`g_textureRefCounts`, `0x80EC88`)를 저장한다. 이미 만든 번호면 참조 수만 늘린다.
- 2D 스프라이트(`DrawSprite2D` → `SubmitSprite` → `DrawScreenQuad`)는 **사각형 크기를 `g_textureSizes`에서** 구하고 (`w × 배율 × 0.5`), **UV는 항상 0~1**이다 (`SubmitSprite` `0x4AC140`이 0, 0, 1, 1을 넣고, 뒤집기는 `1 - u`). 실제 D3D 텍스처 크기는 그리기에 쓰이지 않는다.
- 스프라이트 번호 → 텍스처 번호는 `g_spriteTextures` (`0x57C458`).

## 텍스처 표면 만들기

- `CreateTextureFromPvr`는 `CreateTextureSurface(w, h, slot)` (`0x4A53B0`)로 시스템 메모리 표면을 만든다. `slot`은 `g_pvrPixelFormatSlots[fmt & 7]` (`0x570250`, 값 `{5, 2, 6, 8, 8, 8, 7, 0}`, 8은 지원 안 함)이다.
- 슬롯은 `EnumTextureFormat` (`0x4A4FE0`)이 장치에서 찾은 형식(`g_textureFormats`, `0x7DCE50`, 32바이트씩)이다: 0 팔레트 4비트, 1 팔레트 8비트, 2 RGB565, 3 X1R5G5B5, 4 24비트, 5 A1R5G5B5, 6 A4R4G4B4, 7 32비트(마스크 `ff0000`/`ff00`/`ff`). 원본 텍스처는 모두 16비트 슬롯을 쓴다.
- `UploadTextureSurface`는 `ClampTextureSize` (`0x4A5170`)로 비디오 텍스처 크기를 정한다: `hglCore.ini` `SmallTexure`에 따라 256 또는 128로 제한하고, 장치가 요구하면 2의 거듭제곱·정사각형으로 맞추고, 장치 최소·최대 크기로 자른다. 그 다음 시스템 표면을 Blt로 복사한다 (크기가 다르면 늘이거나 줄인다).

실행 중에 읽은 값 (dgVoodoo2 2.87.5, 2026-10-15): 슬롯 0·4는 비어 있고, 7번은 A8R8G8B8 (마스크 `00FF0000 0000FF00 000000FF FF000000`). 최대 텍스처 크기(`0x7DC94C`, `0x7DC950`)는 2048×2048.

## 교체

thod2-native의 `[assets] replace_textures`는 `CreateTextureFromPvr` 호출을 가로채서, 항목의 텍스처 번호에 맞는 PNG가 있으면 그 크기로 7번 슬롯 표면을 만들고 줄을 뒤집어 채운 뒤 `UploadTextureSurface`에 넘긴다. `g_textureSizes`는 원본 크기로 남으므로 화면 위치·크기는 그대로다 ([assets.md](../work/assets.md) G1-4).

## 호출 위치

| 위치 | 호출 |
|---|---|
| `0x4ABFD1` | `LoadBankTexture` → `CreateTextureFromPvr` |
| `0x4ABE1D`, `0x4ABE42` | `LoadModelTextures` (`0x4ABDA0`) → `CreateTextureFromPvr` |
| `0x41CF83` | `LoadTextureBank` → `LoadBankTexture` |
| `0x41CD22` | `0x41CCE0` → `LoadBankTexture` (다른 뱅크 읽기 경로, 미분석) |
| `0x4AB6D7` | `CreateTextureFromPvr` → `CreateTextureSurface` |
| `0x4ABC89` | `CreateTextureFromPvr` → `UploadTextureSurface` |
