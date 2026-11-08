#!/usr/bin/env python3
"""Acceptance tests for the configured, real converter execution contract."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.convert_engine import ConvertEngine, ConversionUnsupported, ProcessExecutionError, parse_directive_line


def gpp() -> str:
    found = shutil.which("g++")
    if found:
        return found
    known = Path(r"C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe")
    if known.is_file():
        return str(known)
    raise RuntimeError("g++ is required for the native directive execution test")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="ykn-directive-exec-") as temp:
        work = Path(temp)
        fixture = ROOT / "tests" / "fallback" / "dynamic_fixture.c"
        library = work / ("fixture.dll" if os.name == "nt" else "fixture.so")
        command = [gpp(), "-shared", "-O2", "-o", str(library), str(fixture)]
        if os.name != "nt":
            command.insert(1, "-fPIC")
        built = subprocess.run(command, capture_output=True, text=True)
        if built.returncode != 0 or not library.is_file():
            print(built.stdout, built.stderr, file=sys.stderr)
            return 1

        (work / "fixture.c").write_text(fixture.read_text(encoding="utf-8"), encoding="utf-8")
        py = work / "module.py"
        py.write_text("def add(a, b):\n    return a + b\n", encoding="utf-8")
        js = work / "module.js"
        js.write_text("exports.add = (a, b) => a + b;\n", encoding="utf-8")
        (work / "module.cs").write_text("public static int Add(int a, int b) { return a + b; }\n", encoding="utf-8")

        engine = ConvertEngine(cache_dir=str(work / "cache"))
        dynamic = parse_directive_line(
            f'#C-convert kaynak="fixture.c", mod="HARICI_DINAMIK", kutuphane="{library}", islev="dyn_add", argumanlar=[19,23];',
            1,
        )
        assert dynamic is not None
        if engine.execute_directive(dynamic, str(work))["value"] != 42:
            return 2

        python_directive = parse_directive_line(
            f'#PY-convert kaynak="module.py", mod="PYTHON_EMBED", kutuphane="{py}", islev="add", argumanlar=[19,23];',
            2,
        )
        assert python_directive is not None
        if engine.execute_directive(python_directive, str(work))["value"] != 42:
            return 3

        js_directive = parse_directive_line(
            f'#JS-convert kaynak="module.js", mod="JS_RPC_IPC", kutuphane="{js}", islev="add", argumanlar=[19,23];',
            3,
        )
        assert js_directive is not None
        if engine.execute_directive(js_directive, str(work))["value"] != 42:
            return 4

        shell_directive = parse_directive_line(
            f'#CS-convert kaynak="module.cs", mod="SHELL_SYSTEM", komut={json.dumps([sys.executable, "-c", "print(42)"])}, timeout=2;',
            4,
        )
        assert shell_directive is not None
        shell_result = engine.execute_directive(shell_directive, str(work))["value"]
        if shell_result["exit_code"] != 0 or shell_result["stdout"].strip() != "42":
            return 5

        bad = parse_directive_line(
            '#C-convert kaynak="fixture.c", mod="HARICI_DINAMIK", kutuphane="missing.dll", islev="missing", argumanlar=[1];',
            5,
        )
        assert bad is not None
        try:
            engine.execute_directive(bad, str(work))
            return 6
        except (ConversionUnsupported, ProcessExecutionError):
            pass

    print("DIRECTIVE_EXECUTION_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
