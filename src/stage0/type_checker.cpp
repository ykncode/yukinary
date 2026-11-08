#include "ykn_std.h"
// ============================================================

// Yukinary Self-Hosting Type Checker (v1)

// ============================================================

// Parser'dan uretilen AST Dugumlerini alip tip kontrolleri yapar.


// ice_aktar "ast";


struct TipKontrolDurumu {
    tamsayi hata_var;
};

// Sembol tablosu iskeleti (Sozluk yapisi tam eklenene kadar basit bir liste ile simule edilecek)

struct Sembol {
    metin isim;
    metin tip;
};

// ============================================================

// KAPSAM YIĞITI (Scope Stack) — İç-içe fonksiyon/anonim_blok/loop

// kapsam desteği. Her alt kapsam için yeni bir Sembol listesi.

// Sembol arama İÇTEN DIŞA (LIFO) gerçekleşir → Shadowing doğru.

// ============================================================

struct KapsamYigiti {
    Liste<Liste<Sembol>>   # Her bir seviye kapsamlar;
    tamsayi           # 0-tabanlı, en üst (global) seviye mevcut_seviye;
};

    KapsamYigiti kapsam_yigiti;
// Global tek-seviye geriye dönük uyumluluk (eski kodlar calissin):

    Liste<Sembol> sembol_tablosu;

// --- KapsamYığıtı Temel İşlevleri ---


tamsayi kapsam_yigiti_ilk_kur() {
    // Bir tane global (seviye 0) boş kapsam oluştur

    Liste<Sembol> ilk_kapsam;
    liste_ekle(kapsam_yigiti.kapsamlar, ilk_kapsam);
    kapsam_yigiti.mevcut_seviye = 0;
    // Geriye dönük: sembol_tablosu'nu global kapsama bagla

    sembol_tablosu = liste_al(kapsam_yigiti.kapsamlar, 0);
    return 1;
}

tamsayi kapsam_ekle(KapsamYigiti yigit) {
    // Yeni bos kapsam ekle, seviyeyi bir artır

    Liste<Sembol> yeni_kapsam;
    liste_ekle(yigit.kapsamlar, yeni_kapsam);
    yigit.mevcut_seviye = yigit.mevcut_seviye + 1;
    return 1;
}

tamsayi kapsam_cikar(KapsamYigiti yigit) {
    // En son eklenen kapsamı kaldır (LIFO), mevcut_seviye--

    tamsayi l = liste_uzunluk(yigit.kapsamlar);
    if (l > 1) {
        // En sondaki kapsami listeden sil

        liste_sondan_cikar(yigit.kapsamlar);
        if (yigit.mevcut_seviye > 0) {
            yigit.mevcut_seviye = yigit.mevcut_seviye - 1;
        }
    }
    return 1;
}

tamsayi sembol_ekle_kapsam(KapsamYigiti yigit, metin isim, metin tip) {
    // SADECE mevcut_seviye'deki kapsama ekle (dış kapsamları bozma)

    tamsayi sev = yigit.mevcut_seviye;
    Liste<Sembol> kapsam = liste_al(yigit.kapsamlar, sev);
    Sembol s;
    s.isim = isim;
    s.tip = tip;
    liste_ekle(kapsam, s);
    return 1;
}

metin sembol_bul_kapsam(KapsamYigiti yigit, metin isim) {
    // İÇTEN DIŞA (mevcut_seviye → 0) ara, ilk bulunanı dön (Shadowing dogru)

    tamsayi sev = yigit.mevcut_seviye;
    while (sev >= 0) {
    Liste<Sembol> kapsam = liste_al(yigit.kapsamlar, sev);
    tamsayi l = liste_uzunluk(kapsam);
    tamsayi i = 0;
        while (i < l) {
    Sembol s = liste_al(kapsam, i);
            if (s.isim == isim) {
                return s.tip;
            }
            i = i + 1;
        }
        sev = sev - 1;
    }
    return "Bilinmiyor";
}

// --- Geriye Dönük Uyumluluk: Eski sembol_ekle/sembol_bul KapsamYığıtı kullanır ---


tamsayi sembol_ekle(metin isim, metin tip) {
    // Ilk kullanimda yigiti kur (tek seferlik)

    if (liste_uzunluk(kapsam_yigiti.kapsamlar) == 0) {
        _ = kapsam_yigiti_ilk_kur();
    }
    return sembol_ekle_kapsam(kapsam_yigiti, isim, tip);
}

metin sembol_bul(metin isim) {
    if (liste_uzunluk(kapsam_yigiti.kapsamlar) == 0) {
        _ = kapsam_yigiti_ilk_kur();
    }
    return sembol_bul_kapsam(kapsam_yigiti, isim);
}

tamsayi tip_hatasi(metin mesaj, tamsayi dugum_id) {
    metin m1 = metin_birlestir("[Tip Hatasi] Dugum ID ", tamsayi_metne(dugum_id));
    metin m2 = metin_birlestir(m1, ": ");
    metin m3 = metin_birlestir(m2, mesaj);
    _ = sistem_komutu(metin_birlestir("echo ", m3));
    hata_firlatla(m3);
    return 1;
}

tamsayi dugum_kontrol_et(Liste<Dugum>& ast_arena, tamsayi dugum_id, TipKontrolDurumu durum) {
    if (dugum_id < 0 || dugum_id >= liste_uzunluk(ast_arena)) {
        return 1;
    }

    Dugum n = liste_al(ast_arena, dugum_id);

    if (n.tip == 1 || n.tip == 2 || n.tip == 3) {
        // Literal tipleri: Tamsayi, Gercek, Metin

        return 1;
    }

    if (n.tip == 4) {
        // Kimlik (Identifier) - Sembol tablosunda var mi diye bakilir

    metin tip_adi = sembol_bul(n.metinsel_deger);
        if (tip_adi == "Bilinmiyor") {
            _ = tip_hatasi(metin_birlestir("Tanimlanmamis degisken: ", n.metinsel_deger), dugum_id);
            durum.hata_var = 1;
        }
        return 1;
    }

    if (n.tip == 5) {
        // AST_IKILI_OP

        _ = dugum_kontrol_et(ast_arena, n.sol_id, durum);
        _ = dugum_kontrol_et(ast_arena, n.sag_id, durum);
        // TODO: Sol ve sagin tipleri uyumlu mu?

        return 1;
    }

    if (n.tip == 6) {
        // AST_ATAMA

        _ = dugum_kontrol_et(ast_arena, n.sol_id, durum);
        _ = dugum_kontrol_et(ast_arena, n.sag_id, durum);
        return 1;
    }

    if (n.tip == 7) {
        // AST_DEGISKEN_TANIMI

        if (n.sag_id >= 0) {
            _ = dugum_kontrol_et(ast_arena, n.sag_id, durum);
        }
        _ = sembol_ekle(n.metinsel_deger, n.veri_tipi);
        return 1;
    }

    if (n.tip == 15) {
        // AST_BLOK - Yeni kapsam ac, cocuklari gez, kapsami kapat

        _ = kapsam_ekle(kapsam_yigiti);
    Liste cocuklar_liste = metin_bol(n.cocuklar, ",");
    tamsayi l = liste_uzunluk(cocuklar_liste);
    tamsayi k = 0;
        while (k < l) {
    metin cocuk_str = liste_al(cocuklar_liste, k);
            if (cocuk_str != "") {
                _ = dugum_kontrol_et(ast_arena, metni_tamsayiya(cocuk_str), durum);
            }
            k = k + 1;
        }
        _ = kapsam_cikar(kapsam_yigiti);
        return 1;
    }

    // 8: AST_EGER

    if (n.tip == 8) {
        // Kosul: mevcut (dis) kapsamda kontrol edilir

        _ = dugum_kontrol_et(ast_arena, n.kosul_id, durum);
        // True body (govde_id): kendi ic kapsami

        _ = kapsam_ekle(kapsam_yigiti);
        _ = dugum_kontrol_et(ast_arena, n.govde_id, durum);
        _ = kapsam_cikar(kapsam_yigiti);
        // False body (alternatif_id): kendi ic kapsami

        if (n.alternatif_id >= 0) {
            _ = kapsam_ekle(kapsam_yigiti);
            _ = dugum_kontrol_et(ast_arena, n.alternatif_id, durum);
            _ = kapsam_cikar(kapsam_yigiti);
        }
        return 1;
    }

    // 9: AST_DONGU (While)

    if (n.tip == 9) {
        // Kosul: mevcut kapsam

        _ = dugum_kontrol_et(ast_arena, n.dongu_kosul_id, durum);
        // Govde: kendi ic kapsami

        _ = kapsam_ekle(kapsam_yigiti);
        _ = dugum_kontrol_et(ast_arena, n.dongu_govde_id, durum);
        _ = kapsam_cikar(kapsam_yigiti);
        return 1;
    }

    // 10: AST_HER (For)

    if (n.tip == 10) {
        // Baslangic/kosul/adim: mevcut kapsam

        _ = dugum_kontrol_et(ast_arena, n.dongu_baslangic_id, durum);
        _ = dugum_kontrol_et(ast_arena, n.dongu_kosul_id, durum);
        _ = dugum_kontrol_et(ast_arena, n.dongu_adim_id, durum);
        // Govde: kendi ic kapsami

        _ = kapsam_ekle(kapsam_yigiti);
        _ = dugum_kontrol_et(ast_arena, n.dongu_govde_id, durum);
        _ = kapsam_cikar(kapsam_yigiti);
        return 1;
    }

    // 20: AST_YAPI (Struct)

    if (n.tip == 20) {
        // Yapi uyeleri zaten AST_DEGISKEN_TANIMI olarak tutuluyor

    Liste uyeler_liste = metin_bol(n.yapi_uyeleri, ",");
    tamsayi l = liste_uzunluk(uyeler_liste);
    tamsayi k = 0;
        while (k < l) {
    metin uye_str = liste_al(uyeler_liste, k);
            if (uye_str != "") {
                _ = dugum_kontrol_et(ast_arena, metni_tamsayiya(uye_str), durum);
            }
            k = k + 1;
        }
        return 1;
    }

    // 23: AST_ICE_AKTAR (Import)

    if (n.tip == 23) {
        // Modulun varligi ve icerigi daha sonra kontrol edilecek

        return 1;
    }

    // 21: AST_DENEME (Try/Catch/Finally)

    if (n.tip == 21) {
        _ = dugum_kontrol_et(ast_arena, n.govde_id, durum);
        if (n.alternatif_id >= 0) {
            _ = dugum_kontrol_et(ast_arena, n.alternatif_id, durum);
        }
        if (n.cocuklar != "" && metni_tamsayiya(n.cocuklar) >= 0) {
            _ = dugum_kontrol_et(ast_arena, metni_tamsayiya(n.cocuklar), durum);
        }
        return 1;
    }

    return 1;
}

// Temel fonksiyon - ast uzerinde gezinir

tamsayi tip_kontrol_baslat(Liste<Dugum>& ast_arena, tamsayi root_id) {
    TipKontrolDurumu durum;
    durum.hata_var = 0;

    // Kapsam yiginitini sifirla / global kapsami ilk kur (tekrar calistirmalarda tutarlilik)

    if (liste_uzunluk(kapsam_yigiti.kapsamlar) > 0) {
        // Eski kapsamlari temizle

    tamsayi l2 = liste_uzunluk(kapsam_yigiti.kapsamlar);
        while (l2 > 0) {
            liste_sondan_cikar(kapsam_yigiti.kapsamlar);
            l2 = l2 - 1;
        }
    }
    _ = kapsam_yigiti_ilk_kur();

    _ = dugum_kontrol_et(ast_arena, root_id, durum);

    if (durum.hata_var == 1) {
        return 0;
    }
    return 1;
}