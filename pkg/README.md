# Yerel YKN paketleri

Görev 9.4 paket yöneticisi taslağı, ağ veya registry kullanmadan proje içindeki
paketleri çözer. Bir paket şu biçimde tutulur:

```text
pkg/<isim>/mod.ykn
```

`mod.ykn` yerine geriye dönük uyumluluk için `pkg/<isim>/<isim>.ykn` veya
`pkg/<isim>/main.ykn` da kullanılabilir.

## Kullanım

Önce paket dizinini oluşturun ve modül kaynağını ekleyin:

```text
pkg/sayac/mod.ykn
```

Proje kökünde:

```powershell
.\yukinary.exe add sayac
.\yukinary.exe build
.\build\ornek.exe
```

`add` komutu `yukinary.toml` dosyasını oluşturur veya günceller ve şu kaydı
ekler:

```toml
[dependencies]
sayac = "pkg/sayac"
```

Manifestteki `main` alanı giriş YKN dosyasını, `output` alanı üretilen native
binary yolunu belirtir:

```toml
[package]
name = "ornek"
version = "0.1.0"
main = "main.ykn"
output = "build/ornek.exe"

[dependencies]
sayac = "pkg/sayac"
```

Bu taslak yalnızca yerel `pkg/` kaynaklarını destekler; ağ erişimi, registry,
versiyon çözümleme ve lockfile henüz kapsam dışıdır.
