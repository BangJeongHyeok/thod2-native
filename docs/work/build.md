# 빌드·배포

- **상태:** 시작 전
- **관련 로드맵:** ROADMAP 0
- **관련 코드:** `CMakeLists.txt`, `tools/`
- **마지막 갱신:** 2026-10-14

## 주요 목표

누구나 같은 절차로 x86(Win32) 빌드를 만들고, 테스트용 게임 폴더에 배포해서 바로 실행해 볼 수 있는 빌드 체계를 만든다. 이후 오픈 소스 기여자도 같은 절차로 빌드할 수 있어야 한다.

**완료 기준:** 깨끗한 개발 환경에서 문서에 적힌 명령만으로 빌드와 테스트 폴더 배포가 끝난다.

## 중목표 / 소목표

<!-- 작업하면서 채운다. 양식은 _TEMPLATE.md 참고 -->

아직 없음.

## 개발 환경 (현재 PC, 2026-10-14 확인)

| 도구 | 버전 / 위치 | 비고 |
|---|---|---|
| Visual Studio | Community 2022 17.14.41 | |
| MSVC | 14.44.35207 | x86 컴파일러(`bin\Hostx64\x86\cl.exe`) 있음 |
| Windows SDK | 10.0.22621.0, 10.0.26100.0 | 사용자가 VS Installer로 추가 설치 |
| CMake | 3.31.6-msvc6 | VS 내장 (`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`). **PATH에 없음** → "Developer PowerShell for VS 2022"에서 쓰거나 전체 경로로 호출 |
| Ninja | 1.12.1 | VS 내장 (`Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`) |
| Git | 2.55.0.windows.5 | |
| Python | 3.14.7 | 조사 스크립트용 |
| Ghidra, x64dbg | 없음 | 리버싱 단계에서 설치 (사용자 확인 후) |

## 결정 사항

| 날짜 | 결정 | 이유 / 대안 |
|---|---|---|

## 미해결 질문

- 빌드 도구 버전 (Visual Studio / MSVC 버전, CMake 최소 버전)
- CI를 언제 도입할지 (ROADMAP 4단계와 연계)

## 작업 기록

### 2026-10-14 — 개발 환경 확인

- **한 일:** 설치된 빌드 도구를 확인했다. 사용자가 Windows SDK와 "C++ CMake tools for Windows"를 추가 설치했다.
- **결과:** 위 "개발 환경" 표. 테스트용 게임 사본은 `E:\Repos\hod2-native\test-game\THOTD2`에 `robocopy original-game\THOTD2 test-game\THOTD2 /E`로 만들었다 (파일 2004개, 542,336,749 bytes).
- **다음:** 0단계 빌드 골격 (CMake x86 DLL 프로젝트).
