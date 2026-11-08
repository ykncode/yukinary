"""Minimal end-to-end JSON-RPC smoke test for the Yukinary LSP."""
import json
import os
import subprocess
import sys
import threading
from queue import Empty, Queue


def message(method, params=None, request_id=None):
    payload = {"jsonrpc": "2.0"}
    if request_id is not None:
        payload["id"] = request_id
    payload["method"] = method
    if params is not None:
        payload["params"] = params
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    return b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body


def reader(stream, queue):
    pending = b""
    while True:
        chunk = os.read(stream.fileno(), 4096)
        if not chunk:
            return
        pending += chunk
        while b"\r\n\r\n" in pending:
            header, pending = pending.split(b"\r\n\r\n", 1)
            length = None
            for line in header.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1].strip())
            if length is None:
                raise AssertionError("Content-Length header missing")
            while len(pending) < length:
                more = os.read(stream.fileno(), 4096)
                if not more:
                    raise AssertionError("LSP response body truncated")
                pending += more
            raw, pending = pending[:length], pending[length:]
            queue.put(json.loads(raw.decode("utf-8")))


def receive(queue, request_id=None, method=None):
    for _ in range(20):
        try:
            item = queue.get(timeout=3)
        except Empty as exc:
            raise AssertionError("LSP response timeout") from exc
        if request_id is not None and item.get("id") == request_id:
            return item
        if method is not None and item.get("method") == method:
            return item
    raise AssertionError("Expected LSP message not received")


def main():
    if len(sys.argv) != 2:
        print("usage: 01_protocol.py <lsp-executable>", file=sys.stderr)
        return 2
    executable = os.path.abspath(sys.argv[1])
    source = "isv ana() -> tamsayi {\n    degisken sayi: tamsayi = 42;\n    dondu sayi;\n}\n"
    uri = "file:///C:/yukinary/lsp-smoke.ykn"
    proc = subprocess.Popen(
        [executable, "--lsp"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=0x08000000 if os.name == "nt" else 0,
    )
    assert proc.stdin is not None
    assert proc.stdout is not None
    assert proc.stderr is not None
    stdin = proc.stdin
    stdout = proc.stdout
    stderr_stream = proc.stderr

    queue = Queue()
    thread = threading.Thread(target=reader, args=(stdout, queue), daemon=True)
    thread.start()

    def send(method, params=None, request_id=None):
        stdin.write(message(method, params, request_id))
        stdin.flush()

    try:
        send("initialize", {"processId": os.getpid(), "rootUri": None, "capabilities": {}}, 1)
        initialized = receive(queue, request_id=1)
        capabilities = initialized.get("result", {}).get("capabilities", {})
        assert capabilities.get("hoverProvider") is True
        assert capabilities.get("definitionProvider") is True

        send("initialized", {})
        send("textDocument/didOpen", {"textDocument": {
            "uri": uri, "languageId": "yukinary", "version": 1, "text": source
        }})
        diagnostics = receive(queue, method="textDocument/publishDiagnostics")
        assert diagnostics["params"]["uri"] == uri
        assert diagnostics["params"]["diagnostics"] == []

        send("textDocument/completion", {"textDocument": {"uri": uri}, "position": {"line": 1, "character": 4}}, 2)
        completion = receive(queue, request_id=2)
        labels = {item["label"] for item in completion["result"]["items"]}
        assert "degisken" in labels

        send("textDocument/hover", {"textDocument": {"uri": uri}, "position": {"line": 1, "character": 14}}, 3)
        hover = receive(queue, request_id=3)
        hover_result = hover.get("result")
        assert hover_result is not None
        assert hover_result["contents"]["kind"] == "markdown"
        assert "Degisken tanimi" in hover_result["contents"]["value"]

        send("textDocument/definition", {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 10}}, 4)
        definition = receive(queue, request_id=4)
        location = definition.get("result")
        assert location is not None
        assert location["uri"] == uri
        assert location["range"]["start"] == {"line": 1, "character": 13}

        # Unicode identifier positions use UTF-16 code units, not UTF-8 bytes.
        unicode_uri = "file:///C:/yukinary/lsp-unicode.ykn"
        unicode_source = (
            "isv ana() -> tamsayi\n"
            "{\n"
            "    degisken say\u0131: tamsayi = 42;\n"
            "    dondu say\u0131;\n"
            "}\n"
        )
        # Keep the declaration on line 1, matching the regular fixture shape.
        unicode_source = unicode_source.replace("tamsayi\n{", "tamsayi {", 1)
        send("textDocument/didOpen", {"textDocument": {
            "uri": unicode_uri, "languageId": "yukinary", "version": 1, "text": unicode_source
        }})
        unicode_diagnostics = receive(queue, method="textDocument/publishDiagnostics")
        assert unicode_diagnostics["params"]["diagnostics"] == []

        # `sayı` starts at UTF-16 character 13 and has four UTF-16 units.
        send("textDocument/hover", {"textDocument": {"uri": unicode_uri}, "position": {"line": 1, "character": 14}}, 7)
        unicode_hover = receive(queue, request_id=7)
        assert unicode_hover.get("result") is not None
        assert "Degisken tanimi" in unicode_hover["result"]["contents"]["value"]

        send("textDocument/definition", {"textDocument": {"uri": unicode_uri}, "position": {"line": 2, "character": 10}}, 8)
        unicode_definition = receive(queue, request_id=8)
        assert unicode_definition["result"]["range"]["start"] == {"line": 1, "character": 13}
        assert unicode_definition["result"]["range"]["end"] == {"line": 1, "character": 17}

        send("textDocument/diagnostic", {"textDocument": {"uri": uri}}, 5)
        pulled = receive(queue, request_id=5)
        assert pulled["result"]["kind"] == "full"
        assert pulled["result"]["items"] == []

        invalid_uri = "file:///C:/yukinary/lsp-invalid.ykn"
        invalid_source = "isv ana() -> tamsayi {\n    degisken sayi: tamsayi = ;\n    dondu sayi;\n}\n"
        send("textDocument/didOpen", {"textDocument": {
            "uri": invalid_uri, "languageId": "yukinary", "version": 1, "text": invalid_source
        }})
        invalid_diagnostics = receive(queue, method="textDocument/publishDiagnostics")
        invalid_items = invalid_diagnostics["params"]["diagnostics"]
        assert invalid_diagnostics["params"]["uri"] == invalid_uri
        assert invalid_items
        assert invalid_items[0]["severity"] == 1
        assert invalid_items[0]["source"] == "yukinary"

        send("shutdown", {}, 6)
        receive(queue, request_id=6)
        send("exit", {})
        proc.wait(timeout=5)
        print("LSP-PROTOCOL-PASS")
        return 0
    finally:
        if proc.poll() is None:
            proc.kill()
        stderr = stderr_stream.read().decode("utf-8", errors="replace")
        if proc.returncode not in (0, None):
            print(stderr[-2000:], file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
