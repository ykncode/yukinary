#!/usr/bin/env python3
"""Real HARICI_DINAMIK backend acceptance test."""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from convert_engine import DynamicLibrary, DynamicLibraryError


def compiler_path() -> str:
    found = shutil.which("g++")
    if found:
        return found
    known = Path(r"C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe")
    if known.is_file():
        return str(known)
    raise RuntimeError("g++ is required for the real dynamic fixture")


def main() -> int:
    fixture = ROOT / "tests" / "fallback" / "dynamic_fixture.c"
    with tempfile.TemporaryDirectory(prefix="ykn-dynamic-") as temp:
        temp_path = Path(temp)
        suffix = ".dll" if os.name == "nt" else ".so"
        library = temp_path / f"dynamic_fixture{suffix}"
        command = [compiler_path(), "-shared", "-O2", "-o", str(library), str(fixture)]
        if os.name != "nt":
            command.insert(1, "-fPIC")
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode != 0 or not library.is_file():
            print(result.stdout, result.stderr, file=sys.stderr)
            return 1

        loader = DynamicLibrary(str(library))
        if loader.call_i64("dyn_add", 19, 23) != 42:
            return 2
        try:
            loader.call_i64("missing_symbol", 1, 2)
            return 3
        except DynamicLibraryError as exc:
            if "E036" not in str(exc):
                return 4
        loader.close()
        try:
            loader.call_i64("dyn_add", 1, 2)
            return 5
        except DynamicLibraryError as exc:
            if "already closed" not in str(exc):
                return 6
    print("DYNAMIC_BACKEND_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
