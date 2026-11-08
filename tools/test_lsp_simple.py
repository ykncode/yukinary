import subprocess
import os
import json
import time

def build_msg(method=None, params=None, req_id=None):
    msg = {"jsonrpc": "2.0"}
    if req_id is not None:
        msg["id"] = req_id
    if method is not None:
        msg["method"] = method
    if params is not None:
        msg["params"] = params
    body = json.dumps(msg)
    header = "Content-Length: " + str(len(body)) + "\r\n\r\n"
    return (header + body).encode("utf-8")

def main():
    base = os.path.dirname(os.path.abspath(__file__))
    exe = os.path.join(base, "out_lsp.exe")

    # === TEST A: --lsp + HEMEN stdin kapat (sadece ilk buffer testi)
    print("=== TEST A: --lsp stdin bos kapatiliyor (communicate timeout=3sn)")
    try:
        p = subprocess.Popen(
            [exe, "--lsp"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=0x08000000
        )
        # Hemen stdin'i kapat (EOF gonder)
        p.stdin.close()
        stdout, stderr = p.communicate(timeout=3)
        print(f"  exit={p.returncode}")
        if stdout: print(f"  STDOUT ({len(stdout)}b): {stdout[:500]}")
        if stderr: print(f"  STDERR ({len(stderr)}b):\n{stderr.decode(errors='replace')[:1500]}")
    except subprocess.TimeoutExpired:
        p.kill()
        try: stdout, stderr = p.communicate(timeout=2)
        except: stdout, stderr = b"", b""
        print(f"  TIMEOUT - OLDURULDU")
        if stdout: print(f"  STDOUT ({len(stdout)}b): {stdout[:500]}")
        if stderr: print(f"  STDERR ({len(stderr)}b):\n{stderr.decode(errors='replace')[:2000]}")

    # === TEST B: Ilk kez initialize gonder + sonra kapat
    print("\n=== TEST B: SADECE initialize (id=1) gonder")
    msg_init = build_msg("initialize", {
        "processId": 1, "rootUri": None, "capabilities": {}
    }, 1)
    # Header + body hex dump
    print(f"  Gonderilen ilk mesaj ({len(msg_init)} bytes):")
    # Header ve body ayir
    hdr_end = msg_init.find(b"\r\n\r\n")
    if hdr_end > 0:
        hdr = msg_init[:hdr_end]
        body = msg_init[hdr_end+4:]
        print(f"  HEADER: {hdr!r}")
        print(f"  BODY  : {body.decode()}")
        print(f"  BODY  : hex({body.hex()})")
    try:
        p = subprocess.Popen(
            [exe, "--lsp"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=0x08000000
        )
        p.stdin.write(msg_init)
        p.stdin.flush()
        # Hemen stdin kapat + timeout
        p.stdin.close()
        stdout, stderr = p.communicate(timeout=8)
        print(f"  exit={p.returncode}")
        if stdout:
            print(f"  STDOUT ({len(stdout)}b):")
            # stdout header/body ayir
            so_hdr_end = stdout.find(b"\r\n\r\n")
            if so_hdr_end > 0:
                print(f"    [Header] : {stdout[:so_hdr_end]}")
                so_body = stdout[so_hdr_end+4:]
                print(f"    [Body  ] : {so_body.decode(errors='replace')}")
            else:
                print(f"    {stdout[:1000]}")
        if stderr: print(f"  STDERR ({len(stderr)}b):\n{stderr.decode(errors='replace')[:4000]}")
    except subprocess.TimeoutExpired:
        p.kill()
        try: stdout, stderr = p.communicate(timeout=2)
        except: stdout, stderr = b"", b""
        print(f"  TIMEOUT - OLDURULDU")
        if stdout: print(f"  STDOUT ({len(stdout)}b): {stdout[:1000]}")
        if stderr: print(f"  STDERR ({len(stderr)}b):\n{stderr.decode(errors='replace')[:4000]}")

if __name__ == "__main__":
    main()
