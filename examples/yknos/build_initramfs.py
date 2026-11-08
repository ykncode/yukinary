import tarfile
import gzip
import os

def build_yknos_initramfs():
    tar_path = 'd:/yukinary/yknos/kernel/alpine-minirootfs.tar.gz'
    out_gz = 'd:/yukinary/yknos/kernel/yknos_initramfs.cpio.gz'
    
    # Custom injected files
    injected_files = {}
    
    # 1. /init script
    init_script = b'''#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export HOME=/root
export TERM=linux
export PS1="root@yknos:\\w# "

# Mount essential filesystems
mount -t proc none /proc 2>/dev/null
mount -t sysfs none /sys 2>/dev/null
mount -t devtmpfs none /dev 2>/dev/null
mkdir -p /dev/pts /tmp /run /home/root/.yuki /var/log
mount -t devpts none /dev/pts 2>/dev/null
mount -t tmpfs none /tmp 2>/dev/null
mount -t tmpfs none /run 2>/dev/null

hostname yknos
ifconfig lo 127.0.0.1 up 2>/dev/null
ifconfig eth0 up 2>/dev/null

clear
cat << "EOF"
================================================================
  YknOS 6.1 LTS - AI-First Neural Linux Operating System
  Kernel: Linux 6.6 LTS x86_64 | Userspace: Pure Yukinary (YKN)
  Y.U.K.I Core: ONLINE | 5-Level Security Sandbox Active
================================================================
EOF

# Run YKN Master Init (Compiled ELF64)
if [ -x /bin/ykn-init ]; then
    /bin/ykn-init
fi

echo "[YknOS]: Sistem hazir. Komutlar: 'yuki', 'uname -a', 'apk', 'ls -la /bin'"
exec /bin/sh
'''
    injected_files['init'] = (0o100755, init_script)

    # 2. /etc/os-release
    os_release = b'''NAME="YknOS"
PRETTY_NAME="YknOS 6.1 LTS (AI-First Neural Linux OS)"
ID=yknos
ID_LIKE=alpine
VERSION="6.1 LTS"
VERSION_ID="6.1"
HOME_URL="https://github.com/ykncode/yknos"
BUG_REPORT_URL="https://github.com/ykncode/yknos/issues"
ANSI_COLOR="0;36"
'''
    injected_files['etc/os-release'] = (0o100644, os_release)

    # 3. /etc/hostname
    injected_files['etc/hostname'] = (0o100644, b'yknos\n')

    # 4. /etc/issue
    issue = b'''================================================================
  YknOS 6.1 LTS - AI-First Neural Linux Operating System
  Kernel: Linux 6.6 LTS x86_64 | Userspace: Pure Yukinary (YKN)
  Y.U.K.I Core: ONLINE | 5-Level Security Sandbox Active
================================================================
'''
    injected_files['etc/issue'] = (0o100644, issue)

    # 5. /bin/ykn-init (ELF64 compiled from core/ykn_init.ykn)
    ykn_init_path = 'd:/yukinary/yknos/rootfs/bin/ykn-init'
    if os.path.exists(ykn_init_path):
        with open(ykn_init_path, 'rb') as f:
            injected_files['bin/ykn-init'] = (0o100755, f.read())

    # 6. /bin/yuki (ELF64 compiled from core/yuki_core.ykn)
    yuki_path = 'd:/yukinary/yknos/rootfs/bin/yuki'
    if os.path.exists(yuki_path):
        with open(yuki_path, 'rb') as f:
            injected_files['bin/yuki'] = (0o100755, f.read())

    # 7. /bin/yknsh (ELF64 compiled from core/ykn_shell.ykn)
    yknsh_path = 'd:/yukinary/yknos/rootfs/bin/yknsh'
    if os.path.exists(yknsh_path):
        with open(yknsh_path, 'rb') as f:
            injected_files['bin/yknsh'] = (0o100755, f.read())

    # 8. /bin/ykn-browser (ELF64 compiled from core/ykn_browser.ykn)
    browser_path = 'd:/yukinary/yknos/rootfs/bin/ykn-browser'
    if os.path.exists(browser_path):
        with open(browser_path, 'rb') as f:
            injected_files['bin/ykn-browser'] = (0o100755, f.read())

    # 9. /home/root/.yuki/vector_memory.db
    injected_files['home/root/.yuki/vector_memory.db'] = (0o100644, b'YUKINARY_VECTOR_MEMORY_V2:14_EVENTS_LEARNED_BITNET_1.58b_LOCAL\n')

    # Build CPIO stream
    cpio_bytes = bytearray()
    ino = 1

    # Read base tarball
    with tarfile.open(tar_path, 'r:gz') as tar:
        for member in tar.getmembers():
            name = member.name.lstrip('./').rstrip('/')
            if not name:
                continue
            
            # If overridden by injected files, skip base version
            if name in injected_files:
                continue

            if member.isdir():
                file_mode = 0o040000 | (member.mode & 0o777)
                filesize = 0
                file_data = b''
            elif member.issym():
                file_mode = 0o120000 | 0o777
                link_bytes = member.linkname.encode('utf-8')
                filesize = len(link_bytes)
                file_data = link_bytes
            elif member.isreg():
                file_mode = 0o100000 | (member.mode & 0o777)
                f = tar.extractfile(member)
                file_data = f.read() if f else b''
                filesize = len(file_data)
            else:
                continue

            filename_bytes = name.encode('utf-8') + b'\x00'
            namesize = len(filename_bytes)
            header = f"070701{ino:08X}{file_mode:08X}{0:08X}{0:08X}{1:08X}{1700000000:08X}{filesize:08X}{0:08X}{0:08X}{0:08X}{0:08X}{namesize:08X}{0:08X}".encode('ascii')
            ino += 1

            cpio_bytes.extend(header)
            cpio_bytes.extend(filename_bytes)
            pad_hdr = (4 - (len(header) + namesize) % 4) % 4
            cpio_bytes.extend(b'\x00' * pad_hdr)

            if filesize > 0:
                cpio_bytes.extend(file_data)
                pad_data = (4 - filesize % 4) % 4
                cpio_bytes.extend(b'\x00' * pad_data)

    # Now add our injected files
    for name, (file_mode, file_data) in injected_files.items():
        filesize = len(file_data)
        filename_bytes = name.encode('utf-8') + b'\x00'
        namesize = len(filename_bytes)
        header = f"070701{ino:08X}{file_mode:08X}{0:08X}{0:08X}{1:08X}{1700000000:08X}{filesize:08X}{0:08X}{0:08X}{0:08X}{0:08X}{namesize:08X}{0:08X}".encode('ascii')
        ino += 1

        cpio_bytes.extend(header)
        cpio_bytes.extend(filename_bytes)
        pad_hdr = (4 - (len(header) + namesize) % 4) % 4
        cpio_bytes.extend(b'\x00' * pad_hdr)

        if filesize > 0:
            cpio_bytes.extend(file_data)
            pad_data = (4 - filesize % 4) % 4
            cpio_bytes.extend(b'\x00' * pad_data)

    # Trailer
    trailer_name = b"TRAILER!!!\x00"
    trailer_namesize = len(trailer_name)
    trailer_hdr = f"070701{0:08X}{0:08X}{0:08X}{0:08X}{1:08X}{0:08X}{0:08X}{0:08X}{0:08X}{0:08X}{0:08X}{trailer_namesize:08X}{0:08X}".encode('ascii')
    cpio_bytes.extend(trailer_hdr)
    cpio_bytes.extend(trailer_name)
    pad_tr = (4 - (len(trailer_hdr) + trailer_namesize) % 4) % 4
    cpio_bytes.extend(b'\x00' * pad_tr)
    pad_end = (512 - len(cpio_bytes) % 512) % 512
    cpio_bytes.extend(b'\x00' * pad_end)

    print(f"Generated CPIO from tarball + YKN injected files: {len(cpio_bytes)} bytes ({ino} files).")
    with gzip.open(out_gz, 'wb', compresslevel=6) as gz:
        gz.write(cpio_bytes)
    print(f"Compressed to {out_gz}: {os.path.getsize(out_gz)} bytes")

if __name__ == '__main__':
    build_yknos_initramfs()
