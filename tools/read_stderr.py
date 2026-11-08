import subprocess
p = subprocess.Popen(['build/dbg_stage2.exe', '--lsp'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
body = b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
msg = b'Content-Length: ' + str(len(body)).encode('ascii') + b'\r\n\r\n' + body
p.stdin.write(msg)
p.stdin.flush()
try:
    p.wait(timeout=2)
except subprocess.TimeoutExpired:
    p.kill()
out, err = p.communicate()
print('STDOUT len:', len(out), repr(out))
print('STDERR len:', len(err), repr(err))
