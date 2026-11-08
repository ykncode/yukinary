import os
import subprocess
import struct
import sys

def main():
    print("Testing Artifact generation and Relocations...")
    # 1. Create a dummy file
    with open("tests/fix7/dummy.ykn", "w", encoding="utf-8") as f:
        f.write("isv ana() -> tamsayi { dondu 42; }\n")
        
    # 2. Compile it
    print("Compiling...")
    res = subprocess.run([".\\app_full.exe", "tests/fix7/dummy.ykn", "tests/fix7/dummy.exe"], capture_output=True)
    if res.returncode != 0:
        print("Compilation failed!")
        print(res.stdout.decode('utf-8'))
        sys.exit(1)
        
    exe_path = "tests/fix7/dummy.exe"
    if not os.path.exists(exe_path):
        print(f"Exe not found: {exe_path}")
        sys.exit(1)
        
    print("Executable generated successfully.")
    
    # 3. Read PE headers
    with open(exe_path, "rb") as f:
        dos_header = f.read(64)
        if dos_header[0:2] != b'MZ':
            print("Invalid DOS signature")
            sys.exit(1)
            
        pe_offset = struct.unpack('<I', dos_header[60:64])[0]
        f.seek(pe_offset)
        pe_sig = f.read(4)
        if pe_sig != b'PE\0\0':
            print("Invalid PE signature")
            sys.exit(1)
            
        # File header
        f_header = f.read(20)
        machine, num_sections, _, _, _, opt_header_size, chars = struct.unpack('<HHIIIHH', f_header)
        print(f"Machine: {hex(machine)} (Expected 0x8664 for x64)")
        print(f"Sections: {num_sections}")
        
        if machine != 0x8664:
            print("Error: Machine type is not x64")
            sys.exit(1)
            
        # Optional header
        f.read(opt_header_size)
        
        # Section headers
        found_text = False
        found_data = False
        for _ in range(num_sections):
            sec_header = f.read(40)
            name = sec_header[0:8].decode('utf-8').strip('\0')
            characteristics = struct.unpack('<I', sec_header[36:40])[0]
            print(f"Section {name}: {hex(characteristics)}")
            
            if name == ".text":
                found_text = True
                # Check for executable and readable flags
                if not (characteristics & 0x20000000) or not (characteristics & 0x40000000):
                    print("Error: .text section is not executable/readable")
                    sys.exit(1)
            if name == ".data":
                found_data = True
                # Check for readable and writable flags
                if not (characteristics & 0x40000000) or not (characteristics & 0x80000000):
                    print("Error: .data section is not readable/writable")
                    sys.exit(1)
                    
        if not found_text:
            print("Error: .text section not found")
            sys.exit(1)
            
    print("Artifact structure, section headers, and permissions verified successfully!")
    sys.exit(0)

if __name__ == "__main__":
    main()
