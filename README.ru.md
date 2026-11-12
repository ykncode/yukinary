<div align="center">

<p align="center">
  <a href="README.md">English</a> |
  <a href="README.tr.md">Türkçe</a> |
  <b>Русский</b> |
  <a href="README.ja.md">日本語</a> |
  <a href="README.zh-CN.md">简体中文</a> |
  <a href="README.ko.md">한국어</a> |
  <a href="README.fr.md">Français</a>
</p>

<img src="assets/logo.png" alt="Логотип языка программирования Yukinary" width="180" />

# Язык программирования Yukinary (YKN)

**Self-hosting компилятор для x86_64 • Прямая генерация PE32+ и ELF64 (без LLVM / без C-рантайма) • Многоязычный синтаксис • Экспериментальное самовосстановление и встроенный ИИ**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE-MIT)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE-APACHE)
[![Platform: Windows x86_64](https://img.shields.io/badge/Platform-Windows_PE32%2B-blue)](https://github.com/ykncode/yukinary/releases)
[![Platform: Linux x86_64](https://img.shields.io/badge/Platform-Linux_ELF64-orange)](https://github.com/ykncode/yukinary/releases)
[![Self-Hosting: Bit-Identical Fixpoint](https://img.shields.io/badge/Self--Hosting-Bit--Identical_Fixpoint-brightgreen)](https://github.com/ykncode/yukinary)
[![Benchmarks: ykncode/benchmarks](https://img.shields.io/badge/Benchmarks-ykncode%2Fbenchmarks-38bdf8)](https://github.com/ykncode/benchmarks)

<p align="center">
  <a href="#бенчмарки-и-производительность-в-сравнении-с-c-rust-go-python">Бенчмарки</a> •
  <a href="#быстрый-старт-и-релизы">Загрузка и быстрый старт</a> •
  <a href="#документация-по-языку">Документация</a> •
  <a href="#архитектура-micro-llm-и-полноценной-llm">ИИ-слой (экспериментальный)</a> •
  <a href="#ключевые-возможности">Архитектура</a> •
  <a href="#разработка-операционных-систем-архитектура-yknos">YknOS</a> •
  <a href="#готовность-к-продакшену-честные-границы-и-гарантии">Честные границы</a> •
  <a href="#экосистема-из-четырёх-репозиториев">Экосистема</a>
</p>

---

</div>

## Обзор

**Yukinary (YKN)** — независимый self-hosting язык системного программирования и нативный компилятор. Вместо генерации LLVM IR или транспиляции в C Yukinary понижает AST напрямую в автономный **машинный код x86_64**, с нуля формируя бинарные заголовки PE32+ (Windows) и ELF64 (Linux), системные вызовы и опкоды — **без каких-либо внешних зависимостей времени выполнения** (`libc`, `msvcrt` или внешних компоновщиков).

* **Побитово идентичный self-hosting:** Компилятор (написанный на Yukinary) детерминированно компилирует сам себя (`Stage 2 SHA-256 == Stage 3 SHA-256`).
* **Канонический и учебный многоязычный синтаксис:** Ключевые слова можно писать на английском, турецком, русском, японском, упрощённом китайском, корейском или французском — это сделано для обучения программированию на родном языке; `ykn fmt` сейчас отвечает за форматирование разметки, а перевод ключевых слов и комментариев в канонический вид запланирован в будущих инструментах.
* **Freestanding-системы и ОС как цель:** На Yukinary работает экспериментальное пользовательское пространство ОС на базе Linux ([`examples/yknos`](examples/yknos)).

---

## Бенчмарки и производительность (в сравнении с C, Rust, Go, Python)

Все обвязки бенчмарков, конкурирующие реализации на C/Rust/Python и сырая JSON-телеметрия опубликованы в нашем отдельном репозитории бенчмарков:  
[**Официальный набор бенчмарков: `github.com/ykncode/benchmarks`**](https://github.com/ykncode/benchmarks) • [**Отчёт о промышленных бенчмарках (TR)**](https://github.com/ykncode/benchmarks/blob/main/industrial/YUKINARY_ENDUSTRIYEL_BENCHMARK_RAPORU.md)

<div align="center">
  <img src="assets/bench_compile_speed.png" alt="Пропускная способность компилятора: Yukinary против GCC, Rustc, Go, G++" width="92%" />
</div>

<div align="center">
  <img src="assets/bench_workloads.png" alt="Производительность во время выполнения и пиковый RSS: системная нагрузка и ядро BitNet" width="92%" />
</div>

### Сводка измеренных результатов (`ykncode/benchmarks`)

| Бенчмарк | Yukinary (`ykn`) | C (`gcc -O3`) | Rust (`rustc -O3`) | Python (`3.13`) | Примечания |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Время сборки компилятора** (`optimizer`) | **18.6 ms** | 219.4 ms | 272.2 ms | — | Нет накладных расходов LLVM/компоновщика (**сборка в 11–31 раз быстрее**) |
| **Системное B-дерево** (100k узлов, 50k поисков) | **213 ms** | 133 ms | 221 ms | 327 ms | Точно проверенная контрольная сумма (`19526` / `6363996222`) |
| **BitNet b1.58** (тернарное ядро, 3.35 GOP) | **0.71 GOP/s** | 1.79 GOP/s | 1.74 GOP/s | 0.02 GOP/s | Скалярная нативная кодогенерация (GCC/Rust используют AVX2 от LLVM/GCC) |
| **Пиковая память (RSS для BitNet)** | **2.6 MB** | 4.8 MB | 5.2 MB | 22.4 MB | **Наименьшее потребление** (freestanding, без libc) |
| **Задержка холодного старта** | **5.4 ms** | 5.9 ms | 6.8 ms | 35.0 ms | Прямая точка входа ОС (`ExitProcess` / `sys_exit`) |

> **Инженерное примечание:** C (`gcc -O3`) и Rust (`rustc -O3`) остаются примерно в 2.5 раза быстрее на плотной матричной арифметике благодаря автовекторизации GCC/LLVM (SIMD AVX2). Yukinary использует собственный скалярный кодировщик x86_64 с распределением регистров методом linear-scan, отдавая приоритет мгновенной компиляции, малому размеру без libc и отсутствию зависимостей от внешних тулчейнов. Все исходные коды нагрузок доступны в [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks).

---

## Быстрый старт и релизы

Для запуска Yukinary **не нужен** компилятор C++ или внешний тулчейн. Автономные нативные бинарники доступны в разделе Releases:

### 1. Загрузка и установка
[**Скачать Yukinary из GitHub Releases**](https://github.com/ykncode/yukinary/releases)

#### Windows (установка в 1 клик)
1. Скачайте **`yukinary-setup-windows-x86_64.zip`** из Releases и распакуйте архив.
2. Дважды щёлкните **`setup.cmd`** (или выполните `powershell -ExecutionPolicy Bypass -File setup.ps1`).

#### Linux и macOS (установка одной командой)
1. Скачайте **`yukinary-setup-linux-x86_64.tar.gz`** и распакуйте архив (`tar -xzf yukinary-setup-linux-x86_64.tar.gz`).
2. Запустите **`./setup.sh`**.

#### Расширение для Visual Studio Code (`.vsix`)
```bash
code --install-extension yukinary-lang-1.0.0.vsix
```

### 2. Напишите и скомпилируйте свою первую программу (`hello.ykn`)
```ykn
# English syntax (canonical keywords; native stdio intrinsic: ekrana_yaz)
fn main() -> i64 {
    ekrana_yaz("Hello, Yukinary!\n".ptr);
    return 0;
}
```

Компиляция напрямую в нативный машинный код Windows PE32+ или Linux ELF64:
```powershell
# Windows PE32+
ykn hello.ykn hello.exe
.\hello.exe

# Linux ELF64: кросс-компиляция из Windows или нативно в Linux
ykn --target linux hello.ykn hello
```

---

## Документация по языку

Руководства для разработчиков и справочники по синтаксису доступны на поддерживаемых естественных языках:

| Язык | Файл | Описание |
| :--- | :--- | :--- |
| **English** | [**`docs/guide_en.md`**](docs/guide_en.md) | Руководство разработчика, справочник по синтаксису, движок `#convert`, архитектура и инструменты |
| **Türkçe** | [**`docs/kullanim_rehberi_tr.md`**](docs/kullanim_rehberi_tr.md) | Türkçe Kullanım Kılavuzu, sözdizimi, `#convert`, Micro-LLM ve FFI motoru |
| **Русский** | [**`docs/guide_ru.md`**](docs/guide_ru.md) | Руководство разработчика, многоязычный синтаксис, `#convert` и архитектура |
| **日本語** | [**`docs/guide_ja.md`**](docs/guide_ja.md) | 開発者ガイド、日本語ネイティブ構文、`#convert`変換、自己修復機能 |
| **简体中文** | [**`docs/guide_zh.md`**](docs/guide_zh.md) | 开发者完全指南、原生中文语法、`#convert`多语言转译与嵌入式 AI |
| **한국어** | [**`docs/guide_ko.md`**](docs/guide_ko.md) | 개발자 가이드, 한국어 네이티브 문법, `#convert` 변환 엔진 및 메모리 안전성 |

> [!TIP]
> **Примечание о локализации:** Многоязычные ключевые слова реализованы через динамические сопоставления токенов лексера (`docs/Multi-lingual-add-language.md`), чтобы снизить порог входа для студентов и тех, кто не говорит по-английски. Для совместных open-source пакетов в качестве канонического стандарта рекомендуется английский.

---

## Архитектура Micro-LLM и полноценной LLM

Yukinary встраивает экспериментальные нейросетевые инструменты в конвейер компиляции через **два отдельных слоя**:

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

### Архитектурное сравнение и рабочие роли

| Параметр | Встроенная Micro-LLM (16 KB) | Внешняя полноценная LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **Расположение и хранение** | Встроена прямо в бинарник компилятора (секция `.rodata`) | Внешний файл `.gguf`, динамически загружаемый из хранилища пользователя |
| **Размер и веса** | **16,448 байт (~16 KB)** тернарных весов BitNet `{-1, 0, +1}` | **1 GB – 40 GB** (BitNet b1.58, Llama-3, Mistral, Qwen и др.) |
| **Требования к оборудованию** | **Без GPU, без Python, без интернета.** Работает на любом 64-битном CPU с использованием арифметики с фиксированной точкой Q16.16 | Требует 4 GB – 32 GB RAM / VRAM в зависимости от квантизации и размера контекста |
| **Задержка инференса** | **Менее миллисекунды (< 1 ms)** — минимальные накладные расходы при компиляции | 10 – 50 миллисекунд на токен |
| **Основная область применения** | **Рефлексы компилятора и инструменты:**<br>• Исправление синтаксиса и опечаток (`--self-heal`: точки с запятой, скобки)<br>• Защитная вставка `null-guard` против падений<br>• Автодополнение полей структур и токенов в LSP в реальном времени<br>• Лёгкий офлайн-фолбэк для ключевых слов и сигнатур | **Экспериментальные генеративные инструменты:**<br>• **Полиглотный `#convert`:** помощь в переводе фрагментов на C, Python и Rust в YKN<br>• Директивы на естественном языке (`#prompt "..."`) |
| **Режим активации** | **Всегда включена** (внешняя модель не требуется) | **По запросу** через `--llm-model <path.gguf>` |

### Как скачать и использовать полноценные LLM-модели (BitNet и Bonsai)

Yukinary компилирует код «из коробки» **без загрузки каких-либо моделей**. Для тестирования тяжёлых сценариев `#prompt` или `#convert` можно указать компилятору стандартные GGUF-модели, размещённые на [Hugging Face](https://huggingface.co).

#### 1. Загрузка одной командой
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

#### 2. Ручная загрузка (прямые ссылки на Hugging Face)

| Модель | Размер | Формат | Прямая ссылка для загрузки | Локальный путь назначения |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Скачать с Hugging Face](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Скачать с Hugging Face](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### 3. Как использовать модели с компилятором

##### Способ A: директива `#config` в исходном коде
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

##### Способ B: флаги командной строки
```powershell
# Компиляция с генеративными директивами #prompt
ykn main.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Перевод файла Python в Yukinary
ykn convert math_helper.py -o math_helper.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf

# Экспериментальное самовосстановление синтаксиса (использует встроенный движок на 16 KB)
ykn main.ykn --self-heal
```

---

## Ключевые возможности

### 1. Прямая генерация нативного кода (без C-рантайма)
* Генерирует автономные исполняемые файлы напрямую из AST без LLVM, транспиляции в C или внешних компоновщиков.
* Бинарники Windows PE32+ напрямую вызывают функции Kernel32 / NT (`VirtualAlloc`, `WriteFile`, `ExitProcess`).
* Бинарники Linux ELF64 используют «сырые» 64-битные системные вызовы (`sys_mmap`, `sys_write`, `sys_exit`).

### 2. Многоязычная нативная грамматика (TR, EN, RU, JA, ZH, KO, FR)
Ключевые слова сопоставляются динамически на этапе лексического анализа:
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

### 3. Интерфейс внешних функций (`harici isv`) и `#convert`
* **Прямой FFI (`harici isv`):** Вызов скомпилированных разделяемых библиотек на C/C++/Rust (`.dll` / `.so`) через динамические загрузчики ОС (`LoadLibraryA` / `dlopen`).
* **Директивы преобразования (`#C-convert`, `#PY-convert`, `#JS-convert`):** Экспериментальные блоки транспиляции, кэшируемые по SHA-256 в `.ykn_cache/convert/`.

### 4. Тегированные перечисления (алгебраические типы данных) и сопоставление с образцом
Поддерживаются варианты с полезной нагрузкой и без неё, представленные в виде `{ discriminant, payload }` с выравниванием по 16 байтам:
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

### 5. Иерархический оркестратор `#config`
Настройка бинарных заголовков ОС, размеров стека и целевых флагов прямо из исходного кода:
```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack stamped into PE32+/ELF headers
    is_parcacigi: 8,          # Worker pool capacity
    bellek: "arena",          # Default memory allocator
    islemci: "native"         # Target microarchitecture
}
```

### 6. Автономный движок самовосстановления (`--onar` / `--self-heal`)
* **Автоматические снимки:** Перед применением исправлений создаётся снимок `.bak`.
* **Исправление опечаток и пунктуации:** Исправляет ключевые слова с ошибками и пропущенные точки с запятой с помощью сопоставления по расстоянию Левенштейна.
* **Вставка null-guard:** Добавляет защитные проверки (`if ptr != null`) вокруг незащищённых разыменований.
* **Проверка компиляцией:** Исправления должны успешно пройти компиляцию, прежде чем будут записаны в исходный файл на диске.

### 7. Безопасность памяти на этапе компиляции и обнаружение UAF
* Статически отслеживает время жизни указателей и освобождения памяти.
* Разыменование освобождённого указателя вызывает диагностику на этапе компиляции, что позволяет выявить паттерны Use-After-Free до выполнения.

### 8. Защита Sandbox Gate
Ограничивает недоверенные скрипты на уровне AST, не позволяя им вызывать неразрешённые API ОС (`CreateProcess`, `socket`, `send`, `LoadLibrary`, `dlopen`).

### 9. Распределение регистров
Индукционные переменные циклов назначаются непосредственно на callee-saved регистры процессора (`RBX`, `R12`..`R15`), что исключает лишние обращения к стеку.

### 10. Воспроизводимый self-hosting fixpoint
Компилятор чисто компилирует сам себя; бинарники Stage 2 и Stage 3 дают **на 100% побитово идентичные хеши SHA-256**:
`EF607B9A47F8586970C7F7C68404B96BF7E58B1C2BC6B25C51DA280522B4A225`

---

## Целевые области применения

Поскольку Yukinary компилирует напрямую в неуправляемый 64-битный машинный код без зависимостей, он предназначен для:

1. **CLI-инструментов и компиляторов:** Быстрые итерации сборки и исполняемые файлы размером менее мегабайта (сам `src/` написан на Yukinary в режиме self-hosting).
2. **Сетевых сервисов:** Низкоуровневые асинхронные примитивы (`std/ag/http.ykn`, Windows IOCP, Linux `io_uring`/`epoll`).
3. **Движков хранения данных:** Индексация на B-деревьях (`std/veritabani/b_tree.ykn`), журнал упреждающей записи (`wal.ykn`) и векторная индексация (`hnsw.ykn`).
4. **Встроенного инференса с фиксированной точкой:** Целочисленный инференс Q16.16 и разбор GGUF v3 (`std/ai/`) без Python и PyTorch.
5. **Встраиваемых систем и bare-metal платформ:** Прямой доступ к указателям без зависимостей от C-рантайма.

---

## Разработка операционных систем: архитектура YknOS

Поскольку Yukinary генерирует freestanding-код x86_64 без опоры на `libc`, он может напрямую ориентироваться на bare-metal окружения и пользовательские пространства ОС:

* В [`std/os/kernel_real/`](https://github.com/ykncode/yukinary/tree/main/std/os/kernel_real) минимальное proof-of-concept ядро Ring-0 для x86_64 реализует точку входа Multiboot2 (`boot.asm`), GDT (`gdt.ykn`), IDT (`idt.ykn`), страничную адресацию PML4 (`paging.ykn`), последовательный порт COM1 UART и текстовый вывод VGA.
* В [`examples/yknos/`](examples/yknos) Yukinary обеспечивает работу полноценной системы **Linux 6.6 LTS + Alpine musl + пользовательское пространство YKN ELF64** (супервизор `ykn-init` с PID 1, оболочка `yknsh`, CLI `yuki`), загружаемой в QEMU.

---

## Готовность к продакшену, честные границы и гарантии

### A. Проверенные инженерные свойства
* **Детерминированный побитово идентичный fixpoint:** Self-hosted бинарники Stage 2 и Stage 3 совпадают бит в бит (`EF607B9A...`).
* **Отсутствие внешних зависимостей:** Исполняемые файлы работают на стандартной Windows (Kernel32) и Linux («сырые» системные вызовы) без каких-либо внешних DLL или разделяемых библиотек.
* **Прозрачность измерений производительности:** Полные исходные коды бенчмарков в сравнении с GCC, Rustc, Go и CPython открыто опубликованы в [`ykncode/benchmarks`](https://github.com/ykncode/benchmarks).

### B. Честные границы (где пока НЕ стоит использовать YKN)
* **Экспериментальный статус:** Yukinary — исследовательский системный компилятор версии v1.0.0, созданный одним разработчиком. Он не прошёл десятилетий отладки пограничных случаев, как GCC, Clang или Rust. Не используйте его в критически важных для безопасности или финансовых production-системах.
* **Плотные автовекторизованные вычисления:** Поскольку Yukinary использует собственный скалярный бэкенд x86_64 без автовекторизации LLVM SIMD AVX2, C (`-O3`) и Rust (`opt-level=3`) остаются быстрее (~2.5 раза) в тяжёлых циклах с плавающей точкой или матричных вычислениях.
* **Нативный мобильный UI:** В настоящее время нет нативных UI-привязок для iOS UIKit/SwiftUI или Android Jetpack Compose.

### C. Гарантии
Распространяется под лицензиями MIT и Apache 2.0 и предоставляется **«КАК ЕСТЬ», БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ**.

---

## Экосистема из четырёх репозиториев

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

1. [**`ykncode/yukinary`**](https://github.com/ykncode/yukinary): Self-hosting компилятор Yukinary, стандартная библиотека, документация и пример пользовательского пространства Linux YknOS.
2. [**`ykncode/benchmarks`**](https://github.com/ykncode/benchmarks): Промышленные сравнения производительности, телеметрия времени компиляции и нагрузки конкурентов (C, Rust, Go, Python).
3. [**`ykncode/packages`**](https://github.com/ykncode/packages): Реестр пакетов модульных библиотек (`net/http`, `math`, `crypto/sha256`, `data/json`, `terminal/colors`).
4. [**`ykncode/bootstrap`**](https://github.com/ykncode/bootstrap): Автономный начальный компилятор на C++17 для раскрутки (bootstrapping) Yukinary с нуля на новых машинах.

---

## Двойная лицензия

Yukinary распространяется под двойной лицензией — на ваш выбор **MIT License** или **Apache License (Version 2.0)**:

* [**LICENSE-MIT**](LICENSE-MIT)
* [**LICENSE-APACHE**](LICENSE-APACHE)
* [**COPYRIGHT**](COPYRIGHT)