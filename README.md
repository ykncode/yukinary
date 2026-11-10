<div align="center">

<img src="assets/logo.png" alt="Yukinary Programming Language Logo" width="180" />

# Yukinary (YKN) Programming Language

**Self-Hosting x86_64 Compiler • Direct PE32+ & ELF64 Codegen (No LLVM / No C-Runtime) • Multi-Lingual Syntax • Experimental Self-Healing & Embedded AI**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Benchmarks: ykncode/benchmarks](https://img.shields.io/badge/Benchmarks-ykncode%2Fbenchmarks-38bdf8)](https://github.com/ykncode/benchmarks)

<p align="center">
  <a href="#benchmarks--performance-vs-c-rust-go-python">Benchmarks</a> •
  <a href="#quick-start--releases">Download & Quick Start</a> •
  <a href="#language-documentation">Documentation</a> •
  <a href="#micro-llm-vs-full-llm-architecture">AI Layer (Experimental)</a> •
  <a href="#key-features">Architecture</a> •
  <a href="#operating-system-development-the-yknos-architecture">YknOS</a> •
  <a href="#production-readiness-honest-boundaries--guarantees">Honest Boundaries</a> •
  <a href="#four-repository-ecosystem">Ecosystem</a>
</p>

---

</div>

## Overview

**Yukinary (YKN)** is an independent, self-hosting systems programming language and native compiler. Rather than targeting LLVM IR or transpiling to C, Yukinary lowers its AST directly to standalone **x86_64 machine code**—writing PE32+ (Windows) and ELF64 (Linux) binary headers, syscalls, and opcodes from scratch with **zero external runtime dependencies** (`libc`, `msvcrt`, or external linkers).

* **Bit-Identical Self-Hosting:** The compiler (written in Yukinary) compiles itself deterministically (`Stage 2 SHA-256 == Stage 3 SHA-256`).
* **Canonical + Educational Multi-Lingual Syntax:** Keywords can be written in English, Turkish, Russian, Japanese, Simplified Chinese, Korean, or French—designed for localized programming education, with `ykn fmt` intended to normalize shared libraries to canonical form.
* **Freestanding Systems & OS Target:** Powers an experimental Linux-based OS userspace ([`examples/yknos`](examples/yknos)).

---

## Benchmarks & Performance (vs. C, Rust, Go, Python)

All benchmark harnesses, C/Rust/Python competitor implementations, and raw JSON telemetry are published in our dedicated benchmark repository:  
[**Official Benchmark Suite: `github.com/ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) • [**Industrial Benchmark Report (TR)**](https://github.com/ykncode/benchmarks/blob/main/industrial/YUKINARY_ENDUSTRIYEL_BENCHMARK_RAPORU.md)

<div align="center">
  <img src="assets/bench_compile_speed.png" alt="Compiler Throughput: Yukinary vs GCC, Rustc, Go, G++" width="92%" />
</div>

<div align="center">
  <img src="assets/bench_workloads.png" alt="Runtime Performance and Peak RSS: Systems Workload and BitNet Kernel" width="92%" />
</div>

### Summary of Measured Results (`ykncode/benchmarks`)

| Benchmark | Yukinary (`ykn`) | C (`gcc -O3`) | Rust (`rustc -O3`) | Python (`3.13`) | Notes |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Compiler Build Time** (`optimizer`) | **18.6 ms** | 219.4 ms | 272.2 ms | — | Zero LLVM/linker overhead (**11x–31x faster builds**) |
| **Systems B-Tree** (100k nodes, 50k search) | **213 ms** | 133 ms | 221 ms | 327 ms | Exact verified checksum (`19526` / `6363996222`) |
| **BitNet b1.58** (3.35 GOP ternary kernel) | **0.71 GOP/s** | 1.79 GOP/s | 1.74 GOP/s | 0.02 GOP/s | Scalar native codegen (GCC/Rust use LLVM/GCC AVX2) |
| **Peak Memory (BitNet RSS)** | **2.6 MB** | 4.8 MB | 5.2 MB | 22.4 MB | **Lowest footprint** (freestanding, zero libc) |
| **Cold-Start Latency** | **5.4 ms** | 5.9 ms | 6.8 ms | 35.0 ms | Direct OS entry point (`ExitProcess` / `sys_exit`) |

> **Engineering Note:** C (`gcc -O3`) and Rust (`rustc -O3`) remain ~2.5x faster on dense matrix arithmetic due to GCC/LLVM auto-vectorization (SIMD AVX2). Yukinary relies on a custom scalar x86_64 encoder with linear-scan register allocation, prioritizing instant compilation, small zero-libc footprint, and zero external toolchain dependencies. Browse all source workloads in [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks).

---

## Quick Start & Releases

You **do not need** a C++ compiler or external toolchain to run Yukinary. Standalone native binaries are available in Releases:

### 1. Download & Setup
[**Download Yukinary from GitHub Releases**](https://github.com/ykncode/yukinary/releases)

#### Windows (1-Click Setup)
1. Download **`yukinary-setup-windows-x86_64.zip`** from Releases and extract it.
2. Double-click **`setup.cmd`** (or run `powershell -ExecutionPolicy Bypass -File setup.ps1`).

#### Linux & macOS (1-Line Setup)
1. Download **`yukinary-setup-linux-x86_64.tar.gz`** and extract it (`tar -xzf yukinary-setup-linux-x86_64.tar.gz`).
2. Run **`./setup.sh`**.

#### Visual Studio Code Extension (`.vsix`)
```bash
code --install-extension yukinary-lang-1.0.0.vsix
```

### 2. Write & Compile Your First Program (`hello.ykn`)
```ykn
# English syntax (canonical keywords; native stdio intrinsic: ekrana_yaz)
fn main() -> i64 {
    ekrana_yaz("Hello, Yukinary!\n".ptr);
    return 0;
}
```

Compile directly to native Windows PE32+ or Linux ELF64 machine code:
```powershell
# Windows PE32+
ykn hello.ykn hello.exe
.\hello.exe

# Linux ELF64 cross-compile from Windows or native Linux
ykn --target linux hello.ykn hello
```

---

## Language Documentation

Developer guides and syntax references are available across supported spoken languages:

| Language | File | Description |
| :--- | :--- | :--- |
| **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能 |
| **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译与嵌入式 AI |
| **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진 및 메모리 안전성 |

> [!TIP]
> **Localization Design Note:** Multi-lingual keywords use dynamic lexer token mappings (`docs/Multi-lingual-add-language.md`) to lower the barrier for students and non-English speakers. For collaborative open-source packages, English is recommended as the canonical standard.

---

## Micro-LLM vs. Full LLM Architecture

Yukinary integrates experimental neural tooling into the compilation pipeline through **two distinct layers**:

```text
┌────────────────────────────────────────────────────────────────────────┐
│                      YUKINARY AI ENGINE ARCHITECTURE                   │
└────────────────────────────────────────────────────────────────────────┘
                                    │
         ┌──────────────────────────┴──────────────────────────┐
         ▼                                                     ▼
┌───────────────────────────────────┐       ┌───────────────────────────────────┐
│     1. EMBEDDED MICRO-LLM         │       │       2. EXTERNAL FULL LLM        │
│          (Always-On)              │       │          (On-Demand)              │
├───────────────────────────────────┤       ├───────────────────────────────────┤
│ * std/ai/micro_llm.ykn            │       │ * src/llm.ykn                     │
│ * std/ai/embedded_weights.ykn     │       │ * std/ai/gguf_loader.ykn          │
│ • Size: 16,448 Bytes (~16 KB)     │       │ • Size: 1 GB – 40 GB GGUF v3      │
│ • Baked into binary (.rodata)     │       │ • Loaded dynamically from disk    │
│ • Zero files, zero internet,      │       │ • CLI: `--llm-model model.gguf`   │
│   zero external dependencies      │       │ • Polyglot `#convert` Engine      │
│ • Sub-millisecond latency (<1ms)  │       │   (Deep semantic library trans-   │
│ • Purpose: Compiler Reflexes,     │       │   pilation: C/C++/Python/Rust)    │
│   Self-Healing, LSP Autocomplete, │       │ • Natural language `#prompt "..."`│
│   Offline signature fallback      │       │   algorithmic code synthesis      │
└───────────────────────────────────┘       └───────────────────────────────────┘
```

### Architectural Comparison & Operational Roles

| Dimension | Embedded Micro-LLM (16 KB) | External Full LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **Location & Storage** | Baked directly into the compiler binary (`.rodata` section) | External `.gguf` file loaded dynamically from user storage |
| **Size & Weights** | **16,448 Bytes (~16 KB)** ternary `{-1, 0, +1}` BitNet weights | **1 GB – 40 GB** (BitNet b1.58, Llama-3, Mistral, Qwen, etc.) |
| **Hardware Reqs** | **Zero GPU, Zero Python, Zero Internet.** Runs on any 64-bit CPU using Q16.16 fixed-point math | Requires 4 GB – 32 GB RAM / VRAM depending on quantization and context size |
| **Inference Latency** | **Sub-millisecond (< 1 ms)** — minimal compile-time overhead | 10 – 50 milliseconds per token |
| **Primary Scope** | **Compiler Reflexes & Tooling:**<br>• Syntax & typo repair (`--self-heal`: semicolons, parens)<br>• Defensive `null-guard` injection against crashes<br>• Real-time LSP struct field and token autocompletion<br>• Lightweight offline keyword/signature fallback | **Experimental Generative Tooling:**<br>• **Polyglot `#convert`:** Assisting translation of C, Python, and Rust snippets into YKN<br>• Natural language prompt directives (`#prompt "..."`) |
| **Activation Mode** | **Always-On** (no external model required) | **Opt-In** via `--llm-model <path.gguf>` |

### How to Download & Use Full LLM Models (BitNet & Bonsai)

Yukinary compiles code out-of-the-box with **zero model downloads**. When testing heavyweight `#prompt` or `#convert` workflows, you can point the compiler to standard GGUF models hosted on [Hugging Face](https://huggingface.co).

#### 1. One-Command Download
```powershell
# Windows (PowerShell)
powershell -File tools/download_models.ps1 bitnet    # Microsoft BitNet b1.58 (1.18 GB)
powershell -File tools/download_models.ps1 bonsai    # Prism ML Bonsai 8B (1.15 GB)
powershell -File tools/download_models.ps1 all
```

```bash
# Linux / macOS (Bash)
sh tools/download_models.sh bitnet
sh tools/download_models.sh bonsai
sh tools/download_models.sh all
```

#### 2. Manual Download (Direct Hugging Face Links)

| Model | Size | Format | Direct Download Link | Target Local Path |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Download from Hugging Face](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Download from Hugging Face](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. How to Use Models with the Compiler

##### Method A: In-Source `#config` Directive
```ykn
#config {
    model: "bitnet",          # Loads 'models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf'
    optimizasyon: 3,          # Enable O3 loop and register optimization
    yigin_boyutu: 16777216,   # 16 MB stack size stamped into binary headers
    guvenli: true             # Enable compile-time bounds checking
}

#PY-convert
def multiply(a, b):
    return a * b
#end-convert

isv ana() -> tamsayi {
    degisken res: tamsayi = multiply(6, 7);
    dondu 0;
}
```

```powershell
ykn main.ykn
```

##### Method B: Command-Line Flags
```powershell
# Compile with #prompt Generative Directives
ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Translate a Python file into Yukinary
ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Experimental self-healing syntax repair (uses embedded 16 KB engine)
ykn main.ykn --self-heal
```

---

## Key Features

### 1. Direct Native Code Generation (Zero C-Runtime)
* Emits standalone executables directly from the AST without LLVM, C transpilation, or external linkers.
* Windows PE32+ binaries invoke Kernel32 / NT calls directly (`VirtualAlloc`, `WriteFile`, `ExitProcess`).
* Linux ELF64 binaries invoke raw 64-bit syscalls (`sys_mmap`, `sys_write`, `sys_exit`).

### 2. Multi-Lingual Native Grammar (TR, EN, RU, JA, ZH, KO, FR)
Keywords are mapped dynamically at the lexer stage:
```ykn
# Turkish
isv topla(a: tamsayi, b: tamsayi) -> tamsayi { dondu a + b; }

# English
fn add(a: i64, b: i64) -> i64 { return a + b; }

# Russian
функция сумма(а: целое, б: целое) -> целое { вернуть а + б; }

# Japanese
関数 合計(a: 整数, b: 整数) -> 整数 { 戻す a + b; }

# Chinese
函数 求和(a: 整数, b: 整数) -> 整数 { 返回 a + b; }

# Korean
함수 합계(a: 정수, b: 정수) -> 정수 { 반환 a + b; }
```

### 3. Foreign Function Interface (`harici isv`) & `#convert`
* **Direct FFI (`harici isv`):** Invoke compiled C/C++/Rust shared libraries (`.dll` / `.so`) via OS dynamic loaders (`LoadLibraryA` / `dlopen`).
* **Convert Directives (`#C-convert`, `#PY-convert`, `#JS-convert`):** Experimental transpilation blocks cached with SHA-256 in `.ykn_cache/convert/`.

### 4. Tagged Enums (Algebraic Data Types) & Pattern Matching
Supports variants with and without payloads using 16-byte aligned `{ discriminant, payload }` representations:
```ykn
enum Result {
    Success(tamsayi),
    Failure(tamsayi),
    Pending
}

isv process(r: Result) -> tamsayi {
    esles r {
        Result.Success(val) => { dondu val; }
        Result.Failure(err) => { dondu 0 - err; }
        Result.Pending      => { dondu 0; }
    }
}
```

### 5. Hierarchical `#config` Orchestrator
Tune OS binary headers, stack allocations, and target flags directly from source:
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target microarchitecture
}
```

### 6. Autonomous Self-Healing Engine (`--onar` / `--self-heal`)
* **Automatic Snapshots:** Creates a `.bak` snapshot before applying repairs.
* **Typo & Punctuation Healing:** Repairs misspelled keywords and missing semicolons via Levenshtein matching.
* **Null-Guard Injection:** Injects protective guards (`if ptr != null`) around unguarded dereferences.
* **Compile Verification Gate:** Patches must pass compilation before touching source on disk.

### 7. Compile-Time Memory Safety & UAF Detection
* Tracks pointer lifetimes and deallocations statically.
* Dereferencing a freed pointer triggers a compile-time diagnostic to catch Use-After-Free patterns before execution.

### 8. Sandbox Gate Security
Restricts untrusted scripts at the AST level from invoking unauthorized OS APIs (`CreateProcess`, `socket`, `send`, `LoadLibrary`, `dlopen`).

### 9. Register Allocation
Loop induction variables are assigned directly to callee-saved machine registers (`RBX`, `R12`..`R15`), avoiding redundant stack traffic.

### 10. Reproducible Self-Hosting Fixpoint
The compiler compiles itself cleanly; Stage 2 and Stage 3 binaries produce **100% bit-for-bit identical SHA-256 hashes**:
`EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## Target Domains

Because Yukinary compiles directly to unmanaged, zero-dependency 64-bit machine code, it is designed for:

1. **CLI Tools & Compilers:** Fast build iteration and sub-megabyte executables (`src/` itself is self-hosted in Yukinary).
2. **Network Services:** Low-level async primitives (`std/ag/http.ykn`, Windows IOCP, Linux `io_uring`/`epoll`).
3. **Storage Engines:** B-Tree indexing (`std/veritabani/b_tree.ykn`), Write-Ahead Logging (`wal.ykn`), and vector indexing (`hnsw.ykn`).
4. **Embedded Fixed-Point Inference:** Q16.16 integer inference and GGUF v3 parsing (`std/ai/`) without Python or PyTorch.
5. **Embedded Systems & Bare-Metal Targets:** Direct pointer access without C-runtime dependencies.

---

## Operating System Development: The YknOS Architecture

Because Yukinary emits freestanding x86_64 code without relying on `libc`, it can target bare-metal environments and OS userspaces directly:

* Inside [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real), a minimal proof-of-concept Ring-0 x86_64 kernel implements Multiboot2 entry (`boot.asm`), GDT (`gdt.ykn`), IDT (`idt.ykn`), PML4 paging (`paging.ykn`), COM1 serial UART, and VGA text output.
* Inside [`examples/yknos/`](examples/yknos), Yukinary powers a working **Linux 6.6 LTS + Alpine musl + YKN ELF64 Userspace** system (`ykn-init` PID 1 supervisor, `yknsh` shell, `yuki` CLI) bootable in QEMU.

---

## Production Readiness, Honest Boundaries & Guarantees

### A. Verified Engineering Properties
* **Deterministic Bit-Identical Fixpoint:** Stage 2 and Stage 3 self-hosted binaries match bit-for-bit (`EF607B9A...`).
* **Zero External Dependencies:** Executables run on stock Windows (Kernel32) and Linux (raw syscalls) with zero external DLLs or shared libraries.
* **Measured Performance Transparency:** Full source benchmarks against GCC, Rustc, Go, and CPython are published openly in [`ykncode/benchmarks`](https://github.com/ykncode/benchmarks).

### B. Honest Boundaries (Where NOT to Use YKN Yet)
* **Experimental Status:** Yukinary is a v1.0.0 systems research compiler built by a solo developer. It has not accumulated the decades of edge-case hardening that GCC, Clang, or Rust possess. Do not deploy it in safety-critical or financial production systems.
* **Dense Auto-Vectorized Math:** Because Yukinary uses a custom scalar x86_64 backend without LLVM SIMD AVX2 auto-vectorization, C (`-O3`) and Rust (`opt-level=3`) remain faster (~2.5x) on heavy floating-point or matrix loops.
* **Native Mobile UI:** There are currently no native UI bindings for iOS UIKit/SwiftUI or Android Jetpack Compose.

### C. Warranty
Distributed under the MIT and Apache 2.0 licenses, provided **"AS IS", WITHOUT WARRANTY OF ANY KIND**.

---

## Four-Repository Ecosystem

```text
┌────────────────────────────────────────────────────────────────────────┐
│                        YKNCODE GITHUB ECOSYSTEM                        │
└────────────────────────────────────────────────────────────────────────┘
                                    │
    ┌───────────────────┬───────────┴───────────┬───────────────────┐
    ▼                   ▼                       ▼                   ▼
┌──────────────┐ ┌──────────────┐       ┌──────────────┐     ┌──────────────┐
│ ykncode/     │ │ ykncode/     │       │ ykncode/     │     │ ykncode/     │
│   yukinary   │ │   packages   │       │  bootstrap   │     │  benchmarks  │
├──────────────┤ ├──────────────┤       ├──────────────┤     ├──────────────┤
│ • Pure YKN   │ │ • Package    │       │ • C++17 seed │     │ • C, Rust,   │
│   Compiler   │ │   Registry   │       │   for initial│     │   Go, Python │
│ • src/ & std/│ │ • Community  │       │   bootstrap  │     │   workloads  │
│ • docs/ &    │ │   libraries  │       │ • Porting    │     │ • Compile &  │
│   examples/  │ │ • ykn add    │       │   targets    │     │   cold-start │
└──────────────┘ └──────────────┘       └──────────────┘     └──────────────┘
```

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary): Self-hosting Yukinary compiler, standard library, documentation, and YknOS Linux userspace example.
2. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks): Industrial performance comparisons, compile-time telemetry, and competitor workloads (C, Rust, Go, Python).
3. [**`ykncode/packages`**](https://github.com/ykncode/packages): Package registry for modular libraries (`net/http`, `math`, `crypto/sha256`, `data/json`, `terminal/colors`).
4. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap): Standalone C++17 seed compiler for bootstrapping Yukinary from scratch on new machines.

---

## Dual License

Yukinary is dual-licensed under either the **MIT License** or the **Apache License (Version 2.0)**, at your option:

* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)
