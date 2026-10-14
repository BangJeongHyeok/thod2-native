"""Install dgVoodoo2 into a game folder and edit dgVoodoo.conf values.

dgVoodoo2 is not shipped with thod2-native. Download it from the official
releases (https://github.com/dege-diosg/dgVoodoo2/releases) and extract it first.

Usage:
  python dgvoodoo-setup.py install   <extracted dgVoodoo folder> <game folder>
  python dgvoodoo-setup.py preset    <game folder> [preset file]
  python dgvoodoo-setup.py set       <game folder> General.FullScreenMode=false DirectX.Resolution=2x
  python dgvoodoo-setup.py uninstall <game folder>

install   copies DDraw.dll and D3DImm.dll from MS/x86 plus the default dgVoodoo.conf.
preset    applies a preset (default: config/dgvoodoo-recommended.ini) to dgVoodoo.conf.
set       changes existing keys in dgVoodoo.conf, given as "Section.Key=Value".
uninstall removes the three files again.
"""
import io
import os
import re
import shutil
import sys

FILES = ["DDraw.dll", "D3DImm.dll"]
DEFAULT_PRESET = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config",
                              "dgvoodoo-recommended.ini")


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
    lines = io.open(path, encoding="latin-1", newline="").read().split("\n")
    for pair in pairs:
        key, value = pair.split("=", 1)
        section, name = key.strip().split(".", 1)
        value = value.strip()
        current = None
        done = False
        for i, line in enumerate(lines):
            m = re.match(r"^\[(.+)\]", line)
            if m:
                current = m.group(1)
                continue
            if current == section and re.match(r"^%s\s*=" % re.escape(name), line):
                # Keep the alignment before '=' and the line's own ending ('\r' if any).
                ending = "\r" if line.endswith("\r") else ""
                lines[i] = line[: line.index("=")] + "= " + value + ending
                done = True
                break
        if not done:
            sys.exit("key not found: %s" % key)
        print("set [%s] %s = %s" % (section, name, value))
    io.open(path, "w", encoding="latin-1", newline="").write("\n".join(lines))


def read_preset(path):
    """Reads "[Section]" / "Key = Value" lines; ';' starts a comment line."""
    pairs = []
    section = None
    for raw in io.open(path, encoding="utf-8"):
        line = raw.strip()
        if not line or line.startswith(";"):
            continue
        m = re.match(r"^\[(.+)\]$", line)
        if m:
            section = m.group(1)
            continue
        if "=" not in line or section is None:
            sys.exit("bad preset line: %s" % line)
        key, value = line.split("=", 1)
        pairs.append("%s.%s=%s" % (section, key.strip(), value.strip()))
    return pairs


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "install":
        install(sys.argv[2], sys.argv[3])
    elif cmd == "preset":
        set_values(sys.argv[2], read_preset(sys.argv[3] if len(sys.argv) > 3 else DEFAULT_PRESET))
    elif cmd == "uninstall":
        uninstall(sys.argv[2])
    elif cmd == "set":
        set_values(sys.argv[2], sys.argv[3:])
    else:
        sys.exit(__doc__)
