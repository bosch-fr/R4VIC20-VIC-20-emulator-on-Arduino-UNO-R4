# bin2c.py — Convert a binary file (.prg) to a C header for PROGMEM
# Usage: python bin2c.py input.prg output.h array_name

import sys
import os

if len(sys.argv) != 4:
    print("Usage: python bin2c.py input.prg output.h array_name")
    print("Example: python bin2c.py meteor.prg meteor.h meteor_prg")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]
array_name = sys.argv[3]

if not os.path.exists(input_file):
    print(f"Error: {input_file} not found")
    sys.exit(1)

with open(input_file, 'rb') as f:
    data = f.read()

if len(data) < 2:
    print("Error: file too small to contain a load address")
    sys.exit(1)

# First two bytes are the load address (little-endian)
load_addr = data[0] | (data[1] << 8)
data_len = len(data) - 2

with open(output_file, 'w') as f:
    f.write(f"// Auto-generated from {os.path.basename(input_file)}\n")
    f.write(f"// Load address: ${load_addr:04X}\n")
    f.write(f"// Data size: {data_len} bytes\n")
    f.write(f"// Total file: {len(data)} bytes\n\n")
    f.write(f"#ifndef {array_name.upper()}_H\n")
    f.write(f"#define {array_name.upper()}_H\n\n")
    f.write(f"#include <stdint.h>\n\n")
    f.write(f"const uint8_t {array_name}[] PROGMEM = {{\n")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        f.write("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",\n")
    f.write("};\n\n")
    f.write(f"const uint16_t {array_name}_size = {len(data)};\n")
    f.write(f"const uint16_t {array_name}_load_addr = 0x{load_addr:04X};\n")
    f.write(f"const uint16_t {array_name}_data_len = {data_len};\n\n")
    f.write(f"#endif\n")

print(f"Converted {input_file} ({len(data)} bytes) -> {output_file}")
print(f"  Load address: ${load_addr:04X}")
print(f"  Data length:  {data_len} bytes")
print(f"  Array name:   {array_name}")