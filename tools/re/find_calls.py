"""Lists every `call <target>` (E8 rel32) in Hod2.exe .text.

Candidates from a byte scan are kept only when a linear disassembly (capstone)
of the containing function has a call instruction at that address, so no data
or operand bytes are reported.

Usage:
  python tools/re/find_calls.py <Hod2.exe> <funcs.tsv> <target> [<target> ...] [--header NAME=TARGET ...]

<funcs.tsv> is the function list written by tools/re/scope.ps1 -Keep.
With --header, prints C++ arrays in the format of src/core/patches/hud_call_sites.h.
Needs capstone (pip install capstone); run without -I so the user site-packages are found.
"""
import bisect
import csv
import struct
import sys

import capstone


def text_section(exe):
    pe = struct.unpack_from('<I', exe, 0x3C)[0]
    opt = struct.unpack_from('<H', exe, pe + 20)[0]
    _, _, va, raw_size, raw_off = struct.unpack_from('<8sIIII', exe, pe + 24 + opt)
    return 0x400000 + va, exe[raw_off:raw_off + raw_size]


def find_calls(exe_path, funcs_path, targets):
    exe = open(exe_path, 'rb').read()
    base, text = text_section(exe)
    with open(funcs_path, encoding='utf-8', newline='') as f:
        funcs = sorted(((int(r['entry'], 16), int(r['bytes'])) for r in csv.DictReader(f, delimiter='\t')))
    starts = [s for s, _ in funcs]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    result = {t: [] for t in targets}
    for i in range(len(text) - 5):
        if text[i] != 0xE8:
            continue
        address = base + i
        target = (address + 5 + struct.unpack_from('<i', text, i + 1)[0]) & 0xFFFFFFFF
        if target not in result:
            continue
        k = bisect.bisect_right(starts, address) - 1
        if k < 0:
            continue
        start, size = funcs[k]
        if not start <= address < start + size:
            continue
        code = text[start - base:start - base + size]
        if any(ins.address == address and ins.mnemonic == 'call' for ins in md.disasm(code, start)):
            result[target].append((address, start))
    return result


def main():
    args = sys.argv[1:]
    headers = [a for a in args if '=' in a]
    plain = [a for a in args if '=' not in a and a != '--header']
    exe_path, funcs_path = plain[0], plain[1]
    named = {int(v, 16): n for n, v in (h.split('=') for h in headers)}
    targets = [int(t, 16) for t in plain[2:]] + list(named)
    result = find_calls(exe_path, funcs_path, targets)
    if named:
        for target, name in named.items():
            print(f'inline constexpr CallSite {name}[] = {{')
            for call, function in result[target]:
                print(f'    {{0x{call:06X}, 0x{function:06X}}},')
            print('};')
    else:
        for target, sites in result.items():
            print(f'0x{target:06X}: {len(sites)} calls')
            for call, function in sites:
                print(f'  0x{call:06X} in 0x{function:06X}')


if __name__ == '__main__':
    main()
