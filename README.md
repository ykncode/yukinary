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
  <a href="#-key-features">Key Features</a> •
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

### 2. Write Your First Program (`hello.ykn`)
```ykn
isv ana() -> tamsayi {
    ekrana_yaz("Merhaba, Yukinary Dunyasi!\n".ptr);
    dondu 0;
}
```

### 3. Compile Directly to Native Machine Code
```powershell
# Windows
.\ykn.exe hello.ykn -o hello.exe
.\hello.exe

# Linux
./ykn hello.ykn -o hello
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
