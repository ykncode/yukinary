#include "ykn_std.h"
// ============================================================

// Yukinary AST (Soyut Sozdizim Agaci) Veri Yapilari

// ============================================================

// Yukinary'de kalitim olmadigi icin, agac yapisini duz bir "Arena"

// listesinde indekslerle (ID'ler) birbirine baglanan genel amacli

// bir Dugum (Node) yapisi ile temsil ediyoruz.


// -- AST Dugum Tipleri --

    auto AST_BOS = 0;
    auto AST_SAYI = 1;
    auto AST_GERCEK = 2;
    auto AST_METIN = 3;
    auto AST_KIMLIK = 4;
    auto AST_IKILI_OP = 5;
    auto AST_ATAMA = 6;
    auto AST_DEGISKEN_TANIMI = 7;
    auto AST_EGER = 8;
    auto AST_DONGU = 9;
    auto AST_HER = 10;
    auto AST_ISLEV = 11;
    auto AST_ISLEV_CAGRISI = 12;
    auto AST_METOD_CAGRISI = 13;
    auto AST_DONDUR = 14;
    auto AST_BLOK = 15;
    auto AST_DURDUR = 16;
    auto AST_DEVAM_ET = 17;
    auto AST_ALAN_ERISIMI = 18;
    auto AST_INDEKS_ERISIMI = 19;
    auto AST_BREAK          = 27; // Break ifadesi
    auto AST_CONTINUE       = 28; // Continue ifadesi
    auto AST_REFERANS       = 29; // &x
    auto AST_MUT_REFERANS   = 30; // &mut x
    auto AST_DAVRANIS       = 31;
    auto AST_UYGULA         = 32;
    auto AST_MAKRO_KUR      = 33;
    auto AST_MAKRO_CAGRI    = 34;
    auto AST_NOTASYON       = 35;
    auto AST_YAPI = 20;
    auto AST_ICE_AKTAR = 23; # Yeni import dugum tipi
    auto AST_DENEME = 21;
    auto AST_FIRLATMA = 22;

// Genel Amacli AST Dugum Yapisi

struct Dugum {
    tamsayi        # Dugumun arena (liste) icindeki indeksi id;
    tamsayi satir;
    tamsayi sutun;
    metin dosya_adi;

    // Generic parametreler (Örn: "T,U" veya "[24, 25]")
    std::string generic_parametreler;

    // Notasyonlar (Örn: "[Turet(Serialize)]")
    std::string notasyonlar;

    tamsayi ust_kapsam;
    tamsayi yoksa_id;
    tamsayi koleksiyon_id;
    tamsayi yakala_id;
    tamsayi nihayet_id;
    tamsayi sync_id;
    tamsayi        # AST_SAYI, AST_EGER vb. tip;

    // Deger Alanlari

    metin # "x", "12", "merhaba" vb. metinsel_deger;
    metin      # "TAMSAYI_TIP" vs (type checker veya degisken tanimi icin) veri_tipi;
    metin cozumlenmis_tip;

    // Baglanti Alanlari (Diger dugumlerin ID'leri)

    tamsayi       # BinaryOp sol, Atama hedefi, If kosulu sol_id;
    tamsayi       # BinaryOp sag, Atama degeri sag_id;

    tamsayi     # If veya While kosulu kosul_id;
    tamsayi     # Govde blogunun ID'si (AST_BLOK) govde_id;
    tamsayi# Else blogunun ID'si (AST_BLOK) alternatif_id;
    tamsayi # Dongu kosulu icin (While) dongu_kosul_id;
    tamsayi # Dongu govdesi icin (While, For) dongu_govde_id;
    tamsayi # For dongusu baslangic ifadesi dongu_baslangic_id;
    tamsayi # For dongusu bitis ifadesi dongu_bitis_id;
    tamsayi # For dongusu adim ifadesi dongu_adim_id;
    metin # Yapi tanimi icin (Struct) yapi_adi;
    metin # Yapi uyeleri (virgulle ayrilmis Dugum ID'leri) yapi_uyeleri;

    // Liste veya coklu oge saklama ihtiyaci (Virgulle ayrilmis metin olarak)

    metin       # Parametreler, argumanlar veya blok ifadelerinin ID'leri "1,2,3" cocuklar;
};