<div align="center">

<img src="assets/logo.png" alt="Yukinary Programming Language Logo" width="180" />

# Yukinary (YKN) Programming Language

**Zero-Dependency • Direct Native Machine Code • Multi-Lingual Grammar • Self-Healing • Embedded BitNet AI • Polyglot `#convert` & FFI**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Architecture: 100% Pure YKN](https://img.shields.io/badge/Language-100%25_Pure_YKN-purple)](https://github.com/ykncode/yukinary)

<p align="center">
  <a href="#-quick-start--releases">Download & Releases</a> •
  <a href="#-language-documentation--dil-kılavuzları">Documentation</a> •
  <a href="#-micro-llm-vs-full-llm-architecture">Micro-LLM vs Full LLM</a> •
  <a href="#-how-to-download--use-full-llm-models-bitnet--bonsai">Models Guide</a> •
  <a href="#-key-features">Key Features</a> •
  <a href="#-what-can-you-build-with-yukinary-target-domains">What to Build</a> •
  <a href="#️-operating-system-development-the-yknos-architecture">YknOS</a> •
  <a href="#️-production-readiness-honest-boundaries--guarantees">Guarantees</a> •
  <a href="#-four-repository-ecosystem">Ecosystem</a> •
  <a href="#-dual-license">License</a>
</p>

---

</div>

## 🚀 Quick Start & Releases

You **do not need** a C++ compiler or external toolchain to run Yukinary. Standalone, single-file native binaries are pre-compiled and ready to download:

### 1. Download Standalone Compiler
👉 [**Download Yukinary from GitHub Releases**](https://github.com/ykncode/yukinary/releases)

* **Windows:** Download `ykn.exe` (Standalone PE32+ binary)
* **Linux:** Download `ykn` (Standalone ELF64 binary, `chmod +x ykn`)

### 2. Global CLI Setup (Run `ykn` Anywhere Without `.exe`)
To run `ykn` directly from any directory without typing `.exe` or `.\`:

```powershell
# Windows: Installs to %USERPROFILE%\.yukinary\bin and adds to PATH permanently
powershell -File tools/install.ps1

# Linux / macOS: Installs to ~/.yukinary/bin and adds to PATH in ~/.bashrc / ~/.zshrc
sh tools/install.sh
```

### 3. Write Your First Program (`hello.ykn`)
```ykn
isv ana() -> tamsayi {
    ekrana_yaz("Merhaba, Yukinary Dunyasi!\n".ptr);
    dondu 0;
}
```

### 4. Compile Directly to Native Machine Code
```powershell
# Windows
ykn hello.ykn -o hello.exe
.\hello.exe

# Linux
ykn hello.ykn -o hello
./hello
```

---

## 🌐 Language Documentation / Dil Kılavuzları

Yukinary provides comprehensive, beginner-friendly ("mala anlatır gibi") complete guides for all supported spoken languages:

| Language | File | Description |
| :--- | :--- | :--- |
| 🇹🇷 **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Detaylı Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| 🇬🇧 **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Complete Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| 🇷🇺 **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Полное руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| 🇯🇵 **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能、AIエンジン解説 |
| 🇨🇳 **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译、自愈编译机制与嵌入式 AI 引擎 |
| 🇰🇷 **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진, 메모리 안전성 및 임베디드 AI |

> [!TIP]
> **🌍 Your Language Missing from the List? Add It Yourself in 15 Minutes!**  
> Don't worry! In Yukinary, human language is just a syntax token. Follow our step-by-step community guide: [**`docs/Multi-lingual-add-language.md`**](docs/Multi-lingual-add-language.md) to add your mother tongue and submit a Pull Request!

---

## 🧠 Micro-LLM vs. Full LLM Architecture

Yukinary integrates artificial intelligence into the compilation pipeline through **two distinct, complementary layers**. Understanding the difference removes all misconceptions:

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
│ 📁 std/ai/micro_llm.ykn           │       │ 📁 src/llm.ykn                    │
│ 📁 std/ai/embedded_weights.ykn    │       │ 📁 std/ai/gguf_loader.ykn         │
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

### Architectural Comparison & Operational Roles:

| Dimension | 🧠 Embedded Micro-LLM (16 KB) | 🚀 External Full LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **Location & Storage** | Baked directly into the compiler binary (`.rodata` section) | External `.gguf` file loaded dynamically from user storage |
| **Size & Weights** | **16,448 Bytes (~16 KB)** ternary $\{-1, 0, +1\}$ BitNet causal transformer | **1 GB – 40 GB** (BitNet b1.58, Llama-3, Mistral, Qwen, etc.) |
| **Hardware Reqs** | **Zero GPU, Zero Python, Zero Internet.** Runs on any standard 64-bit CPU using Q16.16 fixed-point arithmetic | Requires 4 GB – 32 GB RAM / VRAM depending on quantization and context size |
| **Inference Latency** | **Sub-millisecond (< 1 ms)** — imperceptible compile-time overhead | 10 – 50 milliseconds per token |
| **Primary Scope** | **Compiler Reflexes & Tooling:**<br>• Syntax & typo repair (`--self-heal`: semicolons, parens)<br>• Defensive `null-guard` injection against crashes<br>• Real-time LSP struct field and token autocompletion<br>• Lightweight offline keyword/signature translation fallback | **Deep Semantic & Generative Power:**<br>• **Full Polyglot `#convert` Engine:** Translating entire Python, C, C++, and Rust libraries into native idiomatic YKN<br>• Natural language prompt execution (`#prompt "write an HTTP server"` -> native YKN)<br>• Synthesizing complex data structures and algorithmic modules |
| **Activation Mode** | **Transparent (Always-On).** Silent, ubiquitous, and always available | **Explicit (Opt-in).** Invoked when developer passes `--llm-model <path.gguf>` |

### 📥 How to Download & Use Full LLM Models (BitNet & Bonsai)

Yukinary compiles code out-of-the-box with **zero model downloads** thanks to its embedded 16 KB Micro-LLM. 

When you want heavyweight generative synthesis (`#prompt`) or full library translation (`#convert`), you can plug in any standard GGUF model (Microsoft BitNet b1.58, Prism ML Bonsai, Qwen, etc.).

> [!NOTE]
> **Why are large GGUF models not hosted directly inside the Git repository?**
> Git is designed for source code. GitHub enforces a strict **100 MB per-file limit**, whereas production neural models range from 1.1 GB to 40 GB. Following industry best practices (like Ollama and llama.cpp), model weights are hosted on [Hugging Face](https://huggingface.co).

#### 1. One-Command Download
Use the automated helper scripts included in the repository:

```powershell
# Windows (PowerShell)
powershell -File tools/download_models.ps1 bitnet    # Downloads Microsoft BitNet b1.58 (1.18 GB)
powershell -File tools/download_models.ps1 bonsai    # Downloads Prism ML Bonsai 8B (1.15 GB)
powershell -File tools/download_models.ps1 all       # Downloads both models
```

```bash
# Linux / macOS (Bash)
sh tools/download_models.sh bitnet
sh tools/download_models.sh bonsai
sh tools/download_models.sh all
```

#### 2. Manual Download (Direct Hugging Face Links)
If you prefer downloading manually via browser or `curl`:

| Model | Size | Format | Direct Download Link | Target Local Path |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Download from Hugging Face](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Download from Hugging Face](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. How to Use Models with the Compiler

You have two clean ways to configure and run models:

##### Method A: Zero CLI Flags via In-Source `#config` (Recommended)
Instead of typing long paths on every build, declare your project's AI and compiler configuration directly in your source code:

```ykn
#config {
    model: "bitnet",          # Automatically loads 'models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf'
    optimizasyon: 3,          # Enable O3 loop and register optimization
    yigin_boyutu: 16777216,   # 16 MB stack size stamped into binary headers
    guvenli: true             # Enable compile-time bounds checking
}

# You can even embed Python/C functions directly in the file:
#PY-convert
def multiply(a, b):
    return a * b
#end-convert

isv ana() -> tamsayi {
    degisken res: tamsayi = multiply(6, 7);
    dondu 0;
}
```

Then compile with a clean, single command:
```powershell
ykn main.ykn
```

##### Method B: Command-Line Flags
If you prefer passing models explicitly via the CLI:

* **Compile with `#prompt` Generative Directives:**
  ```powershell
  ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Translate a Python Library into Native Yukinary:**
  ```powershell
  ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Interactive AI Code Completion:**
  ```powershell
  ykn ai "isv fibonacci(n: tamsayi) -> tamsayi" --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Zero-Model Default (Always-On Micro-LLM):**
  If you don't supply `--llm-model`, Yukinary uses its in-binary 16 KB engine for instant self-healing and completions:
  ```powershell
  ykn main.ykn --self-heal
  ```

---

## ⚡ Key Features

### 1. Direct Native Code Generation (Zero C-Runtime)
* Emits standalone executables directly from the AST.
* Windows binaries use direct Kernel32 / NT calls (`VirtualAlloc`, `WriteFile`, `ExitProcess`).
* Linux binaries use raw 64-bit system calls (`sys_mmap`, `sys_write`, `sys_exit`).
* Binaries are tiny, start instantaneously, and have **zero external runtime dependencies**.

### 2. Multi-Lingual Native Grammar (TR, EN, RU, JA, ZH, KO)
Write code using your natural tongue. All keywords are recognized as first-class tokens:
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

### 3. Polyglot Interop & LLM-Powered Convert Engine (`#convert` & `harici isv`)
YKN never suffers from the "no libraries yet" dilemma:
* **Zero-Cost FFI (`harici isv`):** Directly invoke compiled C/C++/Rust shared libraries (`.dll` / `.so`) via OS dynamic loaders (`LoadLibraryA` / `dlopen`).
* **Direct Convert Directives (`#C-convert`, `#PY-convert`, `#JS-convert`, etc.):** Embed code from other languages directly into your source files. The convert engine translates them to idiomatic YKN and caches them with two-level SHA-256 in `.ykn_cache/convert/`.
* **In-Compiler Polyglot Transpiler:** The Full LLM engine (with offline AST transpiler fallback) translates C functions, structs, types, Python `def`s, and Rust syntax directly into idiomatic YKN.

### 4. Tagged Enums (Algebraic Data Types) & Pattern Matching
Supports variants with and without payloads with 16-byte aligned `{ discriminant, payload }` representations:
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
Tune OS binary headers, stack allocations, worker threads, and microarchitecture directly from source:
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target AVX2 / SIMD microarchitecture
}
```

### 6. Autonomous Self-Healing Engine (`--onar` / `--self-heal`)
Acts as an automated code mechanic:
* **Automatic Snapshots:** Takes a `.bak` snapshot before any modification.
* **Typo & Punctuation Healing:** Repairs misspelled keywords and inserts missing semicolons.
* **Null-Guard Injection:** Detects unguarded pointer dereferences (`ptr.field`) and injects safe protective guards (`if ptr != null`).
* **Safe Gate:** Candidate patches must compile cleanly before being written to disk.

### 7. Compile-Time Memory Safety & UAF Detection
* Monitors pointer lifetimes and deallocations.
* Any attempt to dereference a freed pointer triggers a compile-time fatal diagnostic, preventing **Use-After-Free** bugs before runtime.

### 8. Sandbox Gate Security
Restricts untrusted scripts and external plugins at the AST level from executing unauthorized system calls (`CreateProcess`, `socket`, `send`, `LoadLibrary`, `dlopen`).

### 9. High-Performance Register Allocation
* Loop induction variables are automatically assigned to machine callee-saved registers (`RBX`, `R12`..`R15`), minimizing stack memory roundtrips.

### 10. Self-Hosting Reproducible Build Fixpoint
* The compiler compiles itself! Stage 2 and Stage 3 executables produce **100% bit-for-bit identical SHA-256 hashes**:
  `EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## 🎯 What Can You Build with Yukinary? (Target Domains)

Yukinary compiles directly to native 64-bit machine code with **zero external dependencies** (no C-runtime, no JVM, no interpreter). It is engineered for performance-critical and resource-constrained environments:

1. **High-Performance CLI Tools & Compilers:**
   * Instant cold-start latency (< 1ms) with sub-megabyte binary footprints.
   * **Living Proof:** The Yukinary compiler itself (over 40,000 lines of source code in `src/`) is written entirely in pure Yukinary and compiles itself deterministically.
2. **High-Throughput Network Servers & Microservices:**
   * Asynchronous I/O via `std/ag/http.ykn`, `std/ag/websocket.ykn`, Windows IOCP (`std/es/iocp_loop.ykn`), and Linux `io_uring`/`epoll` (`std/os/os_linux_uring.ykn`).
   * Delivers microsecond-level latency and orders of magnitude lower memory consumption than Node.js or Python.
3. **Database & Storage Engines:**
   * Standard library primitives include B-Tree indexing (`std/veritabani/b_tree.ykn`), Write-Ahead Logging (`std/veritabani/wal.ykn`), embedded SQL parser (`std/veritabani/sql_motoru.ykn`), and Vector Database search (`std/veritabani/hnsw.ykn`).
4. **Embedded Neural Inference & Edge AI:**
   * Embedded BitNet causal transformer weights, integer Q16.16 fixed-point math, and GGUF v3 parser (`std/ai/`).
   * Run local inference without Python, PyTorch, or GPU driver installations.
5. **Gömülü Sistemler (Embedded Systems) & IoT:**
   * Direct hardware access, minimal RAM overhead, and standalone execution without OS dependencies.

---

## 🖥️ Operating System Development: The YknOS Architecture

**Can you build an operating system in Yukinary? Yes, and the foundational kernel is already implemented in the repository.**

Operating systems cannot be written in languages that depend on an existing OS or garbage-collected runtime (like Python, Java, or C#). Because Yukinary emits freestanding, unmanaged x86_64 machine code with raw pointer manipulation, it compiles directly to bare-metal Ring-0 code.

Inside [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real), Yukinary provides a working proof-of-concept x86_64 kernel:

* **Multiboot2 Entry (`std/os/kernel_real/boot.asm`):** 32-bit to 64-bit Long Mode bootstrap compatible with GRUB.
* **Global Descriptor Table (`std/os/kernel_real/gdt.ykn`):** 64-bit kernel code/data segment descriptors.
* **Interrupt Descriptor Table (`std/os/kernel_real/idt.ykn`):** Ring-0 hardware interrupt, exception, and trap gates.
* **Virtual Memory Paging (`std/os/kernel_real/paging.ykn`):** 4-level PML4 page tables with 1 GB identity mapping.
* **Hardware Drivers:** COM1 Serial UART driver (`serial.ykn`) and VGA 0xB8000 text-mode video memory driver (`vga_text.ykn`).
* **Kernel Entry (`std/os/kernel_real/kmain.ykn`):** The main Ring-0 execution entry point (`yukinary_cekirdek_baslat`), testable in QEMU / VirtualBox.

---

## ⚠️ Production Readiness, Honest Boundaries & Guarantees

As a senior-engineered language, Yukinary believes in **radical engineering honesty**:

### A. Proven Technical Guarantees
* **Deterministic Bit-Identical Fixpoint:** Stage 2 and Stage 3 compilers generate bit-for-bit identical binaries (`EF607B9A...`).
* **Zero External Dependencies:** Produced executables run on clean Windows (Kernel32) and Linux (syscalls) with zero runtime dependencies.
* **Compile-Time Safety:** Static type enforcement, optional array bounds checking (`--guvenli`), Use-After-Free lifetime analysis, and AST sandbox capability gates.

### B. Honest Engineering Boundaries (Where NOT to Use YKN Yet)
* **Mission-Critical Life-Support / Core Banking:** While Yukinary is mathematically verified via fixpoint tests, it is a young programming language. It has not accumulated the decades of production mileage that C or Ada possess in aerospace or high-frequency trading.
* **Native Mobile Apps (iOS/Android):** Yukinary does not yet provide native UI bindings for Apple UIKit/SwiftUI or Android Jetpack Compose.
* **Dynamic Ad-Hoc Scripting:** If your use case requires an interactive, untyped, interpreted shell script with zero compilation delay, Python or Bash remains more suitable.

### C. Legal & Commercial Warranty
Like Rust, Go, Linux, and all major open-source systems, Yukinary is licensed under the MIT and Apache 2.0 licenses and provided **"AS IS", WITHOUT WARRANTY OF ANY KIND**.

---

## 🏛️ Four-Repository Ecosystem

The Yukinary ecosystem is organized into four purpose-built repositories:

```
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
│ • 100% Pure  │ │ • Package    │       │ • app_full   │     │ • Systems    │
│   YKN Source │ │   Registry   │       │   .cpp seed  │     │   benchmarks │
│ • src/ & std/│ │ • Community  │       │ • Standalone │     │ • C, Rust,   │
│ • docs/ &    │ │   libraries  │       │   C++17 seed │     │   Go, Python │
│   tests/     │ │ • ykn add    │       │ • Porting    │     │   comparison │
│ • Releases   │ │   support    │       │   targets    │     │ • Cold-start │
└──────────────┘ └──────────────┘       └──────────────┘     └──────────────┘
```

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary): Pure, 100% Yukinary self-hosting compiler, standard library, and multi-lingual documentation.
2. [**`ykncode/packages`**](https://github.com/ykncode/packages): Official package registry for modular libraries (`net/http`, `math`, `crypto/sha256`, `data/json`, `terminal/colors`).
3. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap): C++17 single-file seed for bootstrapping Yukinary from scratch on new hardware architectures.
4. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks): Industrial performance comparisons, cold-start latency measurements, and competitor workloads.

---

## 📜 Dual License

Yukinary is distributed under the terms of both the **MIT License** and the **Apache License (Version 2.0)**, at your option (matching the Rust licensing model).

See:
* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)

Contributions submitted for inclusion in Yukinary shall be dual-licensed as above without any additional terms or conditions.
