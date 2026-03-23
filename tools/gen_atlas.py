#!/usr/bin/env python3
"""
gen_atlas.py - Texture atlas generator with resource pack support
Usage:
  python3 tools/gen_atlas.py                        # use active pack
  python3 tools/gen_atlas.py --pack default         # use specific pack
  python3 tools/gen_atlas.py --list                 # list available packs

Resource packs live in: data/resource_packs/<name>/
Each pack must have:
  pack.json   - { "name", "description", "tile_size", "author" }
  textures/   - PNG files matching the fixed order below

Supported tile sizes: 4, 8, 16, 24, 32, 48, 64, 128
"""

import os, sys, math, struct, zlib, json, argparse

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
ROOT         = os.path.join(SCRIPT_DIR, '..')
PACKS_DIR    = os.path.join(ROOT, 'data', 'resource_packs')
ACTIVE_FILE  = os.path.join(ROOT, 'data', 'active_pack.txt')
OUTPUT_PNG   = os.path.join(ROOT, 'data', 'textures', 'atlas.png')
OUTPUT_H     = os.path.join(ROOT, 'include', 'atlas_regions.h')

VALID_SIZES  = {4, 8, 16, 24, 32, 48, 64, 128}

FIXED_ORDER = [
    'grass_top.png',
    'grass_side.png',
    'dirt.png',
    'stone.png',
    'wood.png',
    'leaves.png',
    'planks.png',
    'crafting_top.png',
    'crafting_side.png',
    'tnt_side.png',
    'tnt_top.png',
    'tnt_bottom.png',
    'flint_steel.png',
    'sand.png',
    'gravel.png',
    'water.png',
    'bedrock.png',
    'pickaxe.png',
]

# ── Alias / fallback map ────────────────────────────────────────────────────
# If a required texture is missing, try these alternatives in order.
# First match wins. This lets packs use "tnt.png" instead of tnt_side/top/bottom.
FALLBACKS = {
    # Multi-face blocks — fall back to single texture
    "grass_top.png":    ["grass.png"],
    "grass_side.png":   ["grass.png"],
    "wood.png":         ["log.png", "oak_log.png"],
    "tnt_side.png":     ["tnt.png"],
    "tnt_top.png":      ["tnt.png"],
    "tnt_bottom.png":   ["tnt.png"],
    "crafting_top.png": ["crafting_table.png", "crafting.png"],
    "crafting_side.png":["crafting_table.png", "crafting.png", "planks.png"],
    "planks.png":       ["oak_planks.png", "plank.png"],
    "flint_steel.png":  [],  # tool — skip gracefully
    # Common renames
    "dirt.png":         ["earth.png"],
    "stone.png":        ["rock.png"],
    "leaves.png":       ["leaf.png", "leaves_oak.png"],
}

# ── Minimal PNG reader ────────────────────────────────────────────────────────

def read_png(path):
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
        if ctype == 'IHDR':   chunks['IHDR'] = cdata
        elif ctype == 'IDAT': idat_data += cdata
        elif ctype == 'IEND': break
    ihdr      = chunks['IHDR']
    width     = struct.unpack('>I', ihdr[0:4])[0]
    height    = struct.unpack('>I', ihdr[4:8])[0]
    bpp       = ihdr[8]
    ctype_val = ihdr[9]
    assert bpp == 8, f"Only 8-bit PNGs supported: {path}"
    channels  = {0:1, 2:3, 3:1, 4:2, 6:4}.get(ctype_val, 3)
    raw       = zlib.decompress(idat_data)
    stride    = width * channels + 1
    pixels    = []
    prev_row  = [0] * (width * channels)
    def paeth(a, b, c):
        p = a+b-c; pa,pb,pc = abs(p-a),abs(p-b),abs(p-c)
        if pa<=pb and pa<=pc: return a
        if pb<=pc: return b
        return c
    for y in range(height):
        rs    = y * stride
        ftype = raw[rs]
        rr    = list(raw[rs+1:rs+1+width*channels])
        row   = [0]*len(rr)
        for i,byte in enumerate(rr):
            a = row[i-channels] if i>=channels else 0
            b = prev_row[i]
            c = prev_row[i-channels] if i>=channels else 0
            if   ftype==0: row[i]=byte
            elif ftype==1: row[i]=(byte+a)&0xFF
            elif ftype==2: row[i]=(byte+b)&0xFF
            elif ftype==3: row[i]=(byte+((a+b)>>1))&0xFF
            elif ftype==4: row[i]=(byte+paeth(a,b,c))&0xFF
        for x in range(width):
            base=x*channels
            if channels==3:   r,g,b,a=row[base],row[base+1],row[base+2],255
            elif channels==4: r,g,b,a=row[base],row[base+1],row[base+2],row[base+3]
            elif channels==1: r=g=b=row[base]; a=255
            else:             r=g=b=row[base]; a=row[base+1]
            pixels.append((r,g,b,a))
        prev_row = row
    return width, height, pixels

# ── PNG resizer (nearest-neighbor) ───────────────────────────────────────────

def resize_pixels(pixels, src_w, src_h, dst_w, dst_h):
    out = []
    for y in range(dst_h):
        sy = int(y * src_h / dst_h)
        for x in range(dst_w):
            sx = int(x * src_w / dst_w)
            out.append(pixels[sy * src_w + sx])
    return out

# ── PNG writer ────────────────────────────────────────────────────────────────

def write_png(path, width, height, pixels):
    def chunk(ctype, data):
        c = ctype.encode() + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    # Fast bytearray-based raw data construction
    raw = bytearray(height * (1 + width * 4))
    idx = 0
    for y in range(height):
        raw[idx] = 0; idx += 1
        for x in range(width):
            r,g,b,a = pixels[y*width+x]
            raw[idx]=r; raw[idx+1]=g; raw[idx+2]=b; raw[idx+3]=a; idx+=4
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk('IHDR', ihdr))
        f.write(chunk('IDAT', zlib.compress(bytes(raw), 1)))
        f.write(chunk('IEND', b''))

# ── Helpers ───────────────────────────────────────────────────────────────────

def next_pow2(n):
    p = 1
    while p < n: p <<= 1
    return p

def name_to_define(filename):
    return 'TEX_' + os.path.splitext(filename)[0].upper()

def get_active_pack():
    if os.path.exists(ACTIVE_FILE):
        with open(ACTIVE_FILE) as f:
            name = f.read().strip()
        if name:
            return name
    return 'default'

def set_active_pack(name):
    os.makedirs(os.path.dirname(ACTIVE_FILE), exist_ok=True)
    with open(ACTIVE_FILE, 'w') as f:
        f.write(name)

def list_packs():
    if not os.path.isdir(PACKS_DIR):
        print("No resource packs directory found.")
        return
    active = get_active_pack()
    packs = [d for d in os.listdir(PACKS_DIR)
             if os.path.isdir(os.path.join(PACKS_DIR, d))]
    if not packs:
        print("No resource packs found.")
        return
    print("Available resource packs:")
    for p in sorted(packs):
        meta = load_pack_meta(p)
        marker = " *" if p == active else ""
        size = meta.get('tile_size', '?')
        desc = meta.get('description', '')
        print(f"  {p} ({size}px) — {desc}{marker}")
    print("\n* = active pack")

def load_pack_meta(pack_name):
    meta_path = os.path.join(PACKS_DIR, pack_name, 'pack.json')
    if os.path.exists(meta_path):
        with open(meta_path) as f:
            return json.load(f)
    return {"tile_size": 16}

# ── Texture resolver ────────────────────────────────────────────────────────

def resolve_texture(filename, texture_dir, existing):
    """Return (resolved_filename, is_copy) — finds file or best fallback.
    Returns None if nothing found."""
    # Direct match
    if filename in existing:
        return filename, False
    # Try fallbacks
    for alt in FALLBACKS.get(filename, []):
        if alt in existing:
            return alt, True
    return None, False

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='Generate texture atlas')
    parser.add_argument('--pack',   help='Resource pack name to use')
    parser.add_argument('--list',   action='store_true', help='List packs')
    parser.add_argument('--set',    help='Set active pack and exit')
    args = parser.parse_args()

    if args.list:
        list_packs()
        return

    if args.set:
        pack_path = os.path.join(PACKS_DIR, args.set)
        if not os.path.isdir(pack_path):
            print(f"Pack '{args.set}' not found in {PACKS_DIR}")
            sys.exit(1)
        set_active_pack(args.set)
        print(f"Active pack set to: {args.set}")
        return

    pack_name = args.pack if args.pack else get_active_pack()
    pack_path = os.path.join(PACKS_DIR, pack_name)

    # Fall back to default pack if not found
    if not os.path.isdir(pack_path):
        print(f"Pack '{pack_name}' not found, falling back to default")
        pack_name = 'default'
        pack_path = os.path.join(PACKS_DIR, 'default')

    meta = load_pack_meta(pack_name)
    TILE_SIZE = int(meta.get('tile_size', 16))

    if TILE_SIZE not in VALID_SIZES:
        print(f"Invalid tile_size {TILE_SIZE}. Must be one of: {sorted(VALID_SIZES)}")
        sys.exit(1)

    texture_dir = os.path.join(pack_path, 'textures')
    # Fall back to data/textures/ if pack textures folder is missing or empty
    legacy_dir = os.path.join(ROOT, 'data', 'textures')
    if not os.path.isdir(texture_dir):
        os.makedirs(texture_dir, exist_ok=True)
    # If pack folder has fewer than 3 PNGs, sync from data/textures/
    pack_pngs = [f for f in os.listdir(texture_dir)
                 if f.endswith('.png') and f != 'atlas.png']
    if len(pack_pngs) < 3 and os.path.isdir(legacy_dir):
        import shutil
        for f in os.listdir(legacy_dir):
            if f.endswith('.png') and f != 'atlas.png':
                shutil.copy(os.path.join(legacy_dir, f),
                            os.path.join(texture_dir, f))
        print(f"  synced textures from data/textures/ to pack folder")

    print(f"Generating texture atlas...")
    print(f"Pack: {meta.get('name', pack_name)} ({TILE_SIZE}x{TILE_SIZE}px tiles)")

    existing = set(f for f in os.listdir(texture_dir) if f.endswith('.png') and f != 'atlas.png')

    # Resolve FIXED_ORDER — use fallbacks for missing files
    pngs = []         # final ordered list of TEX_* names
    sources = {}      # tex_name -> actual filename to load
    for fname in FIXED_ORDER:
        resolved, is_fallback = resolve_texture(fname, texture_dir, existing)
        if resolved:
            pngs.append(fname)
            sources[fname] = resolved
            if is_fallback:
                print(f"  fallback: {fname} -> {resolved}")
        else:
            # Missing with no fallback — use a magenta error tile
            pngs.append(fname)
            sources[fname] = None
            print(f"  missing: {fname} (will use error color)")

    # Add extra textures not in FIXED_ORDER
    extras = sorted(existing - set(FIXED_ORDER) - set(sources.values()))
    for fname in extras:
        pngs.append(fname)
        sources[fname] = fname

    if not pngs:
        print(f"No PNG files found in {texture_dir}")
        sys.exit(1)

    print(f"Found {len(pngs)} textures: {', '.join(pngs)}")

    count = len(pngs)
    cols  = next_pow2(math.ceil(math.sqrt(count)))
    rows  = next_pow2(math.ceil(count / cols))
    while cols * rows < count:
        rows <<= 1

    # Atlas must be power-of-2 for GX — no padding, UV inset handles bleeding
    atlas_w = next_pow2(cols * TILE_SIZE)
    atlas_h = next_pow2(rows * TILE_SIZE)
    atlas_pixels = [(0,0,0,0)] * (atlas_w * atlas_h)

    print(f"Atlas: {atlas_w}x{atlas_h} ({cols}x{rows} grid, {TILE_SIZE}px tiles)")

    regions = {}
    for i, filename in enumerate(pngs):
        col  = i % cols
        row  = i // cols
        src_file = sources.get(filename)
        if src_file is None:
            # Missing — search default pack for this texture or its aliases
            candidates = [filename] + FALLBACKS.get(filename, [])
            default_pixels = None
            for cand in candidates:
                default_tex = os.path.join(PACKS_DIR, 'default', 'textures', cand)
                if os.path.exists(default_tex):
                    try:
                        dw, dh, default_pixels = read_png(default_tex)
                        # Resize default texture to current pack tile size
                        if dw != TILE_SIZE or dh != TILE_SIZE:
                            default_pixels = resize_pixels(default_pixels, dw, dh, TILE_SIZE, TILE_SIZE)
                        pixels = default_pixels
                        w, h = TILE_SIZE, TILE_SIZE
                        print(f"  default fallback: {filename} <- {cand}")
                    except Exception as e:
                        print(f"  error loading default {cand}: {e}")
                    break
            if default_pixels is None:
                # Absolute last resort — solid grey
                print(f"  WARNING: no fallback found for {filename}, using grey")
                pixels = [(128,128,128,255)] * (TILE_SIZE * TILE_SIZE)
                w, h = TILE_SIZE, TILE_SIZE
        else:
            path = os.path.join(texture_dir, src_file)
            try:
                w, h, pixels = read_png(path)
            except Exception as e:
                print(f"  ERROR reading {src_file}: {e}")
                sys.exit(1)

        # Resize to TILE_SIZE if needed (nearest-neighbor)
        if w != TILE_SIZE or h != TILE_SIZE:
            pixels = resize_pixels(pixels, w, h, TILE_SIZE, TILE_SIZE)
            w, h = TILE_SIZE, TILE_SIZE

        # Paste into atlas
        T = TILE_SIZE
        ox = col * T
        oy = row * T
        for py in range(T):
            base_atlas = (oy + py) * atlas_w + ox
            base_tile  = py * T
            atlas_pixels[base_atlas:base_atlas+T] = pixels[base_tile:base_tile+T]

        regions[os.path.splitext(filename)[0]] = (
            ox / atlas_w,
            oy / atlas_h,
            (ox + T) / atlas_w,
            (oy + T) / atlas_h,
        )
        print(f"  [{col},{row}] {name_to_define(filename)}")

    os.makedirs(os.path.dirname(OUTPUT_PNG), exist_ok=True)
    write_png(OUTPUT_PNG, atlas_w, atlas_h, atlas_pixels)
    print(f"\nSaved atlas: {OUTPUT_PNG}")

    os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)
    with open(OUTPUT_H, 'w') as f:
        f.write("// AUTO-GENERATED by tools/gen_atlas.py -- do not edit\n")
        f.write("#ifndef ATLAS_REGIONS_H\n#define ATLAS_REGIONS_H\n\n")
        f.write(f"#define ATLAS_TILE_SIZE {TILE_SIZE}\n\n")
        for i, filename in enumerate(pngs):
            f.write(f"#define {name_to_define(filename)} {i}\n")
        f.write(f"#define TEX_COUNT {len(pngs)}\n\n")
        f.write("#ifdef ATLAS_REGIONS_IMPL\n")
        f.write("static const float atlas_uvs[TEX_COUNT][4] = {\n")
        for filename in pngs:
            stem = os.path.splitext(filename)[0]
            u0,v0,u1,v1 = regions[stem]
            f.write(f"    {{{u0:.8f}f, {v0:.8f}f, {u1:.8f}f, {v1:.8f}f}}, // {name_to_define(filename)}\n")
        f.write("};\n#endif\n\n#endif // ATLAS_REGIONS_H\n")

    print(f"Saved header: {OUTPUT_H}")
    print(f"\nDone! Run 'make' to rebuild.")

if __name__ == '__main__':
    main()