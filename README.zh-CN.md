<div align="center">

<p align="center">
  <a href="README.md">English</a> |
  <a href="README.tr.md">Türkçe</a> |
  <a href="README.ru.md">Русский</a> |
  <a href="README.ja.md">日本語</a> |
  <b>简体中文</b> |
  <a href="README.ko.md">한국어</a> |
  <a href="README.fr.md">Français</a>
</p>

<img src="assets/logo.png" alt="Yukinary 编程语言 Logo" width="180" />

# Yukinary (YKN) 编程语言

**自举 (Self-Hosting) x86_64 编译器 • 直接生成 PE32+ 与 ELF64 代码（无 LLVM / 无 C 运行时）• 多语言语法 • 实验性自我修复与嵌入式 AI**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Benchmarks: ykncode/benchmarks](https://img.shields.io/badge/Benchmarks-ykncode%2Fbenchmarks-38bdf8)](https://github.com/ykncode/benchmarks)

<p align="center">
  <a href="#基准测试与性能-对比-c-rust-go-python">基准测试</a> •
  <a href="#快速开始与版本发布">下载与快速开始</a> •
  <a href="#语言文档">文档</a> •
  <a href="#micro-llm-与完整-llm-架构">AI 层（实验性）</a> •
  <a href="#核心特性">架构</a> •
  <a href="#操作系统开发-yknos-架构">YknOS</a> •
  <a href="#生产就绪度-诚实边界与保证">诚实边界</a> •
  <a href="#四仓库生态系统">生态系统</a>
</p>

---

</div>

## 概述

**Yukinary (YKN)** 是一门独立的、可自举的系统编程语言及其原生编译器。Yukinary 不以 LLVM IR 为目标，也不转译为 C，而是将 AST 直接降级为独立的 **x86_64 机器码**——从零开始写出 PE32+（Windows）和 ELF64（Linux）二进制头、系统调用与操作码，**无任何外部运行时依赖**（`libc`、`msvcrt` 或外部链接器）。

* **逐位一致的自举：** 编译器（使用 Yukinary 编写）能够确定性地编译自身（`Stage 2 SHA-256 == Stage 3 SHA-256`）。
* **规范 + 教学型多语言语法：** 关键字可以用英语、土耳其语、俄语、日语、简体中文、韩语或法语书写——专为本地化编程教育而设计；目前 `ykn fmt` 负责排版格式化，规范化的关键字/注释翻译则作为未来工具链功能进行规划。
* **独立运行 (Freestanding) 系统与操作系统目标：** 驱动一个实验性的基于 Linux 的操作系统用户空间（[`examples/yknos`](examples/yknos)）。

---

## 基准测试与性能 (对比 C, Rust, Go, Python)

所有基准测试框架、C/Rust/Python 对照实现以及原始 JSON 遥测数据均发布在我们专门的基准测试仓库中：  
[**官方基准测试套件：`github.com/ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) • [**工业级基准测试报告（土耳其语）**](https://github.com/ykncode/benchmarks/blob/main/industrial/YUKINARY_ENDUSTRIYEL_BENCHMARK_RAPORU.md)

<div align="center">
  <img src="assets/bench_compile_speed.png" alt="编译器吞吐量：Yukinary 对比 GCC、Rustc、Go、G++" width="92%" />
</div>

<div align="center">
  <img src="assets/bench_workloads.png" alt="运行时性能与峰值 RSS：系统负载与 BitNet 内核" width="92%" />
</div>

### 实测结果汇总（`ykncode/benchmarks`）

| 基准测试 | Yukinary (`ykn`) | C (`gcc -O3`) | Rust (`rustc -O3`) | Python (`3.13`) | 备注 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **编译器构建时间**（`optimizer`） | **18.6 ms** | 219.4 ms | 272.2 ms | — | 无 LLVM/链接器开销（**构建速度快 11x–31x**） |
| **系统 B-Tree**（10 万节点，5 万次查找） | **213 ms** | 133 ms | 221 ms | 327 ms | 校验和精确验证（`19526` / `6363996222`） |
| **BitNet b1.58**（3.35 GOP 三值内核） | **0.71 GOP/s** | 1.79 GOP/s | 1.74 GOP/s | 0.02 GOP/s | 标量原生代码生成（GCC/Rust 使用 LLVM/GCC AVX2） |
| **峰值内存（BitNet RSS）** | **2.6 MB** | 4.8 MB | 5.2 MB | 22.4 MB | **占用最低**（独立运行，无 libc） |
| **冷启动延迟** | **5.4 ms** | 5.9 ms | 6.8 ms | 35.0 ms | 直接进入操作系统入口点（`ExitProcess` / `sys_exit`） |

> **工程说明：** 由于 GCC/LLVM 的自动向量化（SIMD AVX2），C（`gcc -O3`）和 Rust（`rustc -O3`）在密集矩阵运算上仍快约 2.5x。Yukinary 依靠自研的标量 x86_64 编码器和线性扫描寄存器分配，优先保证即时编译、无 libc 的小体积占用以及零外部工具链依赖。所有源码负载可在 [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) 中浏览。

---

## 快速开始与版本发布

运行 Yukinary **无需** C++ 编译器或外部工具链。Releases 中提供独立的原生二进制文件：

### 1. 下载与安装
[**从 GitHub Releases 下载 Yukinary**](https://github.com/ykncode/yukinary/releases)

#### Windows（一键安装）
1. 从 Releases 下载 **`yukinary-setup-windows-x86_64.zip`** 并解压。
2. 双击 **`setup.cmd`**（或运行 `powershell -ExecutionPolicy Bypass -File setup.ps1`）。

#### Linux 与 macOS（一行命令安装）
1. 下载 **`yukinary-setup-linux-x86_64.tar.gz`** 并解压（`tar -xzf yukinary-setup-linux-x86_64.tar.gz`）。
2. 运行 **`./setup.sh`**。

#### Visual Studio Code 扩展（`.vsix`）
```bash
code --install-extension yukinary-lang-1.0.0.vsix
```

### 2. 编写并编译你的第一个程序（`hello.ykn`）
```ykn
# English syntax (canonical keywords; native stdio intrinsic: ekrana_yaz)
fn main() -> i64 {
    ekrana_yaz("Hello, Yukinary!\n".ptr);
    return 0;
}
```

直接编译为原生 Windows PE32+ 或 Linux ELF64 机器码：
```powershell
# Windows PE32+
ykn hello.ykn hello.exe
.\hello.exe

# 从 Windows 交叉编译或在原生 Linux 上编译 Linux ELF64
ykn --target linux hello.ykn hello
```

---

## 语言文档

开发者指南和语法参考提供多种自然语言版本：

| 语言 | 文件 | 说明 |
| :--- | :--- | :--- |
| **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能 |
| **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译与嵌入式 AI |
| **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진 및 메모리 안전성 |

> [!TIP]
> **本地化设计说明：** 多语言关键字使用动态的词法分析器 token 映射（`docs/Multi-lingual-add-language.md`），以降低学生和非英语母语者的入门门槛。对于协作开发的开源软件包，建议以英语作为规范标准。

---

## Micro-LLM 与完整 LLM 架构

Yukinary 通过**两个不同的层**将实验性神经网络工具集成到编译流水线中：

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

### 架构对比与运行角色

| 维度 | 嵌入式 Micro-LLM（16 KB） | 外部完整 LLM（1B – 70B GGUF） |
| :--- | :--- | :--- |
| **位置与存储** | 直接内嵌于编译器二进制文件中（`.rodata` 段） | 从用户存储中动态加载的外部 `.gguf` 文件 |
| **大小与权重** | **16,448 字节（约 16 KB）** 三值 `{-1, 0, +1}` BitNet 权重 | **1 GB – 40 GB**（BitNet b1.58、Llama-3、Mistral、Qwen 等） |
| **硬件要求** | **无需 GPU、无需 Python、无需联网。** 使用 Q16.16 定点运算，可在任意 64 位 CPU 上运行 | 根据量化方式和上下文大小，需要 4 GB – 32 GB RAM / VRAM |
| **推理延迟** | **亚毫秒级（< 1 ms）**——编译期开销极小 | 每个 token 10 – 50 毫秒 |
| **主要用途** | **编译器反射与工具链：**<br>• 语法与拼写错误修复（`--self-heal`：分号、括号）<br>• 注入防御性 `null-guard` 以防止崩溃<br>• 实时 LSP 结构体字段与 token 自动补全<br>• 轻量级离线关键字/签名回退 | **实验性生成式工具：**<br>• **Polyglot `#convert`：** 辅助将 C、Python 和 Rust 代码片段翻译为 YKN<br>• 自然语言提示指令（`#prompt "..."`） |
| **启用方式** | **始终启用**（无需外部模型） | 通过 `--llm-model <path.gguf>` **按需启用** |

### 如何下载和使用完整 LLM 模型（BitNet 与 Bonsai）

Yukinary 开箱即可编译代码，**无需下载任何模型**。在测试重量级的 `#prompt` 或 `#convert` 工作流时，可以让编译器指向托管在 [Hugging Face](https://huggingface.co) 上的标准 GGUF 模型。

#### 1. 一条命令下载
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

#### 2. 手动下载（Hugging Face 直链）

| 模型 | 大小 | 格式 | 直接下载链接 | 本地目标路径 |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [从 Hugging Face 下载](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [从 Hugging Face 下载](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. 如何在编译器中使用模型

##### 方法 A：源码内 `#config` 指令
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

##### 方法 B：命令行参数
```powershell
# 使用 #prompt 生成式指令进行编译
ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# 将 Python 文件翻译为 Yukinary
ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# 实验性自我修复语法修正（使用内嵌的 16 KB 引擎）
ykn main.ykn --self-heal
```

---

## 核心特性

### 1. 直接生成原生代码（无 C 运行时）
* 无需 LLVM、C 转译或外部链接器，直接从 AST 生成独立的可执行文件。
* Windows PE32+ 二进制文件直接调用 Kernel32 / NT 接口（`VirtualAlloc`、`WriteFile`、`ExitProcess`）。
* Linux ELF64 二进制文件直接调用原始 64 位系统调用（`sys_mmap`、`sys_write`、`sys_exit`）。

### 2. 多语言原生语法（TR、EN、RU、JA、ZH、KO、FR）
关键字在词法分析阶段动态映射：
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

### 3. 外部函数接口（`harici isv`）与 `#convert`
* **直接 FFI（`harici isv`）：** 通过操作系统动态加载器（`LoadLibraryA` / `dlopen`）调用已编译的 C/C++/Rust 共享库（`.dll` / `.so`）。
* **转换指令（`#C-convert`、`#PY-convert`、`#JS-convert`）：** 实验性的转译代码块，以 SHA-256 缓存于 `.ykn_cache/convert/`。

### 4. 带标签枚举（代数数据类型）与模式匹配
支持带负载和不带负载的变体，采用 16 字节对齐的 `{ discriminant, payload }` 表示：
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

### 5. 分层式 `#config` 编排器
直接在源码中调整操作系统二进制头、栈分配和目标参数：
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target microarchitecture
}
```

### 6. 自主自我修复引擎（`--onar` / `--self-heal`）
* **自动快照：** 在应用修复前创建 `.bak` 快照。
* **拼写与标点修复：** 通过 Levenshtein 匹配修复拼错的关键字和缺失的分号。
* **Null-Guard 注入：** 在未加保护的解引用周围注入保护性检查（`if ptr != null`）。
* **编译验证关卡：** 补丁必须先通过编译，才会修改磁盘上的源码。

### 7. 编译期内存安全与 UAF 检测
* 静态跟踪指针的生命周期和释放操作。
* 解引用已释放的指针会触发编译期诊断，从而在执行前捕获 Use-After-Free 模式。

### 8. 沙箱关卡安全
在 AST 层面限制不受信任的脚本调用未经授权的操作系统 API（`CreateProcess`、`socket`、`send`、`LoadLibrary`、`dlopen`）。

### 9. 寄存器分配
循环归纳变量被直接分配到被调用者保存（callee-saved）的机器寄存器（`RBX`、`R12`..`R15`），避免多余的栈访问。

### 10. 可复现的自举 Fixpoint
编译器能够干净地编译自身；Stage 2 与 Stage 3 二进制文件生成**100% 逐位一致的 SHA-256 哈希**：
`EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## 目标领域

由于 Yukinary 直接编译为非托管、零依赖的 64 位机器码，它适用于：

1. **CLI 工具与编译器：** 快速的构建迭代和小于 1 MB 的可执行文件（`src/` 本身即以 Yukinary 自举实现）。
2. **网络服务：** 底层异步原语（`std/ag/http.ykn`、Windows IOCP、Linux `io_uring`/`epoll`）。
3. **存储引擎：** B-Tree 索引（`std/veritabani/b_tree.ykn`）、预写日志（`wal.ykn`）和向量索引（`hnsw.ykn`）。
4. **嵌入式定点推理：** 无需 Python 或 PyTorch 的 Q16.16 整数推理与 GGUF v3 解析（`std/ai/`）。
5. **嵌入式系统与裸机目标：** 无 C 运行时依赖的直接指针访问。

---

## 操作系统开发: YknOS 架构

由于 Yukinary 生成不依赖 `libc` 的独立 x86_64 代码，它可以直接面向裸机环境和操作系统用户空间：

* 在 [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real) 中，一个最小化的概念验证 Ring-0 x86_64 内核实现了 Multiboot2 入口（`boot.asm`）、GDT（`gdt.ykn`）、IDT（`idt.ykn`）、PML4 分页（`paging.ykn`）、COM1 串口 UART 以及 VGA 文本输出。
* 在 [`examples/yknos/`](examples/yknos) 中，Yukinary 驱动着一个可运行的 **Linux 6.6 LTS + Alpine musl + YKN ELF64 用户空间** 系统（`ykn-init` PID 1 守护进程、`yknsh` shell、`yuki` CLI），可在 QEMU 中启动。

---

## 生产就绪度: 诚实边界与保证

### A. 已验证的工程特性
* **确定性的逐位一致 Fixpoint：** Stage 2 与 Stage 3 自举二进制文件逐位一致（`EF607B9A...`）。
* **零外部依赖：** 可执行文件可在原生 Windows（Kernel32）和 Linux（原始系统调用）上运行，无需任何外部 DLL 或共享库。
* **实测性能透明：** 与 GCC、Rustc、Go 和 CPython 对比的完整源码基准测试已在 [`ykncode/benchmarks`](https://github.com/ykncode/benchmarks) 中公开发布。

### B. 诚实边界（目前尚不适合使用 YKN 的场景）
* **实验性状态：** Yukinary 是由个人开发者构建的 v1.0.0 系统研究型编译器。它尚未像 GCC、Clang 或 Rust 那样经过数十年的边界情况打磨。请勿将其部署于安全关键或金融生产系统中。
* **密集型自动向量化数学运算：** 由于 Yukinary 使用自研的标量 x86_64 后端，不具备 LLVM SIMD AVX2 自动向量化，C（`-O3`）和 Rust（`opt-level=3`）在繁重的浮点或矩阵循环上仍然更快（约 2.5x）。
* **原生移动端 UI：** 目前尚无 iOS UIKit/SwiftUI 或 Android Jetpack Compose 的原生 UI 绑定。

### C. 担保
依据 MIT 和 Apache 2.0 许可证分发，按**"原样"提供，不附带任何形式的担保**。

---

## 四仓库生态系统

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

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary)：可自举的 Yukinary 编译器、标准库、文档以及 YknOS Linux 用户空间示例。
2. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks)：工业级性能对比、编译期遥测数据以及对照负载（C、Rust、Go、Python）。
3. [**`ykncode/packages`**](https://github.com/ykncode/packages)：模块化库的软件包注册中心（`net/http`、`math`、`crypto/sha256`、`data/json`、`terminal/colors`）。
4. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap)：独立的 C++17 种子编译器，用于在新机器上从零开始引导 Yukinary。

---

## 双重许可

Yukinary 采用双重许可，您可以任选 **MIT 许可证** 或 **Apache 许可证（2.0 版）**：

* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)