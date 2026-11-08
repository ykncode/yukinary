# YknOS Bare-Metal QEMU GUI Prototipi (Arşiv)

Bu dizin, YknOS projesinin **Faz 1 Bare-Metal Prototipi**ni içerir.

## Özellikler:
- BGA (Bochs Graphics Adapter) 1024x768x32bpp TrueColor Framebuffer
- PS/2 Donanım Faresi ve Klavye Sürücüsü (Streaming packets & dirty-rect restore)
- Etkileşimli Pencere Sürükleme (Window Dragging)
- Dinamik Z-Order Sıralaması (`bring_to_front`)
- macOS/GNOME tarzı pencere kontrolleri (🔴 Kapat, 🟡 Küçült, 🟢 Büyüt)
- YknBrowser (Çoklu sekme, navigasyon, omnibox, canlı REST mikroservisleri)
- Y.U.K.I Core Nöral İşletim Sistemi Merkezi (5 Sekme: Genel, Arkeoloji, Bakım, Sandbox, Modeller)
- RamFS Dosya Gezgini ve yknsh Terminali
- Yüzen Akrilik Alt Dock (Floating Acrylic Dock)

## Çalıştırma:
```powershell
.\boot_qemu.ps1
```
veya
```cmd
boot_qemu.bat
```
