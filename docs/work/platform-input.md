# 플랫폼: 입력

- **상태:** 시작 전
- **관련 로드맵:** ROADMAP 1-3, 2-2
- **관련 코드:** `src/platform/`
- **마지막 갱신:** 2026-10-14

## 주요 목표

마우스, 게임패드, 라이트건 입력을 현대 API로 처리한다. 최종적으로 DirectInput 사용을 우리 입력 계층으로 교체하고, 현대 라이트건(Sinden, GUN4IR 등)과 2인 플레이를 지원할 수 있는 구조를 만든다.

**완료 기준:** 원본이 DirectInput을 직접 쓰지 않고, 마우스 조준이 어떤 해상도·화면 비율에서도 정확하며, 2인 입력 장치를 설정할 수 있다.

## 중목표 / 소목표

<!-- 작업하면서 채운다. 양식은 _TEMPLATE.md 참고 -->

아직 없음.

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|

## 미해결 질문

- `Hod2.ini`의 `PLAYER*_INDEX`, `PLAYER*_DEVID` 의미
- 마우스 좌표가 조준점으로 변환되는 위치
- 라이트건이 원본에서 어떤 장치로 인식되는지
- "MC PC Light Gun"이 어떤 장치인지, `DeviceIoControl`로 무엇을 주고받는지

## 작업 기록

### 2026-10-14 — 호환성 조사에서 넘어온 내용

- **결과:** `Config.exe`(MFC 앱 `Hod2_Tool`)는 Player1/Player2 컨트롤러 선택, "MC PC Light Gun" 속성, 화면 밝기, 번쩍임 시간·색상을 설정해서 `Hod2.ini`에 저장한다. `Hod2.exe`와 `Config.exe` 둘 다 `DeviceIoControl`을 임포트한다 (당시 전용 라이트건 드라이버와 통신한 것으로 추정). 사용자 플레이 결과 마우스 조준은 정상이다.
- **상세:** [compat-survey.md](compat-survey.md), [reverse-engineering.md](reverse-engineering.md)
- **추가:** DirectInput 장치 `SetCooperativeLevel(g_hWnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND)` 호출이 `0x41EBA3`에 있다. 장치 배열 `0x7DAE18`에서 꺼내므로 컨트롤러·라이트건 장치로 추정된다.
