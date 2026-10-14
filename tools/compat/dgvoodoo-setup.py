"""dgVoodoo2를 테스트 게임 폴더에 설치하고 dgVoodoo.conf 값을 바꾼다.

사용 예:
  python dgvoodoo-setup.py install  <dgVoodoo 압축 해제 폴더> <게임 폴더>
  python dgvoodoo-setup.py set      <게임 폴더> General.FullScreenMode=false DirectX.Resolution=2x
  python dgvoodoo-setup.py uninstall <게임 폴더>

install은 MS/x86의 DDraw.dll, D3DImm.dll과 기본 dgVoodoo.conf를 복사한다.
set은 "섹션.키=값" 형식으로 dgVoodoo.conf의 기존 키 값을 바꾼다.
"""
import io
import os
import re
import shutil
import sys

FILES = ["DDraw.dll", "D3DImm.dll"]


def install(src, game):
    for f in FILES:
        shutil.copy2(os.path.join(src, "MS", "x86", f), os.path.join(game, f))
    shutil.copy2(os.path.join(src, "dgVoodoo.conf"), os.path.join(game, "dgVoodoo.conf"))
    print("installed:", FILES + ["dgVoodoo.conf"])


def uninstall(game):
    for f in FILES + ["dgVoodoo.conf"]:
        p = os.path.join(game, f)
        if os.path.exists(p):
            os.remove(p)
    print("removed:", FILES + ["dgVoodoo.conf"])


def set_values(game, pairs):
    path = os.path.join(game, "dgVoodoo.conf")
    lines = io.open(path, encoding="latin-1").read().split("\n")
    for pair in pairs:
        key, value = pair.split("=", 1)
        section, name = key.split(".", 1)
        current = None
        done = False
        for i, line in enumerate(lines):
            m = re.match(r"^\[(.+)\]", line)
            if m:
                current = m.group(1)
                continue
            if current == section and re.match(r"^%s\s*=" % re.escape(name), line):
                lines[i] = re.sub(r"=.*", "= " + value, line, count=1)
                done = True
                break
        if not done:
            sys.exit("key not found: %s" % key)
        print("set [%s] %s = %s" % (section, name, value))
    io.open(path, "w", encoding="latin-1", newline="").write("\n".join(lines))


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "install":
        install(sys.argv[2], sys.argv[3])
    elif cmd == "uninstall":
        uninstall(sys.argv[2])
    elif cmd == "set":
        set_values(sys.argv[2], sys.argv[3:])
    else:
        sys.exit(__doc__)
