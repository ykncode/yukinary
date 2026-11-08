#!/usr/bin/env python3
"""Real shell and Node JSON-RPC fallback acceptance tests."""

import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from convert_engine import NodeJsonRpcClient, ProcessExecutionError, ShellProcess


def main() -> int:
    shell = ShellProcess()
    shell_result = shell.run([sys.executable, "-c", "import sys; print('out'); print('err', file=sys.stderr)"])
    if not shell_result["ok"] or shell_result["exit_code"] != 0:
        return 1
    if shell_result["stdout"].strip() != "out" or shell_result["stderr"].strip() != "err":
        return 2
    failed = shell.run([sys.executable, "-c", "raise SystemExit(7)"])
    if failed["exit_code"] != 7 or failed["ok"]:
        return 3
    try:
        shell.run([sys.executable, "-c", "import time; time.sleep(2)"], timeout=0.05)
        return 4
    except ProcessExecutionError:
        pass

    node = shutil.which("node")
    if not node:
        raise RuntimeError("node is required for the real JSON-RPC acceptance test")
    with tempfile.TemporaryDirectory(prefix="ykn-node-rpc-") as temp:
        script = Path(temp) / "rpc.js"
        script.write_text(
            "const readline = require('readline');\n"
            "const rl = readline.createInterface({input: process.stdin});\n"
            "rl.on('line', line => { const r = JSON.parse(line);\n"
            "if (r.method === 'add') console.log(JSON.stringify({jsonrpc:'2.0',id:r.id,result:{value:r.params.a+r.params.b}}));\n"
            "else console.log(JSON.stringify({jsonrpc:'2.0',id:r.id,error:{code:-32601,message:'method not found'}}));\n"
            "});\n",
            encoding="utf-8",
        )
        client = NodeJsonRpcClient(node, str(script))
        if client.request("add", {"a": 19, "b": 23})["value"] != 42:
            return 5
        if client.request("add", {"a": -2, "b": 7})["value"] != 5:
            return 6
        try:
            client.request("missing", {})
            return 7
        except ProcessExecutionError as exc:
            if "E036" not in str(exc):
                return 8
        client.close()
        try:
            client.request("add", {"a": 1, "b": 1})
            return 9
        except ProcessExecutionError:
            pass

    print("PROCESS_BACKENDS_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
