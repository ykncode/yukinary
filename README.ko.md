<div align="center">

<p align="center">
  <a href="README.md">English</a> |
  <a href="README.tr.md">Türkçe</a> |
  <a href="README.ru.md">Русский</a> |
  <a href="README.ja.md">日本語</a> |
  <a href="README.zh-CN.md">简体中文</a> |
  <b>한국어</b> |
  <a href="README.fr.md">Français</a>
</p>

<img src="assets/logo.png" alt="Yukinary 프로그래밍 언어 로고" width="180" />

# Yukinary (YKN) 프로그래밍 언어

**Self-Hosting x86_64 컴파일러 • PE32+ 및 ELF64 직접 코드 생성 (LLVM 없음 / C 런타임 없음) • 다국어 문법 • 실험적 Self-Healing 및 내장 AI**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Benchmarks: ykncode/benchmarks](https://img.shields.io/badge/Benchmarks-ykncode%2Fbenchmarks-38bdf8)](https://github.com/ykncode/benchmarks)

<p align="center">
  <a href="#벤치마크-및-성능-vs-c-rust-go-python">벤치마크</a> •
  <a href="#빠른-시작-및-릴리스">다운로드 및 빠른 시작</a> •
  <a href="#언어-문서">문서</a> •
  <a href="#micro-llm-vs-full-llm-아키텍처">AI 레이어 (실험적)</a> •
  <a href="#주요-기능">아키텍처</a> •
  <a href="#운영체제-개발-yknos-아키텍처">YknOS</a> •
  <a href="#프로덕션-준비도-솔직한-한계-및-보장">솔직한 한계</a> •
  <a href="#4개-저장소-생태계">생태계</a>
</p>

---

</div>

## 개요

**Yukinary (YKN)**는 독립적인 self-hosting 시스템 프로그래밍 언어이자 네이티브 컴파일러입니다. Yukinary는 LLVM IR을 타깃으로 하거나 C로 트랜스파일하는 대신, AST를 독립 실행형 **x86_64 기계어**로 직접 낮춥니다. PE32+ (Windows) 및 ELF64 (Linux) 바이너리 헤더, syscall, opcode를 처음부터 직접 작성하며 **외부 런타임 의존성이 전혀 없습니다** (`libc`, `msvcrt`, 외부 링커 불필요).

* **비트 단위로 동일한 Self-Hosting:** Yukinary로 작성된 컴파일러가 자기 자신을 결정론적으로 컴파일합니다 (`Stage 2 SHA-256 == Stage 3 SHA-256`).
* **표준 + 교육용 다국어 문법:** 키워드를 영어, 터키어, 러시아어, 일본어, 중국어 간체, 한국어 또는 프랑스어로 작성할 수 있습니다. 현지화된 프로그래밍 교육을 위해 설계되었으며, 현재 `ykn fmt`는 레이아웃 포매팅을 담당하고, 표준 키워드/주석 번역은 향후 도구로 계획되어 있습니다.
* **Freestanding 시스템 및 OS 타깃:** 실험적인 Linux 기반 OS 사용자 공간을 구동합니다 ([`examples/yknos`](examples/yknos)).

---

## 벤치마크 및 성능 (vs. C, Rust, Go, Python)

모든 벤치마크 하네스, C/Rust/Python 비교 대상 구현, 원시 JSON 텔레메트리는 전용 벤치마크 저장소에 공개되어 있습니다:  
[**공식 벤치마크 스위트: `github.com/ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) • [**산업용 벤치마크 보고서 (TR)**](https://github.com/ykncode/benchmarks/blob/main/industrial/YUKINARY_ENDUSTRIYEL_BENCHMARK_RAPORU.md)

<div align="center">
  <img src="assets/bench_compile_speed.png" alt="컴파일러 처리량: Yukinary vs GCC, Rustc, Go, G++" width="92%" />
</div>

<div align="center">
  <img src="assets/bench_workloads.png" alt="런타임 성능 및 Peak RSS: 시스템 워크로드와 BitNet 커널" width="92%" />
</div>

### 측정 결과 요약 (`ykncode/benchmarks`)

| 벤치마크 | Yukinary (`ykn`) | C (`gcc -O3`) | Rust (`rustc -O3`) | Python (`3.13`) | 비고 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **컴파일러 빌드 시간** (`optimizer`) | **18.6 ms** | 219.4 ms | 272.2 ms | — | LLVM/링커 오버헤드 없음 (**빌드 11x–31x 고속**) |
| **시스템 B-Tree** (노드 100k, 검색 50k) | **213 ms** | 133 ms | 221 ms | 327 ms | 체크섬 정확히 검증됨 (`19526` / `6363996222`) |
| **BitNet b1.58** (3.35 GOP 3진 커널) | **0.71 GOP/s** | 1.79 GOP/s | 1.74 GOP/s | 0.02 GOP/s | 스칼라 네이티브 코드 생성 (GCC/Rust는 LLVM/GCC AVX2 사용) |
| **최대 메모리 (BitNet RSS)** | **2.6 MB** | 4.8 MB | 5.2 MB | 22.4 MB | **최소 메모리 사용량** (freestanding, libc 없음) |
| **콜드 스타트 지연 시간** | **5.4 ms** | 5.9 ms | 6.8 ms | 35.0 ms | OS 엔트리 포인트 직접 사용 (`ExitProcess` / `sys_exit`) |

> **엔지니어링 노트:** C (`gcc -O3`)와 Rust (`rustc -O3`)는 GCC/LLVM 자동 벡터화(SIMD AVX2) 덕분에 밀집 행렬 연산에서 여전히 약 2.5x 더 빠릅니다. Yukinary는 linear-scan 레지스터 할당을 갖춘 자체 스칼라 x86_64 인코더를 사용하며, 즉각적인 컴파일, libc 없는 작은 메모리 사용량, 외부 툴체인 의존성 제거를 우선시합니다. 모든 워크로드 소스는 [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks)에서 확인할 수 있습니다.

---

## 빠른 시작 및 릴리스

Yukinary를 실행하는 데 C++ 컴파일러나 외부 툴체인은 **필요하지 않습니다**. 독립 실행형 네이티브 바이너리를 Releases에서 받을 수 있습니다:

### 1. 다운로드 및 설치
[**GitHub Releases에서 Yukinary 다운로드**](https://github.com/ykncode/yukinary/releases)

#### Windows (원클릭 설치)
1. Releases에서 **`yukinary-setup-windows-x86_64.zip`**을 다운로드하고 압축을 풉니다.
2. **`setup.cmd`**를 더블 클릭합니다 (또는 `powershell -ExecutionPolicy Bypass -File setup.ps1` 실행).

#### Linux 및 macOS (한 줄 설치)
1. **`yukinary-setup-linux-x86_64.tar.gz`**를 다운로드하고 압축을 풉니다 (`tar -xzf yukinary-setup-linux-x86_64.tar.gz`).
2. **`./setup.sh`**를 실행합니다.

#### Visual Studio Code 확장 (`.vsix`)
```bash
code --install-extension yukinary-lang-1.0.0.vsix
```

### 2. 첫 프로그램 작성 및 컴파일 (`hello.ykn`)
```ykn
# English syntax (canonical keywords; native stdio intrinsic: ekrana_yaz)
fn main() -> i64 {
    ekrana_yaz("Hello, Yukinary!\n".ptr);
    return 0;
}
```

네이티브 Windows PE32+ 또는 Linux ELF64 기계어로 직접 컴파일합니다:
```powershell
# Windows PE32+
ykn hello.ykn hello.exe
.\hello.exe

# Windows에서 크로스 컴파일 또는 네이티브 Linux에서 Linux ELF64 생성
ykn --target linux hello.ykn hello
```

---

## 언어 문서

개발자 가이드와 문법 레퍼런스는 지원되는 여러 언어로 제공됩니다:

| 언어 | 파일 | 설명 |
| :--- | :--- | :--- |
| **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能 |
| **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译与嵌入式 AI |
| **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진 및 메모리 안전성 |

> [!TIP]
> **현지화 설계 노트:** 다국어 키워드는 동적 lexer 토큰 매핑(`docs/Multi-lingual-add-language.md`)을 사용하여 학생과 비영어권 사용자의 진입 장벽을 낮춥니다. 협업 기반 오픈 소스 패키지에는 영어를 표준으로 사용할 것을 권장합니다.

---

## Micro-LLM vs. Full LLM 아키텍처

Yukinary는 **두 개의 독립된 레이어**를 통해 실험적인 신경망 도구를 컴파일 파이프라인에 통합합니다:

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

### 아키텍처 비교 및 운영상 역할

| 항목 | 내장 Micro-LLM (16 KB) | 외부 Full LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **위치 및 저장 방식** | 컴파일러 바이너리에 직접 내장 (`.rodata` 섹션) | 사용자 저장소에서 동적으로 로드되는 외부 `.gguf` 파일 |
| **크기 및 가중치** | **16,448 Bytes (~16 KB)** 3진 `{-1, 0, +1}` BitNet 가중치 | **1 GB – 40 GB** (BitNet b1.58, Llama-3, Mistral, Qwen 등) |
| **하드웨어 요구 사항** | **GPU, Python, 인터넷 모두 불필요.** Q16.16 고정소수점 연산으로 모든 64비트 CPU에서 실행 | 양자화 및 컨텍스트 크기에 따라 4 GB – 32 GB RAM / VRAM 필요 |
| **추론 지연 시간** | **1밀리초 미만 (< 1 ms)** — 컴파일 시간 오버헤드 최소 | 토큰당 10 – 50밀리초 |
| **주요 범위** | **컴파일러 반사 작용 및 도구:**<br>• 문법 및 오타 복구 (`--self-heal`: 세미콜론, 괄호)<br>• 크래시 방지를 위한 방어적 `null-guard` 삽입<br>• 실시간 LSP 구조체 필드 및 토큰 자동 완성<br>• 경량 오프라인 키워드/시그니처 폴백 | **실험적 생성형 도구:**<br>• **Polyglot `#convert`:** C, Python, Rust 코드 조각을 YKN으로 변환하는 작업 지원<br>• 자연어 프롬프트 지시문 (`#prompt "..."`) |
| **활성화 방식** | **항상 활성** (외부 모델 불필요) | `--llm-model <path.gguf>`로 **선택적 활성화** |

### Full LLM 모델 다운로드 및 사용 방법 (BitNet 및 Bonsai)

Yukinary는 **모델을 전혀 다운로드하지 않아도** 바로 코드를 컴파일할 수 있습니다. 무거운 `#prompt` 또는 `#convert` 워크플로를 테스트할 때는 [Hugging Face](https://huggingface.co)에 호스팅된 표준 GGUF 모델을 컴파일러에 지정할 수 있습니다.

#### 1. 단일 명령 다운로드
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

#### 2. 수동 다운로드 (Hugging Face 직접 링크)

| 모델 | 크기 | 형식 | 직접 다운로드 링크 | 로컬 대상 경로 |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Hugging Face에서 다운로드](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Hugging Face에서 다운로드](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. 컴파일러에서 모델 사용하기

##### 방법 A: 소스 내 `#config` 지시문
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

##### 방법 B: 명령줄 플래그
```powershell
# #prompt 생성형 지시문과 함께 컴파일
ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Python 파일을 Yukinary로 변환
ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# 실험적 self-healing 문법 복구 (내장 16 KB 엔진 사용)
ykn main.ykn --self-heal
```

---

## 주요 기능

### 1. 네이티브 코드 직접 생성 (C 런타임 없음)
* LLVM, C 트랜스파일, 외부 링커 없이 AST에서 직접 독립 실행 파일을 생성합니다.
* Windows PE32+ 바이너리는 Kernel32 / NT 호출을 직접 사용합니다 (`VirtualAlloc`, `WriteFile`, `ExitProcess`).
* Linux ELF64 바이너리는 원시 64비트 syscall을 직접 호출합니다 (`sys_mmap`, `sys_write`, `sys_exit`).

### 2. 다국어 네이티브 문법 (TR, EN, RU, JA, ZH, KO, FR)
키워드는 lexer 단계에서 동적으로 매핑됩니다:
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

### 3. 외부 함수 인터페이스 (`harici isv`) 및 `#convert`
* **직접 FFI (`harici isv`):** OS 동적 로더(`LoadLibraryA` / `dlopen`)를 통해 컴파일된 C/C++/Rust 공유 라이브러리(`.dll` / `.so`)를 호출합니다.
* **변환 지시문 (`#C-convert`, `#PY-convert`, `#JS-convert`):** `.ykn_cache/convert/`에 SHA-256으로 캐시되는 실험적 트랜스파일 블록입니다.

### 4. 태그드 열거형 (대수적 데이터 타입) 및 패턴 매칭
16바이트 정렬된 `{ discriminant, payload }` 표현을 사용하여 페이로드가 있는 variant와 없는 variant를 모두 지원합니다:
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

### 5. 계층형 `#config` 오케스트레이터
OS 바이너리 헤더, 스택 할당, 타깃 플래그를 소스에서 직접 조정할 수 있습니다:
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target microarchitecture
}
```

### 6. 자율 Self-Healing 엔진 (`--onar` / `--self-heal`)
* **자동 스냅샷:** 복구를 적용하기 전에 `.bak` 스냅샷을 생성합니다.
* **오타 및 구두점 복구:** Levenshtein 매칭을 통해 철자가 틀린 키워드와 누락된 세미콜론을 복구합니다.
* **Null-Guard 삽입:** 보호되지 않은 역참조 주변에 보호 가드(`if ptr != null`)를 삽입합니다.
* **컴파일 검증 게이트:** 패치는 디스크의 소스를 수정하기 전에 반드시 컴파일을 통과해야 합니다.

### 7. 컴파일 타임 메모리 안전성 및 UAF 탐지
* 포인터의 수명과 해제를 정적으로 추적합니다.
* 해제된 포인터를 역참조하면 컴파일 타임 진단이 발생하여, 실행 전에 Use-After-Free 패턴을 잡아냅니다.

### 8. 샌드박스 게이트 보안
신뢰할 수 없는 스크립트가 승인되지 않은 OS API(`CreateProcess`, `socket`, `send`, `LoadLibrary`, `dlopen`)를 호출하지 못하도록 AST 수준에서 제한합니다.

### 9. 레지스터 할당
루프 유도 변수는 callee-saved 기계 레지스터(`RBX`, `R12`..`R15`)에 직접 할당되어 불필요한 스택 트래픽을 방지합니다.

### 10. 재현 가능한 Self-Hosting Fixpoint
컴파일러는 자기 자신을 깔끔하게 컴파일하며, Stage 2와 Stage 3 바이너리는 **100% 비트 단위로 동일한 SHA-256 해시**를 생성합니다:
`EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## 대상 분야

Yukinary는 비관리형이며 의존성이 없는 64비트 기계어로 직접 컴파일되므로, 다음 분야를 위해 설계되었습니다:

1. **CLI 도구 및 컴파일러:** 빠른 빌드 반복과 1MB 미만의 실행 파일 (`src/` 자체가 Yukinary로 self-hosting됨).
2. **네트워크 서비스:** 저수준 비동기 프리미티브 (`std/ag/http.ykn`, Windows IOCP, Linux `io_uring`/`epoll`).
3. **스토리지 엔진:** B-Tree 인덱싱 (`std/veritabani/b_tree.ykn`), Write-Ahead Logging (`wal.ykn`), 벡터 인덱싱 (`hnsw.ykn`).
4. **임베디드 고정소수점 추론:** Python이나 PyTorch 없이 Q16.16 정수 추론 및 GGUF v3 파싱 (`std/ai/`).
5. **임베디드 시스템 및 베어메탈 타깃:** C 런타임 의존성 없는 직접 포인터 접근.

---

## 운영체제 개발: YknOS 아키텍처

Yukinary는 `libc`에 의존하지 않는 freestanding x86_64 코드를 생성하므로, 베어메탈 환경과 OS 사용자 공간을 직접 타깃으로 할 수 있습니다:

* [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real)에는 Multiboot2 엔트리 (`boot.asm`), GDT (`gdt.ykn`), IDT (`idt.ykn`), PML4 페이징 (`paging.ykn`), COM1 시리얼 UART, VGA 텍스트 출력을 구현한 최소한의 개념 증명용 Ring-0 x86_64 커널이 있습니다.
* [`examples/yknos/`](examples/yknos)에서는 Yukinary가 QEMU에서 부팅 가능한 실제 동작하는 **Linux 6.6 LTS + Alpine musl + YKN ELF64 사용자 공간** 시스템 (`ykn-init` PID 1 슈퍼바이저, `yknsh` 셸, `yuki` CLI)을 구동합니다.

---

## 프로덕션 준비도, 솔직한 한계 및 보장

### A. 검증된 엔지니어링 특성
* **결정론적이고 비트 단위로 동일한 Fixpoint:** Stage 2와 Stage 3 self-hosted 바이너리가 비트 단위로 일치합니다 (`EF607B9A...`).
* **외부 의존성 없음:** 실행 파일은 외부 DLL이나 공유 라이브러리 없이 기본 Windows (Kernel32)와 Linux (원시 syscall)에서 실행됩니다.
* **측정된 성능의 투명성:** GCC, Rustc, Go, CPython과 비교한 전체 소스 벤치마크가 [`ykncode/benchmarks`](https://github.com/ykncode/benchmarks)에 공개되어 있습니다.

### B. 솔직한 한계 (아직 YKN을 사용하지 말아야 할 곳)
* **실험적 상태:** Yukinary는 1인 개발자가 만든 v1.0.0 시스템 연구용 컴파일러입니다. GCC, Clang, Rust처럼 수십 년에 걸친 엣지 케이스 강화 과정을 거치지 않았습니다. 안전이 중요한 시스템이나 금융 프로덕션 시스템에는 배포하지 마십시오.
* **밀집 자동 벡터화 연산:** Yukinary는 LLVM SIMD AVX2 자동 벡터화가 없는 자체 스칼라 x86_64 백엔드를 사용하므로, 무거운 부동소수점 또는 행렬 루프에서는 C (`-O3`)와 Rust (`opt-level=3`)가 여전히 더 빠릅니다 (~2.5x).
* **네이티브 모바일 UI:** 현재 iOS UIKit/SwiftUI 또는 Android Jetpack Compose용 네이티브 UI 바인딩은 없습니다.

### C. 보증
MIT 및 Apache 2.0 라이선스로 배포되며, **"있는 그대로(AS IS)", 어떠한 종류의 보증도 없이** 제공됩니다.

---

## 4개 저장소 생태계

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

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary): self-hosting Yukinary 컴파일러, 표준 라이브러리, 문서, YknOS Linux 사용자 공간 예제.
2. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks): 산업용 성능 비교, 컴파일 시간 텔레메트리, 비교 대상 워크로드 (C, Rust, Go, Python).
3. [**`ykncode/packages`**](https://github.com/ykncode/packages): 모듈형 라이브러리를 위한 패키지 레지스트리 (`net/http`, `math`, `crypto/sha256`, `data/json`, `terminal/colors`).
4. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap): 새 머신에서 Yukinary를 처음부터 부트스트랩하기 위한 독립 실행형 C++17 시드 컴파일러.

---

## 이중 라이선스

Yukinary는 **MIT License** 또는 **Apache License (Version 2.0)** 중 하나를 선택할 수 있는 이중 라이선스로 제공됩니다:

* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)
