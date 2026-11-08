import subprocess
import os
import time
import threading
from queue import Queue, Empty

def main():
    base = os.path.dirname(os.path.abspath(__file__))
    exe = os.path.join(base, "out_lsp.exe")

    print("[TEST 1] out_lsp.exe --lsp spawn ediliyor (sadece stdout kontrolu, stdin bos)")
    proc = subprocess.Popen(
        [exe, "--lsp"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        creationflags=0x08000000
    )

    # 2 saniye boyunca stdout oku
    stdout_data = b""
    start = time.time()
    while time.time() - start < 2:
        try:
            chunk = proc.stdout.read(4096)
            if not chunk:
                break
            stdout_data += chunk
        except:
            break
    try:
        proc.terminate()
    except:
        pass
    proc.wait()

    if len(stdout_data) > 0:
        print(f"  STDOUT ALINDI ({len(stdout_data)} bytes):", repr(stdout_data[:500]))
    else:
        print(f"  2 sn SONRA STDOUT BOS (exit code={proc.returncode})")

    print("\n[TEST 2] Elle ham byte initialize gonder (dogrudan byte seviyesi)")
    
    # JSON body
    body = b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":1,"rootUri":null,"capabilities":{}}}'
    header = ("Content-Length: " + str(len(body)) + "\r\n\r\n").encode("utf-8")
    full_msg = header + body
    print(f"  Gonderilecek: header={len(header)} byte, body={len(body)} byte, TOPLAM={len(full_msg)} byte")
    print(f"  Header hex: {header.hex()}")
    print(f"  Body: {body.decode()}")

    proc2 = subprocess.Popen(
        [exe, "--lsp"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        creationflags=0x08000000
    )

    # Mesaji yaz ve flush et
    proc2.stdin.write(full_msg)
    proc2.stdin.flush()
    # shutdown + exit yaz
    sd = b'{"jsonrpc":"2.0","id":2,"method":"shutdown"}\r\n\r\n{"jsonrpc":"2.0","method":"exit"}'
    sd_header = ("Content-Length: " + str(len(sd) - len(b'\r\n\r\n{"jsonrpc":"2.0","method":"exit"}')) + "\r\n\r\n").encode("utf-8")
    # Daha temiz: sadece shutdown gonder
    time.sleep(0.5)
    shutdown_body = b'{"jsonrpc":"2.0","id":99,"method":"shutdown"}'
    shutdown_full = ("Content-Length: " + str(len(shutdown_body)) + "\r\n\r\n").encode("utf-8") + shutdown_body
    try:
        proc2.stdin.write(shutdown_full)
        proc2.stdin.flush()
    except Exception as e:
        print(f"  shutdown yazilamadi: {e}")
    
    # 5 saniye boyunca tum ciktilari oku
    recv = b""
    t0 = time.time()
    while time.time() - t0 < 5:
        try:
            chunk = proc2.stdout.read(4096)
            if not chunk:
                break
            recv += chunk
            # Header gelmis mi kontrol et
            if b"\r\n\r\n" in recv:
                print(f"  [VERI ALINDI] Toplam {len(recv)} byte")
                break
        except Exception as e:
            print(f"  Okuma hatasi: {e}")
            break
    
    try:
        proc2.terminate()
    except:
        pass
    proc2.wait()

    if len(recv) > 0:
        print(f"\n[SONUC] {len(recv)} byte alindi:")
        # Header ve body ayir
        if b"\r\n\r\n" in recv:
            header_part, body_part = recv.split(b"\r\n\r\n", 1)
            print("  HEADER:", header_part.decode(errors="replace"))
            # Content-Length al
            try:
                for line in header_part.split(b"\r\n"):
                    if line.lower().startswith(b"content-length"):
                        cl = int(line.split(b":",1)[1].strip())
                        print(f"  Content-Length={cl}, body alinan={len(body_part)}")
                        if len(body_part) >= cl:
                            actual_body = body_part[:cl]
                            print(f"  BODY JSON: {actual_body.decode(errors='replace')}")
                        else:
                            print(f"  Body eksik: {body_part.decode(errors='replace')}")
            except Exception as e:
                print(f"  Header parse err: {e}")
                print(f"  Ham veri: {recv.decode(errors='replace')[:1500]}")
        else:
            print(f"  Header yok, ham: {recv[:800]}")
    else:
        print("\n[SONUC] HIC BYTE ALINMADI (timeout 5sn)")
        print(f"  exit code={proc2.returncode}")

if __name__ == "__main__":
    main()
