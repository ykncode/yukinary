# 🌍 How to Add a New Spoken Language to Yukinary
## (Multi-Lingual Community Contribution Guide)

> **"Your mother tongue is not in the list? Don't worry! In Yukinary, human language is just a syntax token. You can add your native language in under 15 minutes."**

Yukinary is built on the philosophy of **Universal Multi-Lingual Computing**. It does not privilege English over any other human tongue. All keywords are first-class tokens inside the Lexer (`src/lexer.ykn`).

Follow this step-by-step guide to add your language (e.g., Spanish, German, Arabic, French, Italian, Hindi, etc.) and submit a Pull Request to become an official Yukinary contributor!

---

## 🧭 How It Works Under the Hood

In conventional compilers, `fn` or `def` are hardcoded English strings. In Yukinary:

```
"isv"      (Turkish)    ──┐
"fn"       (English)    ──┤
"функция"  (Russian)    ──┼──► Token: TIP_ISLEV (Function Definition)
"関数"     (Japanese)   ──┤
"función"  (Spanish)    ──┘
```

The Parser and Code Generator **do not care** what spoken language you wrote in. As long as the Lexer maps your native word to the canonical Token ID, the program compiles directly to identical, high-performance native machine code!

---

## 📋 The Canonical Token Translation Table

When adding a new language, translate each of the core keywords below into your language:

| Token Name | English Default | Turkish Default | What It Does | Your Language (Example: Spanish) |
| :--- | :--- | :--- | :--- | :--- |
| **`TIP_ISLEV`** | `fn` / `function` | `isv` / `islev` | Declares a function | `funcion` / `func` |
| **`TIP_DEGISKEN`** | `let` / `var` / `mut` | `degisken` | Declares a mutable variable | `variable` / `var` |
| **`TIP_SABIT`** | `const` | `sabit` | Declares an immutable constant | `constante` / `const` |
| **`TIP_EGER`** | `if` | `eger` | Conditional branching | `si` |
| **`TIP_ISE`** | `then` | `ise` | Then block gate | `entonces` |
| **`TIP_YOKSA`** | `else` | `yoksa` | Else branch | `sino` |
| **`TIP_DONGU`** | `while` / `loop` | `dongu` | Loop construct | `mientras` / `bucle` |
| **`TIP_DURDUR`** | `break` | `durdur` | Break out of a loop | `romper` |
| **`TIP_DEVAM`** | `continue` | `devam_et` | Next loop iteration | `continuar` |
| **`TIP_DONDUR`** | `return` | `dondu` | Returns a value from function | `retornar` / `devolver` |
| **`TIP_YAPI`** | `struct` | `yapi` | Defines a custom data structure | `estructura` |
| **`TIP_ENUM`** | `enum` | `enum` | Defines a tagged enumeration | `enumeracion` |
| **`TIP_ESLES`** | `match` | `esles` | Pattern matching expression | `coincidir` |
| **`TIP_ICE_AKTAR`** | `import` | `ice_aktar` | Imports a module or file | `importar` |
| **`TIP_HARICI`** | `extern` | `harici` | Foreign function interface (C FFI) | `externo` |
| **`TIP_TAMSAYI`** | `i64` / `int` | `tamsayi` | 64-bit signed integer type | `entero` |
| **`TIP_GERCEK`** | `f64` / `float` | `gercek` | 64-bit floating point type | `flotante` / `real` |
| **`TIP_MANTIK`** | `bool` | `mantik` | Boolean type (`true`/`false`) | `booleano` |
| **`TIP_METIN`** | `string` | `metin` | String reference pointer | `cadena` / `texto` |
| **`TIP_BOS`** | `void` | `bos` | Void / Empty unit return type | `vacio` |
| **`TIP_DOGRU`** | `true` | `dogru` | Boolean true literal | `verdadero` |
| **`TIP_YANLIS`** | `false` | `yanlis` | Boolean false literal | `falso` |

---

## 🛠️ Step-by-Step Implementation

### Step 1: Register in `tools/keyword_registry.json`
Open [`tools/keyword_registry.json`](file:///d:/yukinary/yukinary-language/tools/keyword_registry.json) and add your language code (e.g., `"es"`, `"de"`, `"ar"`):

```json
{
  "languages": ["tr", "en", "ru", "ja", "zh", "ko", "es"],
  "aliases": [
    {"name": "funcion", "token": "TIP_ISLEV", "language": "es"},
    {"name": "retornar", "token": "TIP_DONDUR", "language": "es"},
    {"name": "si", "token": "TIP_EGER", "language": "es"},
    {"name": "sino", "token": "TIP_YOKSA", "language": "es"}
  ]
}
```

### Step 2: Add Keyword Rules to Lexer (`src/lexer.ykn`)
In [`src/lexer.ykn`](file:///d:/yukinary/yukinary-language/src/lexer.ykn), find the keyword matching section (`kelime_tipi_bul`). Add string comparisons for your new keywords:

```ykn
# Example: Spanish 'funcion' (7 chars) -> TIP_ISLEV
eger uzunluk == 7 ve bellek_esit_mi(adres, "funcion".ptr, 7) == 1 ise {
    dondu TIP_ISLEV;
}

# Example: Spanish 'retornar' (8 chars) -> TIP_DONDUR
eger uzunluk == 8 ve bellek_esit_mi(adres, "retornar".ptr, 8) == 1 ise {
    dondu TIP_DONDUR;
}
```

*(Note for non-Latin alphabets like Cyrillic, Arabic, or Devanagari: Length is measured in UTF-8 bytes. Use the exact byte count of the UTF-8 encoded string).*

### Step 3: Write a Test File (`tests/languages/hello_yourlang.ykn`)
Write a simple hello world in your native language to verify syntax parsing:

```ykn
# Spanish Example (hello_es.ykn)
funcion sumar(a: entero, b: entero) -> entero {
    retornar a + b;
}

funcion principal() -> entero {
    ekrana_yaz("¡Hola, Mundo desde Yukinary!\n".ptr);
    devolver sumar(10, 20);
}
```

### Step 4: Recompile the Compiler
Compile Yukinary using itself:
```powershell
ykn src/main.ykn -o ykn.exe
```

Test your new language file:
```powershell
ykn tests/languages/hello_es.ykn -o test.exe
.\test.exe
```

### Step 5: Submit a Pull Request
1. Fork [ykncode/yukinary](https://github.com/ykncode/yukinary).
2. Create a branch: `git checkout -b feat/add-spanish-language`.
3. Commit your changes: `git commit -m "feat(i18n): add native Spanish keywords to lexer"`.
4. Open a Pull Request! Your name will be permanently immortalized in the official contributors list.

---

**Empowering developers to program in the language they dream in.** ❄️

