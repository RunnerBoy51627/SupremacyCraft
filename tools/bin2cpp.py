#!/usr/bin/env python3
"""
bin2cpp.py — converts a binary file to a C++ source with the same
symbol layout that bin2s produces, so textures.cpp can use it on PC.
Usage: python3 tools/bin2cpp.py <input_file> <output.cpp> <symbol_name>
"""
import sys, os

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input> <output.cpp> <symbol>")
        sys.exit(1)

    inp, out, sym = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(inp, 'rb') as f:
        data = f.read()

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w') as f:
        f.write(f'#include <stdint.h>\n')
        f.write(f'extern const unsigned char {sym}[];\n')
        f.write(f'extern const uint32_t {sym}_size;\n\n')
        f.write(f'const uint32_t {sym}_size = {len(data)};\n')
        f.write(f'const unsigned char {sym}[] = {{\n')
        for i, b in enumerate(data):
            if i % 16 == 0:
                f.write('    ')
            f.write(f'0x{b:02x},')
            if i % 16 == 15:
                f.write('\n')
        f.write('\n};\n')

    print(f"Wrote {len(data)} bytes as '{sym}' to {out}")

if __name__ == '__main__':
    main()