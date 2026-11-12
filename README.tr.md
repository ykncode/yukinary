<div align="center">

<p align="center">
  <a href="README.md">English</a> |
  <b>Türkçe</b> |
  <a href="README.ru.md">Русский</a> |
  <a href="README.ja.md">日本語</a> |
  <a href="README.zh-CN.md">简体中文</a> |
  <a href="README.ko.md">한국어</a> |
  <a href="README.fr.md">Français</a>
</p>

<img src="assets/logo.png" alt="Yukinary Programlama Dili Logosu" width="180" />

# Yukinary (YKN) Programlama Dili

**Self-Hosting x86_64 Derleyici • Doğrudan PE32+ ve ELF64 Kod Üretimi (LLVM Yok / C-Runtime Yok) • Çok Dilli Sözdizimi • Deneysel Kendi Kendini Onarma ve Gömülü Yapay Zekâ**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Benchmarks: ykncode/benchmarks](https://img.shields.io/badge/Benchmarks-ykncode%2Fbenchmarks-38bdf8)](https://github.com/ykncode/benchmarks)

<p align="center">
  <a href="#benchmark-sonuçları-ve-performans-c-rust-go-python-ile-karşılaştırma">Benchmark'lar</a> •
  <a href="#hızlı-başlangıç-ve-sürümler">İndirme ve Hızlı Başlangıç</a> •
  <a href="#dil-dokümantasyonu">Dokümantasyon</a> •
  <a href="#micro-llm-ve-tam-llm-mimarisi">Yapay Zekâ Katmanı (Deneysel)</a> •
  <a href="#temel-özellikler">Mimari</a> •
  <a href="#os-geliştirme-yknos-mimarisi">YknOS</a> •
  <a href="#üretime-hazırlık-dürüst-sınırlar-ve-garantiler">Dürüst Sınırlar</a> •
  <a href="#dört-depolu-ekosistem">Ekosistem</a>
</p>

---

</div>

## Genel Bakış

**Yukinary (YKN)**, bağımsız, self-hosting bir sistem programlama dili ve native derleyicidir. Yukinary, LLVM IR'ı hedeflemek veya C'ye transpile etmek yerine AST'sini doğrudan bağımsız **x86_64 makine koduna** indirger; PE32+ (Windows) ve ELF64 (Linux) ikili başlıklarını, syscall'ları ve opcode'ları **sıfır harici runtime bağımlılığı** (`libc`, `msvcrt` veya harici linker'lar) ile sıfırdan yazar.

* **Bit Düzeyinde Özdeş Self-Hosting:** (Yukinary ile yazılmış) derleyici kendini deterministik olarak derler (`Stage 2 SHA-256 == Stage 3 SHA-256`).
* **Kanonik + Eğitim Amaçlı Çok Dilli Sözdizimi:** Anahtar kelimeler İngilizce, Türkçe, Rusça, Japonca, Basitleştirilmiş Çince, Korece veya Fransızca yazılabilir. Yerelleştirilmiş programlama eğitimi için tasarlanmıştır; `ykn fmt` şu anda yerleşim biçimlendirmesini üstlenir, kanonik anahtar kelime/yorum çevirisi ise gelecekteki bir araç olarak planlanmaktadır.
* **Freestanding Sistemler ve OS Hedefi:** Deneysel, Linux tabanlı bir işletim sistemi userspace'ine güç verir ([`examples/yknos`](examples/yknos)).

---

## Benchmark Sonuçları ve Performans (C, Rust, Go, Python ile Karşılaştırma)

Tüm benchmark düzenekleri, C/Rust/Python rakip implementasyonları ve ham JSON telemetri verileri özel benchmark depomuzda yayımlanmaktadır:  
[**Resmî Benchmark Paketi: `github.com/ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) • [**Endüstriyel Benchmark Raporu (TR)**](https://github.com/ykncode/benchmarks/blob/main/industrial/YUKINARY_ENDUSTRIYEL_BENCHMARK_RAPORU.md)

<div align="center">
  <img src="assets/bench_compile_speed.png" alt="Derleyici Verimi: Yukinary, GCC, Rustc, Go ve G++ karşılaştırması" width="92%" />
</div>

<div align="center">
  <img src="assets/bench_workloads.png" alt="Çalışma Zamanı Performansı ve Tepe RSS: Sistem İş Yükü ve BitNet Çekirdeği" width="92%" />
</div>

### Ölçülen Sonuçların Özeti (`ykncode/benchmarks`)

| Benchmark | Yukinary (`ykn`) | C (`gcc -O3`) | Rust (`rustc -O3`) | Python (`3.13`) | Notlar |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Derleyici Build Süresi** (`optimizer`) | **18.6 ms** | 219.4 ms | 272.2 ms | — | Sıfır LLVM/linker yükü (**11x–31x daha hızlı build**) |
| **Sistem B-Tree** (100k düğüm, 50k arama) | **213 ms** | 133 ms | 221 ms | 327 ms | Birebir doğrulanmış checksum (`19526` / `6363996222`) |
| **BitNet b1.58** (3.35 GOP üçlü çekirdek) | **0.71 GOP/s** | 1.79 GOP/s | 1.74 GOP/s | 0.02 GOP/s | Skaler native kod üretimi (GCC/Rust, LLVM/GCC AVX2 kullanır) |
| **Tepe Bellek (BitNet RSS)** | **2.6 MB** | 4.8 MB | 5.2 MB | 22.4 MB | **En düşük bellek ayak izi** (freestanding, sıfır libc) |
| **Soğuk Başlatma Gecikmesi** | **5.4 ms** | 5.9 ms | 6.8 ms | 35.0 ms | Doğrudan OS giriş noktası (`ExitProcess` / `sys_exit`) |

> **Mühendislik Notu:** C (`gcc -O3`) ve Rust (`rustc -O3`), GCC/LLVM otomatik vektörleştirmesi (SIMD AVX2) sayesinde yoğun matris aritmetiğinde ~2.5x daha hızlı kalmaktadır. Yukinary, linear-scan register allocation kullanan özel bir skaler x86_64 encoder'a dayanır ve anında derlemeyi, küçük ve libc'siz bellek ayak izini ve sıfır harici toolchain bağımlılığını önceliklendirir. Tüm kaynak iş yüklerini [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) deposunda inceleyebilirsiniz.

---

## Hızlı Başlangıç ve Sürümler

Yukinary'yi çalıştırmak için bir C++ derleyicisine veya harici bir toolchain'e **ihtiyacınız yoktur**. Bağımsız native ikili dosyalar Releases sayfasında mevcuttur:

### 1. İndirme ve Kurulum
[**Yukinary'yi GitHub Releases'tan İndirin**](https://github.com/ykncode/yukinary/releases)

#### Windows (Tek Tıkla Kurulum)
1. Releases sayfasından **`yukinary-setup-windows-x86_64.zip`** dosyasını indirip arşivden çıkarın.
2. **`setup.cmd`** dosyasına çift tıklayın (veya `powershell -ExecutionPolicy Bypass -File setup.ps1` komutunu çalıştırın).

#### Linux ve macOS (Tek Satırda Kurulum)
1. **`yukinary-setup-linux-x86_64.tar.gz`** dosyasını indirip arşivden çıkarın (`tar -xzf yukinary-setup-linux-x86_64.tar.gz`).
2. **`./setup.sh`** komutunu çalıştırın.

#### Visual Studio Code Eklentisi (`.vsix`)
```bash
code --install-extension yukinary-lang-1.0.0.vsix
```

### 2. İlk Programınızı Yazın ve Derleyin (`hello.ykn`)
```ykn
# English syntax (canonical keywords; native stdio intrinsic: ekrana_yaz)
fn main() -> i64 {
    ekrana_yaz("Hello, Yukinary!\n".ptr);
    return 0;
}
```

Doğrudan native Windows PE32+ veya Linux ELF64 makine koduna derleyin:
```powershell
# Windows PE32+
ykn hello.ykn hello.exe
.\hello.exe

# Windows'tan cross-compile veya native Linux üzerinde Linux ELF64
ykn --target linux hello.ykn hello
```

---

## Dil Dokümantasyonu

Geliştirici kılavuzları ve sözdizimi referansları desteklenen konuşma dillerinde mevcuttur:

| Dil | Dosya | Açıklama |
| :--- | :--- | :--- |
| **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Developer Guide, Syntax Reference, `#convert` Engine, Architecture, and Tooling |
| **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能 |
| **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译与嵌入式 AI |
| **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진 및 메모리 안전성 |

> [!TIP]
> **Yerelleştirme Tasarım Notu:** Çok dilli anahtar kelimeler, öğrenciler ve İngilizce bilmeyenler için giriş engelini düşürmek amacıyla dinamik lexer token eşlemeleri (`docs/Multi-lingual-add-language.md`) kullanır. Ortak geliştirilen açık kaynak paketler için kanonik standart olarak İngilizce önerilir.

---

## Micro-LLM ve Tam LLM Mimarisi

Yukinary, deneysel nöral araçları derleme hattına **iki ayrı katman** aracılığıyla entegre eder:

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

### Mimari Karşılaştırma ve Operasyonel Roller

| Boyut | Gömülü Micro-LLM (16 KB) | Harici Tam LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **Konum ve Depolama** | Doğrudan derleyici ikili dosyasına gömülüdür (`.rodata` bölümü) | Kullanıcı depolamasından dinamik olarak yüklenen harici `.gguf` dosyası |
| **Boyut ve Ağırlıklar** | **16,448 Bayt (~16 KB)** üçlü `{-1, 0, +1}` BitNet ağırlıkları | **1 GB – 40 GB** (BitNet b1.58, Llama-3, Mistral, Qwen vb.) |
| **Donanım Gereksinimleri** | **Sıfır GPU, Sıfır Python, Sıfır İnternet.** Q16.16 fixed-point aritmetiği kullanarak herhangi bir 64-bit CPU üzerinde çalışır | Quantization ve bağlam boyutuna bağlı olarak 4 GB – 32 GB RAM / VRAM gerektirir |
| **Çıkarım Gecikmesi** | **Milisaniyenin altında (< 1 ms)** — minimum derleme zamanı yükü | Token başına 10 – 50 milisaniye |
| **Birincil Kapsam** | **Derleyici Refleksleri ve Araçlar:**<br>• Sözdizimi ve yazım hatası onarımı (`--self-heal`: noktalı virgüller, parantezler)<br>• Çökmelere karşı savunmacı `null-guard` enjeksiyonu<br>• Gerçek zamanlı LSP struct alanı ve token otomatik tamamlama<br>• Hafif, çevrimdışı anahtar kelime/imza yedeği | **Deneysel Üretken Araçlar:**<br>• **Polyglot `#convert`:** C, Python ve Rust kod parçacıklarının YKN'ye çevrilmesine yardımcı olma<br>• Doğal dil prompt direktifleri (`#prompt "..."`) |
| **Etkinleştirme Modu** | **Her Zaman Açık** (harici model gerekmez) | `--llm-model <path.gguf>` ile **İsteğe Bağlı** |

### Tam LLM Modelleri Nasıl İndirilir ve Kullanılır (BitNet ve Bonsai)

Yukinary, **hiçbir model indirmeden** kodu kutudan çıktığı gibi derler. Ağır `#prompt` veya `#convert` iş akışlarını test ederken derleyiciyi [Hugging Face](https://huggingface.co) üzerinde barındırılan standart GGUF modellerine yönlendirebilirsiniz.

#### 1. Tek Komutla İndirme
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

#### 2. Manuel İndirme (Doğrudan Hugging Face Bağlantıları)

| Model | Boyut | Format | Doğrudan İndirme Bağlantısı | Hedef Yerel Yol |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Hugging Face'ten İndir](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Hugging Face'ten İndir](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. Modeller Derleyiciyle Nasıl Kullanılır

##### Yöntem A: Kaynak İçi `#config` Direktifi
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

##### Yöntem B: Komut Satırı Bayrakları
```powershell
# #prompt üretken direktifleriyle derleme
ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Bir Python dosyasını Yukinary'ye çevirme
ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Deneysel kendi kendini onaran sözdizimi düzeltmesi (gömülü 16 KB motoru kullanır)
ykn main.ykn --self-heal
```

---

## Temel Özellikler

### 1. Doğrudan Native Kod Üretimi (Sıfır C-Runtime)
* LLVM, C transpilation veya harici linker'lar olmadan doğrudan AST'den bağımsız çalıştırılabilir dosyalar üretir.
* Windows PE32+ ikili dosyaları Kernel32 / NT çağrılarını doğrudan yapar (`VirtualAlloc`, `WriteFile`, `ExitProcess`).
* Linux ELF64 ikili dosyaları ham 64-bit syscall'ları çağırır (`sys_mmap`, `sys_write`, `sys_exit`).

### 2. Çok Dilli Native Gramer (TR, EN, RU, JA, ZH, KO, FR)
Anahtar kelimeler lexer aşamasında dinamik olarak eşlenir:
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

### 3. Yabancı Fonksiyon Arayüzü (`harici isv`) ve `#convert`
* **Doğrudan FFI (`harici isv`):** Derlenmiş C/C++/Rust paylaşımlı kütüphanelerini (`.dll` / `.so`) işletim sisteminin dinamik yükleyicileri (`LoadLibraryA` / `dlopen`) aracılığıyla çağırın.
* **Convert Direktifleri (`#C-convert`, `#PY-convert`, `#JS-convert`):** `.ykn_cache/convert/` içinde SHA-256 ile önbelleğe alınan deneysel transpilation blokları.

### 4. Etiketli Enum'lar (Cebirsel Veri Tipleri) ve Desen Eşleme
16 bayt hizalı `{ discriminant, payload }` temsilleri kullanarak payload'lı ve payload'sız varyantları destekler:
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

### 5. Hiyerarşik `#config` Orkestratörü
OS ikili başlıklarını, stack tahsislerini ve hedef bayraklarını doğrudan kaynak koddan ayarlayın:
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target microarchitecture
}
```

### 6. Otonom Kendi Kendini Onarma Motoru (`--onar` / `--self-heal`)
* **Otomatik Anlık Görüntüler:** Onarımları uygulamadan önce bir `.bak` anlık görüntüsü oluşturur.
* **Yazım ve Noktalama Hatası Onarımı:** Yanlış yazılmış anahtar kelimeleri ve eksik noktalı virgülleri Levenshtein eşleştirmesiyle düzeltir.
* **Null-Guard Enjeksiyonu:** Korumasız dereference işlemlerinin etrafına koruyucu kontroller (`if ptr != null`) ekler.
* **Derleme Doğrulama Kapısı:** Yamalar, diskteki kaynağa dokunmadan önce derlemeden geçmek zorundadır.

### 7. Derleme Zamanı Bellek Güvenliği ve UAF Tespiti
* Pointer yaşam sürelerini ve bellek serbest bırakmalarını statik olarak izler.
* Serbest bırakılmış bir pointer'ın dereference edilmesi, Use-After-Free kalıplarını çalıştırmadan önce yakalamak için derleme zamanı tanılaması tetikler.

### 8. Sandbox Kapısı Güvenliği
Güvenilmeyen betiklerin yetkisiz OS API'lerini (`CreateProcess`, `socket`, `send`, `LoadLibrary`, `dlopen`) çağırmasını AST düzeyinde engeller.

### 9. Register Allocation
Döngü indüksiyon değişkenleri doğrudan callee-saved makine register'larına (`RBX`, `R12`..`R15`) atanarak gereksiz stack trafiği önlenir.

### 10. Yeniden Üretilebilir Self-Hosting Fixpoint
Derleyici kendini sorunsuz şekilde derler; Stage 2 ve Stage 3 ikili dosyaları **%100 bit düzeyinde özdeş SHA-256 hash'leri** üretir:
`EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## Hedef Alanlar

Yukinary doğrudan yönetilmeyen (unmanaged), sıfır bağımlılıklı 64-bit makine koduna derlendiği için şu alanlar için tasarlanmıştır:

1. **CLI Araçları ve Derleyiciler:** Hızlı build döngüleri ve megabaytın altında çalıştırılabilir dosyalar (`src/` dizininin kendisi Yukinary ile self-hosted'dır).
2. **Ağ Servisleri:** Düşük seviyeli async primitifleri (`std/ag/http.ykn`, Windows IOCP, Linux `io_uring`/`epoll`).
3. **Depolama Motorları:** B-Tree indeksleme (`std/veritabani/b_tree.ykn`), Write-Ahead Logging (`wal.ykn`) ve vektör indeksleme (`hnsw.ykn`).
4. **Gömülü Fixed-Point Çıkarım:** Python veya PyTorch olmadan Q16.16 tamsayı çıkarımı ve GGUF v3 ayrıştırma (`std/ai/`).
5. **Gömülü Sistemler ve Bare-Metal Hedefler:** C-runtime bağımlılığı olmadan doğrudan pointer erişimi.

---

## OS Geliştirme: YknOS Mimarisi

Yukinary, `libc`'ye dayanmadan freestanding x86_64 kodu ürettiği için bare-metal ortamları ve işletim sistemi userspace'lerini doğrudan hedefleyebilir:

* [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real) içinde, minimal bir proof-of-concept Ring-0 x86_64 çekirdeği Multiboot2 girişini (`boot.asm`), GDT'yi (`gdt.ykn`), IDT'yi (`idt.ykn`), PML4 sayfalamayı (`paging.ykn`), COM1 seri UART'ı ve VGA metin çıktısını uygular.
* [`examples/yknos/`](examples/yknos) içinde Yukinary, QEMU'da önyüklenebilen, çalışan bir **Linux 6.6 LTS + Alpine musl + YKN ELF64 Userspace** sistemine (`ykn-init` PID 1 supervisor, `yknsh` kabuğu, `yuki` CLI) güç verir.

---

## Üretime Hazırlık, Dürüst Sınırlar ve Garantiler

### A. Doğrulanmış Mühendislik Özellikleri
* **Deterministik, Bit Düzeyinde Özdeş Fixpoint:** Stage 2 ve Stage 3 self-hosted ikili dosyaları bit bit eşleşir (`EF607B9A...`).
* **Sıfır Harici Bağımlılık:** Çalıştırılabilir dosyalar standart Windows (Kernel32) ve Linux (ham syscall'lar) üzerinde hiçbir harici DLL veya paylaşımlı kütüphane olmadan çalışır.
* **Ölçülen Performansta Şeffaflık:** GCC, Rustc, Go ve CPython'a karşı tam kaynak kodlu benchmark'lar [`ykncode/benchmarks`](https://github.com/ykncode/benchmarks) deposunda açıkça yayımlanmaktadır.

### B. Dürüst Sınırlar (YKN'nin Henüz Kullanılmaması Gereken Yerler)
* **Deneysel Durum:** Yukinary, tek bir geliştirici tarafından geliştirilmiş v1.0.0 sürümünde bir sistem araştırma derleyicisidir. GCC, Clang veya Rust'ın sahip olduğu onlarca yıllık uç durum sağlamlaştırmasını henüz biriktirmemiştir. Güvenlik açısından kritik veya finansal üretim sistemlerinde kullanmayın.
* **Yoğun, Otomatik Vektörleştirilmiş Matematik:** Yukinary, LLVM SIMD AVX2 otomatik vektörleştirmesi olmayan özel bir skaler x86_64 backend kullandığından, ağır kayan noktalı veya matris döngülerinde C (`-O3`) ve Rust (`opt-level=3`) daha hızlı (~2.5x) kalmaktadır.
* **Native Mobil Arayüz:** Şu anda iOS UIKit/SwiftUI veya Android Jetpack Compose için native UI bağlamaları bulunmamaktadır.

### C. Garanti
MIT ve Apache 2.0 lisansları altında dağıtılır ve **"OLDUĞU GİBİ", HİÇBİR TÜRDE GARANTİ OLMAKSIZIN** sunulur.

---

## Dört Depolu Ekosistem

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

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary): Self-hosting Yukinary derleyicisi, standart kütüphane, dokümantasyon ve YknOS Linux userspace örneği.
2. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks): Endüstriyel performans karşılaştırmaları, derleme zamanı telemetrisi ve rakip iş yükleri (C, Rust, Go, Python).
3. [**`ykncode/packages`**](https://github.com/ykncode/packages): Modüler kütüphaneler için paket kayıt deposu (`net/http`, `math`, `crypto/sha256`, `data/json`, `terminal/colors`).
4. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap): Yeni makinelerde Yukinary'yi sıfırdan bootstrap etmek için bağımsız C++17 seed derleyicisi.

---

## Çift Lisans

Yukinary, tercihinize bağlı olarak **MIT Lisansı** veya **Apache Lisansı (Sürüm 2.0)** altında çift lisanslıdır:

* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)