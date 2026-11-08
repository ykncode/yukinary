import subprocess
import json
import os
import time
import sys
import threading
from queue import Queue, Empty

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

def read_message(proc, out_q, stop_event):
    buf = b""
    while not stop_event.is_set():
        try:
            chunk = proc.stdout.read(4096)
            if not chunk:
                break
            buf += chunk
            while b"\r\n\r\n" in buf:
                header, rest = buf.split(b"\r\n\r\n", 1)
                cl = None
                for line in header.split(b"\r\n"):
                    if line.lower().startswith(b"content-length"):
                        cl = int(line.split(b":",1)[1].strip())
                if cl is None:
                    buf = rest
                    continue
                if len(rest) >= cl:
                    raw = rest[:cl]
                    buf = rest[cl:]
                    try:
                        obj = json.loads(raw.decode("utf-8"))
                        out_q.put(obj)
                    except Exception as e:
                        print("[PARSE ERR]", e)
                        print(raw[:200])
                else:
                    break
        except Exception as e:
            print("[READ ERR]", e)
            break

def get_msg(q, timeout=15):
    try:
        return q.get(timeout=timeout)
    except Empty:
        return None

def main():
    base = os.path.dirname(os.path.abspath(__file__))
    exe = os.path.join(base, "out_lsp.exe")
    test_path = os.path.join(base, "test_lsp.ykn")
    uri = "file:///" + test_path.replace("\\", "/")
    
    with open(test_path, "r", encoding="utf-8") as f:
        source = f.read()
    
    print("[SPAWN]", exe, "--lsp")
    proc = subprocess.Popen(
        [exe, "--lsp"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        creationflags=0x08000000
    )
    
    q = Queue()
    stop = threading.Event()
    t = threading.Thread(target=read_message, args=(proc, q, stop), daemon=True)
    t.start()
    
    def send(msg):
        proc.stdin.write(msg)
        proc.stdin.flush()
    
    # 1) INITIALIZE
    print("\n=== [1] Sending initialize (id=1)")
    send(build_msg("initialize", {
        "processId": os.getpid(),
        "rootUri": None,
        "capabilities": {}
    }, 1))
    r = get_msg(q, 20)
    if r:
        print("   ↓ RECV id=", r.get("id"), "method=", r.get("method"))
        if "result" in r:
            caps = r["result"].get("capabilities", {})
            print("   CAPABILITIES:")
            for k,v in caps.items():
                print("     -", k, ":", json.dumps(v)[:120])
        else:
            print("   RAW:", json.dumps(r)[:500])
    else:
        print("   [TIMEOUT] initialize yanit yok")
        try: proc.terminate()
        except: pass
        stop.set()
        return 1
    
    # 2) INITIALIZED notification
    print("\n=== [2] Sending initialized notification")
    send(build_msg("initialized", {}))
    time.sleep(1)
    
    # 3) textDocument/didOpen
    print("\n=== [3] Sending textDocument/didOpen")
    send(build_msg("textDocument/didOpen", {
        "textDocument": {
            "uri": uri,
            "languageId": "yukinary",
            "version": 1,
            "text": source
        }
    }))
    time.sleep(2)
    
    # publishDiagnostics notification bekle
    diag_notif = None
    for _ in range(5):
        m = get_msg(q, 2)
        if m and m.get("method") == "textDocument/publishDiagnostics":
            diag_notif = m
            break
    if diag_notif:
        diags = diag_notif["params"].get("diagnostics", [])
        print("   ↓ publishDiagnostics:", len(diags), "adet hata")
        for d in diags[:5]:
            s = d.get('range',{}).get('start',{})
            print("     - [", d.get('severity'), "] L", s.get('line','?'), ":", str(d.get('message',''))[:120])
    else:
        print("   [NOT] publishDiagnostics notification gelmedi (varsayilan temiz)")
    
    # 4) textDocument/completion (id=2)
    print("\n=== [4] Sending textDocument/completion (id=2, line 12, char 4)")
    send(build_msg("textDocument/completion", {
        "textDocument": {"uri": uri},
        "position": {"line": 11, "character": 4},
        "context": {"triggerKind": 1}
    }, 2))
    r = get_msg(q, 15)
    items = []
    if r and "result" in r:
        items = r["result"].get("items", r["result"] if isinstance(r["result"], list) else [])
    print("   ↓ Completion items sayisi:", len(items))
    for it in items[:15]:
        print("     - [", it.get('kind'), "]", it.get('label'), " ::", str(it.get('detail',''))[:80])
    
    # 5) textDocument/hover (id=3) - 'topla' fonksiyonu uzerinde
    print("\n=== [5] Sending textDocument/hover (id=3, line 12 char 32, 'topla')")
    send(build_msg("textDocument/hover", {
        "textDocument": {"uri": uri},
        "position": {"line": 12, "character": 32}
    }, 3))
    r = get_msg(q, 10)
    if r and "result" in r and r["result"] is not None:
        cont = r["result"].get("contents", {})
        if isinstance(cont, dict):
            print("   ↓ Hover:", str(cont.get("value", cont))[:300])
        else:
            print("   ↓ Hover:", str(cont)[:300])
    else:
        print("   [YOK] hover yaniti:", json.dumps(r)[:200] if r else "YANIT YOK")
    
    # 6) textDocument/definition (id=4) - 'sayi' degiskeni
    print("\n=== [6] Sending textDocument/definition (id=4, 'sayi' tanimi)")
    send(build_msg("textDocument/definition", {
        "textDocument": {"uri": uri},
        "position": {"line": 12, "character": 41}
    }, 4))
    r = get_msg(q, 10)
    if r and "result" in r and r["result"]:
        res = r["result"]
        if isinstance(res, list): res = res[0] if res else None
        if res:
            print("   ↓ Definition uri=", str(res.get('uri','?'))[:80])
            rg = res.get("range", {})
            print("     satir=", rg.get('start',{}).get('line'), "-", rg.get('end',{}).get('line'))
    else:
        print("   [YOK] definition yaniti:", json.dumps(r)[:200] if r else "YANIT YOK")
    
    # 7) SHUTDOWN + EXIT
    print("\n=== [7] Sending shutdown + exit")
    send(build_msg("shutdown", {}, 99))
    time.sleep(0.5)
    send(build_msg("exit", {}))
    time.sleep(1)
    
    try:
        proc.terminate()
    except:
        pass
    stop.set()
    print("\n[BITTI]")
    return 0

if __name__ == "__main__":
    sys.exit(main())
