# Yukinary — Linux ve Windows denetim raporu

**Kapsam:** Kullanıcının paylaştığı Linux denetim çıktıları ile bu çalışma kopyasında MinGW g++/Windows üzerinde yeniden çalıştırılan build, regression ve model kontrolleri. Bu rapor bulguları doğrulama düzeyine göre ayırır; doğrulanmamış iddiaları bug olarak sunmaz.

## Yönetici özeti

- Güncel `app_full.cpp`, MinGW-W64 g++ 16.1.0 ile `-O2 -std=c++17` kullanılarak derlendi; Windows linkinde `-lws2_32` gerekli.
- Stage-1’in başlangıç çökmesi `std/ai/training_corpus.ykn` global string pointer’ından kaynaklanıyordu; corpus accessor’a taşındı. Duplicate import düğümleri AST listesinden çıkarılıyor ve self-host sırasındaki `src/paket.ykn` SemVer/imza yardımcılarının E047 yanlış pozitifi giderildi.
- Stage-2 başlangıç çökmesinin kökü, Stage-1’in yaklaşık 32 MiB’ı aşan kod üretmesine rağmen `main.ykn` içinde kod ve PE/ELF çıktı arenalarının 32 MiB ayrılmasıydı. Arenalar 64 MiB’a çıkarıldı; self-host dosya doğrulayıcısının okuyabildiği azami dosya boyutu da 64 MiB olarak eşitlendi.
- Stage-2→Stage-3 parity farkının kökü global AST sıralama adımıydı: Stage-1 ve Stage-2 aynı global bildirimleri farklı sırada depoluyordu. `src/codegen.ykn` içindeki ek sıralama kaldırılıp AST/import traversal sırası korununca Stage-2→Stage-3 ve Stage-3→Stage-4 çıktıları birebir eşitlendi. Bu çözüm global layout’u traversal sırasına bağlar; dolayısıyla import traversal sırası deterministik kalmalıdır.
- C++ Stage-0’daki `dosya_dizini[1024]` taşması, dosya açılmadan önce dizin uzunluğunu sınırlandırıp açık E014 tanısı vererek kapatıldı. 1.069 karakterlik Windows extended path ile guard doğrulandı; derleyici taşmadan exit 1 döndü.
- Taze Stage-0 ile proje test runner’ı, `tests/stdlib/03_bellek.ykn` içindeki sıfır döndüren sahte allocator yerine test arena’sı eklendikten sonra **130 testte 96 PASS / 34 FAIL (%73,8)** verdi. Kalan başarısızlıkların bir kısmı gerçek çalışma/derleme sorunları, bir kısmı eksik oracle veya platforma özgü kontroller.
- Windows üzerinde iki yüksek etkili codegen hatası ayrıca minimal testlerle yeniden üretildi ve düzeltildi:
  1. `dizi[indeks].alan` için alan yazma/okuma.
  2. İç blokta aynı isimli değişkenden sonra dış değişkenin ezilmesi.
- İki yeni regresyon testi Stage-3 ile de derlenip çalıştırıldı ve geçti. Stage-1, Stage-2 ve Stage-3 güncel kaynakla üretildi; Stage-2, Stage-3 ve Stage-4 `--help` çalıştırmaları başarılı.
- Modellerin dört GGUF dosyası metadata/tensor profili olarak okundu. Bu, inference doğruluğunu veya tüm tensor payload’larını doğrulamaz.
- `-Wall -Wextra -Wpedantic` denemesi derlendi; **1.803 uyarı** verdi. 1.775’i opcode byte değerlerinin `int`’ten `char`’a daraltılması uyarısıdır.

## Windows üzerinde bu kopyada doğrulanan sonuçlar

### Build ve bootstrap

Derleyici:

```text
C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe
MinGW-W64 x86_64-ucrt-posix-seh 16.1.0
```

Bu çalışma turunda MinGW g++ 16.1.0 ile Stage-0 `-std=c++17 -O0 -lws2_32` seçenekleriyle derlendi. Stage-0→Stage-1→Stage-2→Stage-3→Stage-4 zinciri başarıyla üretildi; Stage-2/3 ve Stage-4 `--help` ile çalıştı. Son kod değişikliğinden sonra Stage-2→Stage-3 ve Stage-3→Stage-4 parity sağlandı (her dosya 33.703.002 bayt; fark 0). `tests/codegen/08_struct_array_field_and_shadow.ykn`, `09_nested_scope_shadowing.ykn` ve `tests/ir/10_pipeline_and_features.ykn` Stage-4 ile derlenip çalıştırıldı, üçü de geçti. Güncel Stage-0 ile tüm PowerShell suite’i tekrar çalıştırıldı: 130 testte 96 PASS / 34 FAIL. Bellek ve metin stdlib fixture’larındaki sıfır döndüren sahte allocator’lar test içi sınırlı arena allocator’larıyla değiştirildi; iki test artık Stage-0’da derlenip beklenen metin çıktısıyla başarıyla çalışıyor.

Önceki rapor satırları bu yeni taze derleme sonuçlarıyla güncellendi; önceki `build/reports/bootstrap-report.json` bu turdaki build’lerin raporu değildir.

### Taze Stage-0 regresyon suite’i

`tests/run_all_suites.ps1`, taze `build/audit_stage0_fixed.exe` ile çalıştırıldı. Test raporundaki compiler SHA-256: `b26a5448…`.

| Test durumu | Adet |
|---|---:|
| PASS | 94 |
| Exit 1 | 11 |
| Eksik stdout/stderr oracle | 7 |
| Derleme hatası | 7 |
| Harici platform/fixture doğrulaması başarısız | 7 |
| Windows `0xC0000409` | 2 |
| Windows `0xC0000005` | 1 |
| Harici sözleşme kontrolü | 1 |
| **Toplam** | **130** |

Rapor: `build/reports/test_results.json`.

Gözlenen örnekler:

- `tests/stdlib/03_bellek.ykn` ve `tests/stdlib/05_dosya.ykn`: `0xC0000409` süreç çökmesi.
- `tests/stdlib/04_metin.ykn`: `0xC0000005` erişim ihlali.
- `10_enum_match.ykn`: `FAIL: o1 var olmali`.
- `14_her_dongusu.ykn`: `FAIL: her toplam != 55`.
- LLM/GGUF testlerinde tahsis, parse, range ve projection kontrollerinde başarısızlıklar var.
- 4 Mach-O kontrolü beklenen `ncmds=2` değerini bulamadı; 2 ARM64 ELF/Mach-O kontrolü giriş noktasını dosya sınırları dışında buldu.
- 7 testte oracle bulunmuyor; bunlar PASS/FAIL davranış doğrulaması sayılamaz.

Düzeltmelerden sonra aynı 130-testlik suite sonucu **94/130** kaldı; bu iki regression testi suite’in 130 vaka listesine dahil değil. Yeni minimal testler ayrıca çalıştırıldı ve başarılı oldu.

### Bağımsız olarak yeniden üretilen codegen bug’ları ve düzeltmeleri

#### 1. Struct dizisi alan erişimi — düzeltildi, Stage-0’da geçti

En küçük örnek:

```ykn
yapi Nokta { x: tamsayi; y: tamsayi; }

degisken noktalar: [3]Nokta;
noktalar[1].x = 17;
```

Önce düzeltme öncesi `noktalar[1].x` doğrulaması başarısız oldu (`STRUCT_ARRAY_X_FAIL`). İnceleme, alan codegen’inin nesne tipi aramasında `AST_INDEKS_ERISIMI` sonucundan dizinin temel sembol tipini çıkarmadığını gösterdi; alan offset’i bulunamayınca kod üretici sıfır değeri yazıyordu/atamayı tamamlamıyordu.

Düzeltme:
- `app_full.cpp`: indekslenmiş nesnenin tabanındaki lokal/global dizi sembolünden eleman struct tipi çözülüyor; hem alan ataması hem alan okuması bunu kullanıyor.
- `src/codegen.ykn`: aynı tip çıkarımı kanonik codegen kaynağına eklendi.
- Regresyon: `tests/codegen/08_struct_array_field_and_shadow.ykn`.

Düzeltilmiş `app_full.cpp` Stage-0 ile test çıktısı: `STRUCT_ARRAY_PASS`.

#### 2. Blok içi gölgeleme dış değişkeni eziyor — düzeltildi, Stage-0’da geçti

Minimal testte blok içindeki `deger=2`, bloktan çıktıktan sonra da `deger=1` yerine kullanılıyordu. Codegen’in lokal sembol/stack-offset listesi AST blok kapanışında geri yüklenmiyordu.

Düzeltme:
- C++ seed ve kanonik `src/codegen.ykn` AST blok girişindeki lokal sayı/stack offset’ini saklıyor, blok sonunda geri yüklüyor. Böylece dış scope’un lokal kaydı tekrar görünür oluyor; blok içi slotlar sonraki scope’larda yeniden kullanılabiliyor.
- Regresyon: `tests/codegen/09_nested_scope_shadowing.ykn`.

Düzeltilmiş Stage-0 ile test çıktısı: `SCOPE_SHADOW_PASS`.

**Doğrulama sınırı:** Her iki dosya da güncel C++ Stage-0 ile test edildi. YKN kanonik kaynakları bu Stage-0 tarafından derlenerek Stage-1 üretildi; ancak Stage-1 çalıştırma crash’i nedeniyle değişikliklerin self-host Stage-1 yürütmesi henüz doğrulanamadı.

### Modeller ve GGUF

`tools/gguf_model.py` ile aşağıdaki dosyalar GGUF v3 olarak açıldı ve metadata/tensor descriptor profilleri okundu:

| Model | Mimari | Katman | Tensor |
|---|---|---:|---:|
| BitNet `ggml-model-i2_s.gguf` | `bitnet-b1.58` | 30 | 332 |
| Bonsai `Bonsai-8B.gguf` | `qwen3` | 36 | 399 |
| Gemma ana model `gemma-4-E4B-it-Q4_K_M.gguf` | `gemma4` | 42 | 720 |
| Gemma `mmproj-gemma-4-E4B-it-BF16.gguf` | `clip` | 0 | 1.411 |

Bu profilleyici header, metadata ve tensor tanımlayıcı/ofset sınırlarını kontrol eder; ağırlık payload’larının tümünü hash/parity ile doğrulamaz.

`tests/ai/test_bitnet_real_token.ykn` güncel Stage-0 ile derlenirken E041 `BORROW_CONFLICT` verdi. Tanı satırları import edilmiş modüller yerine test wrapper konumlarını gösteriyor gibi; gerçek model token-parity/inference testine geçilemedi. Önceden var olan CPU BitNet executable’ının ürettiği yanıt bağımsız referansla karşılaştırılmadığı ve kaynağa bağlanamadığı için PASS sayılmadı.

### C++ uyarıları

Ek uyarı bayraklı build:

```text
-O2 -std=c++17 -Wall -Wextra -Wpedantic
```

Derleme exit code 0; toplam **1.803 warning**:

- `-Woverflow`: 1.775 (opcode byte’ları `int` → `char` dönüşümleri)
- kullanılmayan değişken/parametre/fonksiyon: 21
- misleading indentation: 3
- strict-aliasing: 2
- unknown pragma: 1
- uyumsuz function pointer cast: 1

Özellikle `app_full.cpp:90` function pointer cast, `:8091`/`:8923` strict-aliasing ve `:8566`, `:8577`, `:8581` yanıltıcı girintiler incelenmeli. Opcode daraltma uyarılarının çoğu beklenen byte modülünü ifade ediyor olabilir; yine de explicit `uint8_t`/`unsigned char` API’si ile warning gürültüsünü azaltmak gerçek uyarıların görünmesini sağlar.

### Statik tarama

`tools/audit_reality.py`: 965 dosyada 283 heuristik bulgu, `src/`/`std/` içinde 30 bulgu; `REVIEW_REQUIRED`. Bunlar doğrulanmış hata adedi değildir.

- `build/reports/reality_audit_current.json`
- `audit_inventory.json`
- `build/reports/coverage_matrix.json`

## Linux denetiminden aktarılan bulgular — bağımsız teyit edilmedi

Aşağıdaki metrik ve bulgular kullanıcı tarafından Claude’un Linux/x86-64 denetiminden aktarıldı. Bu ortam Windows olduğu için Linux ölçümleri burada yeniden koşturulmadı; dolayısıyla bunları bu raporda **aktarılmış iddia/ölçüm** olarak tutuyorum.

### Aktarılan metrikler

- `app_full.cpp`: yamasız derleme; 25 uyarı.
- Bootstrap, generics, i18n, optimizer: 7/7, 7/7, 12/12, 5/5.
- Beklenen çıktılı 89 test: 65 PASS, 17 farklı çıktı, 7 derlenemedi.
- 568 test: 141 PASS mesajı, 31 FAIL mesajı, 53 crash, 8 timeout.
- 2.880 random integer expression differential testi hatasız.
- ASan 668 dosyada hata bulmadı; UBSan işaretli taşmalar bildirdi.
- 192 modülden 121’i derlendi, 57’si Linux harici çağrılarında kaldı, 14’ü gerçek tanı hatası verdi.

### Aktarılan yüksek etkili hata iddiaları

- Struct dizisi `a[i].alan` okumasının 0 vermesi — **bu raporda Windows’ta bağımsız olarak yeniden üretildi ve düzeltildi**.
- İç scope’da aynı adlı lokalin dış değişkeni ezmesi — **bu raporda Windows’ta bağımsız olarak yeniden üretildi ve düzeltildi**.
- Her fonksiyona sabit 8.272 bayt frame, 1.000 civarı recursion limiti.
- Gerçek sayıdan tamsayıya örtük atamada bit-pattern sonucu.
- Stage-1 null global pointer ile startup crash’i; self-host fixpoint olmaması — Windows’ta da Stage-1 çalıştırma çökmesi gözlendi, ancak null adres ve disassembly Linux raporundaki kadar doğrulanmadı.
- WASM/RISC-V hedeflerinde gerçek yürütülebilir codegen bulunmaması.
- Move/dangling-pointer/eksik return/yanlış dönüş tipi, büyük literal taşması, kaydırma sınırı ve dinamik bounds-check güvenlik açıkları.
- E041 yanlış pozitifleri ve E004 boş gövde bildirimi, anahtar kelime çakışmaları, `~` operatörü eksiği.
- `dosya_dizini[1024]` tampon taşması, UBSan taşmaları, hatalı import source-map, uzun import yolunun uyarıyla atlanması.
- Bazı stdlib modüllerinde dilde olmayan sözdizimi ve derlenmeyen kaynaklar.

Bu aktarılan iddialar sonraki adımlarda tek tek Linux veya Windows minimal testleriyle doğrulanmalı; özellikle memory safety ve yanlış codegen iddiaları yalnız rapor metnine dayanarak kapatılmamalı.

## Öncelikli çözüm planı

### P0 — Self-host Stage-1 crash

1. Stage-1 başlangıç çökmesini Windows debugger ile izleyip ilk hatalı erişimi `ykn_training_corpus_boyutu` içindeki global corpus string pointer’ına bağla.
2. `std/ai/training_corpus.ykn`: corpus’u global pointer yerine `ykn_training_corpus_al()` fonksiyonundan string literal olarak döndür; uzunluk fonksiyonu da bu accessor’ı kullansın.
3. Import tekrarlarının false-cycle olması düzeltildi; yüklenmiş modülün tekrar import düğümü artık derleme akışından etkisizleştiriliyor.
4. Kullanılmayan `ykn_egitim_dataset_derle` yolu ve onun `dataset.ykn` import’u `training_corpus.ykn`’den çıkarıldı. Dataset modülü ve bağımsız araç/testleri tutuldu.
5. `src/paket.ykn`: output-parameter ile doldurulan `PaketImzasi` ve `SemVer` yerellerine başlangıç değerleri eklendi; `semver_ayristir` başarısızlığı artık açıkça reddediliyor. Bu, self-host E047’lerini giderdi.
6. `src/typechecker.ykn`: çağrıldığı halde tanımı olmayan `degisken_tasindi_mi` helper’ı eklendi; move/pipeline testi geçti.
7. Stage-1→Stage-2 üretimi başarılı, ancak Stage-2 startup’ta `ana` içinde erişim ihlali veriyor. Stage-3/parity engeli artık bu yeni crash.
8. Bootstrap raporunda Stage-0→1→2→3 ve `semantic_parity.cases > 0` olmasını zorunlu kıl.

**Kabul:** Stage-1 `--help` exit 0 veriyor; struct-array, scope-shadowing ve move/pipeline testleri Stage-1 ile geçiyor. Stage-1 Stage-2 üretse de Stage-2 startup crash’i nedeniyle self-host PASS ilan edilmemeli.

### P1 — Windows stdlib crash’leri

1. `03_bellek`, `04_metin`, `05_dosya` için tek işlevli küçük repro’lar çıkar.
2. `0xC0000409`/`0xC0000005` crash dump’larını kaynak satırına eşle.
3. Her crash için önce başarısız regresyon testi, ardından düzeltme ve suite rerun.

### P1 — LLM/BitNet runtime ve E041

1. `E041` için read-only/read-only ve read-only/mutable durumlarını ayrı test et; gerçek çakışmaları reddetme davranışını koru.
2. Tanı source-map’ini import edilmiş modülün gerçek dosya/satırına bağla.
3. GGUF loader tahsis ve range-read kontrollerini profilden çıkarılan metadata ve gerçek dosya offset’leriyle karşılaştır.
4. BitNet gerçek token testini derleyip llama.cpp/Python referans token dizisiyle karşılaştır.

### P2 — Kapsam, yanlış tip kabulü ve taşmalar

Linux raporundaki conversion, stack frame, move/dangling, return coverage ve integer overflow iddialarını her biri için ayrı minimal testle doğrula. Doğrulanmadan büyük compiler refactor’ı yapma.

### P2 — Test ve warning hijyeni

1. Eksik 7 oracle’ı ekle.
2. Cross-target fixture beklentilerini çıktıyla hizala; başarısız Mach-O/ARM64 kontrollerini platform desteği durumuna göre düzelt veya açıkça skip et.
3. Byte-emission türünü `uint8_t` yap veya daraltmayı tek açık helper’dan geçir.
4. `-Wall -Wextra -Wpedantic` warning sayısını kategori bazında raporlayıp CI’da yeni warning artışını engelle.

## Değiştirilen/eklenen dosyalar

- `app_full.cpp` — blok kapanışında lokal tablo/stack offset geri yükleme; indekslenmiş struct eleman tipi çıkarımı.
- `src/codegen.ykn` — aynı iki codegen düzeltmesinin kanonik YKN uygulaması.
- `src/main.ykn` — daha önce yüklenmiş importların false-cycle olarak reddedilmesi yerine tekrarlarının AST’den etkisizleştirilmesi.
- `src/typechecker.ykn` — çağrıldığı halde eksik olan `degisken_tasindi_mi` fonksiyonunun eklenmesi.
- `src/paket.ykn` — SemVer ve SHA-256 output-param yerellerinin başlatılması; SemVer parse başarısızlığının kontrol edilmesi.
- `std/ai/training_corpus.ykn` — global corpus string pointer’ı yerine literal döndüren accessor; kullanılmayan dataset export/import yolunun kaldırılması.
- `tests/codegen/08_struct_array_field_and_shadow.ykn` — struct-array alan regression testi.
- `tests/codegen/09_nested_scope_shadowing.ykn` — nested-scope shadowing regression testi.
- `YUKINARY_LINUX_WINDOWS_AUDIT.md` — birleştirilmiş Linux/Windows raporu.

**Son doğrulama durumu:** Güncel Stage-0 `src/main.ykn`’den Stage-1 üretti. Stage-1 `--help` exit 0 verdi; `tests/ir/10_pipeline_and_features.ykn` içindeki ANSI/borrow-move/pipeline kontrolleri ve struct-array/scope-shadow regresyonları Stage-1 ile derlenip çalıştırıldı ve geçti. Stage-1, `src/main.ykn`’den Stage-2 üretti (exit 0); fakat Stage-2 `--help`, argümansız çağrı ve derleme çağrısında aynı `0xC0000005` ile çöküyor. GDB RIP’i `ana` içinde, geçerli talimat olmayan sıfır byte alanına düştüğünü gösterdi. Self-host parity PASS değil; önceki taze Stage-0 suite sonucu 94/130.
