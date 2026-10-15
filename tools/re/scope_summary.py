"""Summarizes the TSV files written by tools/re/scope.ps1 per code region.

Usage: python tools/re/scope_summary.py <funcs.tsv> <decomp.tsv>

The region boundaries are inferred from imports and strings, see
docs/work/reverse-engineering.md "리버싱 규모 산정".
"""
import csv
import sys

# (start, end, name). Half-open VA ranges in .text.
REGIONS = [
    (0x401000, 0x49D8B0, "game"),
    (0x49D8B0, 0x4A6380, "hgl-platform"),
    (0x4A6380, 0x4AB280, "hgl-math"),
    (0x4AB280, 0x4B6800, "crt"),
    (0x4B6800, 0x4C20A0, "d3dx"),
    (0x4C20A0, 0x4C3000, "hic-exception"),
]

SIZE_BUCKETS = [0, 16, 64, 256, 1024, 4096, 1 << 30]


def region_of(addr):
    for lo, hi, name in REGIONS:
        if lo <= addr < hi:
            return name
    return "other"


def read_tsv(path):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def main():
    funcs = read_tsv(sys.argv[1])
    decomp = {r["entry"]: r for r in read_tsv(sys.argv[2])}

    print(f"functions={len(funcs)} bytes={sum(int(r['bytes']) for r in funcs)} "
          f"insns={sum(int(r['insns']) for r in funcs)}")
    print()
    print(f"{'region':14} {'funcs':>6} {'bytes':>8} {'insns':>7} {'float%':>6} "
          f"{'no_caller':>9} {'dec_lines':>9} {'warn_fn':>7} {'cast_ln':>7}")
    names = [name for _, _, name in REGIONS] + ["other"]
    for name in names:
        rows = [r for r in funcs if region_of(int(r["entry"], 16)) == name]
        if not rows:
            continue
        insns = sum(int(r["insns"]) for r in rows)
        flt = sum(int(r["float_insns"]) for r in rows)
        dec = [decomp[r["entry"]] for r in rows if r["entry"] in decomp]
        print(f"{name:14} {len(rows):6} {sum(int(r['bytes']) for r in rows):8} {insns:7} "
              f"{100 * flt / max(1, insns):6.0f} {sum(r['callers'] == '0' for r in rows):9} "
              f"{sum(int(d['lines']) for d in dec):9} {sum(int(d['warnings']) > 0 for d in dec):7} "
              f"{sum(int(d['casts']) for d in dec):7}")
    print()
    print("function size (bytes):")
    for lo, hi in zip(SIZE_BUCKETS, SIZE_BUCKETS[1:]):
        rows = [r for r in funcs if lo <= int(r["bytes"]) < hi]
        print(f"  {lo:>5}-{hi if hi < (1 << 30) else '':<6} n={len(rows):5} "
              f"bytes={sum(int(r['bytes']) for r in rows):7}")
    failed = sum(r["ok"] == "0" for r in decomp.values())
    print(f"decompile failures: {failed}")


if __name__ == "__main__":
    main()
