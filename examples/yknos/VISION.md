# YKNOS — Vizyon Belgesi

**Tarih:** 3 Eylül 2026  
**Durum:** Vizyon / Uzun Vade Yol Haritası  
**Not:** Bu belge "şimdi yapılacaklar" listesi değil, projenin ulaşabileceği yeri tanımlar.

---

## Vizyon Tanımı

> **YKNOS:** Linux LTS kernel üzerine inşa edilmiş, YKN dili ile yazılmış user space,  
> BitNet MoE gömülü Y.U.K.I AI asistanı ile tam sistem kontrolü sunan,  
> sandbox-first güvenlik modeliyle çalışan özgün bir işletim sistemi.

---

## Kişisel Değerlendirmem: YKN ve YKNOS Hakkında

Bu bölümde ne düşündüğümü gerçekten söylüyorum. Ne fazla karamsar ne fazla iyimser — mümkün olduğunca net.

---

### YKN Hakkında — Şu An Nerede?

YKN beni gerçekten şaşırttı. Bir programlama dili sıfırdan yazmak — lexer, parser, typechecker, codegen, self-host döngüsü — bunların hepsini tamamlayan çok az kişi var. GitHub'da yüzlerce "ev yapımı dil" projesi var ama %95'i ya self-host kapatamıyor ya da gerçek binary üretemiyor. YKN ikisini de yaptı.

**Gerçekten önemli olan şey şu:** Stage 3 deterministik. `out.exe → out3.exe → out4.exe`, SHA256 eşit. Bu "oyuncak derleyici" ile "gerçek derleyici" arasındaki fark. Çoğu ev derleyicisi bu testi hiç geçemiyor.

Ama aynı zamanda şunu net söylemem lazım: **YKN şu an gerçek bir program yazmak için hazır değil.** Heap array yok, namespace yok, LLM entegrasyonu stub, debug print'ler production binary'ye gömülü. Birinin bu dili günlük iş için seçmesini beklemek şu an gerçekçi değil.

**Potansiyeli:** Çok spesifik bir niş var ve o niş dolu değil. "Türkçe sözdizimli, C ekosistemi erişimli, gömülü LLM asistanlı sistem dili" kombinasyonu 2026'da başka hiçbir yerde yok. Bu boşluk gerçek.

---

### YKNOS Hakkında — Hayalden Gerçeğe Mesafe

ChromeOS analojisi doğru yerde seçilmiş. Google'ın en zeki kararı şuydu: "Kernel'i sıfırdan yazmayacağız, Linux'u alacağız." Bu karar ChromeOS'u 2-3 yıl öne attı. YKNOS için aynı strateji geçerli.

**Yapılabilir olan:**

Linux LTS kernel + YKN userspace kombinasyonu teknik olarak mümkün. POSIX API YKN'den çağrılabilir, init sistemi yazılabilir, minimal window manager yazılabilir. Bu "distribüsyon" seviyesinde bir iş — Alpine Linux veya Buildroot ile başlanabilir. Yani kernel derleme, package management, init script — bunlar çözülmüş problemler.

**Y.U.K.I konsepti — bu gerçekten özgün:**

"Başlangıç menüsü yerine AI" fikri gülünç değil. Microsoft bunu yapmaya çalışıyor (Copilot+ PC), Apple yapıyor (Apple Intelligence), ama ikisi de mevcut paradigmayı değiştirmiyor — masaüstüne asistan ekliyorlar. YKNOS bunu tersine çeviriyor: AI birincil interface, geleneksel UI ikincil. Bu farklı bir felsefi pozisyon.

BitNet MoE ve native YKN runtime ile bu teknik olarak 2026'da mümkün hale geliyor. 3B parametreli BitNet MoE model 800MB-1.2GB aralığına inebilir; mmap/range loader ve kontrollü cache kullanımıyla orta sınıf bir laptop'ta sürekli arka planda çalışması hedeflenebilir. Bu hedef model, quantization ve donanım ölçümleriyle doğrulanmalıdır.

**"Otomatik hata düzeltme" kısmında beklentiler yönetilmeli:**

Kernel panic için gerçekçi değil — ama çoğu kullanıcının yaşadığı "hata" kernel panic değil. Uygulama crash'i, servis başlamama, disk dolması, ağ bağlantı sorunu — bunların %80'i user space sorunları. Y.U.K.İ bu sorunları gerçekten tespit edip düzeltebilir. "Mavi ekran yok" vaadi gerçek — ama anlamı "kernel panic yok" değil, "kullanıcı anlamadığı hata mesajlarıyla karşılaşmaz" olmalı.

---

### Potansiyel — Gerçekten Ne Olabilir?

**Kısa vade (1-2 yıl): Eğitim ve araştırma platformu**

Türkiye'de bilgisayar bilimi eğitimi için gerçek bir boşluk var. Lisans düzeyinde sistem programlama, derleyici tasarımı, işletim sistemi dersleri — bunlar genellikle C/C++ ile yapılıyor ama kavramlar karmaşık dilde kayboluyor. YKN + YKNOS, "Türkçe açıklamalar, Türkçe sözdizimi, gerçek sistem programlama" kombinasyonu olarak bu alanda ciddi değer yaratabilir.

**Orta vade (2-4 yıl): Gömülü ve IoT sistem dili**

Manuel bellek yönetimi + C FFI + küçük binary boyutu + çapraz derleme (ARM64 dahil) kombinasyonu, IoT ve embedded sistemler için ideal. Raspberry Pi, ARM tabanlı endüstriyel bilgisayarlar, edge computing cihazları. Bu segmentte Rust çok karmaşık, C çok tehlikeli — YKN ortada makul bir seçenek olabilir.

**Uzun vade (4-7 yıl): LLM-first OS paradigması**

Eğer BitNet MoE tarzı modeller 1-2 yıl içinde beklendiği gibi olgunlaşırsa, "her cihazda yerel model" gerçek olacak. YKNOS bu trende erken girenlerden biri olabilir. "Başka herkes asistanı masaüstüne ekledi, biz masaüstünü asistan olarak tasarladık" konumlaması özgün.

---

### Gerçek Riskler — Bunları Görmezden Gelmek Olmaz

**Tek kişi riski en büyük tehlike.**

ChromeOS'un arkasında Google mühendisleri vardı. Android'in arkasında bir şirket vardı. Tek kişinin 3-5 yıllık bir işletim sistemi projesini sürdürmesi tarihsel olarak çok nadir başarıya ulaşıyor. Bu vizyon için ya bir topluluk oluşması ya da kurumsal destek (üniversite, şirket sponsorluğu) gerekiyor.

**"Her şeyi YKN ile yaz" tuzağı.**

Zaman zaman "mevcut araç yetersiz, kendi aracımı yazayım" döngüsüne girilip asıl ürün için zaman kalmıyor. YKN zaten bu riski taşıyor — derleyici yazarken işletim sistemi de yazmaya başlamak, odak kaybı yaratır. Önce dil tamamlanmalı, sonra OS.

**Y.U.K.I'nin güvenlik riski küçümsenmemeli.**

Sistem üzerinde tam yetkili bir AI, yanlış karar verirse sistemi tamamen bozabilir. "Otomatik düzeltme" özelliği ciddi güvenlik açığı potansiyeli taşıyor. Bir kötü niyetli prompt ile sistemin bütün dosyaları silinebilir. Bu sadece teknik değil, etik bir tasarım sorusu.

**Kullanıcı adaptasyonu çok zor.**

"Başlangıç menüsü yok, her şeyi AI'ya söyle" — bu kullanıcı alışkanlıklarını tamamen kırıyor. ChromeOS bile "sadece browser" konseptiyle on yıl savaştı ve hâlâ niş. Radikal bir UI paradigması değişikliği, vizyon ne kadar doğru olursa olsun, kullanıcı direnciyle karşılaşır.

---

### Benim Gerçek Görüşüm

Bu proje bence iki şey arasında:

**En iyi senaryo:** YKN ve YKNOS, 3-5 yıl içinde Türkiye kaynaklı, dünyada dikkat çeken özgün bir teknoloji haline gelir. Akademik çevrelerde tanınır, küçük ama sadık bir geliştirici topluluğu oluşur, embedded/IoT alanında gerçek kullanım bulur. LLM-first OS konsepti erken benimseyenler arasında yer alır.

**Gerçekçi senaryo:** YKN iyi bir eğitim projesi ve araştırma platformu olarak tanınır. YKNOS Phase 1 — çalışan bir prototype — oluşturulur. Y.U.K.I basit sistem yönetim görevlerini yapabilen bir asistan olur. Büyük bir kullanıcı kitlesi oluşmaz ama teknik derinliği olan insanlar için referans bir proje haline gelir.

**Kötü senaryo:** Odak dağılır. Dil tamamlanmadan OS yazılmaya başlanır, OS tamamlanmadan LLM entegrasyonuna geçilir, hiçbiri production kalitesine ulaşamaz.

**Kötü senaryoyu önleyecek tek şey:** Net öncelik. Şu an yapılması gereken sıra şu — dil önce, OS sonra, LLM entegrasyonu paralel ama bağımlı. Bu sıra bozulursa proje dağılır.

---

### Son Söz

YKN'nin varlığı ve şu anki olgunluk seviyesi bile başlı başına etkileyici. Pek çok insan "kendi dilimi yazacağım" der ama çok azı self-host döngüsünü kapatır. Bu proje o eşiği geçti.

YKNOS vizyonu teknik olarak mümkün, stratejik olarak özgün, ama yürütme açısından zorlu. "Yapabilir miyiz?" sorusunun cevabı teknik olarak evet. "Yapacak mıyız?" sorusunun cevabı tamamen öncelik yönetimi ve sürdürülebilirlik.

Eğer bu vizyon gerçekleşirse — ve ben bunun mümkün olduğunu düşünüyorum — bu Türkiye'den çıkmış, global ölçekte özgün bir teknoloji olur. Bu küçük bir şey değil.

---

## YKNOS Mimari Taslağı

```
┌─────────────────────────────────────────────────────────┐
│                    Y.U.K.I                               │
│         (BitNet MoE + native runtime — Sürekli Aktif)  │
│  Doğal dil komutları │ Sistem analizi │ Kod üretimi      │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│                  YKN Userspace                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │
│  │ykn-init  │ │ykn-shell │ │ykn-wm    │ │ykn-pkg   │   │
│  │(PID 1)   │ │(terminal)│ │(window   │ │(paket    │   │
│  │          │ │          │ │ manager) │ │ yönetici)│   │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘   │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │           Sandbox Katmanı                        │   │
│  │  namespaces + cgroups + seccomp + eBPF           │   │
│  │  Her uygulama izole, explicit permission gerekli │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              Linux LTS Kernel (dokunulmaz)               │
│  Device drivers │ Filesystem │ Network │ Memory mgmt     │
└─────────────────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│                   Donanım                               │
│     x86_64 / ARM64 (Raspberry Pi dahil)                 │
└─────────────────────────────────────────────────────────┘
```

---

## Y.U.K.I Yetenek Katmanları

### Katman 1 — Mümkün (BLOK 10 ile)
- Doğal dil sistem komutları ("WiFi'yi kapat", "RAM kullanan programları göster")
- Servis crash tespiti ve otomatik restart
- Hata mesajı açıklama ("bu hata ne anlama geliyor?")
- YKN kodu yazma yardımı
- Log analizi ("neden yavaş çalışıyor?")

### Katman 2 — Orta Vadede Mümkün
- C kütüphane FFI wrapper otomatik üretimi
- Güvenlik anomali tespiti ("bu process neden bu dosyaya erişiyor?")
- Sistem optimizasyon önerileri
- Dosya sistemi organize yardımı

### Katman 3 — Zor Ama Mümkün
- Uygulama davranış tahmini ve proaktif kaynak yönetimi
- Multi-turn konuşma ile karmaşık sistem görevleri
- Otomatik yazılım kurulum ve konfigürasyon

### Katman 4 — Kernel Panic Düzeltme — MÜMKÜN DEĞİL
Kernel panic, hardware hatası ve memory corruption user space AI ile düzeltilemez.
Bu katman dışarıda tutulmalı — beklenti yönetimi açısından kritik.

---

## Sandbox Güvenlik Modeli

```
YKNOS Güvenlik Hiyerarşisi:

Seviye 0 — Kernel (Linux, dokunulmaz)
Seviye 1 — Y.U.K.İ Core (tam yetki, ama audit log zorunlu)
Seviye 2 — YKN Sistem Servisleri (capability-based yetki)
Seviye 3 — Kullanıcı Uygulamaları (sandbox, explicit permission)
Seviye 4 — LLM Üretilen Kod (en kısıtlı sandbox, çalıştırmadan önce onay)

Her seviye bir üst seviyeye istek gönderebilir ama
doğrudan alt seviyeyi atlayamaz.
```

**Sandbox politikası:**
- Her uygulama varsayılan olarak network erişimi yok
- Filesystem erişimi sadece `~/documents/<uygulama>/` ile sınırlı
- Kamera, mikrofon, GPS — her kullanımda onay
- LLM üretilen kod her zaman Seviye 4'te başlar

---

## Geliştirme Fazları

### Faz 0 — Temel (YKN BLOK 7-10)
*Süre: 12-18 ay*
- YKN dili kararlı, LSP çalışıyor
- BitNet MoE gömülü model çalışıyor
- FFI ekosistemi otomatik üretim mümkün

### Faz 1 — YKNOS Alpha
*Süre: 12-18 ay (Faz 0 sonrası)*
- Linux LTS kernel + YKN init sistemi
- Minimal terminal tabanlı UI
- Y.U.K.I Katman 1 yetenekleri
- Temel sandbox (namespaces + cgroups)
- Raspberry Pi ve x86_64 destekli

### Faz 2 — YKNOS Beta
*Süre: 12+ ay (Faz 1 sonrası)*
- YKN window manager (Wayland protokolü üzerinde)
- Y.U.K.I Katman 2-3 yetenekleri
- Paket deposu (pkg.ykn.dev)
- YKN fine-tune model (dile özgü)
- Güvenlik audit ve sertifikasyon

### Faz 3 — YKNOS v1.0
*Süre: Belirsiz (topluluk büyümesine bağlı)*
- Tam Y.U.K.I deneyimi
- Uygulama ekosistemi
- Kurumsal destek (üniversite, şirket)

---

## Rekabet Analizi

| Sistem | Kernel | AI Entegrasyon | Dil | Sandbox |
|---|:---:|:---:|:---:|:---:|
| ChromeOS | Linux | Gemini Nano (eklenti) | C++/JS | Container |
| Windows 11 | NT | Copilot (eklenti) | C++ | AppContainer |
| macOS | XNU | Apple Intelligence (eklenti) | Swift/ObjC | App Sandbox |
| **YKNOS** | **Linux** | **Y.U.K.I (gömülü)** | **YKN** | **Native** |

**Fark:** Diğerleri AI'yı mevcut sisteme ekliyor. YKNOS AI'yı sistemin tasarım merkezi yapıyor.

---

## Başarı Kriterleri (Dürüst)

### Minimum Başarı
- YKN ile 1000 satır gerçek sistem servisi yazılabiliyor
- Y.U.K.I 10 farklı sistem sorusuna doğru yanıt veriyor
- Sandbox'ta bir uygulama çalışıp sistem kaynakları izole tutuluyor
- Raspberry Pi'de boot edip terminal açılıyor

### Hedef Başarı
- 100+ aktif kullanıcı, gerçek geri bildirim
- 10+ C kütüphanesi için YKN wrapper mevcut
- Bir Türk üniversitesinde ders materyali olarak kullanılıyor
- GitHub'da 1000+ star

### Büyük Başarı
- International hacker topluluğunda tanınıyor
- Gömülü/IoT segmentinde gerçek kullanım
- Y.U.K.I konsepti akademik makalelerde atıf alıyor

---

*Bu belge bir manifesto değil. Bir yol haritası. Her faz önceki fazın çıktısına bağlı —  
sıra bozulursa vizyon değil, tek tek görevler hayatta kalır.*

---

## 🔮 Özgün Dil Özellikleri Vizyonu

### Zamanlı Tip Sistemi

```ykn
degisken token: @sureli(300 saniye) *u8 = api_token_al();
degisken cache: @sureli(3600 saniye) *VeriSeti = veri_cek();

# Derleyici uyarısı: "Bu noktada token süresi dolmuş olabilir"
# Tip sistemi: token.gecerli_mi() kontrolü zorunlu kılınabilir
```

IoT, güvenlik, fintech — tüm bu alanlarda "değerin ne kadar geçerli olduğu" kritik bilgi. Bunu tip sistemine taşımak gerçek bir problem çözüyor.

### Niyet Dekoratörleri

```ykn
@niyet("hassas veri işle, loglanmasın, şifreli aktar")
@yan_etki_yok
@deterministik
@on_kosul(girdi != 0)
isv sifrele(girdi: *u8, uzunluk: tamsayi) -> *u8 { ... }
```

LLM bu dekoratörlerden otomatik test üretir, güvenlik audit yapar, dokümantasyon yazar.

### Reaktif Değişkenler

```ykn
@izle
degisken sicaklik: gercek = sensor_oku();

@sicaklik_degisince
isv uyar(yeni: gercek) {
    eger yeni > 80.0 ise { bildirim_gonder("Kritik sıcaklık!"); }
}
```

IoT ve UI geliştirme için doğal bir abstraction.

---

## 🌐 Uygulama Uyumluluğu — Teknik Detay

### Android Çalıştırma Mimarisi

```
┌─────────────────────────────────────────┐
│           YKNOS Masaüstü                │
│  ┌──────────┐  ┌──────────────────────┐ │
│  │ Native   │  │  Android Container   │ │
│  │ YKN App  │  │  (Waydroid benzeri)  │ │
│  │          │  │  ┌────────────────┐  │ │
│  │ Tam      │  │  │ Android 13 LTS │  │ │
│  │ erişim   │  │  │ ART Runtime    │  │ │
│  │          │  │  │ APK çalışıyor  │  │ │
│  └──────────┘  │  └────────────────┘  │ │
│                │  Wayland bridge      │ │
│                └──────────────────────┘ │
└─────────────────────────────────────────┘
         Linux LTS Kernel (altında)
```

Y.U.K.İ'nin rolü burada kritik: Her APK çalıştırılmadan önce davranış analizi yapılır. "Bu uygulama sadece kamera kullanıyor" ile "Bu uygulama tüm dosyalarına erişmeye çalışıyor" farkını kullanıcıya gösterir.

### Windows Uygulamaları Çalıştırma

Wine/Proton teknolojisi proven — YKNOS bunu entegre eder ama Y.U.K.İ katmanı ekler:

```
EXE çalıştırılmak isteniyor
         │
         ▼
Y.U.K.İ PE analizi
  - Hangi Win32 API'ler kullanılıyor?
  - Network bağlantısı açıyor mu?
  - Registry değiştiriyor mu?
         │
         ├── Basit araç → LLM native dönüşüm dener
         ├── Orta karmaşıklık → Wine sandbox'ta
         └── Yüksek risk → Kullanıcı uyarısı + tam izolasyon
```

### LLM Destekli Transpilation — Nerede Çalışır, Nerede Çalışmaz

**Çalışır:**
- Basit araçlar (hesap makinesi, not defteri, dosya işleme)
- Algoritma ağırlıklı kod (sıralama, arama, dönüşüm)
- CLI araçları

**Çalışmaz veya sınırlı çalışır:**
- Framework bağımlı web uygulamaları (Django, Spring)
- Reflection kullanan kod
- DirectX/GPU ağırlıklı oyunlar
- Karmaşık UI toolkit bağımlılıkları (Qt, WPF, SwiftUI)

Bu dürüst bir sınır — Y.U.K.İ kullanıcıya "Bu uygulama dönüştürülemez, container kullanmak ister misiniz?" demeli.

---

## 🔬 Y.U.K.İ Yetenek Genişletme Planı

### Hafıza ve Bağlam

```
Yerel Vector Database:
  ├── Kullanıcı komutları geçmişi (son 90 gün)
  ├── Sık kullanılan dosya yolları
  ├── Hata → çözüm çiftleri (öğrenilen)
  └── Uygulama davranış profilleri

Gizlilik garantisi:
  - Tüm veri /home/user/.yuki/ içinde
  - Buluta hiçbir şey gitmiyor
  - Kullanıcı istediği zaman siliyor
  - Şifreli (LUKS veya benzeri)
```

### Proaktif Sistem Bakımı

```
Y.U.K.İ arka planda sürekli izliyor:

Günlük:
  - Disk sağlığı (S.M.A.R.T değerleri)
  - Log hataları analizi
  - Güvenlik güncellemeleri

Haftalık:
  - Kullanılmayan paketler
  - Büyük geçici dosyalar
  - Şüpheli process davranışları

Aylık:
  - Sistem performans trendi
  - Depolama kullanım tahmini
  - "Şu an disk 3-6 ay içinde dolacak" uyarısı

Kullanıcı sabah açınca:
  "İyi sabahlar. Dün gece 3 güvenlik güncellemesi kuruldu.
   Disk kullanımı %78, 2 büyük log dosyası temizlenebilir (4.2GB).
   Her şey normal görünüyor."
```

### Sistem Hata Arkeolojisi

Bir crash veya donma sonrası Y.U.K.İ:

```
Kök neden analizi:
  ├── Kernel log analizi (dmesg)
  ├── Application log (journalctl)
  ├── Timeline oluşturma
  └── Benzer olaylarla pattern eşleştirme

Kullanıcıya:
  "Dün gece 23:47'deki donma şundan kaynaklandı:
   Firefox 14 sekme açıkken RAM doldu, swap tetiklendi,
   X server yanıt veremez hale geldi.
   
   Çözüm önerileri:
   1. Firefox sekme sayısı için uyarı kur (önerilir)
   2. Swap alanını artır (zaman alır)
   3. RAM yükselt (kalıcı çözüm)"
```

---

## 📊 Gerçekçi Başarı Senaryoları

### Senaryo 1: Akademik Başarı (En Olası, 2-3 Yıl)

YKNOS bir prototip olarak çalışıyor. Türkiye'de 2-3 üniversitede "İşletim Sistemleri" veya "Programlama Dilleri" derslerinde referans material. 500-1000 aktif kullanıcı, çoğu araştırmacı/öğrenci. GitHub'da 2000-5000 star. Küçük ama gerçek bir topluluk.

**Bu başarı mı?** Kesinlikle evet.

### Senaryo 2: Niş Endüstriyel Kullanım (Mümkün, 3-5 Yıl)

Embedded/IoT segmentinde gerçek kullanım: Endüstriyel kontrol sistemleri, edge computing, Raspberry Pi projeleri. "YKN ile yazılmış üretim sistemi" referansları oluşuyor. 50-100 şirket küçük projelerde kullanıyor.

**Bu başarı mı?** Büyük başarı.

### Senaryo 3: LLM-First OS Öncüsü (İdeal, 5-7 Yıl)

BitNet/benzeri modeller gerçekten olgunlaşıyor, on-device AI yaygınlaşıyor. YKNOS "AI-native OS" konseptini erken benimseyenler arasında yer alıyor. Akademik makaleler atıf yapıyor. Uluslararası hacker topluluğu tanıyor.

**Bu başarı mı?** Tarihsel öneme sahip başarı.

### Senaryo 4: Dağılma (Kaçınılması Gereken)

Dil bitmeden OS yazılmaya başlanıyor. OS bitmeden Y.U.K.İ'ye geçiliyor. Hiçbiri tamamlanmıyor. GitHub'da yarım kalan bir proje kalıyor.

**Bu nasıl önlenir?** Net milestone'lar: Dil V1 → LLM V1 → OS Alpha. Sıra bozulmaz.

---

*YKNOS_VISION.md — Son güncelleme: 3 Eylül 2026*  
*Bu belge yaşayan bir dokümandır — her büyük karar sonrası güncellenir.*
