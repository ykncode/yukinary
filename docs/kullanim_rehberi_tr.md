# Yukinary (YKN) Kapsamlı Kullanım Kılavuzu

> **Sürüm:** 1.0.0 (Faz 1-6 Tam Olgunluk Seviyesi)  
> **Hedef Sistemler:** Windows x86-64 (PE32+) & Linux x86-64 (ELF64)  
> **Lisans:** MIT / Apache-2.0 Çift Lisanslı (Rust Tarzı)

---

## 📌 İçindekiler
1. [Giriş ve Felsefe](#1-giriş-ve-felsefe)
2. [Kurulum ve Hızlı Başlangıç](#2-kurulum-ve-hızlı-başlangıç)
3. [Çok Dilli Sözdizimi (Multi-Lingual Grammar)](#3-çok-dilli-sözdizimi)
4. [Temel Tipler, Değişkenler ve Operatörler](#4-temel-tipler-değişkenler-ve-operatörler)
5. [Fonksiyonlar ve Kontrol Akışı](#5-fonksiyonlar-ve-kontrol-akışı)
6. [Yapılar (Struct) ve Bellek İşaretçileri (Pointers)](#6-yapılar-ve-bellek-işaretçileri)
7. [Etiketli Enum'lar (Tagged Enums) ve Örüntü Eşleme (`esles`)](#7-etiketli-enumlar-ve-örüntü-eşleme)
8. [Hiyerarşik `#config` Orkestratörü](#8-hiyerarşik-config-orkestratörü)
9. [Kendi Kendini Onarma: Self-Healing Motoru](#9-kendi-kendini-onarma-self-healing)
10. [Hafıza Zabıtası: Safety & Use-After-Free Tespiti](#10-hafıza-zabıtası-safety-ve-uaf)
11. [X-Ray Güvenlik Kapısı: Sandbox Gate](#11-x-ray-güvenlik-kapısı-sandbox-gate)
12. [Dahili 16 KB Nöral Beyin: Always-On BitNet Micro-LLM](#12-dahili-16-kb-nöral-beyin-micro-llm)
13. [LSP ve VSCode Editör Desteği](#13-lsp-ve-vscode-editör-desteği)
14. [Paket Yöneticisi ve İki Depolu (Two-Repo) Mimari](#14-paket-yöneticisi-ve-iki-depolu-mimari)
15. [Çok Dilli Kod Dönüştürme (`#convert`) ve Harici Kütüphane Çağırma (FFI)](#15-çok-dilli-kod-dönüştürme-convert-ve-harici-kütüphane-çağırma-ffi)
16. [Derleme ve Çapraz Platform Doğrulama (Windows & Docker Linux)](#16-derleme-ve-doğrulama)

---

## 1. Giriş ve Felsefe

**Yukinary (YKN)**; harici bir C çalışma zamanına (libc/msvcrt), LLVM'e veya harici bağlayıcıya (linker) ihtiyaç duymadan **doğrudan makine kodu (Windows PE32+ ve Linux ELF64)** üreten, kendi kendini derleyen (self-hosting), yapay zeka destekli modern bir sistem programlama dilidir.

### Neden YKN?
* **Sıfır Bağımlılık:** C derleyicisi veya harici linkere ihtiyaç duymadan doğrudan `.exe` veya ELF ikili dosyası basar.
* **Hafıza Güvenliği:** Garbage Collector (çöp toplayıcı) duraksamaları olmadan, derleme zamanında Use-After-Free ve sınır kontrolleri uygular.
* **Çok Dilli Doğal Gramer:** Türkçe, İngilizce, Rusça, Japonca, Çince ve Korece anahtar kelimeleri doğrudan derleyici seviyesinde ana dil olarak kabul eder.
* **Can Simidi (Self-Healing):** Kodda eksik noktalı virgül, parantez veya null erişimi varsa programı çökertmez; otomatik onarır.
* **Yerleşik Nöral Ağ:** 16 KB'lık dahili BitNet 1.58b Transformer modeli ile internet veya GPU olmadan 0.1 ms'de kod önerisi üretir.

---

## 2. Kurulum ve Hızlı Başlangıç

### Windows Üzerinde Çalıştırma
Derleyici ikilisi `app_full.exe` veya saf YKN ile üretilen `ykn_full.exe` dosyasını doğrudan çağırabilirsiniz:

```powershell
# Merhaba Dunyasi dosyasını derle
.\ykn_full.exe merhaba.ykn -o merhaba.exe

# Çalıştır
.\merhaba.exe
```

### Linux (Docker veya Doğal) Üzerinde Çalıştırma
```bash
# Linux ELF64 binary üretimi
./ykn_full merhaba.ykn -o merhaba
chmod +x merhaba
./merhaba
```

---

## 3. Çok Dilli Sözdizimi

YKN'de ana diliniz ne olursa olsun kod yazabilirsiniz. İster saf Türkçe, ister küresel İngilizce, ister Kiril veya Doğu Asya alfabeleri:

| Kavram | Türkçe | English | Русский | 日本語 | 简体中文 | 한국어 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Fonksiyon** | `isv` / `islev` | `fn` / `func` | `функция` | `関数` | `函数` | `함수` |
| **Değişken** | `degisken` | `let` / `var` | `переменная` | `変数` | `变量` | `변수` |
| **Eğer / İse** | `eger` .. `ise` | `if` .. `then` | `если` .. `то` | `もし` .. `ならば` | `如果` .. `那么` | `만약` .. `이면` |
| **Yoksa** | `yoksa` | `else` | `иначе` | `それ以外` | `否则` | `그외` |
| **Döngü** | `dongu` | `while` / `loop` | `пока` | `繰り返す` | `循环` | `반복` |
| **Dönüş** | `dondu` | `return` | `вернуть` | `戻す` | `返回` | `반환` |
| **Yapı** | `yapi` | `struct` | `структура` | `構造体` | `结构体` | `구조체` |
| **Enum** | `enum` | `enum` | `перечисление` | `列挙型` | `枚举` | `열거형` |
| **Eşleme** | `esles` | `match` | `сопоставление`| `一致` | `匹配` | `일치` |
| **Boş Pointer**| `bos` | `null` / `nil` | `пусто` | `無` | `空` | `널` |

---

## 4. Temel Tipler, Değişkenler ve Operatörler

### Değişken Tanımlama
```ykn
degisken sayi: tamsayi = 42;          # 64-bit işaretli tamsayı (i64)
degisken bayt_verisi: u8 = 255;       # 8-bit işaretsiz tamsayı
degisken aktif_mi: mantiksal = dogru; # boolean (dogru / yanlis)
degisken pi: ondalik = 3.14159;       # 64-bit kayan nokta (f64)
degisken mesaj: *u8 = "Merhaba YKN!".ptr; # Bayt işaretçisi
```

### Mantıksal ve Aritmetik Operatörler
* **Ve:** `ve`, `and`, `и`, `かつ`, `并且`, `그리고`
* **Veya:** `veya`, `or`, `или`, `または`, `或者`, `또는`
* **Değil:** `degil`, `not`, `не`, `否定`, `非`, `아니다`

```ykn
eger (sayi > 10 ve aktif_mi == dogru) veya degil yanlis ise {
    ekrana_yaz("Kosul saglandi!\n".ptr);
}
```

---

## 5. Fonksiyonlar ve Kontrol Akışı

### Fonksiyon Bildirimi
Fonksiyonlar `isv` (veya `fn`) ile başlar, dönüş tipi `-> Tip` ile belirtilir ve `dondu` ile sonuç döndürülür:

```ykn
isv topla(a: tamsayi, b: tamsayi) -> tamsayi {
    dondu a + b;
}
```

### Koşul Blokları (`eger` / `yoksa`)
```ykn
isv mutlak_deger(x: tamsayi) -> tamsayi {
    eger x < 0 ise {
        dondu 0 - x;
    } yoksa {
        dondu x;
    }
}
```

### Döngüler (`dongu`)
YKN optimize edilmiş döngü değişkenlerini otomatik olarak donanım kayıtlarına (`RBX`, `R12`..`R15`) atar:

```ykn
isv faktoriyel(n: tamsayi) -> tamsayi {
    degisken sonuc: tamsayi = 1;
    degisken i: tamsayi = 1;
    dongu i <= n {
        sonuc = sonuc * i;
        i = i + 1;
    }
    dondu sonuc;
}
```

---

## 6. Yapılar (Struct) ve Bellek İşaretçileri

### `yapi` Tanımlama
```ykn
yapi Oyuncu {
    kimlik: tamsayi;
    puan: tamsayi;
    can: tamsayi;
}

isv oyuncu_olustur(id: tamsayi) -> Oyuncu {
    degisken o: Oyuncu;
    o.kimlik = id;
    o.puan = 0;
    o.can = 100;
    dondu o;
}
```

### Dinamik Bellek Yönetimi (Arena ve Heap)
```ykn
degisken p: *Oyuncu = bellek_tahsis_et(24);
p.kimlik = 1;
p.can = 100;
```

---

## 7. Etiketli Enum'lar (Tagged Enums) ve Örüntü Eşleme

YKN, Rust benzeri veri taşıyan (payload'lu) cebirsel veri tiplerini (ADT) tam destekler.

### Payload Taşıyan Enum Tanımlama
```ykn
enum IslemDurumu {
    Beklemede,
    DevamEdiyor(tamsayi),      # % ilerleme yükü taşır
    HataKodu(tamsayi),         # Hata kodu yükü taşır
    Tamamlandi(tamsayi)        # Sonuç kodu taşır
}
```

### Örüntü Eşleme (`esles`)
```ykn
isv durum_isle(d: IslemDurumu) -> tamsayi {
    esles d {
        IslemDurumu.Beklemede => {
            dondu 0;
        }
        IslemDurumu.DevamEdiyor(ilerleme) => {
            dondu ilerleme;
        }
        IslemDurumu.HataKodu(kod) => {
            dondu 0 - kod;
        }
        IslemDurumu.Tamamlandi(sonuc) => {
            dondu sonuc;
        }
    }
}
```

---

## 8. Hiyerarşik `#config` Orkestratörü

Kodun en başında dosya düzeyinde donanım parametrelerini belirleyebilirsiniz:

```ykn
#config {
    yigin_boyutu: 16777216,   # 16 MB Stack boyutu (PE/ELF header'ına doğrudan işlenir)
    is_parcacigi: 8,          # Eşzamanlı iş parçacığı havuzu
    bellek: "arena",          # Varsayılan bellek tahsisçisi ("arena" veya "sistem")
    islemci: "native"         # Hedef mikromimari (AVX2, POPCNT, SIMD optimizasyonları)
}
```

---

## 9. Kendi Kendini Onarma (Self-Healing)

`--onar` veya `--self-heal` bayrağı ile çalıştırıldığında derleyici bir oto tamircisi gibi davranır:

1. **Geri Yükleme Noktası (.bak):** Dosyaya dokunmadan önce anında orijinal yedeğini alır.
2. **Tur 1 (Yazım Hataları):** `islv` -> `islev`, `dndu` -> `dondu` kelimelerini ve satır sonundaki unutulmuş noktalı virgülleri (`;`) tamir eder.
3. **Tur 2 (Null-Guard Hava Yastığı):** Kontrolsüz pointer erişimi (`dondu ptr.alan;`) tespit ettiğinde kodu otomatik olarak güvenliğe alır:
   ```ykn
   eger ptr != bos ise {
       dondu ptr.alan;
   }
   dondu 0;
   ```
4. **Tur 3 (Mantık Tamamlama):** Unutulan dönüş değerlerini (`dondu 0;`) süslü parantez kapanışına ekler.
5. **Güvenlik Bariyeri:** Onarılan kod derlenemiyorsa asla koda uygulanmaz!

---

## 10. Hafıza Zabıtası: Safety ve UAF

C ve C++'taki en tehlikeli hata olan **Use-After-Free (UAF)**, YKN derleyicisinde derleme anında tespit edilir:

```ykn
# Güvenlik motoru bir işaretçi serbest bırakıldığında onu kara listeye yazar:
bellek_serbest_birak(ptr);

# Eğer ilerleyen satırlarda serbest bırakılan adrese dokunursanız:
# HATA: [UAF] Serbest birakilmis pointer'a erisim tespit edildi!
```

---

## 11. X-Ray Güvenlik Kapısı: Sandbox Gate

Harici modüller veya AI tarafından üretilen kodlar AST seviyesinde denetlenir:
* İzin verilmemiş `socket`, `send`, `recv` (Ağ işlemleri),
* `CreateProcess`, `WinExec` (Program başlatma),
* `LoadLibrary`, `dlopen` (Dinamik kütüphane sızması),
* İzinsiz disk yazmaları doğrudan engellenir:
  `"YKNLLM sandbox: capability fonksiyonu cagrisi reddedildi!"`

---

## 12. Yapay Zeka Mimarisi: Micro-LLM ve Full LLM Ayrımı

YKN'de yapay zeka iki tamamlayıcı katmandan oluşur. Bu iki katmanın görevleri ve donanım gereksinimleri birbirinden tamamen farklıdır:

### İki Katmanlı Yapay Zeka Özeti

| Kriter | 🧠 Gömülü Micro-LLM (16 KB) | 🚀 Harici Full LLM (1B – 70B GGUF) |
| :--- | :--- | :--- |
| **Konum & Bellek** | Derleyici ikilisinin (`ykn.exe`) `.rodata` veri kesitine gömülü | Kullanıcının diskten gösterdiği harici `.gguf` dosyası |
| **Boyut & Ağırlıklar** | **16.448 Bayt (~16 KB)** ternary $\{-1, 0, +1\}$ BitNet omurgası | **1 GB – 40 GB** (Llama-3, Mistral, Qwen, BitNet b1.58 vb.) |
| **Donanım Şartı** | **Sıfır GPU, Sıfır Python, Sıfır İnternet.** Her standart CPU'da çalışır | Model boyutuna göre 4 GB – 32 GB RAM veya GPU gerektirir |
| **Gecikme (Latency)** | **0.1 milisaniye (insan gözünün algılayamayacağı kadar anlık)** | Token başına 10 – 50 milisaniye |
| **Kullanım Amacı** | **Derleyicinin Refleksleri:**<br>• Sözdizimi onarımı (`Self-Healing`: noktalı virgül, parantez)<br>• Çökmeleri önleyen `null-guard` enjeksiyonu<br>• VSCode'da anlık kod ve struct alanı tamamlama<br>• Çevrimdışı temel fonksiyon imzası çeviri yedeği | **Derin Semantik ve Üretkenlik:**<br>• **Kapsamlı `#convert` Motoru:** Tüm Python, C, C++, Rust kütüphanelerini semantik olarak anlayıp YKN'ye dönüştürme<br>• Doğal dilden YKN kodu üretme (`#prompt "HTTP sunucusu yaz"`)<br>• Sıfırdan karmaşık veri yapıları ve algoritmalar tasarlama |
| **Kullanıcı Hisseder mi?** | **Hayır.** Arka planda sessiz, kesintisiz ve her daim aktiftir ("Always-On") | **Evet.** Geliştirici komut satırından özellikle model dosyasını vererek çağırır |

### 1. Gömülü Micro-LLM Nasıl Çalışır?
Her derleme anında arka planda otomatik devreye girer. Kullanıcının model indirmesine gerek yoktur. Kodda bir hata varsa Levenshtein mesafesi ve nöral logitler birleşerek anında en olası tamiri sunar.

### 2. Harici Full LLM Nasıl Çalışır ve Modeller Nasıl İndirilir?

Yukinary, gömülü 16 KB Micro-LLM sayesinde **sıfır indirme ile kutudan çıktığı gibi çalışır**. 

Ancak `#prompt` ile sıfırdan büyük algoritmalar yazdırmak veya harici Python/C++ kütüphanelerini `#convert` ile bütünüyle YKN'ye dönüştürmek istediğinizde harici bir GGUF modeli (Microsoft BitNet b1.58, Prism ML Bonsai 8B vb.) bağlayabilirsiniz.

#### Neden Modeller Doğrudan Git Deposunda Yok?
Git kaynak kod versiyonlama içindir. GitHub'da **dosya başına 100 MB katı sınırı** vardır. Yapay zeka modelleri ise 1.1 GB ile 40 GB arasındadır. Tüm dünyada (llama.cpp, Ollama, PyTorch vb.) olduğu gibi model ağırlıkları [Hugging Face](https://huggingface.co) üzerinde barındırılır.

#### Tek Komutla Otomatik Model İndirme:
Repodaki hazır indirme araçlarını kullanarak modelleri doğrudan indirebilirsiniz:

```powershell
# Windows (PowerShell)
powershell -File tools/download_models.ps1 bitnet    # Microsoft BitNet b1.58 (1.18 GB) indirir
powershell -File tools/download_models.ps1 bonsai    # Prism ML Bonsai 8B (1.15 GB) indirir
powershell -File tools/download_models.ps1 all       # Her iki modeli de indirir
```

```bash
# Linux / macOS (Bash)
sh tools/download_models.sh bitnet
sh tools/download_models.sh bonsai
sh tools/download_models.sh all
```

#### Manuel İndirme Linkleri (Hugging Face Doğrudan İndirme):
| Model | Boyut | Format | Doğrudan İndirme Linki | Hedef Klasör Yolu |
| :--- | :--- | :--- | :--- | :--- |
| **Microsoft BitNet b1.58 2B4T** | 1.18 GB | GGUF (i2_s) | [Hugging Face İndir](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T-gguf/resolve/main/ggml-model-i2_s.gguf) | `models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf` |
| **Prism ML Bonsai 8B** | 1.15 GB | GGUF | [Hugging Face İndir](https://huggingface.co/prism-ml/Bonsai-8B-gguf/resolve/main/Bonsai-8B.gguf) | `models/bonsai-8b/Bonsai-8B.gguf` |

#### Derleyici ile Kullanım Örnekleri:
İndirdiğiniz modeli derleyiciye `--llm-model` parametresiyle verin:

* **`#prompt` ile Kod Üretimi:**
  ```powershell
  ykn.exe ana.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Python Kütüphanesini YKN'ye Çevirme:**
  ```powershell
  ykn.exe convert hesap.py --into-file hesap.ykn --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Terminalden Canlı Kod Ürettirme:**
  ```powershell
  ykn.exe ai "isv fibonacci(n: tamsayi) -> tamsayi" --llm-model models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf
  ```
* **Model Olmadan Çalışma (Gömülü Micro-LLM):**
  Hiçbir model belirtmezseniz dahili 16 KB motor devrededir:
  ```powershell
  ykn.exe ana.ykn --self-heal
  ```

---

## 13. LSP ve VSCode Editör Desteği

YKN, yerleşik bir **Language Server Protocol (LSP)** sunucusuna (`ykn_lsp.exe`) sahiptir:
* `.` koyduğunuzda struct alanlarını otomatik listeler,
* Fonksiyon parametre ipuçlarını sunar,
* Hatalı satırların altına kırmızı dalga çizerek anında canlı düzeltme önerisi verir.

---

## 14. Paket Yöneticisi ve İki Depolu Mimari

YKN ekosistemi iki bağımsız depodan meydana gelir:
1. **Çekirdek Depo (`yukinary/yukinary`):** Derleyici, çalışma zamanı, standart kütüphane ve LSP.
2. **Paket Deposu (`yukinary/packages`):** Topluluk paketlerinin ve kütüphanelerin barındığı merkezi registry.

### `yukinary.toml` Manifesti
```toml
[package]
name = "benim_projem"
version = "0.1.0"
main = "ana.ykn"
output = "build/uygulama.exe"

[dependencies]
matematik = "1.2.0"
ag_istemcisi = { git = "https://github.com/ykncode/packages", path = "net/http" }
```

Paket ekleme komutu:
```powershell
.\ykn_full.exe add matematik
.\ykn_full.exe build
```

---

## 15. Çok Dilli Kod Dönüştürme (`#convert`) ve Harici Kütüphane Çağırma (FFI)

YKN, "kütüphanemiz henüz az" sendromunu kökünden çözmek için iki güçlü interoperabilite kanadına sahiptir:

### A. Doğrudan C-ABI Kütüphane Çağırma (`harici isv`)
YKN, C veya C++ ile derlenmiş tüm `.dll` (Windows) ve `.so` (Linux) paylaşımlı kütüphanelerini doğrudan çalıştırabilir:

```ykn
# C dinamik kütüphanesinden fonksiyon bildirimi
harici isv sqlite3_open(dosya_adi: *u8, db_ptr: **u8) -> tamsayi;
harici isv sqlite3_close(db_ptr: *u8) -> tamsayi;

isv veritabani_baslat() -> tamsayi {
    degisken db: *u8 = 0;
    degisken sonuc: tamsayi = sqlite3_open("test.db".ptr, &db);
    dondu sonuc;
}
```

### B. `#convert` Direktifleri ile Kod Dönüştürme
Başka bir dilde yazılmış hazır bir algoritmanız veya kütüphane fonksiyonunuz varsa, kaynak dosyanıza direktif olarak gömebilirsiniz:

* `#C-convert` — C kodunu idiomatik YKN'ye dönüştürür
* `#PY-convert` — Python fonksiyonunu YKN'ye dönüştürür
* `#JS-convert` — JavaScript fonksiyonunu YKN'ye dönüştürür
* `#CS-convert` / `#RB-convert` — C# ve Ruby kodlarını dönüştürür

Örnek Python dönüştürme:
```ykn
#PY-convert
def kare_al(x):
    return x * x
#end-convert

# Yukarıdaki kod derleme anında otomatik olarak şu YKN koduna dönüştürülür:
# isv kare_al(x: tamsayi) -> tamsayi {
#     dondu x * x;
# }
```

### C. İki Aşamalı SHA-256 Önbellek (`.ykn_cache/convert/`)
Dönüştürülen tüm yabancı kodlar `.ykn_cache/convert/` altında SHA-256 parmak izi ile önbelleğe alınır. Kod değişmediği sürece dönüştürücü tekrar çalıştırılmaz; derleme süresi 0 ms ek maliyetle tamamlanır.

---

## 16. Derleme ve Doğrulama

### Bütünsel Doğrulama (Windows & Docker Linux)
Proje kökündeki `scratch/verify_all.ps1` betiği tüm Faz testlerini hem Windows PE32+ hem Docker Linux ELF64 ortamında sıfır hata ile doğrular:

```powershell
powershell -ExecutionPolicy Bypass -File scratch/verify_all.ps1
```

**Tebrikler! Artık Yukinary'nin tüm mimari gücüne hakimsiniz.**

