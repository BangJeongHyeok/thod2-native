# 렌더러

- **상태:** 시작 전
- **관련 로드맵:** ROADMAP 1-3, 2-3
- **관련 코드:** `src/renderer/`
- **마지막 갱신:** 2026-10-14

## 주요 목표

DX7(DirectDraw7 + Direct3D7)을 쓰지 않는 자체 렌더러를 만든다. DX7 API 수준이 아니라 게임의 그리기 함수 수준에서 가로채서 렌더 인터페이스로 넘기고, 백엔드는 인터페이스 뒤에 둔다 (1차: D3D11). 이 결과로 임의 해상도, 와이드스크린, 안티에일리어싱, 고프레임 보간 렌더링을 지원한다. 초기 호환성 확보용으로 기존 래퍼를 임시로 쓸 수 있다.

**완료 기준:** 원본이 DX7을 전혀 쓰지 않고, 원본과 같은 화면이 나오며, 해상도와 화면 비율을 자유롭게 설정할 수 있다.

## 중목표 / 소목표

<!-- 작업하면서 채운다. 양식은 _TEMPLATE.md 참고 -->

아직 없음.

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|

## 미해결 질문

- 원본의 그리기 경로 구조 (정적 링크된 D3DX `CD3duContext` 사용 범위)
- HUD/2D 스프라이트를 그리는 방식
- 사격 시 화면 번쩍임(`Flush Setting`) 구현 방식
- 1차 백엔드를 D3D11로 확정할지

## 작업 기록

### 2026-10-14 — 화면 모드 분석에서 넘어온 내용

- **결과:**
  - 렌더 구성: `SetDisplayMode(640, 480, 16bpp)`, primary 플립 체인(백버퍼 1장, `3DDEVICE`), 16비트 Z 버퍼 640×480, 16비트 텍스처 약 700개(일부 알파 포함). 매 프레임 `Flip(DDFLIP_WAIT)`.
  - Direct3D는 `IDirectDraw7::QueryInterface(IID_IDirect3D7)`로 얻는다 (호출 위치 `0x4BD2F3`).
  - Windows 호환 모드(`DWM8And16BitMitigation`, `apphelp.dll`)가 DirectDraw 함수 표를 덮어쓴다. DX7 수준에서 후킹하는 코드는 이것을 고려해야 한다.
- **상세:** [platform-display.md](platform-display.md) "화면 모드 분석 결과"

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:**
  - Windows 11은 DX7을 `DDRAW.dll` → `D3DIM700.DLL` → `dxgi.dll` + 그래픽 드라이버 순서로 처리한다.
  - dgVoodoo2 v2.87.5(DX7 → D3D11 래퍼)를 게임 폴더에 넣으면 원본 수정 없이 **테두리 없는 창 + 1440×1080(4:3) 렌더링**이 가능하다. 3D와 HUD 모두 선명해진다. 1단계의 임시 렌더링 해결책으로 쓸 수 있다.
  - dgVoodoo2 기본 설정은 다중 모니터에서 보조 모니터를 전체 화면으로 잡는다. `AppControlledScreenMode=false`로 게임의 화면 모드 요청을 무시해야 창 모드가 된다.
- **상세:** [compat-survey.md](compat-survey.md) "dgVoodoo2 시험 결과"
