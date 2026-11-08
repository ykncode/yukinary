<div align="center">

# ❄️ Yukinary (YKN) Programming Language

**Zero-Dependency • Direct Native Machine Code • Multi-Lingual Grammar • Self-Healing • Embedded BitNet AI • Polyglot `#convert` & FFI**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Architecture: Zero C-Runtime](https://img.shields.io/badge/Runtime-Zero_C_Runtime-red)](https://github.com/ykncode/yukinary)

<p align="center">
  <a href="#-language-documentation--dil-kılavuzları">Documentation</a> •
  <a href="#-key-features">Key Features</a> •
  <a href="#-polyglot-convert-and-ffi-interop">Polyglot & FFI</a> •
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-two-repository-ecosystem">Ecosystem</a> •
  <a href="#-dual-license">License</a>
</p>

---

</div>

## 🌐 Language Documentation / Dil Kılavuzları

Yukinary provides comprehensive, beginner-friendly ("mala anlatır gibi") complete guides for all supported spoken languages:

| Language | File | Description |
| :--- | :--- | :--- |
| 🇹🇷 **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Detaylı Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, self-healing ve AI motoru |
| 🇬🇧 **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Complete Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| 🇷🇺 **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Полное руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| 🇯🇵 **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能、AIエンジン解説 |
| 🇨🇳 **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译、自愈编译机制与嵌入式 AI 引擎 |
| 🇰🇷 **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진, 메모리 안전성 및 임베디드 AI |

---

## ⚡ What is Yukinary?

**Yukinary (YKN)** is an independent, statically-typed, self-hosting systems programming language designed from the ground up to compile directly to executable machine code (**Windows PE32+** and **Linux ELF64**) without relying on external C libraries (libc/msvcrt), LLVM, or third-party linkers.

Yukinary introduces a unique balance between **high-performance systems programming**, **first-class multi-lingual grammar**, **autonomous compiler resilience**, and **instant access to the world's existing libraries via FFI and AI-driven code transpilation**.

---

## 🚀 Key Features

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
* **In-Compiler Polyglot Transpiler:** The embedded neural engine translates C functions, structs, types, Python `def`s, and Rust syntax directly into YKN.
* **Bridge Runtimes:** Supports embedded Python (`PYTHON_EMBED`) and Node.js JSON-RPC (`JS_RPC_IPC`) IPC process bridges.

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

### 9. Always-On 16 KB BitNet 1.58b Micro-LLM & External Full LLM Runtime
* Features an embedded 16,448-byte ternary $\{-1, 0, +1\}$ Transformer directly inside the compiler binary for zero-latency offline self-healing and linting.
* Can also load massive external GGUF models (7B, 13B, 70B BitNet / LLaMA / Mistral) via `--llm-model model.gguf` with custom IQ2_S and BPE tokenizer execution engines.

### 10. High-Performance Register Allocation
* Loop induction variables are automatically assigned to machine callee-saved registers (`RBX`, `R12`..`R15`), minimizing stack memory roundtrips.

### 11. Self-Hosting Reproducible Build Fixpoint
* The compiler compiles itself! Stage 2 and Stage 3 executables produce **100% bit-for-bit identical SHA-256 hashes**:
  `EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## 🛠️ Quick Start

### 1. Build the Bootstrap Compiler
On Windows (using MinGW GCC or Clang):
```powershell
g++ -O2 -std=c++17 -o app_full.exe app_full.cpp -lws2_32
```

On Linux:
```bash
g++ -O2 -std=c++17 -o app_full app_full.cpp -lpthread
```

### 2. Compile Your First Program
Create `hello.ykn`:
```ykn
isv ana() -> tamsayi {
    ekrana_yaz("Merhaba, Yukinary Dunyasi!\n".ptr);
    dondu 0;
}
```

Compile and run:
```powershell
# Windows
.\app_full.exe hello.ykn -o hello.exe
.\hello.exe

# Linux
./app_full hello.ykn -o hello
./hello
```

### 3. Run the Unified Test Suite
Run tests across both Windows native and Docker Alpine Linux:
```powershell
powershell -ExecutionPolicy Bypass -File scratch/verify_all.ps1
```

---

## 📦 Two-Repository Ecosystem

Yukinary is structured into two clean repositories:

```
┌──────────────────────────────────────┐       ┌──────────────────────────────────────┐
│        1. COMPILER CORE REPO         │       │       2. PACKAGE REGISTRY REPO       │
│         yukinary/yukinary            │       │          yukinary/packages           │
├──────────────────────────────────────┤       ├──────────────────────────────────────┤
│ • Bootstrap (app_full.cpp)           │       │ • Official & Community Libraries     │
│ • Self-Hosted Compiler (src/)        │       │ • Net, HTTP, Crypto, GUI, Math       │
│ • Standard Library (std/)            │       │ • Managed by `ykn add <pkg>`         │
│ • Language Server Protocol (ykn_lsp) │       │ • Synced via `yukinary.toml`         │
└──────────────────────────────────────┘       └──────────────────────────────────────┘
```

### Managing Dependencies in `yukinary.toml`:
```toml
[package]
name = "my_project"
version = "0.1.0"
main = "main.ykn"
output = "build/app.exe"

[dependencies]
math = "1.0.0"
http = { git = "https://github.com/ykncode/packages", path = "net/http" }
```

---

## 📜 Dual License

Yukinary is distributed under the terms of both the **MIT License** and the **Apache License (Version 2.0)**, at your option (matching the Rust licensing model).

See:
* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)

Contributions submitted for inclusion in Yukinary shall be dual-licensed as above without any additional terms or conditions.
