#!/usr/bin/env python3
"""
gen_atlas.py - Texture atlas generator (no dependencies, pure Python stdlib)
Usage: python3 tools/gen_atlas.py
"""

import os, sys, math, struct, zlib

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
ROOT        = os.path.join(SCRIPT_DIR, '..')
TEXTURE_DIR = os.path.join(ROOT, 'data', 'textures')
OUTPUT_PNG  = os.path.join(TEXTURE_DIR, 'atlas.png')
OUTPUT_H    = os.path.join(ROOT, 'include', 'atlas_regions.h')
TILE_SIZE   = 16

# ── Minimal PNG reader ────────────────────────────────────────────────────────

def read_png(path):
    """Returns (width, height, pixels) where pixels is list of (r,g,b,a) tuples."""
    with open(path, 'rb') as f:
        data = f.read()

    assert data[:8] == b'\x89PNG\r\n\x1a\n', f"Not a PNG: {path}"
    pos = 8
    chunks = {}
    idat_data = b''

    while pos < len(data):
        length = struct.unpack('>I', data[pos:pos+4])[0]
        ctype  = data[pos+4:pos+8].decode('ascii')
        cdata  = data[pos+8:pos+8+length]
        pos   += 12 + length
        if ctype == 'IHDR':
            chunks['IHDR'] = cdata
        elif ctype == 'IDAT':
            idat_data += cdata
        elif ctype == 'IEND':
            break

    ihdr   = chunks['IHDR']
    width  = struct.unpack('>I', ihdr[0:4])[0]
    height = struct.unpack('>I', ihdr[4:8])[0]
    bpp    = ihdr[8]   # bit depth
    ctype_val = ihdr[9]  # color type
    assert bpp == 8, f"Only 8-bit PNGs supported: {path}"
    # color types: 2=RGB, 6=RGBA
    channels = {0:1, 2:3, 3:1, 4:2, 6:4}.get(ctype_val, 3)
    has_alpha = ctype_val in (4, 6)

    raw    = zlib.decompress(idat_data)
    stride = width * channels + 1  # +1 for filter byte per row
    pixels = []

    prev_row = [0] * (width * channels)

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
        if pa <= pb and pa <= pc: return a
        if pb <= pc: return b
        return c

    for y in range(height):
        row_start = y * stride
        ftype     = raw[row_start]
        row_raw   = list(raw[row_start+1:row_start+1+width*channels])
        row       = [0] * len(row_raw)

        for i, byte in enumerate(row_raw):
            a = row[i - channels] if i >= channels else 0
            b = prev_row[i]
            c = prev_row[i - channels] if i >= channels else 0
            if   ftype == 0: row[i] = byte
            elif ftype == 1: row[i] = (byte + a) & 0xFF
            elif ftype == 2: row[i] = (byte + b) & 0xFF
            elif ftype == 3: row[i] = (byte + ((a + b) >> 1)) & 0xFF
            elif ftype == 4: row[i] = (byte + paeth(a, b, c)) & 0xFF

        for x in range(width):
            base = x * channels
            if channels == 3:
                r, g, b, a = row[base], row[base+1], row[base+2], 255
            elif channels == 4:
                r, g, b, a = row[base], row[base+1], row[base+2], row[base+3]
            elif channels == 1:
                r = g = b = row[base]; a = 255
            else:
                r = g = b = row[base]; a = row[base+1]
            pixels.append((r, g, b, a))

        prev_row = row

    return width, height, pixels

# ── Minimal PNG writer ────────────────────────────────────────────────────────

def write_png(path, width, height, pixels):
    """Write RGBA pixels as PNG."""
    def chunk(ctype, data):
        c = ctype.encode() + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)

    raw = b''
    for y in range(height):
        raw += b'\x00'  # filter type None
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            raw += bytes([r, g, b, a])

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    idat = zlib.compress(raw, 9)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk('IHDR', ihdr))
        f.write(chunk('IDAT', idat))
        f.write(chunk('IEND', b''))

# ── Atlas builder ─────────────────────────────────────────────────────────────

def next_pow2(n):
    p = 1
    while p < n: p <<= 1
    return p

def name_to_define(filename):
    return 'TEX_' + os.path.splitext(filename)[0].upper()

def main():
    # Fixed order ensures TEX_* IDs are stable and match chunk.cpp expectations
    FIXED_ORDER = [
        'grass_top.png',
        'grass_side.png',
        'dirt.png',
        'stone.png',
        'wood.png',
        'leaves.png',
    ]

    # Start with fixed-order files that exist, then append any extras alphabetically
    existing = set(f for f in os.listdir(TEXTURE_DIR)
                   if f.endswith('.png') and f != 'atlas.png')
    pngs = [f for f in FIXED_ORDER if f in existing]
    extras = sorted(existing - set(FIXED_ORDER))
    pngs += extras

    if not pngs:
        print("No PNG files found in", TEXTURE_DIR)
        sys.exit(1)

    print(f"Found {len(pngs)} textures: {', '.join(pngs)}")

    count = len(pngs)
    cols  = next_pow2(math.ceil(math.sqrt(count)))
    rows  = next_pow2(math.ceil(count / cols))
    while cols * rows < count:
        rows <<= 1

    atlas_w = cols * TILE_SIZE
    atlas_h = rows * TILE_SIZE
    atlas_pixels = [(0,0,0,0)] * (atlas_w * atlas_h)

    print(f"Atlas: {atlas_w}x{atlas_h} ({cols}x{rows} grid)")

    regions = {}

    for i, filename in enumerate(pngs):
        col = i % cols
        row = i // cols
        path = os.path.join(TEXTURE_DIR, filename)

        try:
            w, h, pixels = read_png(path)
        except Exception as e:
            print(f"  ERROR reading {filename}: {e}")
            sys.exit(1)

        # Paste into atlas
        for py in range(min(h, TILE_SIZE)):
            for px in range(min(w, TILE_SIZE)):
                ax = col * TILE_SIZE + px
                ay = row * TILE_SIZE + py
                atlas_pixels[ay * atlas_w + ax] = pixels[py * w + px]

        stem = os.path.splitext(filename)[0]
        regions[stem] = (
            col * TILE_SIZE / atlas_w,
            row * TILE_SIZE / atlas_h,
            (col+1) * TILE_SIZE / atlas_w,
            (row+1) * TILE_SIZE / atlas_h,
        )
        print(f"  [{col},{row}] {name_to_define(filename)}")

    write_png(OUTPUT_PNG, atlas_w, atlas_h, atlas_pixels)
    print(f"\nSaved atlas: {OUTPUT_PNG}")

    os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)
    with open(OUTPUT_H, 'w') as f:
        f.write("// AUTO-GENERATED by tools/gen_atlas.py -- do not edit\n")
        f.write("#ifndef ATLAS_REGIONS_H\n#define ATLAS_REGIONS_H\n\n")
        for i, filename in enumerate(pngs):
            f.write(f"#define {name_to_define(filename)} {i}\n")
        f.write(f"#define TEX_COUNT {len(pngs)}\n\n")
        f.write("#ifdef ATLAS_REGIONS_IMPL\n")
        f.write("static const float atlas_uvs[TEX_COUNT][4] = {\n")
        for filename in pngs:
            stem = os.path.splitext(filename)[0]
            u0,v0,u1,v1 = regions[stem]
            f.write(f"    {{{u0:.6f}f, {v0:.6f}f, {u1:.6f}f, {v1:.6f}f}}, // {name_to_define(filename)}\n")
        f.write("};\n#endif\n\n#endif // ATLAS_REGIONS_H\n")

    print(f"Saved header: {OUTPUT_H}")
    print("\nDone! Run 'make' to rebuild.")

if __name__ == '__main__':
    main()