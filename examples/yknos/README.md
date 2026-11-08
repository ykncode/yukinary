# YknOS: Linux-Compatible Dual-Target Operating System Showcase
# YknOS: Linux-Uyumlu Çift Hedefli İşletim Sistemi Gösterimi (Bare-Metal QEMU + Host Native)

[Türkçe](#türkçe-özet) | [English](#english-summary)

---

## Türkçe Özet

**YknOS**, **Yukinary** programlama dili ile geliştirilmiş, harici C, C++ veya Rust çalışma zamanı bağımlılığı bulunmayan, gerçek bir Linux/Unix benzeri alt sistem mimarisine sahip hibrit bir işletim sistemi ve masaüstü arayüzüdür.

YknOS, iki farklı çalışma hedefini (Target) aynı çatı altında birleştirir:

1. **Bare-Metal QEMU Hedefi (Ring-0 Multiboot1 Kernel)**:
   - Doğrudan QEMU sanal makinesinde sıfırdan önyüklenir (altında Windows veya Linux ana işletim sistemi yoktur).
   - Gerçek fiziksel `0xB8000` VGA video belleğine grafiksel masaüstü arayüzü (Desktop GUI) çizer.
   - **Sekmeler (Tabs)**: `1: Dosyalar`, `2: SQL`, `3: Ag`, `4: Terminal` arasında anında geçiş.
   - **Başlat Menüsü**: `b` tuşu veya fare tıklaması ile açılan açılır menü.
   - **Linux VFS Dosya Yöneticisi**: `/etc/os-release`, `/etc/passwd`, `/var/log/dmesg.log`, `/home/root/notlar.txt` dosyaları arasında gezinme (`j`/`k`) ve canlı önizleme.
   - **Tıklanabilir Fare İmleci (Interactive Mouse)**: `w`, `a`, `s`, `d` tuşları ile hareket eden ve `Space` / `t` tuşu ile butonlara ve menülere tıklayabilen imleç (`^`).
   - 16550 UART COM1 (`0x3F8`) donanım seri portu üzerinden terminale çekirdek günlükleri (`dmesg`) akıtır.
   - PS/2 Klavye Denetleyicisi (Port `0x64` ve `0x60`) donanım seviyesinde tuş tarama kodlarını okur. `TAB` tuşuna basarak GUI Navigasyon Modu ile `root@yknos:/# ` komut satırı arasında geçiş yapılabilir.

2. **Host Yerel Etkileşimli Kabuk Hedefi (Native Windows / Linux PE & ELF)**:
   - Doğrudan `ykn.exe` ile 64-bit yerel ikiliye dönüştürülür (`yknos.exe`).
   - Gerçek Linux dosya ağacı hiyerarşisi (`/bin`, `/etc`, `/proc`, `/dev`, `/home/root`, `/var/log`).
   - Dinamik sanal `/proc` dosya sistemi (`cat /proc/cpuinfo`, `cat /proc/meminfo`, `cat /proc/version`, `cat /proc/uptime`).
   - Gerçek metin editörü: `nano <dosya>` veya `edit <dosya>` (Satır satır düzenleme, `:w` kaydetme, `:wq` kaydedip çıkma).
   - Süreç yönetimi: `ps aux`, `top`, `spawn <komut>`, `kill <pid>`.
   - Bellek ve disk raporları: `free -m`, `df -h`.
   - Çekirdek komut seti: `uname -a`, `uptime`, `whoami`, `hostname`, `date`, `pwd`, `cd`, `ls -la`, `mkdir`, `touch`, `rm`, `echo`, `grep`, `wc`, `env`, `export`, `history`.
   - Gömülü SQL motoru (`sql hepsi`, `sql ekle`) ve gömülü HTTP 1.1 JSON ağ yığını (`http durum`, `http sunucu` Port 8080).
   - Masaüstü GUI arayüzü (`masaustu` veya `gui`).

---

## Mimari Diyagramı / Architecture Diagram

```mermaid
flowchart TD
    subgraph Hedef1 ["Hedef 1: QEMU Bare-Metal (Ring-0)"]
        MB["Multiboot 1 (0x1BADB002)"]
        VGA_HW["Fiziksel VGA 0xB8000 (Desktop GUI & Konsol)"]
        PS2_HW["PS/2 Klavye Port 0x60/0x64 (TAB/WASD/Space)"]
        COM1_HW["COM1 0x3F8 UART 38400 Baud"]
        BM_GUI["Bare-Metal GUI (Sekmeler + Fare Tiklamasi + Baslat)"]
        BM_SHELL["root@yknos:/# Linux Komut Yorumlayici"]

        MB --> BM_GUI
        BM_GUI <--> VGA_HW
        BM_GUI <--> PS2_HW
        BM_GUI --> BM_SHELL
        BM_SHELL --> COM1_HW
    end

    subgraph Hedef2 ["Hedef 2: Host Yerel Linux-Uyumlu Sistem"]
        YKN_COMP["Yukinary Derleyicisi (ykn.exe)"]
        YKN_EXE["yknos.exe (64-bit Yerel İkili)"]
        LINUX_VFS["Linux VFS (/bin, /etc, /proc, /home, /var)"]
        PROC_FS["Dinamik /proc (cpuinfo, meminfo, version)"]
        NANO_ED["Gömülü Metin Editörü (nano / edit)"]
        PROC_MGR["Süreç Yöneticisi (ps aux, top, kill, spawn)"]
        LIVE_SHELL["root@yknos:<cwd># Etkileşimli Kabuk"]
        SQL_CORE["Gömülü SQL Motoru (Tablolar & Agregasyon)"]
        HTTP_CORE["Gömülü HTTP 1.1 REST Motoru (Port 8080)"]

        YKN_COMP --> YKN_EXE
        YKN_EXE --> LIVE_SHELL
        LIVE_SHELL <--> LINUX_VFS
        LINUX_VFS <--> PROC_FS
        LIVE_SHELL <--> NANO_ED
        LIVE_SHELL <--> PROC_MGR
        LIVE_SHELL <--> SQL_CORE
        LIVE_SHELL <--> HTTP_CORE
    end
```

---

## Nasıl Çalıştırılır? / How to Run

### 1. QEMU Bare-Metal Sanal Makinesinde Başlatma (Masaüstü Arayüzü & Çekirdek)

Bu komut, çekirdeği NASM ile anında derler ve QEMU sanal makine penceresini açar:

```powershell
# PowerShell:
.\examples\yknos\boot_qemu.ps1

# Windows CMD:
examples\yknos\boot_qemu.bat
```

* Açılan mavi renkli QEMU penceresinde:
  - `1`, `2`, `3`, `4`: Sekmeler arasında geçiş yapın (1: Dosyalar, 2: SQL, 3: Ağ, 4: Terminal).
  - `b`: Başlat Menüsü penceresini açıp kapatın.
  - `w`, `a`, `s`, `d`: Fare imlecini (`^`) hareket ettirin.
  - `Space` veya `t`: İmlecin altındaki buton veya sekmeye tıklayın!
  - `j`, `k`: Dosya Yöneticisinde dosyalar arasında gezinip önizlemelerini okuyun.
  - `TAB`: GUI Modu ile `root@yknos:/# ` komut satırı arasında geçiş yapın. Komut modundayken `uname -a`, `uptime`, `whoami`, `ps aux`, `top`, `free -m`, `cat /proc/cpuinfo`, `cat /etc/os-release`, `gui`, `exit` yazabilirsiniz.
* Aynı anda ana terminalinizde COM1 seri portundan akan canlı çekirdek loglarını görebilirsiniz.

### 2. Host Üzerinde Canlı Linux-Uyumlu Kabuğu Başlatma

Yukinary ile derlenmiş yerel ikiliyi çalıştırmak için:

```powershell
# PowerShell:
.\examples\yknos\run_demo.ps1

# Windows CMD:
examples\yknos\run_demo.bat
```

Açılan `root@yknos:/# ` kabuğunda kullanabileceğiniz komutlar:
- `uname -a`              : Çekirdek, derleme ve mimari bilgileri
- `uptime`                : Çalışma süresi ve yük ortalamaları
- `whoami`                : Geçerli kullanıcı (`root`)
- `hostname`              : Makine adı (`yknos-box`)
- `pwd`                   : Mevcut çalışma dizini
- `cd <dizin>`            : Dizin değiştirme (`cd /etc`, `cd /home/root`, `cd ..`)
- `ls`, `ls -la`          : İzinler, boyut ve sahiplikle dizin listeleme
- `cat <dosya>`           : Dosya içeriğini okuma (`cat /etc/os-release`, `cat /etc/passwd`, `cat /var/log/dmesg.log`)
- `cat /proc/cpuinfo`     : Canlı üretilen sanal CPU bilgisi
- `cat /proc/meminfo`     : Canlı üretilen sanal bellek bilgisi
- `cat /proc/version`     : Çekirdek sürüm bilgisi
- `nano <dosya>`          : Gerçek metin editörü (satır ekleme, `:w` kaydetme, `:wq` çıkış)
- `touch <dosya>`         : Yeni dosya oluşturma
- `mkdir <dizin>`         : Yeni dizin oluşturma
- `rm <dosya>`            : Dosya silme
- `grep <kelime> <dosya>` : Dosya içinde arama yapma
- `wc <dosya>`            : Satır, kelime ve bayt sayısı hesaplama
- `ps aux`                : Çalışan tüm süreçlerin listesi
- `top`                   : Canlı CPU%, bellek, görev tablosu ve uptime
- `spawn <komut>`         : Yeni arka plan süreci başlatma (PID üretimi)
- `kill <pid>`            : Süreci sonlandırma (SIGKILL)
- `free -m`               : Bellek istatistikleri (Total, Used, Free, Buff/Cache, Available)
- `df -h`                 : Disk ve sanal dosya sistemi kullanım oranları
- `env`                   : Ortam değişkenleri tablosu
- `dmesg`                 : Çekirdek açılış günlükleri
- `history`               : Son çalıştırılan komutlar
- `masaustu` / `gui`      : Grafiksel masaüstü ve tıklanabilir fare arayüzü
- `sql hepsi`             : Gömülü ilişkisel SQL veritabanı sorguları
- `http sunucu`           : Port 8080'de gerçek TCP dinleyicisi başlatma
- `exit` / `shutdown`     : Oturumu sonlandırma

---

## English Summary

**YknOS** is a comprehensive Linux-compatible dual-target operating system and desktop environment written in pure **Yukinary**:

1. **Bare-Metal QEMU Kernel**:
   - Boots directly via Multiboot1 (`0x1BADB002`) into 32-bit protected mode.
   - Renders a complete **Graphical Desktop Environment** onto physical VGA `0xB8000` text memory with clickable mouse support (`Space` / `t`).
   - Features **Tabs** (`1: Files`, `2: SQL`, `3: Network`, `4: Terminal`), popup **Start Menu**, and live file previews.
   - Hardware PS/2 keyboard processing with instant `TAB` toggling between GUI navigation and `root@yknos:/# ` command line.
   - Streams live kernel logs over 16550 UART COM1 serial port (`0x3F8`).
2. **Host Linux-Compatible Subsystem & Shell**:
   - Compiles directly to native 64-bit executable via `ykn.exe`.
   - Hierarchical Linux VFS (`/bin`, `/etc`, `/proc`, `/dev`, `/home/root`, `/var/log`).
   - Dynamic `/proc` pseudo-filesystem (`/proc/cpuinfo`, `/proc/meminfo`, `/proc/version`, `/proc/uptime`).
   - Built-in text editor (`nano <file>` / `edit <file>`) with `:w` save and `:wq` exit.
   - Process subsystem: `ps aux`, `top`, `spawn <cmd>`, `kill <pid>`.
   - Coreutils: `uname -a`, `uptime`, `whoami`, `hostname`, `date`, `pwd`, `cd`, `ls -la`, `mkdir`, `touch`, `rm`, `echo`, `grep`, `wc`, `free -m`, `df -h`, `env`, `history`, `dmesg`.
   - Embedded relational SQL and HTTP 1.1 REST microservice on port 8080.
