thod2-native - PC판 The House of the Dead 2를 현대 Windows에서 잘 돌게 하는 모드
https://github.com/BangJeongHyeok/thod2-native

[설치]
1. 이 zip 안의 파일을 모두 게임 폴더(Hod2.exe가 있는 THOTD2 폴더)에 풀어 넣습니다.
2. dgVoodoo2 공식 릴리스(https://github.com/dege-diosg/dgVoodoo2/releases)에서
   dgVoodoo2 zip을 받아, 그 안의 다음 3개 파일을 게임 폴더에 복사합니다.
     MS\x86\DDraw.dll
     MS\x86\D3DImm.dll
     dgVoodoo.conf
   dgVoodoo2 설정은 thod2-native가 게임을 켤 때마다 알아서 맞춥니다.
3. Hod2.exe를 실행합니다. 게임 폴더에 thod2-native.log가 생기면 동작한 것입니다.

[설정]
thod2-native.ini를 메모장으로 열어 바꿉니다.
  와이드스크린:  [display] widescreen = 1
  고주사율 모니터: [display] interpolate = 1

[되돌리기]
dinput.dll, thod2_core.dll, DDraw.dll, D3DImm.dll을 게임 폴더에서 지웁니다.
세이브는 %APPDATA%\thod2-native\save\ 에 남습니다.

[문제가 생기면]
위 주소의 README에 있는 "문제 신고하기" 안내대로 이슈를 올려 주세요.
thod2-native.log 파일을 꼭 같이 올려 주세요.

라이선스: CC BY-NC-SA 4.0 (thod2-native-LICENSE.txt). 비상업적 이용만 허용됩니다.
게임 파일은 포함되어 있지 않습니다. The House of the Dead는 SEGA의 상표입니다.
