# 플랫폼: 메인 루프·타이밍

- **상태:** 시작 전
- **관련 로드맵:** ROADMAP 1-3, 2-2
- **관련 코드:** `src/platform/`
- **마지막 갱신:** 2026-10-14

## 주요 목표

모니터 주사율과 관계없이 게임 로직이 원본 속도로 돌게 한다. 최종적으로 메인 루프를 재구현해서 로직 업데이트(60Hz 고정 스텝으로 추정)와 렌더링을 분리한다. 이것이 고프레임 렌더 보간의 기반이다.

**완료 기준:** 60Hz, 144Hz, 240Hz 모니터와 vsync 켬/끔 모든 조합에서 게임 속도가 원본과 같다.

## 중목표 / 소목표

<!-- 작업하면서 채운다. 양식은 _TEMPLATE.md 참고 -->

아직 없음.

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|

## 미해결 질문

- 게임 로직이 프레임 단위인지 시간 단위인지
- 원본이 타이밍에 실제로 쓰는 API. 임포트에는 `QueryPerformanceCounter`, `GetTickCount`가 있고 `timeGetTime`(winmm)은 없다. vsync 대기 여부는 미확인
- 로직과 렌더링이 한 함수 안에 섞여 있는지
- CPU 100% 루프가 vsync를 기다리는지, 타이머로 바쁜 대기를 하는지

## 작업 기록

### 2026-10-14 — 화면 모드 분석에서 넘어온 내용

- **결과:** 게임은 매 프레임 primary 버퍼에 `Flip(NULL, DDFLIP_WAIT)`를 부른다 (호출 위치 `0x4B9896`). 60Hz 모니터에서 **초당 59.7~59.8회**로, 프레임 속도가 수직 동기화(vsync)에 묶여 있다. 이 상태에서도 CPU 코어 1개를 100% 쓴다.
  - **가설:** 게임 속도가 프레임 수에 묶여 있다면, 120Hz 이상 모니터에서 `Flip`이 더 자주 끝나서 게임이 빨라질 수 있다. 고주사율 환경에서 확인 필요 (G1-5, compat-survey).
- **상세:** [platform-display.md](platform-display.md) "화면 모드 분석 결과"

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:** 실행 중 CPU 코어 1개를 100% 사용하고, 최소화하면 1%로 떨어진다. 대기 없이 도는 루프로 추정된다. 임포트에 `QueryPerformanceCounter`, `QueryPerformanceFrequency`, `GetTickCount`, `MsgWaitForMultipleObjects`가 있다. 고주사율 모니터에서 속도는 아직 확인하지 못했다 (테스트 PC가 60Hz).
- **상세:** [compat-survey.md](compat-survey.md), [reverse-engineering.md](reverse-engineering.md)
