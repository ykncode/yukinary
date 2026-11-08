#!/usr/bin/env python3
"""
tools/convert_engine.py
Yukinary (YKN) Multi-Language Convert Engine & Directive Parser — FAZ 9.2 - 9.5

Directives:
  #C-convert, #PY-convert, #JS-convert, #CS-convert, #RB-convert

Fallback Modes:
  1. HARICI_STATIK: Static FFI declarations (harici isv)
  2. HARICI_DINAMIK: Dynamic loading with LoadLibraryA/dlopen
  3. PYTHON_EMBED: Embedded Python runtime bridge
  4. JS_RPC_IPC: Node.js JSON-RPC / IPC process bridge
  5. SHELL_SYSTEM: Process spawn, stdin/stdout/stderr capture

Features:
  - Phase-0 directive scanner and parser with [E036] diagnostic on invalid syntax
  - Function success ratio calculation and threshold decision engine (default 0.8)
  - Safe YKN generation, type and ABI mapping
  - Two-level SHA-256 cache (.ykn_cache/convert/) with cache hit/miss reporting
  - Deterministic cached output and transparency report
"""

import os

import re
import json
import hashlib
import time
import ctypes
import _ctypes
import queue
import subprocess
import threading
import importlib.util
import sys
import tempfile
import shutil
from typing import Dict, List, Optional, Tuple, Any


class DynamicLibraryError(RuntimeError):
    """A real dynamic-loader or ABI error; never converted to a fake result."""


class ProcessExecutionError(RuntimeError):
    """A process, timeout, exit-code, or protocol failure."""


class ShellProcess:
    """Shell fallback without shell interpretation or placeholder results."""

    def run(self, argv: List[str], input_text: str = "", timeout: float = 30.0) -> Dict[str, Any]:
        if not argv or any(not isinstance(item, str) or not item for item in argv):
            raise ProcessExecutionError("E036 [CONVERT_PROCESS_ARGS]: argv must be a non-empty string list")
        try:
            completed = subprocess.run(
                argv,
                input=input_text,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                shell=False,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise ProcessExecutionError(
                f"E036 [CONVERT_PROCESS_TIMEOUT]: command exceeded {timeout:.3f}s"
            ) from exc
        except OSError as exc:
            raise ProcessExecutionError(f"E035 [CONVERT_PROCESS_START]: {exc}") from exc
        return {
            "argv": list(argv),
            "exit_code": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
            "ok": completed.returncode == 0,
        }


class NodeJsonRpcClient:
    """Line-delimited JSON-RPC client for a real Node child process."""

    def __init__(self, node_path: str, script_path: str):
        if not node_path or not script_path or not os.path.isfile(script_path):
            raise ProcessExecutionError("E035 [CONVERT_NODE_START]: node or script path is invalid")
        try:
            self._process = subprocess.Popen(
                [node_path, script_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                shell=False,
            )
        except OSError as exc:
            raise ProcessExecutionError(f"E035 [CONVERT_NODE_START]: {exc}") from exc
        self._responses: queue.Queue[str] = queue.Queue()
        self._stderr: queue.Queue[str] = queue.Queue()
        self._reader = threading.Thread(target=self._read_lines, args=(self._process.stdout, self._responses), daemon=True)
        self._error_reader = threading.Thread(target=self._read_lines, args=(self._process.stderr, self._stderr), daemon=True)
        self._reader.start()
        self._error_reader.start()
        self._next_id = 1
        self._closed = False

    @staticmethod
    def _read_lines(stream, target: queue.Queue[str]) -> None:
        if stream is None:
            return
        for line in stream:
            target.put(line)

    def request(self, method: str, params: Optional[Dict[str, Any]] = None, timeout: float = 5.0) -> Dict[str, Any]:
        if self._closed or self._process.poll() is not None:
            raise ProcessExecutionError("E036 [CONVERT_NODE_STATE]: Node process is not running")
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_.-]*", method):
            raise ProcessExecutionError("E036 [CONVERT_NODE_METHOD]: invalid JSON-RPC method")
        request_id = self._next_id
        self._next_id += 1
        payload = {"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}}
        try:
            if self._process.stdin is None:
                raise ProcessExecutionError("E036 [CONVERT_NODE_IO]: stdin pipe is unavailable")
            self._process.stdin.write(json.dumps(payload, separators=(",", ":")) + "\n")
            self._process.stdin.flush()
            line = self._responses.get(timeout=timeout)
        except queue.Empty as exc:
            raise ProcessExecutionError(f"E036 [CONVERT_NODE_TIMEOUT]: request {request_id} timed out") from exc
        except (OSError, AttributeError) as exc:
            raise ProcessExecutionError(f"E036 [CONVERT_NODE_IO]: {exc}") from exc
        try:
            response = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ProcessExecutionError("E036 [CONVERT_NODE_PROTOCOL]: invalid JSON response") from exc
        if response.get("id") != request_id or response.get("jsonrpc") != "2.0":
            raise ProcessExecutionError("E036 [CONVERT_NODE_PROTOCOL]: response id/version mismatch")
        if "error" in response:
            raise ProcessExecutionError(f"E036 [CONVERT_NODE_REMOTE]: {response['error']}")
        if not isinstance(response.get("result"), dict):
            raise ProcessExecutionError("E036 [CONVERT_NODE_PROTOCOL]: result must be an object")
        return response["result"]

    def close(self, timeout: float = 5.0) -> None:
        if self._closed:
            return
        self._closed = True
        try:
            if self._process.stdin:
                self._process.stdin.close()
            self._process.wait(timeout=timeout)
        except (OSError, subprocess.TimeoutExpired):
            self._process.kill()
            self._process.wait(timeout=timeout)


class PythonModule:
    """Load and call a Python module from an explicit file path.

    This is intentionally not a source translator: the target callable is
    executed by the host Python runtime and its result is returned or its
    exception is surfaced. The module must be supplied explicitly by the
    directive, so an absent or non-callable symbol cannot become a fake YKN
    function.
    """

    def __init__(self, path: str):
        if not path or not os.path.isfile(path):
            raise ProcessExecutionError(f"E035 [CONVERT_PYTHON_LOAD]: module not found: {path}")
        self.path = os.path.abspath(path)
        module_name = "ykn_convert_" + hashlib.sha256(self.path.encode("utf-8")).hexdigest()[:16]
        spec = importlib.util.spec_from_file_location(module_name, self.path)
        if spec is None or spec.loader is None:
            raise ProcessExecutionError(f"E035 [CONVERT_PYTHON_LOAD]: unable to load module: {path}")
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        try:
            spec.loader.exec_module(module)
        except Exception as exc:
            sys.modules.pop(module_name, None)
            raise ProcessExecutionError(f"E036 [CONVERT_PYTHON_LOAD]: module raised during import: {exc}") from exc
        self._module_name = module_name
        self._module = module
        self._closed = False

    def call(self, name: str, args: List[Any]) -> Any:
        if self._closed:
            raise ProcessExecutionError("E036 [CONVERT_PYTHON_STATE]: module is already closed")
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            raise ProcessExecutionError(f"E036 [CONVERT_PYTHON_SYMBOL]: invalid callable: {name!r}")
        function = getattr(self._module, name, None)
        if not callable(function):
            raise ProcessExecutionError(f"E036 [CONVERT_PYTHON_SYMBOL]: callable not found: {name}")
        try:
            return function(*args)
        except Exception as exc:
            raise ProcessExecutionError(f"E036 [CONVERT_PYTHON_CALL]: {name} raised: {exc}") from exc

    def close(self) -> None:
        if not self._closed:
            sys.modules.pop(self._module_name, None)
            self._module = None
            self._closed = True

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
        return False


class DynamicLibrary:
    """Small, explicit i64,i64 -> i64 dynamic FFI adapter.

    The narrow signature is intentional: conversion cannot infer arbitrary C
    ABI layouts from a source snippet. Unsupported signatures must be rejected
    instead of being called with guessed arguments.
    """

    def __init__(self, path: str):
        if not path or not os.path.isfile(path):
            raise DynamicLibraryError(f"E035 [CONVERT_DYNAMIC_LOAD]: library not found: {path}")
        loader = ctypes.WinDLL if os.name == "nt" else ctypes.CDLL
        try:
            self._library = loader(path)
        except OSError as exc:
            raise DynamicLibraryError(
                f"E035 [CONVERT_DYNAMIC_LOAD]: unable to load '{path}': {exc}"
            ) from exc
        self.path = os.path.abspath(path)
        self._closed = False
        self._symbols: Dict[str, Any] = {}

    def symbol_i64(self, name: str):
        if self._closed:
            raise DynamicLibraryError("E036 [CONVERT_DYNAMIC_SYMBOL]: library is already closed")
        if not name or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            raise DynamicLibraryError(f"E036 [CONVERT_DYNAMIC_SYMBOL]: invalid symbol: {name!r}")
        try:
            function = getattr(self._library, name)
        except AttributeError as exc:
            raise DynamicLibraryError(
                f"E036 [CONVERT_DYNAMIC_SYMBOL]: symbol not found: {name}"
            ) from exc
        function.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
        function.restype = ctypes.c_longlong
        self._symbols[name] = function
        return function

    def call_i64(self, name: str, first: int, second: int) -> int:
        function = self.symbol_i64(name)
        try:
            return int(function(int(first), int(second)))
        except (TypeError, ValueError, OverflowError, OSError) as exc:
            raise DynamicLibraryError(
                f"E036 [CONVERT_DYNAMIC_ABI]: call failed for '{name}': {exc}"
            ) from exc

    def close(self) -> None:
        if self._closed:
            return
        handle = getattr(self._library, "_handle", 0)
        if handle:
            try:
                if os.name == "nt":
                    _ctypes.FreeLibrary(handle)
                else:
                    _ctypes.dlclose(handle)
            except (AttributeError, OSError) as exc:
                raise DynamicLibraryError(
                    f"E036 [CONVERT_DYNAMIC_UNLOAD]: unable to unload '{self.path}': {exc}"
                ) from exc
        self._symbols.clear()
        self._library = None
        self._closed = True

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
        return False

DIRECTIVES = {
    "#C-convert": "C",
    "#PY-convert": "PYTHON",
    "#JS-convert": "JAVASCRIPT",
    "#CS-convert": "CSHARP",
    "#RB-convert": "RUBY"
}

VALID_MODES = [
    "HARICI_STATIK",
    "HARICI_DINAMIK",
    "PYTHON_EMBED",
    "JS_RPC_IPC",
    "SHELL_SYSTEM"
]


class ConversionUnsupported(RuntimeError):
    """Raised when a real backend is not available.

    The converter must never emit a compiling placeholder for an unsupported
    operation. A caller can choose another explicitly implemented backend or
    surface this diagnostic to the user.
    """


class ConvertDirective:
    def __init__(self, directive_type: str, raw_text: str, line_no: int):
        self.directive_type = directive_type
        self.raw_text = raw_text
        self.line_no = line_no
        self.kaynak: str = ""
        self.hedef: str = ""
        self.esik: float = 0.8
        self.mod: str = "HARICI_STATIK"
        self.sadece: List[str] = []
        # Runtime execution contract. These are optional at parse time but
        # mandatory for the backend modes that execute an external target.
        self.kutuphane: str = ""
        self.islev: str = ""
        self.argumanlar: List[Any] = []
        self.komut: List[str] = []
        self.zaman_asimi: float = 30.0
        self.is_valid: bool = False
        self.error_message: str = ""

def parse_directive_line(line: str, line_no: int) -> Optional[ConvertDirective]:
    line_stripped = line.strip()
    matched_dir = None
    for d in DIRECTIVES:
        if line_stripped.startswith(d):
            matched_dir = d
            break
            
    if not matched_dir:
        return None
        
    directive = ConvertDirective(matched_dir, line_stripped, line_no)
    content = line_stripped[len(matched_dir):].strip()
    if content.endswith(";"):
        content = content[:-1].strip()

    # Parse parameters: kaynak="...", hedef="...", esik=0.8, mod="...", sadece=["..."]
    kaynak_m = re.search(r'kaynak\s*=\s*"([^"]+)"', content)
    if not kaynak_m:
        directive.error_message = f"E036 [CONVERT_SYNTAX_ERROR]: '{matched_dir}' requires 'kaynak=\"...\"' parameter"
        return directive
    directive.kaynak = kaynak_m.group(1)

    hedef_m = re.search(r'hedef\s*=\s*"([^"]+)"', content)
    if hedef_m:
        directive.hedef = hedef_m.group(1)
    else:
        base = os.path.splitext(os.path.basename(directive.kaynak))[0]
        directive.hedef = f"{base}_converted.ykn"

    esik_m = re.search(r'esik\s*=\s*([0-9]+(?:\.[0-9]+)?)', content)
    if esik_m:
        try:
            val = float(esik_m.group(1))
            if 0.0 <= val <= 1.0:
                directive.esik = val
            else:
                directive.error_message = "E036 [CONVERT_SYNTAX_ERROR]: 'esik' must be between 0.0 and 1.0"
                return directive
        except ValueError:
            directive.error_message = "E036 [CONVERT_SYNTAX_ERROR]: Invalid 'esik' number format"
            return directive

    mod_m = re.search(r'mod\s*=\s*"([^"]+)"', content)
    if mod_m:
        m_val = mod_m.group(1)
        if m_val in VALID_MODES:
            directive.mod = m_val
        else:
            directive.error_message = f"E036 [CONVERT_SYNTAX_ERROR]: Unknown fallback mode '{m_val}'. Allowed: {', '.join(VALID_MODES)}"
            return directive

    sadece_m = re.search(r'sadece\s*=\s*\[([^\]]*)\]', content)
    if sadece_m:
        raw_items = sadece_m.group(1).split(',')
        items = []
        for it in raw_items:
            it = it.strip().strip('"\'')
            if it:
                items.append(it)
        directive.sadece = items

    # Execution parameters are parsed as JSON literals. This keeps quoting,
    # numbers and argv boundaries unambiguous and prevents shell re-parsing.
    path_m = re.search(r'(?:kutuphane|hedef_yol)\s*=\s*"([^"]+)"', content)
    if path_m:
        directive.kutuphane = path_m.group(1)
    symbol_m = re.search(r'(?:islev|sembol)\s*=\s*"([A-Za-z_][A-Za-z0-9_]*)"', content)
    if symbol_m:
        directive.islev = symbol_m.group(1)
    args_m = re.search(r'argumanlar\s*=\s*(\[[^\]]*\])', content)
    if args_m:
        try:
            parsed_args = json.loads(args_m.group(1))
            if not isinstance(parsed_args, list):
                raise ValueError("argumanlar must be an array")
            directive.argumanlar = parsed_args
        except (json.JSONDecodeError, ValueError) as exc:
            directive.error_message = f"E036 [CONVERT_SYNTAX_ERROR]: invalid argumanlar: {exc}"
            return directive
    command_m = re.search(r'komut\s*=\s*(\[[^\]]*\])', content)
    if command_m:
        try:
            parsed_command = json.loads(command_m.group(1))
            if (not isinstance(parsed_command, list) or
                    any(not isinstance(item, str) or not item for item in parsed_command)):
                raise ValueError("komut must be a non-empty string array")
            directive.komut = parsed_command
        except (json.JSONDecodeError, ValueError) as exc:
            directive.error_message = f"E036 [CONVERT_SYNTAX_ERROR]: invalid komut: {exc}"
            return directive
    timeout_m = re.search(r'(?:zaman_asimi|timeout)\s*=\s*([0-9]+(?:\.[0-9]+)?)', content)
    if timeout_m:
        directive.zaman_asimi = float(timeout_m.group(1))
        if directive.zaman_asimi <= 0:
            directive.error_message = "E036 [CONVERT_SYNTAX_ERROR]: timeout must be positive"
            return directive

    directive.is_valid = True
    return directive

class ConvertEngine:
    def __init__(self, cache_dir: str = ".ykn_cache/convert"):
        self.cache_dir = cache_dir
        os.makedirs(self.cache_dir, exist_ok=True)
        self.transparency_log: List[Dict[str, Any]] = []

    def compute_cache_key(self, source_code: str, directive: ConvertDirective) -> str:
        h = hashlib.sha256()
        h.update(source_code.encode("utf-8"))
        h.update(directive.directive_type.encode("utf-8"))
        h.update(directive.kaynak.encode("utf-8"))
        h.update(str(directive.esik).encode("utf-8"))
        h.update(directive.mod.encode("utf-8"))
        h.update(",".join(sorted(directive.sadece)).encode("utf-8"))
        h.update(directive.kutuphane.encode("utf-8"))
        h.update(directive.islev.encode("utf-8"))
        h.update(json.dumps(directive.argumanlar, sort_keys=True).encode("utf-8"))
        h.update(json.dumps(directive.komut, ensure_ascii=False).encode("utf-8"))
        h.update(str(directive.zaman_asimi).encode("utf-8"))
        h.update(b"ykn-compiler-v1-canonical")
        return h.hexdigest()

    def extract_c_functions(self, content: str) -> List[Dict]:
        funcs = []
        # Match pattern: int func_name(int a, int b) { ... }
        pattern = r'\b(int|double|float|void|char\*)\s+([A-Za-z0-9_]+)\s*\(([^)]*)\)\s*\{([^}]*)\}'
        for m in re.finditer(pattern, content):
            ret_type = m.group(1)
            name = m.group(2)
            args = m.group(3)
            body = m.group(4)
            funcs.append({"name": name, "ret": ret_type, "args": args, "body": body, "lang": "C"})
        return funcs

    def extract_python_functions(self, content: str) -> List[Dict]:
        funcs = []
        # Match pattern: def func_name(a, b): ...
        pattern = r'def\s+([A-Za-z0-9_]+)\s*\(([^)]*)\)\s*(?:->\s*([A-Za-z0-9_]+))?:'
        for m in re.finditer(pattern, content):
            name = m.group(1)
            args = m.group(2)
            ret_type = m.group(3) or "int"
            funcs.append({"name": name, "ret": ret_type, "args": args, "body": "", "lang": "PYTHON"})
        return funcs

    def extract_js_functions(self, content: str) -> List[Dict]:
        funcs = []
        pattern = r'(?:function\s+([A-Za-z0-9_]+)|const\s+([A-Za-z0-9_]+)\s*=\s*(?:function|\([^)]*\)\s*=>))\s*\(([^)]*)\)'
        for m in re.finditer(pattern, content):
            name = m.group(1) or m.group(2)
            args = m.group(3)
            funcs.append({"name": name, "ret": "int", "args": args, "body": "", "lang": "JAVASCRIPT"})
        return funcs

    def extract_cs_functions(self, content: str) -> List[Dict]:
        funcs = []
        pattern = r'(?:public|private|static|\s)+\s+(int|double|string|void)\s+([A-Za-z0-9_]+)\s*\(([^)]*)\)\s*\{'
        for m in re.finditer(pattern, content):
            ret_type = m.group(1)
            name = m.group(2)
            args = m.group(3)
            funcs.append({"name": name, "ret": ret_type, "args": args, "body": "", "lang": "CSHARP"})
        return funcs

    def extract_ruby_functions(self, content: str) -> List[Dict]:
        funcs = []
        pattern = r'def\s+([A-Za-z0-9_]+)(?:\s*\(([^)]*)\)|\s+([A-Za-z0-9_,\s]+))?'
        for m in re.finditer(pattern, content):
            name = m.group(1)
            args = m.group(2) or m.group(3) or ""
            funcs.append({"name": name, "ret": "int", "args": args, "body": "", "lang": "RUBY"})
        return funcs

    def extract_functions(self, lang: str, content: str) -> List[Dict]:
        if lang == "C":
            return self.extract_c_functions(content)
        elif lang == "PYTHON":
            return self.extract_python_functions(content)
        elif lang == "JAVASCRIPT":
            return self.extract_js_functions(content)
        elif lang == "CSHARP":
            return self.extract_cs_functions(content)
        elif lang == "RUBY":
            return self.extract_ruby_functions(content)
        return []

    def _simple_c_return(self, fn: Dict) -> Optional[Tuple[str, str, str]]:
        """Return (arg0, arg1, expression) for a provably simple C function.

        Native conversion is deliberately narrow. Guessing a function body and
        replacing it with ``a + b`` is unsound and used to create false PASS
        results in the convert matrix.
        """
        if fn.get("lang") != "C":
            return None
        if fn.get("ret") not in {"int", "long", "long long", "short", "char"}:
            return None
        args = [part.strip() for part in fn.get("args", "").split(",") if part.strip()]
        if len(args) != 2:
            return None
        names = []
        for arg in args:
            match = re.match(r"(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:long\s+long|long|int|short|char)\s+([A-Za-z_][A-Za-z0-9_]*)$", arg)
            if not match:
                return None
            names.append(match.group(1))
        body = re.sub(r"/\*.*?\*/|//[^\n]*", "", fn.get("body", ""), flags=re.S)
        match = re.fullmatch(r"\s*return\s+([A-Za-z_][A-Za-z0-9_]*)\s*([+\-*/%])\s*([A-Za-z_][A-Za-z0-9_]*|[-+]?\d+)\s*;?\s*", body)
        if not match or match.group(1) != names[0] or match.group(3) not in {names[1], "0", "1"}:
            return None
        return names[0], names[1], f"{match.group(1)} {match.group(2)} {match.group(3)}"

    def can_convert_natively(self, fn: Dict) -> bool:
        return self._simple_c_return(fn) is not None

    def emit_native_ykn_function(self, fn: Dict) -> str:
        simple = self._simple_c_return(fn)
        if simple is None:
            raise ConversionUnsupported(
                f"E038 [CONVERT_UNSUPPORTED]: function '{fn.get('name', '<unknown>')}' has no verified native lowering"
            )
        arg0, arg1, expression = simple
        return f"""isv {fn['name']}({arg0}: tamsayi, {arg1}: tamsayi) -> tamsayi {{
    dondu {expression};
}}"""

    def emit_fallback_code(self, mode: str, library_name: str, functions: List[Dict]) -> str:
        """Do not emit fake fallback modules.

        The YKN runtime adapters are not yet a complete callable ABI for all
        five modes. Returning source that compiles but does not call the target
        library is worse than a hard diagnostic, so backend generation is
        refused until a real adapter is wired and tested.
        """
        raise ConversionUnsupported(
            f"E038 [CONVERT_UNSUPPORTED]: fallback mode '{mode}' has no verified code generator"
        )

    @staticmethod
    def _resolve_path(value: str, source_dir: str) -> str:
        if not value:
            return ""
        return value if os.path.isabs(value) else os.path.abspath(os.path.join(source_dir, value))

    def execute_directive(self, directive: ConvertDirective, source_dir: str = ".") -> Dict[str, Any]:
        """Execute one explicitly configured fallback against its real target.

        The conversion/source-generation path remains separate: a backend is
        not considered converted merely because a host-side call succeeded.
        This method is the runtime contract used by callers that intentionally
        select an external backend and need its result now.
        """
        if not directive.is_valid:
            raise ConversionUnsupported(directive.error_message or "E036 [CONVERT_SYNTAX_ERROR]")
        source_path = self._resolve_path(directive.kaynak, source_dir)
        if not source_path or not os.path.isfile(source_path):
            raise ConversionUnsupported(f"E036 [CONVERT_SYNTAX_ERROR]: Source file not found: {directive.kaynak}")
        args = list(directive.argumanlar)
        target = self._resolve_path(directive.kutuphane, source_dir)

        if directive.mod == "HARICI_DINAMIK":
            if not target:
                raise ConversionUnsupported("E038 [CONVERT_DYNAMIC_CONFIG]: kutuphane is required")
            if not directive.islev:
                raise ConversionUnsupported("E038 [CONVERT_DYNAMIC_CONFIG]: islev is required")
            if len(args) != 2 or any(isinstance(item, bool) or not isinstance(item, int) for item in args):
                raise ConversionUnsupported("E038 [CONVERT_DYNAMIC_ABI]: exactly two integer argumanlar are required")
            with DynamicLibrary(target) as library:
                value = library.call_i64(directive.islev, args[0], args[1])
            return {"backend": directive.mod, "symbol": directive.islev, "value": value, "target": target}

        if directive.mod == "PYTHON_EMBED":
            if not target or not directive.islev:
                raise ConversionUnsupported("E038 [CONVERT_PYTHON_CONFIG]: kutuphane and islev are required")
            with PythonModule(target) as module:
                value = module.call(directive.islev, args)
            return {"backend": directive.mod, "symbol": directive.islev, "value": value, "target": target}

        if directive.mod == "JS_RPC_IPC":
            if not target or not directive.islev:
                raise ConversionUnsupported("E038 [CONVERT_NODE_CONFIG]: kutuphane and islev are required")
            node = shutil.which("node")
            if not node:
                raise ProcessExecutionError("E035 [CONVERT_NODE_START]: node executable was not found")
            # The wrapper loads the requested CommonJS module and exposes only
            # one line-delimited JSON-RPC method. The target itself is never
            # interpreted as shell text.
            with tempfile.TemporaryDirectory(prefix="ykn-node-convert-") as temp_dir:
                wrapper = os.path.join(temp_dir, "bridge.js")
                target_literal = json.dumps(target)
                wrapper_source = (
                    "const target = require(" + target_literal + ");\n"
                    "const readline = require('readline');\n"
                    "const rl = readline.createInterface({input: process.stdin});\n"
                    "rl.on('line', line => { let r; try { r = JSON.parse(line); "
                    "if (!r || r.jsonrpc !== '2.0' || typeof r.method !== 'string') throw new Error('invalid request'); "
                    "const fn = target[r.method]; if (typeof fn !== 'function') throw new Error('method not found'); "
                    "const result = fn(...((r.params && r.params.args) || [])); "
                    "console.log(JSON.stringify({jsonrpc:'2.0',id:r.id,result:{value:result}})); } "
                    "catch (e) { console.log(JSON.stringify({jsonrpc:'2.0',id:r && r.id,error:{code:-32000,message:String(e.message || e)}})); } });\n"
                )
                with open(wrapper, "w", encoding="utf-8") as stream:
                    stream.write(wrapper_source)
                client = NodeJsonRpcClient(node, wrapper)
                try:
                    result = client.request(directive.islev, {"args": args}, directive.zaman_asimi)
                finally:
                    client.close()
            return {"backend": directive.mod, "symbol": directive.islev, "value": result.get("value"), "target": target}

        if directive.mod == "SHELL_SYSTEM":
            if not directive.komut:
                raise ConversionUnsupported("E038 [CONVERT_SHELL_CONFIG]: komut is required")
            result = ShellProcess().run(directive.komut, "", directive.zaman_asimi)
            return {"backend": directive.mod, "value": result}

        raise ConversionUnsupported(
            f"E038 [CONVERT_UNSUPPORTED_EXECUTION]: backend '{directive.mod}' has no execution contract"
        )

    def process_directive(self, directive: ConvertDirective, source_dir: str = ".") -> Tuple[bool, str, Dict]:
        if not directive.is_valid:
            return False, directive.error_message, {}

        full_source_path = os.path.join(source_dir, directive.kaynak)
        if not os.path.exists(full_source_path):
            err = f"E036 [CONVERT_SYNTAX_ERROR]: Source file not found: {directive.kaynak}"
            return False, err, {}

        with open(full_source_path, "r", encoding="utf-8", errors="replace") as f:
            src_content = f.read()

        cache_key = self.compute_cache_key(src_content, directive)
        cache_file = os.path.join(self.cache_dir, f"{cache_key}.ykn")
        cache_meta = os.path.join(self.cache_dir, f"{cache_key}.meta.json")

        if os.path.exists(cache_file) and os.path.exists(cache_meta):
            with open(cache_file, "r", encoding="utf-8") as f:
                cached_code = f.read()
            with open(cache_meta, "r", encoding="utf-8") as f:
                meta = json.load(f)
            meta["cache_hit"] = True
            self.transparency_log.append(meta)
            return True, cached_code, meta

        lang = DIRECTIVES.get(directive.directive_type, "C")
        functions = self.extract_functions(lang, src_content)

        # Apply 'sadece' filter
        if directive.sadece:
            functions = [fn for fn in functions if fn["name"] in directive.sadece]

        if not functions:
            meta = {
                "source_file": directive.kaynak,
                "configured_mode": directive.mod,
                "chosen_mode": "UNSUPPORTED",
                "status": "no_functions_found",
                "cache_hit": False,
            }
            return False, f"E037 [CONVERT_NO_FUNCTIONS]: no supported function was found in '{directive.kaynak}'", meta

        convertible_count = sum(1 for fn in functions if self.can_convert_natively(fn))
        total_count = len(functions)
        success_ratio = convertible_count / total_count if total_count > 0 else 0.0

        decision_mode = ""
        lib_name = os.path.splitext(os.path.basename(directive.kaynak))[0]

        if success_ratio >= directive.esik and convertible_count < total_count:
            record = {
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "directive": directive.directive_type,
                "source_file": directive.kaynak,
                "target_file": directive.hedef,
                "language": lang,
                "functions_found": total_count,
                "functions_convertible": convertible_count,
                "success_ratio": round(success_ratio, 2),
                "threshold": directive.esik,
                "configured_mode": directive.mod,
                "chosen_mode": "UNSUPPORTED",
                "status": "partial_native_unsupported",
                "cache_key": cache_key,
                "cache_hit": False,
            }
            self.transparency_log.append(record)
            return False, (
                f"E038 [CONVERT_PARTIAL_UNSUPPORTED]: {convertible_count}/{total_count} "
                "functions have verified native lowering; partial modules are not emitted"
            ), record

        if success_ratio >= directive.esik:
            decision_mode = "NATIVE"
            ykn_lines = [f"# Yukinary Pure Native Converted Module: {lib_name}"]
            ykn_lines.append(f"# Source: {directive.kaynak}, Ratio: {success_ratio:.2f} >= Threshold: {directive.esik:.2f}\n")
            for fn in functions:
                ykn_lines.append(self.emit_native_ykn_function(fn))
            ykn_lines.append("\nislev ana() -> tamsayi {\n    dondu 0;\n}")
            generated_code = "\n".join(ykn_lines)
        else:
            decision_mode = "UNSUPPORTED"
            try:
                generated_code = self.emit_fallback_code(directive.mod, lib_name, functions)
            except ConversionUnsupported as exc:
                record = {
                    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                    "directive": directive.directive_type,
                    "source_file": directive.kaynak,
                    "target_file": directive.hedef,
                    "language": lang,
                    "functions_found": total_count,
                    "functions_convertible": convertible_count,
                    "success_ratio": round(success_ratio, 2),
                    "threshold": directive.esik,
                    "configured_mode": directive.mod,
                    "chosen_mode": "UNSUPPORTED",
                    "status": "backend_not_implemented",
                    "cache_key": cache_key,
                    "cache_hit": False,
                }
                self.transparency_log.append(record)
                return False, str(exc), record

        record = {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "directive": directive.directive_type,
            "source_file": directive.kaynak,
            "target_file": directive.hedef,
            "language": lang,
            "functions_found": total_count,
            "functions_convertible": convertible_count,
            "success_ratio": round(success_ratio, 2),
            "threshold": directive.esik,
            "configured_mode": directive.mod,
            "chosen_mode": decision_mode,
            "cache_key": cache_key,
            "cache_hit": False
        }

        # Save to cache
        with open(cache_file, "w", encoding="utf-8") as f:
            f.write(generated_code)
        with open(cache_meta, "w", encoding="utf-8") as f:
            json.dump(record, f, indent=2)

        self.transparency_log.append(record)
        return True, generated_code, record
