<div align="center">

<p align="center">
  <a href="README.md">English</a> |
  <a href="README.tr.md">Türkçe</a> |
  <a href="README.ru.md">Русский</a> |
  <b>日本語</b> |
  <a href="README.zh-CN.md">简体中文</a> |
  <a href="README.ko.md">한국어</a> |
  <a href="README.fr.md">Français</a>
</p>

<img src="assets/logo.png" alt="Yukinary プログラミング言語のロゴ" width="180" />

# Yukinary (YKN) プログラミング言語

**セルフホスティング x86_64 コンパイラ • PE32+ & ELF64 の直接コード生成 (LLVM なし / C ランタイムなし) • 多言語構文 • 実験的な自己修復機能と組み込み AI**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Benchmarks: ykncode/benchmarks](https://img.shields.io/badge/Benchmarks-ykncode%2Fbenchmarks-38bdf8)](https://github.com/ykncode/benchmarks)

<p align="center">
  <a href="#ベンチマークとパフォーマンス-c-rust-go-python-との比較">ベンチマーク</a> •
  <a href="#クイックスタートとリリース">ダウンロードとクイックスタート</a> •
  <a href="#言語ドキュメント">ドキュメント</a> •
  <a href="#micro-llm-と-full-llm-のアーキテクチャ">AI レイヤー (実験的)</a> •
  <a href="#主な機能">アーキテクチャ</a> •
  <a href="#os-開発-yknos-アーキテクチャ">YknOS</a> •
  <a href="#本番運用への適合性-正直な限界と保証">正直な限界</a> •
  <a href="#4リポジトリ構成のエコシステム">エコシステム</a>
</p>

---

</div>

## 概要

**Yukinary (YKN)** は、独立したセルフホスティングのシステムプログラミング言語およびネイティブコンパイラです。LLVM IR をターゲットにしたり C へトランスパイルしたりするのではなく、Yukinary は AST を直接スタンドアロンの **x86_64 マシンコード**へと変換します。PE32+ (Windows) および ELF64 (Linux) のバイナリヘッダ、syscall、オペコードをゼロから書き出し、**外部ランタイム依存は一切ありません** (`libc`、`msvcrt`、外部リンカーのいずれも不要)。

* **ビット単位で同一のセルフホスティング:** コンパイラ (Yukinary で記述) は自分自身を決定論的にコンパイルします (`Stage 2 SHA-256 == Stage 3 SHA-256`)。
* **正準 + 教育向けの多言語構文:** キーワードは英語、トルコ語、ロシア語、日本語、簡体字中国語、韓国語、フランス語で記述できます。母語でのプログラミング教育を目的として設計されており、現在 `ykn fmt` はレイアウトの整形を担い、正準キーワード/コメントへの翻訳は将来のツールとして計画されています。
* **フリースタンディングなシステムと OS ターゲット:** 実験的な Linux ベースの OS ユーザースペースを支えています ([`examples/yknos`](examples/yknos))。

---

## ベンチマークとパフォーマンス (C, Rust, Go, Python との比較)

すべてのベンチマークハーネス、C/Rust/Python による比較対象の実装、および生の JSON テレメトリは、専用のベンチマークリポジトリで公開しています:  
[**公式ベンチマークスイート: `github.com/ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) • [**産業向けベンチマークレポート (TR)**](https://github.com/ykncode/benchmarks/blob/main/industrial/YUKINARY_ENDUSTRIYEL_BENCHMARK_RAPORU.md)

<div align="center">
  <img src="assets/bench_compile_speed.png" alt="コンパイラのスループット: Yukinary と GCC、Rustc、Go、G++ の比較" width="92%" />
</div>

<div align="center">
  <img src="assets/bench_workloads.png" alt="実行時パフォーマンスとピーク RSS: システムワークロードと BitNet カーネル" width="92%" />
</div>

### 計測結果のまとめ (`ykncode/benchmarks`)

| ベンチマーク | Yukinary (`ykn`) | C (`gcc -O3`) | Rust (`rustc -O3`) | Python (`3.13`) | 備考 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **コンパイラのビルド時間** (`optimizer`) | **18.6 ms** | 219.4 ms | 272.2 ms | — | LLVM/リンカーのオーバーヘッドなし (**11x–31x 高速なビルド**) |
| **システム B-Tree** (10 万ノード、5 万回検索) | **213 ms** | 133 ms | 221 ms | 327 ms | チェックサムの完全一致を検証済み (`19526` / `6363996222`) |
| **BitNet b1.58** (3.35 GOP の三値カーネル) | **0.71 GOP/s** | 1.79 GOP/s | 1.74 GOP/s | 0.02 GOP/s | スカラーのネイティブコード生成 (GCC/Rust は LLVM/GCC の AVX2 を使用) |
| **ピークメモリ (BitNet RSS)** | **2.6 MB** | 4.8 MB | 5.2 MB | 22.4 MB | **最小のフットプリント** (フリースタンディング、libc なし) |
| **コールドスタートのレイテンシ** | **5.4 ms** | 5.9 ms | 6.8 ms | 35.0 ms | OS のエントリポイントを直接使用 (`ExitProcess` / `sys_exit`) |

> **エンジニアリングノート:** 密な行列演算では、GCC/LLVM の自動ベクトル化 (SIMD AVX2) により、C (`gcc -O3`) と Rust (`rustc -O3`) が依然として約 2.5x 高速です。Yukinary は線形スキャン方式のレジスタ割り当てを備えた独自のスカラー x86_64 エンコーダを採用しており、瞬時のコンパイル、libc 不要の小さなフットプリント、外部ツールチェーンへの依存ゼロを優先しています。すべてのワークロードのソースは [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) で閲覧できます。

---

## クイックスタートとリリース

Yukinary の実行に C++ コンパイラや外部ツールチェーンは**必要ありません**。スタンドアロンのネイティブバイナリを Releases で入手できます:

### 1. ダウンロードとセットアップ
[**GitHub Releases から Yukinary をダウンロード**](https://github.com/ykncode/yukinary/releases)

#### Windows (ワンクリックセットアップ)
1. Releases から **`yukinary-setup-windows-x86_64.zip`** をダウンロードして展開します。
2. **`setup.cmd`** をダブルクリックします (または `powershell -ExecutionPolicy Bypass -File setup.ps1` を実行します)。

#### Linux & macOS (1 行セットアップ)
1. **`yukinary-setup-linux-x86_64.tar.gz`** をダウンロードして展開します (`tar -xzf yukinary-setup-linux-x86_64.tar.gz`)。
2. **`./setup.sh`** を実行します。

#### Visual Studio Code 拡張機能 (`.vsix`)
```bash
code --install-extension yukinary-lang-1.0.0.vsix
```

### 2. 最初のプログラムを書いてコンパイルする (`hello.ykn`)
```ykn
# English syntax (canonical keywords; native stdio intrinsic: ekrana_yaz)
fn main() -> i64 {
    ekrana_yaz("Hello, Yukinary!\n".ptr);
    return 0;
}
```

Windows PE32+ または Linux ELF64 のネイティブマシンコードへ直接コンパイルします:
```powershell
# Windows PE32+
ykn hello.ykn hello.exe
.\hello.exe

# Windows からのクロスコンパイル、またはネイティブ Linux で Linux ELF64 を生成
ykn --target linux hello.ykn hello
```

---

## 言語ドキュメント

開発者ガイドと構文リファレンスは、サポートされている各言語で提供されています:

| 言語 | ファイル | 説明 |
| :--- | :--- | :--- |
| **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能 |
| **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译与嵌入式 AI |
| **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진 및 메모리 안전성 |

> [!TIP]
> **ローカライズに関する設計ノート:** 多言語キーワードは、学生や英語を母語としない人々の参入障壁を下げるため、レキサーの動的なトークンマッピング (`docs/Multi-lingual-add-language.md`) を使用しています。共同開発するオープンソースパッケージでは、正準の標準として英語の使用を推奨します。

---

## Micro-LLM と Full LLM のアーキテクチャ

Yukinary は、**2 つの独立したレイヤー**を通じて、実験的なニューラルツールをコンパイルパイプラインに統合しています:

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

### アーキテクチャの比較と運用上の役割

| 観点 | 組み込み Micro-LLM (16 KB) | 外部 Full LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **配置とストレージ** | コンパイラバイナリに直接埋め込み (`.rodata` セクション) | ユーザーのストレージから動的に読み込まれる外部 `.gguf` ファイル |
| **サイズと重み** | **16,448 Bytes (~16 KB)** の三値 `{-1, 0, +1}` BitNet 重み | **1 GB – 40 GB** (BitNet b1.58、Llama-3、Mistral、Qwen など) |
| **ハードウェア要件** | **GPU 不要、Python 不要、インターネット不要。** Q16.16 固定小数点演算により、あらゆる 64 ビット CPU で動作 | 量子化とコンテキストサイズに応じて 4 GB – 32 GB の RAM / VRAM が必要 |
| **推論レイテンシ** | **1 ミリ秒未満 (< 1 ms)** — コンパイル時のオーバーヘッドは最小限 | トークンあたり 10 – 50 ミリ秒 |
| **主な用途** | **コンパイラの反射機能とツール:**<br>• 構文とタイプミスの修復 (`--self-heal`: セミコロン、括弧)<br>• クラッシュを防ぐ防御的な `null-guard` の挿入<br>• リアルタイムの LSP による構造体フィールドとトークンの補完<br>• 軽量なオフラインのキーワード/シグネチャのフォールバック | **実験的な生成ツール:**<br>• **Polyglot `#convert`:** C、Python、Rust のコード片を YKN へ翻訳する支援<br>• 自然言語のプロンプトディレクティブ (`#prompt "..."`) |
| **有効化モード** | **常時有効** (外部モデル不要) | `--llm-model <path.gguf>` による**オプトイン** |

### Full LLM モデル (BitNet & Bonsai) のダウンロードと使い方

Yukinary は**モデルをダウンロードしなくても**、そのままコードをコンパイルできます。重量級の `#prompt` や `#convert` ワークフローを試す場合は、[Hugging Face](https://huggingface.co) で公開されている標準的な GGUF モデルをコンパイラに指定できます。

#### 1. ワンコマンドでのダウンロード
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

#### 2. 手動ダウンロード (Hugging Face への直接リンク)

| モデル | サイズ | 形式 | 直接ダウンロードリンク | 配置先のローカルパス |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Hugging Face からダウンロード](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Hugging Face からダウンロード](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. コンパイラでモデルを使用する方法

##### 方法 A: ソース内の `#config` ディレクティブ
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

##### 方法 B: コマンドラインフラグ
```powershell
# #prompt 生成ディレクティブを使ってコンパイル
ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Python ファイルを Yukinary に変換
ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# 実験的な自己修復による構文修正 (組み込みの 16 KB エンジンを使用)
ykn main.ykn --self-heal
```

---

## 主な機能

### 1. ネイティブコードの直接生成 (C ランタイム不要)
* LLVM、C へのトランスパイル、外部リンカーを使わずに、AST からスタンドアロンの実行ファイルを直接出力します。
* Windows PE32+ バイナリは Kernel32 / NT の呼び出しを直接行います (`VirtualAlloc`、`WriteFile`、`ExitProcess`)。
* Linux ELF64 バイナリは生の 64 ビット syscall を直接呼び出します (`sys_mmap`、`sys_write`、`sys_exit`)。

### 2. 多言語ネイティブ文法 (TR, EN, RU, JA, ZH, KO, FR)
キーワードはレキサーの段階で動的にマッピングされます:
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

### 3. 外部関数インターフェース (`harici isv`) と `#convert`
* **直接 FFI (`harici isv`):** OS の動的ローダー (`LoadLibraryA` / `dlopen`) を介して、コンパイル済みの C/C++/Rust 共有ライブラリ (`.dll` / `.so`) を呼び出します。
* **変換ディレクティブ (`#C-convert`、`#PY-convert`、`#JS-convert`):** `.ykn_cache/convert/` に SHA-256 でキャッシュされる実験的なトランスパイルブロックです。

### 4. タグ付き列挙型 (代数的データ型) とパターンマッチング
16 バイト境界に整列された `{ discriminant, payload }` 表現を用いて、ペイロードの有無を問わずバリアントをサポートします:
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

### 5. 階層型 `#config` オーケストレーター
OS バイナリヘッダ、スタック割り当て、ターゲットフラグをソースから直接調整できます:
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target microarchitecture
}
```

### 6. 自律型自己修復エンジン (`--onar` / `--self-heal`)
* **自動スナップショット:** 修復を適用する前に `.bak` スナップショットを作成します。
* **タイプミスと句読点の修復:** Levenshtein マッチングにより、綴りを誤ったキーワードや欠落したセミコロンを修復します。
* **Null-Guard の挿入:** ガードされていない参照外しの周囲に保護ガード (`if ptr != null`) を挿入します。
* **コンパイル検証ゲート:** パッチはディスク上のソースに適用される前に、コンパイルを通過する必要があります。

### 7. コンパイル時のメモリ安全性と UAF 検出
* ポインタの生存期間と解放を静的に追跡します。
* 解放済みポインタの参照外しはコンパイル時の診断を発生させ、Use-After-Free パターンを実行前に検出します。

### 8. サンドボックスゲートによるセキュリティ
信頼できないスクリプトが未承認の OS API (`CreateProcess`、`socket`、`send`、`LoadLibrary`、`dlopen`) を呼び出すことを AST レベルで制限します。

### 9. レジスタ割り当て
ループの帰納変数は callee-saved のマシンレジスタ (`RBX`、`R12`..`R15`) に直接割り当てられ、冗長なスタックアクセスを回避します。

### 10. 再現可能なセルフホスティング fixpoint
コンパイラは自分自身をクリーンにコンパイルし、Stage 2 と Stage 3 のバイナリは **100% ビット単位で同一の SHA-256 ハッシュ**を生成します:
`EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## 対象領域

Yukinary はアンマネージドで依存関係のない 64 ビットマシンコードへ直接コンパイルされるため、次の用途を想定して設計されています:

1. **CLI ツールとコンパイラ:** 高速なビルドの反復と 1 MB 未満の実行ファイル (`src/` 自体が Yukinary でセルフホストされています)。
2. **ネットワークサービス:** 低レベルの非同期プリミティブ (`std/ag/http.ykn`、Windows IOCP、Linux `io_uring`/`epoll`)。
3. **ストレージエンジン:** B-Tree インデックス (`std/veritabani/b_tree.ykn`)、ログ先行書き込み (`wal.ykn`)、ベクトルインデックス (`hnsw.ykn`)。
4. **組み込みの固定小数点推論:** Python や PyTorch を使わない Q16.16 整数推論と GGUF v3 パース (`std/ai/`)。
5. **組み込みシステムとベアメタルターゲット:** C ランタイムに依存しない直接的なポインタアクセス。

---

## OS 開発: YknOS アーキテクチャ

Yukinary は `libc` に依存せずにフリースタンディングな x86_64 コードを出力するため、ベアメタル環境や OS ユーザースペースを直接ターゲットにできます:

* [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real) には、最小限の概念実証用 Ring-0 x86_64 カーネルがあり、Multiboot2 エントリ (`boot.asm`)、GDT (`gdt.ykn`)、IDT (`idt.ykn`)、PML4 ページング (`paging.ykn`)、COM1 シリアル UART、VGA テキスト出力を実装しています。
* [`examples/yknos/`](examples/yknos) では、Yukinary が QEMU で起動可能な、動作する **Linux 6.6 LTS + Alpine musl + YKN ELF64 ユーザースペース**システム (`ykn-init` PID 1 スーパーバイザ、`yknsh` シェル、`yuki` CLI) を支えています。

---

## 本番運用への適合性: 正直な限界と保証

### A. 検証済みのエンジニアリング特性
* **決定論的でビット単位に同一な fixpoint:** Stage 2 と Stage 3 のセルフホストされたバイナリはビット単位で一致します (`EF607B9A...`)。
* **外部依存ゼロ:** 実行ファイルは、外部 DLL や共有ライブラリなしで、標準の Windows (Kernel32) と Linux (生の syscall) 上で動作します。
* **計測パフォーマンスの透明性:** GCC、Rustc、Go、CPython と比較した完全なソース付きベンチマークを [`ykncode/benchmarks`](https://github.com/ykncode/benchmarks) で公開しています。

### B. 正直な限界 (まだ YKN を使うべきでない場面)
* **実験的なステータス:** Yukinary は、個人開発者によって作られた v1.0.0 のシステム研究用コンパイラです。GCC、Clang、Rust が持つような、数十年にわたるエッジケースへの堅牢化の蓄積はありません。安全性が重要なシステムや金融系の本番システムには導入しないでください。
* **自動ベクトル化を伴う密な数値演算:** Yukinary は LLVM の SIMD AVX2 自動ベクトル化を持たない独自のスカラー x86_64 バックエンドを使用しているため、重い浮動小数点演算や行列ループでは C (`-O3`) と Rust (`opt-level=3`) の方が高速 (約 2.5x) です。
* **ネイティブモバイル UI:** 現時点では、iOS UIKit/SwiftUI や Android Jetpack Compose 向けのネイティブ UI バインディングはありません。

### C. 保証
MIT および Apache 2.0 ライセンスの下で配布され、**「現状のまま (AS IS)」、いかなる種類の保証もなく**提供されます。

---

## 4リポジトリ構成のエコシステム

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

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary): セルフホスティングの Yukinary コンパイラ、標準ライブラリ、ドキュメント、および YknOS Linux ユーザースペースの例。
2. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks): 産業向けのパフォーマンス比較、コンパイル時のテレメトリ、比較対象のワークロード (C、Rust、Go、Python)。
3. [**`ykncode/packages`**](https://github.com/ykncode/packages): モジュール式ライブラリのためのパッケージレジストリ (`net/http`、`math`、`crypto/sha256`、`data/json`、`terminal/colors`)。
4. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap): 新しいマシン上で Yukinary をゼロからブートストラップするための、スタンドアロンの C++17 シードコンパイラ。

---

## デュアルライセンス

Yukinary は、**MIT License** または **Apache License (Version 2.0)** のいずれかを選択できるデュアルライセンスで提供されています:

* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)
