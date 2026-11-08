# Yukinary (YKN) Comprehensive Developer Guide & Reference

> **Version:** 1.0.0 (Phase 1-6 Enterprise Maturity Fixpoint)  
> **Target Platforms:** Windows x86-64 (PE32+) & Linux x86-64 (ELF64)  
> **License:** Dual-licensed under MIT / Apache-2.0 (Rust-style)

---

## 📌 Table of Contents
1. [Introduction & Architectural Philosophy](#1-introduction--architectural-philosophy)
2. [Installation & Quick Start](#2-installation--quick-start)
3. [Multi-Lingual Grammar System](#3-multi-lingual-grammar-system)
4. [Types, Variables, and Operators](#4-types-variables-and-operators)
5. [Functions and Control Flow](#5-functions-and-control-flow)
6. [Structures (Struct) and Pointers](#6-structures-and-pointers)
7. [Tagged Enums (ADT) & Pattern Matching (`match`)](#7-tagged-enums-and-pattern-matching)
8. [Hierarchical `#config` Orchestrator](#8-hierarchical-config-orchestrator)
9. [Self-Healing Compiler Engine](#9-self-healing-compiler-engine)
10. [Memory Safety & Use-After-Free Detection](#10-memory-safety--use-after-free-detection)
11. [Sandbox Gate: Capability Security](#11-sandbox-gate-capability-security)
12. [Embedded AI Engine: Always-On BitNet 1.58b Micro-LLM](#12-embedded-ai-engine-micro-llm)
13. [LSP & VSCode Editor Integration](#13-lsp--vscode-editor-integration)
14. [Package Manager & Two-Repository Architecture](#14-package-manager--two-repo-architecture)
15. [Polyglot Convert Engine (`#convert`) & Foreign Function Interface (FFI)](#15-polyglot-convert-engine-convert--foreign-function-interface-ffi)
16. [Build Verification & Cross-Platform Testing](#16-build-verification--cross-platform-testing)
17. [What Can You Build with Yukinary? (Target Domains)](#17-what-can-you-build-with-yukinary-target-domains)
18. [Operating System Development: YknOS Architecture](#18-operating-system-development-yknos-architecture)
19. [Production Readiness, Boundaries & Guarantees](#19-production-readiness-boundaries--guarantees)

---

## 1. Introduction & Architectural Philosophy

**Yukinary (YKN)** is an independent, self-hosting systems programming language and native compiler designed to produce direct machine code for Windows PE32+ and Linux ELF64 without relying on an external C runtime (libc/msvcrt), LLVM, or third-party linkers.

### Core Principles
* **Direct Native Codegen:** Generates executable binaries directly from AST without intermediate C or assembly files.
* **Compile-Time Memory Safety:** Enforces ownership, borrow checking, null safety, and Use-After-Free detection without garbage collection pauses.
* **First-Class Multi-Lingual Grammar:** Program natively in English, Turkish, Russian, Japanese, Simplified Chinese, or Korean.
* **Autonomous Self-Healing:** Automatically identifies typos, missing punctuation, unhandled pointers, and injects protective null-guards.
* **16 KB Embedded Micro-LLM:** Features an in-binary ternary Transformer neural network operating at 0.1ms latency on standard CPUs.

---

## 2. Installation & Quick Start

### Compiling on Windows
Invoke the self-hosted compiler `ykn_full.exe` or the bootstrap binary `app_full.exe`:

```powershell
# Compile a program to a native Windows PE32+ binary
.\ykn_full.exe hello.ykn -o hello.exe

# Execute
.\hello.exe
```

### Compiling on Linux (Native or Docker)
```bash
# Compile to a native Linux ELF64 binary
./ykn_full hello.ykn -o hello
chmod +x hello
./hello
```

---

## 3. Multi-Lingual Grammar System

YKN treats multiple languages as first-class citizens. You can write your code using English syntax, Turkish, Russian, Japanese, Chinese, or Korean:

```ykn
# Pure English syntax example
fn calculate(value: i64) -> i64 {
    let result: i64 = value * 2;
    if result > 100 then {
        return result;
    } else {
        return 0;
    }
}
```

### English Keyword Equivalents:
* `fn` / `func` / `function` -> Function declaration
* `let` / `var` -> Variable declaration
* `if` .. `then` .. `else` -> Conditional branch
* `while` / `loop` -> Loop statement
* `return` -> Return statement
* `struct` -> Composite data structure
* `enum` -> Algebraic data type
* `match` -> Pattern matching
* `null` / `nil` -> Null pointer literal

---

## 4. Types, Variables, and Operators

### Variable Declarations
```ykn
let count: i64 = 42;                 # 64-bit signed integer
let raw_byte: u8 = 255;              # 8-bit unsigned integer
let is_valid: bool = true;           # Boolean flag
let ratio: f64 = 3.14159;            # 64-bit floating point
let message: *u8 = "Hello YKN!".ptr; # Byte pointer
```

### Logical and Arithmetic Operators
* **Logical AND:** `and`, `ve`, `&&`
* **Logical OR:** `or`, `veya`, `||`
* **Logical NOT:** `not`, `degil`, `!`

```ykn
if (count > 0 and is_valid == true) or not false then {
    ekrana_yaz("Condition satisfied!\n".ptr);
}
```

---

## 5. Functions and Control Flow

### Function Syntax
```ykn
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
```

### Conditionals
```ykn
fn max(a: i64, b: i64) -> i64 {
    if a > b then {
        return a;
    } else {
        return b;
    }
}
```

### High-Performance Loops
The YKN compiler allocates loop counters directly to callee-saved machine registers (`RBX`, `R12`..`R15`):

```ykn
fn sum_range(limit: i64) -> i64 {
    let total: i64 = 0;
    let i: i64 = 1;
    while i <= limit {
        total = total + i;
        i = i + 1;
    }
    return total;
}
```

---

## 6. Structures and Pointers

### Defining Structures
```ykn
struct Player {
    id: i64;
    score: i64;
    health: i64;
}

fn create_player(player_id: i64) -> Player {
    let p: Player;
    p.id = player_id;
    p.score = 0;
    p.health = 100;
    return p;
}
```

### Pointer Dereferencing & Field Access
```ykn
let p_ptr: *Player = bellek_tahsis_et(24);
p_ptr.id = 101;
p_ptr.health = 95;
```

---

## 7. Tagged Enums and Pattern Matching

YKN provides algebraic data types with 16-byte aligned payload tags `{ discriminant, payload }`:

```ykn
enum TaskStatus {
    Pending,
    InProgress(i64),   # Carries percentage progress
    Failed(i64),       # Carries error code
    Completed(i64)     # Carries return code
}

fn handle_status(s: TaskStatus) -> i64 {
    match s {
        TaskStatus.Pending => {
            return 0;
        }
        TaskStatus.InProgress(pct) => {
            return pct;
        }
        TaskStatus.Failed(err) => {
            return 0 - err;
        }
        TaskStatus.Completed(code) => {
            return code;
        }
    }
}
```

---

## 8. Hierarchical `#config` Orchestrator

Control executable headers and runtime characteristics directly from source:

```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB stack size stamped into PE/ELF headers
    is_parcacigi: 8,          # Worker threads
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Microarchitecture optimization target
}
```

---

## 9. Self-Healing Compiler Engine

Invoked via `--onar` or `--self-heal`:

1. **Snapshot Restore Point (`.bak`):** Creates an automatic backup before making any edits.
2. **Round 1 (Syntax & Typo Repair):** Repairs common misspellings (`fucntion` -> `fn`, `retrun` -> `return`) and injects missing semicolons.
3. **Round 2 (Null-Guard Injection):** Detects unguarded pointer dereferences and injects protective checks:
   ```ykn
   if ptr != null then {
       return ptr.value;
   }
   return 0;
   ```
4. **Round 3 (Semantic Completion):** Inserts missing return statements before closing braces.
5. **Compilation Verification Gate:** Any candidate patch is tested by the compiler; uncompilable candidates are discarded.

---

## 10. Memory Safety & Use-After-Free Detection

The safety engine actively monitors pointer lifetimes:

```ykn
bellek_serbest_birak(ptr); # Pointer freed

# Subsequent access triggers a compile-time fatal diagnostic:
# ERROR: [UAF] Access to freed pointer detected! Compilation halted.
```

---

## 11. Sandbox Gate: Capability Security

Ensures untrusted scripts or AI-generated modules cannot compromise the host:
* Disallows unauthorized `socket`, `connect`, `send` (Network).
* Blocks `CreateProcess`, `WinExec`, `execve` (Process execution).
* Blocks `LoadLibrary`, `dlopen` (Dynamic library hijacking).
* Rejects unauthorized disk writes outside permitted boundaries.

---

## 12. AI Architecture: Micro-LLM vs. Full LLM Engine

Yukinary embeds artificial intelligence through two complementary tiers designed for completely different operational goals:

### Comparison & Purpose Overview

| Dimension | 🧠 Embedded Micro-LLM (16 KB) | 🚀 External Full LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **Location & Memory** | Embedded directly inside compiler binary (`.rodata`) | External `.gguf` file loaded dynamically from disk |
| **Size & Weights** | **16,448 Bytes (~16 KB)** ternary $\{-1, 0, +1\}$ BitNet backbone | **1 GB – 40 GB** (Llama-3, Mistral, Qwen, BitNet b1.58, etc.) |
| **Hardware Prerequisites** | **Zero GPU, Zero Python, Zero Internet.** Runs on any standard CPU | Requires 4 GB – 32 GB RAM or dedicated GPU |
| **Latency** | **0.1 milliseconds (sub-perceptual instant execution)** | 10 – 50 milliseconds per token |
| **Primary Goal** | **Compiler Reflexes & Automation:**<br>• Syntax repair (`Self-Healing`: semicolons, braces)<br>• Protective `null-guard` injection against crashes<br>• Real-time VSCode structure field and intent completion<br>• Lightweight offline signature transpilation fallback | **Deep Semantic & Generative Power:**<br>• **Full Polyglot `#convert` Engine:** Translating entire Python, C, C++, and Rust libraries into native YKN<br>• Natural language to YKN synthesis (`#prompt "write a web server"`)<br>• Synthesizing complex data structures and modules from scratch |
| **User Invocations** | **Transparent (Always-On).** Operates silently in the background | **Explicit.** Invoked via `--llm-model model.gguf --prompt "..."` |

### 1. Embedded Micro-LLM
Runs automatically on every compile cycle. Requires no user configuration or downloads. It guarantees that self-healing, type narrowing, and basic completions function on offline or air-gapped environments.

### 2. External Full LLM Runtime & Model Setup

Yukinary works out-of-the-box with **zero downloads** using its embedded 16 KB Micro-LLM.

When you require deep generative coding (`#prompt`) or polyglot library translation (`#convert`), you can plug in any standard GGUF model (Microsoft BitNet b1.58, Prism ML Bonsai 8B, Qwen, etc.).

#### Why Models Are Not Stored in Git
Git is built for source code. GitHub enforces a strict **100 MB per-file limit**, while neural weights range from 1.1 GB to 40 GB. Like Ollama and llama.cpp, model weights are hosted on [Hugging Face](https://huggingface.co).

#### One-Command Automated Downloader
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

#### Manual Download Links (Hugging Face)
| Model | Size | Format | Direct Download Link | Target Local Path |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Download from Hugging Face](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Download from Hugging Face](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### Compiler CLI Usage Examples:
Pass the model to `ykn` using `--llm-model`:

* **Generative Synthesis (`#prompt`):**
  ```powershell
  ykn.exe main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Translate Python Code to Yukinary:**
  ```powershell
  ykn.exe convert math_helper.py --into-file math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Interactive Code Generation:**
  ```powershell
  ykn.exe ai "isv fibonacci(n: tamsayi) -> tamsayi" --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Offline Default (Embedded Micro-LLM):**
  Zero configuration required when running without `--llm-model`:
  ```powershell
  ykn.exe main.ykn --self-heal
  ```

---

## 13. LSP & VSCode Editor Integration

The embedded LSP server (`ykn_lsp.exe`) communicates via standard JSON-RPC:
* Autocompletes structure fields upon typing `.`,
* Displays inline parameter signatures,
* Emits real-time diagnostics with suggested code fixes.

---

## 14. Package Manager & Two-Repo Architecture

YKN splits its ecosystem into two clean GitHub repositories:
1. **Core Compiler (`yukinary/yukinary`):** The compiler, standard library, and runtime.
2. **Package Registry (`yukinary/packages`):** Central package index for modular downloads.

### `yukinary.toml`
```toml
[package]
name = "my_service"
version = "0.1.0"
main = "main.ykn"
output = "build/app.exe"

[dependencies]
crypto = "1.0.0"
http = { git = "https://github.com/ykncode/packages", path = "net/http" }
```

Command:
```powershell
.\ykn_full.exe add crypto
.\ykn_full.exe build
```

---

## 15. Polyglot Convert Engine (`#convert`) & Foreign Function Interface (FFI)

YKN eliminates the bootstrap library deficit through two primary interoperability pillars:

### A. Zero-Overhead C-ABI FFI (`harici isv`)
Directly invoke functions exported by C/C++ or Rust shared libraries (`.dll` on Windows, `.so` on Linux):

```ykn
# Declare C foreign functions
harici isv sqlite3_open(filename: *u8, db_ptr: **u8) -> i64;
harici isv sqlite3_close(db_ptr: *u8) -> i64;

fn init_db() -> i64 {
    let db: *u8 = 0;
    let rc: i64 = sqlite3_open("app.db".ptr, &db);
    return rc;
}
```

### B. `#convert` Directives & In-Compiler Transpiler
Seamlessly embed source code from other languages directly into your YKN files:

* `#C-convert` — Transpiles C/C++ functions and structs into native YKN
* `#PY-convert` — Transpiles Python definitions (`def`) into typed YKN functions
* `#JS-convert` — Transpiles JavaScript logic into YKN
* `#CS-convert` / `#RB-convert` — C# and Ruby logic conversion

Example Python conversion:
```ykn
#PY-convert
def square(x):
    return x * x
#end-convert

# At compile time, this is automatically translated into native YKN:
# fn square(x: i64) -> i64 {
#     return x * x;
# }
```

### C. Two-Level SHA-256 Cache (`.ykn_cache/convert/`)
All converted code blocks are cached with SHA-256 signatures in `.ykn_cache/convert/`. Repeated builds take 0 ms conversion overhead.

---

## 16. Build Verification & Cross-Platform Testing

Run the unified regression suite:
```powershell
powershell -ExecutionPolicy Bypass -File scratch/verify_all.ps1
```
All tests pass with **Exit Code 0** on native Windows PE32+ and Docker Alpine Linux ELF64.

---

## 17. What Can You Build with Yukinary? (Target Domains)

Yukinary compiles directly to native 64-bit machine code with **zero runtime dependencies** (no C-runtime, no JVM, no interpreter). It is engineered for performance-critical and resource-constrained environments:

### 1. High-Performance CLI Tools & Compilers
* Instant cold-start latency (< 1ms) with sub-megabyte binary footprints.
* **Living Proof:** The Yukinary compiler itself (over 40,000 lines of source code in `src/`) is written entirely in pure Yukinary and compiles itself deterministically (`self-hosting fixpoint`).

### 2. High-Throughput Network Servers & Microservices
* Asynchronous I/O via `std/ag/http.ykn`, `std/ag/websocket.ykn`, Windows IOCP (`std/es/iocp_loop.ykn`), and Linux `io_uring`/`epoll` (`std/os/os_linux_uring.ykn`).
* Delivers microsecond-level latency and orders of magnitude lower memory consumption than Node.js or Python.

### 3. Database & Storage Engines
* Standard library primitives include B-Tree indexing (`std/veritabani/b_tree.ykn`), Write-Ahead Logging (`std/veritabani/wal.ykn`), embedded SQL parser (`std/veritabani/sql_motoru.ykn`), and Vector Database search (`std/veritabani/hnsw.ykn`).

### 4. Embedded Neural Inference & Edge AI
* Embedded BitNet causal transformer weights, integer Q16.16 fixed-point math, and GGUF v3 parser (`std/ai/`).
* Run local inference without Python, PyTorch, or GPU driver installations.

### 5. Embedded Systems & IoT
* Minimal RAM overhead (< 1 MB), direct hardware pointer manipulation, and freestanding bare-metal execution.

---

## 18. Operating System Development: YknOS Architecture

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

## 19. Production Readiness, Boundaries & Guarantees

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

