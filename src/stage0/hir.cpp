#include "ykn_std.h"
// ============================================================

// Yukinary Yüksek Seviye Ara Temsil (HIR) Veri Yapıları

// ============================================================

// AST'den daha düşük, ancak donanım spesifikasyonlarından (MIR) daha

// yüksek seviyeli bir ara dildir (IR). 

// AST gibi bir Arena (liste) içinde düğümler (HirDugum) ile temsil edilir.


// -- HIR İşlem Tipleri (Opcode'lar) --

    auto HIR_ATAMA = 0;
    auto HIR_IKILI_OP = 1;
    auto HIR_CAGRI = 2;
    auto HIR_DONDUR = 3;
    auto HIR_EGER = 4;
    auto HIR_DONGU = 5;
    auto HIR_HER = 6;
    auto HIR_BORU_HATTI = 7;
    auto HIR_SENKRONIZE_ET = 8;
    auto HIR_DURDUR = 9;
    auto HIR_DEVAM_ET = 10;
    auto HIR_ETIKET = 11;
    auto HIR_GOTO = 12;
    auto HIR_KARSILASTIR_GOTO = 13;
    auto HIR_MATRIS_CARP = 14;
    auto HIR_RMSNORM = 15;
    auto HIR_SOFTMAX = 16;
    auto HIR_BIRLESTIRILMIS_OP = 17; // FusedOp
    auto HIR_SPILL_YAZ = 18;
    auto HIR_SPILL_OKU      = 19;
    auto HIR_BITNET_CARP    = 20;
    auto HIR_TEMIZLE        = 21;

// Genel Amaçlı HIR Düğüm Yapısı

struct HirDugum {
    tamsayi id;               // Düğümün arena (liste) içindeki indeksi
    tamsayi tip;              // HIR_ATAMA, HIR_IKILI_OP vb.

    // Degisken İsimleri ve Etiketler
    metin hedef;              // Atama hedefi veya yeni temp değişken adı (dest)
    metin sol_isim;           // İkili işlem sol değişken
    metin sag_isim;           // İkili işlem sağ değişken
    metin kosul_isim;         // Eğer/Döngü için koşul değişkeni

    // Opcod ve Etiketler
    tamsayi op;               // İkili işlem operatörü (AST'deki TIP_ARTI vb.)
    metin etiket_adi;         // Goto/Label için etiket adı
    metin dogru_hedef;        // CmpGoto true target
    metin yanlis_hedef;       // CmpGoto false target

    // Özel Veriler
    metin islev_adi;          // Fonksiyon çağrısı
    metin birlestirilmis_ops; // FusedOp (örn matmul+rmsnorm)

    // Değerler
    metin sabit_deger;        // Sayı veya string sabiti

    // AST'deki gibi çocuk ID'leri
    metin cocuklar;           // İfadelerin argümanları veya alt ifadeleri ("1,2,3")
    metin dogru_govde;        // If doğru bloğu (ID'ler)
    metin yanlis_govde;       // If yanlış bloğu (ID'ler)
    metin dongu_govde;        // Döngü gövdesi (ID'ler)
    metin dongu_kosul_ifadeleri; // While için koşul içi hesaplamalar
};

struct HirIslev {
    metin isim;
    metin parametreler;
    metin donus_tipi;
    tamsayi gpu_mu;

    Liste<tamsayi> govde;     // HirDugum id'leri listesi
};

struct HirProgram {
    Liste<HirIslev> islevler;
    Liste<HirDugum> arena;    // Tüm fonksiyonlardaki düğümlerin saklandığı ana arena
};

// Yardımcı Fonksiyon: Yeni bir HIR düğümü oluşturur ve arenaya ekler

tamsayi hir_dugum_olustur(HirProgram program, tamsayi tip) {
    HirDugum d;
    d.id = liste_uzunluk(program.arena);
    d.tip = tip;
    d.hedef = "";
    d.sol_isim = "";
    d.sag_isim = "";
    d.kosul_isim = "";
    d.etiket_adi = "";
    d.dogru_hedef = "";
    d.yanlis_hedef = "";
    d.islev_adi = "";
    d.birlestirilmis_ops = "";
    d.sabit_deger = "";
    d.cocuklar = "";
    d.dogru_govde = "";
    d.yanlis_govde = "";
    d.dongu_govde = "";
    d.dongu_kosul_ifadeleri = "";

    liste_ekle(program.arena, d);
    return d.id;
}