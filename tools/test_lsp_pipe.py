import subprocess
import sys

exe = sys.argv[1] if len(sys.argv) > 1 else 'build/bootstrap/stage3.exe'
print("Testing:", exe)
p = subprocess.Popen(
    [exe, '--lsp'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE
)

body = b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
msg = b'Content-Length: ' + str(len(body)).encode('ascii') + b'\r\n\r\n' + body

p.stdin.write(msg)
p.stdin.flush()

try:
    header = b""
    while b"\r\n\r\n" not in header:
        ch = p.stdout.read(1)
        if not ch:
            break
        header += ch
    print("Header:", header)
    if b"Content-Length:" in header:
        length = int(header.split(b"Content-Length:")[1].split(b"\r\n")[0].strip())
        content = p.stdout.read(length)
        print("Content:", content.decode())
    p.kill()
except Exception as e:
    print("Exception:", e)
    p.kill()

