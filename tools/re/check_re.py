"""Validates the reverse engineering tables in re/.

    python tools/re/check_re.py [re folder]

Checks the CSV format (header, address format, unique addresses, enums), that
every address constant in hod2_addresses.h and every "Reimplements:" marker in
src/ is listed, and reports addresses mentioned in docs/work but not listed.
Exit code is the number of errors.
"""
import csv
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RE_DIR = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "re"
DOCS_DIR = REPO / "docs" / "work"

# Hod2.exe image: base 0x400000, SizeOfImage ends after .rsrc (0x9CB000 + 0x2000).
IMAGE_BEGIN = 0x400000
IMAGE_END = 0x9CD000

HEADERS = {
    "functions.csv": ["address", "name", "status", "confidence", "signature", "evidence", "doc", "notes"],
    "globals.csv": ["address", "name", "type", "confidence", "evidence", "doc", "notes"],
    "locations.csv": ["address", "kind", "confidence", "evidence", "doc", "notes"],
}
STATUSES = {"unnamed", "named", "implemented", "verified"}
CONFIDENCES = {"confirmed", "guess"}
KINDS = {"insn", "ret"}
ADDRESS_RE = re.compile(r"^0x[0-9A-F]{6}$")
NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
EVIDENCE_RE = re.compile(r"^(bytes|pattern|profiler|runtime|exception\.log|ghidra|trace:[a-z_]+)$")

errors = []


def error(where, message):
    errors.append(f"{where}: {message}")


def load(file_name):
    path = RE_DIR / file_name
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header != HEADERS[file_name]:
            error(file_name, f"header must be {','.join(HEADERS[file_name])}")
            return []
        rows = []
        for line_no, values in enumerate(reader, start=2):
            if len(values) != len(header):
                error(f"{file_name}:{line_no}", f"expected {len(header)} fields, got {len(values)}")
                continue
            rows.append((line_no, dict(zip(header, values))))
        return rows


def check_common(file_name, rows):
    seen = {}
    previous = -1
    for line_no, row in rows:
        where = f"{file_name}:{line_no}"
        address = row["address"]
        if not ADDRESS_RE.match(address):
            error(where, f"address {address!r} must look like 0x4A5C50 (upper case, 6 digits)")
            continue
        value = int(address, 16)
        if not IMAGE_BEGIN <= value < IMAGE_END:
            error(where, f"address {address} is outside the Hod2.exe image")
        if address in seen:
            error(where, f"address {address} is already listed on line {seen[address]}")
        seen[address] = line_no
        if value < previous:
            error(where, "rows must be sorted by address")
        previous = value
        if row["confidence"] not in CONFIDENCES:
            error(where, f"confidence must be one of {sorted(CONFIDENCES)}")
        for evidence in filter(None, row["evidence"].split(";")):
            if not EVIDENCE_RE.match(evidence):
                error(where, f"unknown evidence {evidence!r}")
        if not row["evidence"]:
            error(where, "evidence is required")
        for doc in filter(None, row["doc"].split(";")):
            if not (DOCS_DIR / doc).is_file():
                error(where, f"doc {doc!r} not found in docs/work")
        if not row["notes"]:
            error(where, "notes are required")
        if not row["notes"].isascii():
            error(where, "notes must be ASCII (English)")
    return seen


def main():
    tables = {name: load(name) for name in HEADERS}
    addresses = {name: check_common(name, rows) for name, rows in tables.items()}

    functions = {}
    for line_no, row in tables["functions.csv"]:
        where = f"functions.csv:{line_no}"
        name = row["name"]
        if row["status"] not in STATUSES:
            error(where, f"status must be one of {sorted(STATUSES)}")
        if name and not NAME_RE.match(name):
            error(where, f"name {name!r} is not an identifier")
        if not name and row["status"] != "unnamed":
            error(where, "only unnamed functions may have an empty name")
        if name and row["status"] == "unnamed":
            error(where, "a named function cannot have status unnamed")
        if row["signature"] and not re.search(rf"\b{re.escape(name)}\s*\(", row["signature"]):
            error(where, "signature must contain the function name")
        functions[row["address"]] = row

    for line_no, row in tables["globals.csv"]:
        if row["name"] and not NAME_RE.match(row["name"]):
            error(f"globals.csv:{line_no}", f"name {row['name']!r} is not an identifier")

    for line_no, row in tables["locations.csv"]:
        if row["kind"] not in KINDS:
            error(f"locations.csv:{line_no}", f"kind must be one of {sorted(KINDS)}")

    names = {}
    for file_name in ("functions.csv", "globals.csv"):
        for line_no, row in tables[file_name]:
            if row["name"]:
                if row["name"] in names:
                    error(f"{file_name}:{line_no}", f"name {row['name']} is also used at {names[row['name']]}")
                names[row["name"]] = f"{file_name}:{line_no}"

    listed = set().union(*addresses.values())

    header = REPO / "src" / "interop" / "include" / "thod2" / "interop" / "hod2_addresses.h"
    for match in re.finditer(r"constexpr\s+std::uintptr_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", header.read_text(encoding="utf-8")):
        address = f"0x{int(match.group(2), 16):06X}"
        if address not in listed:
            error("hod2_addresses.h", f"{match.group(1)} = {address} is not listed in re/*.csv")

    for path in sorted((REPO / "src").rglob("*.*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in re.finditer(r"//\s*Reimplements:\s*(0x[0-9A-Fa-f]+)\s+(\w+)(?:.*\n\s*//\s*Status:\s*(\w+))?", text):
            address = f"0x{int(match.group(1), 16):06X}"
            where = f"{path.relative_to(REPO).as_posix()}"
            row = functions.get(address)
            if row is None:
                error(where, f"Reimplements: {address} is not listed in re/functions.csv")
                continue
            if row["name"] != match.group(2):
                error(where, f"Reimplements: {address} name {match.group(2)} differs from functions.csv ({row['name']})")
            status = match.group(3)
            if status and row["status"] != status:
                error(where, f"Status {status} differs from functions.csv ({row['status']})")

    # Addresses mentioned in the work docs but not listed are only reported:
    # the docs also mention section bounds, IAT slots and stack values.
    unlisted = set()
    for path in DOCS_DIR.glob("*.md"):
        for match in re.finditer(r"`(0x[0-9A-Fa-f]{6})`", path.read_text(encoding="utf-8")):
            address = f"0x{int(match.group(1), 16):06X}"
            if IMAGE_BEGIN < int(address, 16) < IMAGE_END and address not in listed:
                unlisted.add(address)

    counts = ", ".join(f"{name} {len(rows)}" for name, rows in tables.items())
    print(f"re/: {counts}")
    if unlisted:
        print(f"info: addresses in docs/work not listed in re/: {' '.join(sorted(unlisted))}")
    for message in errors:
        print(f"error: {message}")
    print(f"{len(errors)} error(s)")
    return len(errors)


if __name__ == "__main__":
    sys.exit(main())
