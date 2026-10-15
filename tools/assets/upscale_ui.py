"""Upscales textures extracted by tex_extract.py with Real-ESRGAN (ncnn-vulkan).

The result is <out folder>/<bank>/<texture id>.png (bank = file name without
.bin), the layout of the game's data\\tex folder ([assets] replace_textures,
docs/work/assets.md G1-6). Texture ids are not unique across banks: banks
loaded in different scenes (chapter titles, option texts, ...) reuse ids for
other images, so replacements are kept per bank.

Model per texture (chosen from the PVR layout in manifest.csv):
  - VQ (layout 0x03): backgrounds and photos -> realesrgan-x4plus (keeps texture);
  - everything else: text, icons, logos -> realesrgan-x4plus-anime (clean edges).
Both scale by 4; the largest UI texture is 512 px, so results stay within the
2048 px device limit measured with dgVoodoo2.

Usage:
  python -I tools/assets/upscale_ui.py <realesrgan-ncnn-vulkan.exe> <extract folder> <work folder> <out folder>

Before upscaling, fully transparent pixels get the colour of the nearest visible
pixel (bleed()); alpha is not changed. Identical images (same PNG bytes) are
upscaled once. <work folder> keeps the
per-model inputs and outputs; images that already have an output there are
not upscaled again. Input and output contain game assets: keep them out of the
repository.
"""
import csv
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import zlib
from collections import deque

MODELS = {
    'photo': 'realesrgan-x4plus',
    'graphic': 'realesrgan-x4plus-anime',
}
SCALE = 4
MAX_SIZE = 2048
# realesrgan-ncnn-vulkan 20220424 loses the Vulkan device on an 8 px wide image
# (vkWaitForFences failed -4) and stops processing; such textures keep the
# original (no replacement file).
MIN_SIZE = 16


def kind(fmt):
    return 'photo' if (int(fmt, 16) >> 8) & 0xFF == 0x03 else 'graphic'


def read_png(path):
    """RGBA of a PNG written by tex_extract.py (8-bit RGBA, filter 0 rows)."""
    data = open(path, 'rb').read()
    pos, idat = 8, b''
    while pos < len(data):
        length, tag = struct.unpack_from('>I4s', data, pos)
        body = data[pos + 8:pos + 8 + length]
        if tag == b'IHDR':
            w, h = struct.unpack_from('>II', body)
        elif tag == b'IDAT':
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = w * 4 + 1
    assert all(raw[y * stride] == 0 for y in range(h)), path
    return w, h, bytearray(b''.join(raw[y * stride + 1:(y + 1) * stride] for y in range(h)))


def write_png(path, w, h, rgba):
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xFFFFFFFF)
    raw = b''.join(b'\0' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def bleed(w, h, rgba):
    """Gives every fully transparent pixel the colour of the nearest visible one.

    The game's transparent pixels are mostly black. Real-ESRGAN mixes that colour
    into the edges and turns small or thin sprites dark; with the neighbouring
    colour there instead, only the alpha channel decides the shape. Alpha is
    left unchanged.
    """
    queue = deque(i for i in range(w * h) if rgba[i * 4 + 3])
    if not queue:
        return
    done = bytearray(1 if rgba[i * 4 + 3] else 0 for i in range(w * h))
    while queue:
        i = queue.popleft()
        x, y = i % w, i // w
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < w and 0 <= ny < h:
                j = ny * w + nx
                if not done[j]:
                    done[j] = 1
                    rgba[j * 4:j * 4 + 3] = rgba[i * 4:i * 4 + 3]
                    queue.append(j)


def main():
    exe, src, work, out = sys.argv[1:5]
    rows = list(csv.DictReader(open(os.path.join(src, 'manifest.csv'), newline='')))

    # One upscale per distinct image; the first file with that content names it.
    first = {}
    for row in rows:
        digest = hashlib.sha256(open(os.path.join(src, row['file']), 'rb').read()).hexdigest()
        row['source'] = first.setdefault(digest, row)
    unique = [r for r in rows if r['source'] is r]
    print(f'{len(rows)} textures, {len(unique)} distinct images')

    for name, model in MODELS.items():
        inp = os.path.join(work, name, 'in')
        res = os.path.join(work, name, 'out')
        os.makedirs(inp, exist_ok=True)
        os.makedirs(res, exist_ok=True)
        todo = []
        for r in unique:
            if kind(r['format']) != name or os.path.exists(os.path.join(res, r['file'])):
                continue
            if int(r['width']) * SCALE > MAX_SIZE or int(r['height']) * SCALE > MAX_SIZE:
                print(f"{r['file']}: {r['width']}x{r['height']} x{SCALE} exceeds {MAX_SIZE}, skipped")
                continue
            if min(int(r['width']), int(r['height'])) < MIN_SIZE:
                print(f"{r['file']}: {r['width']}x{r['height']} is too small for Real-ESRGAN, skipped")
                continue
            todo.append(r)
        # Real-ESRGAN takes a whole folder: leave only this run's inputs in it.
        for old in os.listdir(inp):
            os.remove(os.path.join(inp, old))
        for r in todo:
            w, h, rgba = read_png(os.path.join(src, r['file']))
            bleed(w, h, rgba)
            write_png(os.path.join(inp, r['file']), w, h, rgba)
        print(f'{model}: {len(todo)} to upscale')
        if todo:
            subprocess.run([exe, '-i', inp, '-o', res, '-n', model, '-s', str(SCALE), '-f', 'png'],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    written = 0
    for r in rows:
        result = os.path.join(work, kind(r['format']), 'out', r['source']['file'])
        if not os.path.exists(result):
            print(f"{r['file']}: no output")
            continue
        folder = os.path.join(out, os.path.splitext(r['bank'])[0])
        os.makedirs(folder, exist_ok=True)
        shutil.copyfile(result, os.path.join(folder, f"{r['texture_id']}.png"))
        written += 1
    print(f'{written} files written to {out}')


if __name__ == '__main__':
    main()
