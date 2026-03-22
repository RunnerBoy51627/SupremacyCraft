#!/usr/bin/env python3
"""
sound2raw.py — converts audio files in data/sounds/ to GC-compatible raw PCM
Requires: ffmpeg in PATH
Usage: python3 tools/sound2raw.py
"""
import os, subprocess, sys

SOUNDS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'sounds')
EXTS = ('.mp3', '.wav', '.ogg', '.flac')

# Map source filename -> output raw name
# Output name must match the extern in sound.cpp
NAME_MAP = {
    'minecraft-glass-break': 'block_break',
    'minecraft-hit-sfx':     'player_hit',
    'explode1':              'explode',
}

def main():
    if not os.path.isdir(SOUNDS_DIR):
        print(f"No data/sounds/ directory found.")
        sys.exit(1)

    files = [f for f in os.listdir(SOUNDS_DIR) if f.lower().endswith(EXTS)]
    if not files:
        print("No audio files found in data/sounds/")
        sys.exit(0)

    for f in files:
        inp  = os.path.join(SOUNDS_DIR, f)
        stem = os.path.splitext(f)[0]
        out_stem = NAME_MAP.get(stem, stem)
        out  = os.path.join(SOUNDS_DIR, out_stem + '.raw')
        if os.path.exists(out):
            print(f"  skip {f} -> {out_stem}.raw (already converted)")
            continue
        print(f"  converting {f} -> {out_stem}.raw ...")
        result = subprocess.run([
            'ffmpeg', '-y', '-i', inp,
            '-f', 's16be',   # signed 16-bit big-endian (GC DSP format)
            '-ar', '44100',  # 44.1kHz
            '-ac', '2',      # stereo
            out
        ], capture_output=True)
        if result.returncode != 0:
            print(f"  ERROR: {result.stderr.decode()[-200:]}")
        else:
            size = os.path.getsize(out)
            print(f"  OK ({size//1024} KB)")

    print("\nDone. Run 'make' to rebuild.")

if __name__ == '__main__':
    # Clean up any old wrongly-named raw files
    import glob
    for f in glob.glob(os.path.join(SOUNDS_DIR, "*.raw")):
        stem = os.path.splitext(os.path.basename(f))[0]
        if stem not in NAME_MAP.values():
            print(f"  removing old raw: {os.path.basename(f)}")
            os.remove(f)
    main()