#!/usr/bin/env python3
"""Compile the checked-in quant kernels to PTX and embed them as a YKN string."""
from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nvcc", default=shutil.which("nvcc"))
    parser.add_argument("--source", type=Path, default=Path("tools/cuda_quant_kernels.cu"))
    parser.add_argument("--output", type=Path, default=Path("std/ai/cuda_quant_ptx.ykn"))
    parser.add_argument("--arch", default="compute_75")
    args = parser.parse_args()
    if not args.nvcc:
        parser.error("nvcc was not found; install the CUDA Toolkit or pass --nvcc")
    source = args.source.resolve()
    if not source.is_file():
        parser.error(f"CUDA source does not exist: {source}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ykn-quant-ptx-") as temp:
        ptx_path = Path(temp) / "cuda_quant_kernels.ptx"
        subprocess.run(
            [args.nvcc, "--ptx", f"-arch={args.arch}", str(source), "-o", str(ptx_path)],
            check=True,
        )
        ptx = ptx_path.read_text(encoding="utf-8")
    escaped = ptx.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    args.output.write_text(
        "# Generated from tools/cuda_quant_kernels.cu by "
        "tools/generate_cuda_quant_ptx.py; do not edit by hand.\n"
        "isv ykn_cuda_quant_ptx() -> *u8 { dondu \""
        + escaped
        + "\".ptr; }\n",
        encoding="utf-8",
    )
    print(f"wrote {args.output} ({len(ptx)} PTX characters)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
