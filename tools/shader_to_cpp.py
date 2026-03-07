#!/usr/bin/env python3
import os
import sys

def generate_cpp(input_file, output_file, var_name):
    with open(input_file, "rb") as f:
        data = f.read()

    ascii_codes = ", ".join(str(b) for b in data)

    cpp = f"""// Auto-generated with {__file__}

static constexpr unsigned char {var_name}[{len(data) + 1}] = {{
    {ascii_codes}, 0
}};
"""

    with open(output_file, "w", newline="\n") as f:
        f.write(cpp)

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input-file> <output-file> <variable-name>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    var_name = sys.argv[3]
    if not os.path.isfile(input_file):
        print(f"Error: '{input_file}' does not exist or is not a file.")
        sys.exit(1)

    generate_cpp(input_file, output_file, var_name)

if __name__ == "__main__":
    main()
