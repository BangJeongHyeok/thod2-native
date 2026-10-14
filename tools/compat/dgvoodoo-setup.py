"""Install dgVoodoo2 into a game folder and edit dgVoodoo.conf values.

Usage:
  python dgvoodoo-setup.py install   <extracted dgVoodoo folder> <game folder>
  python dgvoodoo-setup.py set       <game folder> General.FullScreenMode=false DirectX.Resolution=2x
  python dgvoodoo-setup.py uninstall <game folder>

install copies DDraw.dll and D3DImm.dll from MS/x86 plus the default dgVoodoo.conf.
set changes existing keys in dgVoodoo.conf, given as "Section.Key=Value".
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
