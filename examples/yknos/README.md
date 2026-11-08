# YknOS: AI-First Operating System & Userspace Architecture
# Linux LTS Kernel + YKN Userspace + Y.U.K.I Core Neural OS Assistant

[Türkçe](#türkçe-özet) | [English](#english-summary) | [Vizyon Belgesi (VISION.md)](VISION.md)

---

## Türkçe Özet

**YknOS**, **Yukinary** programlama dili ile geliştirilmiş, harici C/C++ bağımlılığı bulunmayan, Linux LTS çekirdeği üzerinde çalışan yerel kullanıcı alanı (YKN Userspace) ve yapay zeka merkezli (**AI-First**) bir işletim sistemi mimarisidir.

### 🌟 Temel Vizyon & Felsefe (AI-First)
YknOS mimarisinde yapay zeka sonradan eklenmiş bir sohbet botu değil, sistemin birincil etkileşim katmanıdır:
- **"Başlangıç menüsü yerine AI"**: Kullanıcı işletim sistemiyle karmaşık menüler veya ezber komutlar yerine doğal dille konuşur.
- **Y.U.K.I Core**: Kullanıcı alanı düzeyinde çalışan, sıfır bulut bağımlılığı olan, 100% yerel çalışan nöral işletim sistemi asistanı.
- **Dürüst Mühendislik Prensibi**: LLM sihirli bir kutu değildir. Bir kullanıcı alanı süreci çekirdek çökmesini (Hard Kernel Panic) tamir edemez. Sınırlarımızı net biliyor ve güvenliği her şeyin üstünde tutuyoruz.

---

## 4 Yetenek Katmanı (Capability Layers)

| Katman | Seviye | Açıklama & İşlev |
| :--- | :--- | :--- |
| **Katman 1** | CLI & Tanılama | `yknsh` entegrasyonu, log analizi (`dmesg`, `journalctl`), servis izleme ve komut önerisi/otomatik düzeltme. |
| **Katman 2** | FFI & Güvenlik | C ABI / Syscall wrapper, dosya sistemi olayları (inotify), ağ anomali tespiti ve izin denetimi. |
| **Katman 3** | Proaktif Bakım | Davranış tahmini, disk/kaynak optimizasyonu, periyodik sağlık raporları ve yerel vektör belleği. |
| **Katman 4** | Dürüst Sınırlar | Güvenli fallback, insan onayı gerektiren kritik eylemler, çekirdek seviyesi ayrımı. |

---

## 5 Seviyeli Sandbox Güvenlik Modeli

YknOS, yetkileri katı biçimde izole eden 5 seviyeli bir güvenlik hiyerarşisi uygular:

```
+---------------------------------------------------------------+
|  Seviye 0: Kernel (Linux LTS, Ring-0, Dokunulmaz Çekirdek)    |
|  - Sürücüler, MMU Sayfalama, Ağ Yığını, Sistem Çağrıları      |
+---------------------------------------------------------------+
|  Seviye 1: Y.U.K.I Core (Tam Yetki + Zorunlu Audit Log)       |
|  - Nöral sistem analizi, hata kurtarma, /home/root/.yuki/     |
+---------------------------------------------------------------+
|  Seviye 2: YKN Sistem Servisleri (Capability-Based Yetki)     |
|  - ykn-init (PID 1), ykn-sql-engine, ykn-httpd (Port 8080)   |
+---------------------------------------------------------------+
|  Seviye 3: Kullanıcı Uygulamaları (İzole Sandbox, Açık İzin)  |
|  - Ağ: Varsayılan KAPALI | Dosya: ~/documents/<uygulama>/     |
|  - Kamera / Mikrofon / Donanım: Her kullanımda kullanıcı onayı|
+---------------------------------------------------------------+
|  Seviye 4: LLM Tarafından Üretilen Kod (En Kısıtlı Sandbox)   |
|  - Sıfır sistem erişimi, çalıştırmadan önce çift onay zorunlu |
+---------------------------------------------------------------+
```

---

## Öne Çıkan Sistem Yetenekleri

1. **Sistem Hata Arkeolojisi (`yuki arkeoloji`)**:
   - Sistem çöküşlerinde veya arızalarda `dmesg` ve olay zaman çizelgesini (timeline) geriye doğru tarayarak donanım/bellek kök neden analizi sunar.
2. **Proaktif Bakım & Sağlık Kontrolleri (`yuki bakim`)**:
   - Günlük S.M.A.R.T, RAMFS ve log denetimi; haftalık log rotasyonu ve zombi süreç temizliği; aylık kaynak trend tahmini.
3. **100% Yerel Vektör Belleği (`yuki hafiza`)**:
   - `/home/root/.yuki/vector_memory.db`: SIFIR bulut, SIFIR telemetri. Kullanıcının terminal alışkanlıklarını ve geçmiş çözümleri cihaz üzerinde tutar.
4. **Uygulama Uyumluluk Analizi (`yuki analiz <exe/apk>`)**:
   - Windows PE ikililerini ve Android Waydroid APK paketlerini çalıştırmadan önce API çağrıları, registry ve ağ gereksinimleri yönünden statik güvenlik taramasından geçirir.
5. **Gömülü Nöral Modeller**:
   - **BitNet 1.58b Mikro-LLM**: 16,384 parametre, ternary {-1, 0, +1} ağırlıklar, sıfır-çarpma ALU, 28 KB RAM ayak izi.
   - **Prism ML Bonsai-8B**: 399 tensörlü, 1.15 GB GGUF v3 disk eşlemeli gelişmiş model.

---

## Çift Hedefli Çalışma Mimarisi (Dual-Target)

### 1. Bare-Metal Multiboot1 Kernel (NASM + QEMU x86_64)
- 8259 PIC yeniden haritalama, 256-girdili IDT ve 100 Hz PIT timer tick donanım kesmeleri.
- 16550 UART COM1 seri port konsolu (çift yönlü TX/RX).
- `0xB8000` VGA fiziksel video belleğinde çalışan grafiksel masaüstü ortamı (Desktop GUI, pencereler, fare imleci, başlat menüsü).

### 2. Host Yerel İnteraktif Kullanıcı Alanı (Native Host PE / ELF)
- Saf Yukinary (`ykn.exe`) ile derlenen 64-bit bağımsız ikili (`yknos.exe`).
- Windows Konsolunda fiziksel donanım faresi desteği (`ENABLE_MOUSE_INPUT`).
- RamFS sanal dosya sistemi, dinamik `/proc` tablosu, gömülü metin editörü (`nano`/`edit`), süreç yöneticisi (`ps aux`, `top`), dahili SQL motoru ve HTTP 1.1 REST mikroservisi.

---

## Hızlı Başlangıç & Derleme

### Bağımsız Sistem Olarak Derleme ve Çalıştırma:

```powershell
# 1. Host Yerel Sistemini Derleme ve Başlatma:
.\build.ps1
.\run_demo.ps1

# veya Windows CMD ile:
build.bat
run_demo.bat
```

### Bare-Metal QEMU Sanal Makinesinde Başlatma:

```powershell
# QEMU ile Bare-Metal Başlatma:
.\boot_qemu.ps1

# veya Windows CMD ile:
boot_qemu.bat
```

### Y.U.K.I Core Komutları:
```bash
root@yknos:/# yuki arkeoloji          # Çekirdek hata kök neden analizi
root@yknos:/# yuki bakim              # Proaktif sistem sağlığı ve trend tahmini
root@yknos:/# yuki sandbox            # Güvenlik izinleri ve seviye tablosu
root@yknos:/# yuki hafiza             # Yerel vektör belleği durumu
root@yknos:/# yuki analiz app.exe     # PE / APK güvenlik davranış analizi
root@yknos:/# yuki sistem sagligi     # Doğal dille nöral asistan sorgusu
```

---

## Lisans & Telif Hakkı

Copyright (c) 2026 Berkan Öztürk (nyxsiz0). Tüm hakları saklıdır.
YknOS mimarisi ve vizyonu [VISION.md](VISION.md) belgesinde detaylandırılmıştır.
