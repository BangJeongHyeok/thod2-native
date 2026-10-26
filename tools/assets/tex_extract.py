"""Extracts the textures of tex/*.bin texture banks to PNG files.

How the game reads them (docs/formats/tex.md):
  - the bank file is LZSS compressed (0x40ACF0); its first uint32 is the
    unpacked size;
  - the texture list of bank `i` is not in the file but in Hod2.exe: a pointer
    per bank at 0x55A9B8 and a count at 0x55B0A8; each entry is 16 bytes
    (int16 width, int16 height, uint32 PVR format, uint32 data offset, int32
    texture id);
  - pixel data is Dreamcast PVR (twiddled, rectangle, 4-bit palette, VQ);
    4-bit palette colours are always ARGB1555;
  - rows are stored bottom-up;
  - 4-bit textures take one of the 16-colour palettes copied from the table at
    0x579020 (0x418220 / 0x4AC0C0); the palette per texture comes from
    0x41CAB0 (palette_index()).

Usage:
  python tools/assets/tex_extract.py <Hod2.exe> <game tex folder> <out folder> [bank name ...]
  e.g. ... scr_common.bin scr_title.bin

The output contains game assets: keep it out of the repository.
"""
import os
import struct
import sys
import zlib


class Exe:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        count = struct.unpack_from('<H', d, pe + 6)[0]
        opt = struct.unpack_from('<H', d, pe + 20)[0]
        self.sections = [struct.unpack_from('<8sIIII', d, pe + 24 + opt + 40 * i) for i in range(count)]

    def offset(self, va):
        rva = va - 0x400000
        for _, vsize, vaddr, rsize, roff in self.sections:
            if vaddr <= rva < vaddr + min(rsize, vsize):
                return rva - vaddr + roff
        return None

    def u32(self, va):
        return struct.unpack_from('<I', self.data, self.offset(va))[0]

    def i16(self, va):
        return struct.unpack_from('<h', self.data, self.offset(va))[0]

    def u16s(self, va, n):
        return struct.unpack_from('<%dH' % n, self.data, self.offset(va))

    def cstr(self, va):
        o = self.offset(va)
        return self.data[o:self.data.index(b'\0', o)].decode('latin1')


BANK_NAMES = 0x4D0410
BANK_ENTRIES = 0x55A9B8
BANK_COUNTS = 0x55B0A8
PALETTES = 0x579020

# Palette index tables of 0x41CAB0 (bank index -> int16 table, by texture index).
PALETTE_TABLES = {0x147: 0x579534, 0x14E: 0x579474, 0x169: 0x57944C, 0x16D: 0x57939C,
                  0x170: 0x5794A0, 0x171: 0x5794EC, 0x173: 0x5794DC, 0x184: 0x579508}
# Banks with one palette for every texture (0x41CAB0); any other bank uses palette 0.
PALETTE_CONSTANTS = {0x156: 0x14, 0x177: 10, 0x179: 1, 0x17A: 0x21, 0x17C: 0x1A,
                     0x1B5: 0x1B, 0x1B8: 0x2E, 0x1B9: 0x2D}
for b in range(0x193, 0x1B5):
    PALETTE_CONSTANTS[b] = 0x14
for b in range(0x186, 0x18C):
    PALETTE_CONSTANTS[b] = 10
for b in range(0x17D, 0x184):
    PALETTE_CONSTANTS[b] = 1
for b in range(0x18C, 0x192):
    PALETTE_CONSTANTS[b] = 0xB


def banks(exe):
    result = {}
    for i in range(1, 0x400):
        p = exe.u32(BANK_NAMES + i * 4)
        if p == 0 or exe.offset(p) is None:
            break
        result[exe.cstr(p)] = i
    return result


def bank_entries(exe, index):
    table = exe.u32(BANK_ENTRIES + index * 4)
    count = exe.i16(BANK_COUNTS + index * 2)
    o = exe.offset(table)
    return [struct.unpack_from('<hhIIi', exe.data, o + k * 16) for k in range(count)]


def palettes(exe):
    result = []
    for k in range(64):
        base = PALETTES + 2 + k * 12
        count = exe.i16(base)
        ptr = exe.u32(base + 2)
        if count != 16 or exe.offset(ptr) is None:
            break
        result.append(exe.u16s(ptr, 16))
    return result


def palette_index(exe, bank, k):
    """Palette of texture k of a bank, as GetTexturePalette (0x41CAB0) picks it."""
    if bank in PALETTE_TABLES:
        return exe.i16(PALETTE_TABLES[bank] + k * 2)
    if bank == 0x168:  # scr_ranking
        return 0x2F if 0x2B <= k <= 0x38 else 0
    if bank == 0x176:  # scr_vmc
        return 0x31 if k == 0x10 else 0
    if bank == 0x185:  # scr_endstaff
        return 0x2B if k <= 0x6D else 0xD
    if bank == 0x192:  # scr_hinichi: one palette per range of textures
        for limit, palette in ((0x4B, 0x1C), (0x96, 0x1D), (0xE1, 0x1E), (300, 0x1F)):
            if k < limit:
                return palette
        return 0x20
    return PALETTE_CONSTANTS.get(bank, 0)


def decompress(src):
    """LZSS of 0x40ACF0. src starts after the uint32 unpacked size."""
    out = bytearray()
    pos = 0
    bits = 0
    nbits = 0

    def bit():
        nonlocal bits, nbits, pos
        if nbits == 0:
            bits = src[pos]
            pos += 1
            nbits = 8
        b = bits & 1
        bits >>= 1
        nbits -= 1
        return b

    while True:
        if bit():
            out.append(src[pos])
            pos += 1
            continue
        if bit():
            w = src[pos] | (src[pos + 1] << 8)
            pos += 2
            if w == 0:
                return bytes(out)
            dist = (w >> 3) - 0x2000
            length = w & 7
            if length:
                length += 2
            else:
                length = src[pos] + 1
                pos += 1
        else:
            n = (bit() << 1) | bit()
            dist = src[pos] - 0x100
            pos += 1
            length = n + 2
        start = len(out) + dist
        for i in range(length):
            out.append(out[start + i])


def colour(v, fmt):
    """PVR 16-bit pixel -> RGBA."""
    if fmt == 0:  # ARGB1555
        return ((v >> 10 & 31) * 255 // 31, (v >> 5 & 31) * 255 // 31, (v & 31) * 255 // 31, 255 if v & 0x8000 else 0)
    if fmt == 1:  # RGB565
        return ((v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31, 255)
    if fmt == 2:  # ARGB4444
        return ((v >> 8 & 15) * 17, (v >> 4 & 15) * 17, (v & 15) * 17, (v >> 12 & 15) * 17)
    return (255, 0, 255, 255)


def twiddle(x, y):
    """PVR twiddled index: y bits on the even positions, x bits on the odd ones."""
    r = 0
    for i in range(12):
        r |= ((y >> i) & 1) << (2 * i)
        r |= ((x >> i) & 1) << (2 * i + 1)
    return r


def twiddled_index(x, y, w, h):
    s = min(w, h)
    block = (x // s) if w > h else (y // s)
    return block * s * s + twiddle(x % s, y % s)


def decode(entry, data, pals, pal_index):
    w, h, fmt, offset, _ = entry
    pixel = fmt & 7
    layout = (fmt >> 8) & 0xFF
    px = []
    if layout in (0x01, 0x0D):  # twiddled (square / rectangle)
        for y in range(h):
            for x in range(w):
                i = twiddled_index(x, y, w, h)
                v = data[offset + 2 * i] | data[offset + 2 * i + 1] << 8
                px.append(colour(v, pixel))
    elif layout == 0x09:  # rectangle
        for y in range(h):
            for x in range(w):
                i = y * w + x
                v = data[offset + 2 * i] | data[offset + 2 * i + 1] << 8
                px.append(colour(v, pixel))
    elif layout == 0x03:  # VQ: 256 codebook entries of 2x2 pixels, then one twiddled byte per block
        book = data[offset:offset + 0x800]
        index = offset + 0x800
        for y in range(h):
            for x in range(w):
                k = data[index + twiddle(x >> 1, y >> 1)]
                i = k * 4 + ((x & 1) << 1 | (y & 1))
                v = book[2 * i] | book[2 * i + 1] << 8
                px.append(colour(v, pixel))
    elif layout == 0x05:  # 4-bit palette, twiddled
        pal = pals[pal_index] if 0 <= pal_index < len(pals) else [i * 0x1111 for i in range(16)]
        for y in range(h):
            for x in range(w):
                i = twiddled_index(x, y, w, h)
                b = data[offset + (i >> 1)]
                # Palette colours are always ARGB1555; the game converts them
                # to the texture's pixel format when it creates the surface.
                px.append(colour(pal[(b >> ((i & 1) * 4)) & 15], 0))
    else:
        return None
    return px


def write_png(path, w, h, px):
    # Rows are stored bottom-up (the game draws them with v flipped), so the
    # last row is written first.
    raw = b''.join(b'\0' + bytes(c for p in px[y * w:(y + 1) * w] for c in p) for y in reversed(range(h)))
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def main():
    exe_path, tex_dir, out_dir = sys.argv[1:4]
    wanted = sys.argv[4:]
    exe = Exe(exe_path)
    pals = palettes(exe)
    names = banks(exe)
    os.makedirs(out_dir, exist_ok=True)
    # One row per written PNG, for tools/assets/upscale_ui.py.
    manifest = open(os.path.join(out_dir, 'manifest.csv'), 'w', newline='')
    manifest.write('file,bank,index,texture_id,width,height,format\n')
    for name in wanted or sorted(names):
        index = names[name]
        path = os.path.join(tex_dir, name)
        if not os.path.exists(path):
            print(f'{name}: missing')
            continue
        raw = open(path, 'rb').read()
        size = struct.unpack_from('<I', raw)[0]
        data = decompress(raw[4:])
        entries = bank_entries(exe, index)
        done = 0
        for k, e in enumerate(entries):
            px = decode(e, data, pals, palette_index(exe, index, k))
            if px is None:
                print(f'  {name} #{k}: format 0x{e[2]:X} not handled')
                continue
            file = f'{os.path.splitext(name)[0]}_{k:03d}_tex{e[4]:X}.png'
            write_png(os.path.join(out_dir, file), e[0], e[1], px)
            manifest.write(f'{file},{name},{k},{e[4]:X},{e[0]},{e[1]},0x{e[2]:X}\n')
            done += 1
        print(f'{name}: bank 0x{index:X}, unpacked {len(data)} of {size} bytes, {done}/{len(entries)} textures')
    manifest.close()


if __name__ == '__main__':
    main()
