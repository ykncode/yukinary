#include "ykn_std.h"
// ============================================================

// Yukinary HIR Oluşturucu (Builder / Lowering)

// ============================================================

// AST'yi (Soyut Sözdizim Ağacı) dolaşarak HIR'a dönüştürür.


// include ast.ykn
// include hir.ykn

// Basit bir sözlük simülasyonu için anahtar-değer yapısı

struct KapsamDegiskeni {
    metin anahtar;
    metin deger;
};

// Kapsam (Scope) Yığıtı seviyesi

struct KapsamSeviyesi {
    Liste<KapsamDegiskeni> degiskenler;
};

struct HirOlusturucu {
    Liste<Dugum> ast_arena;
    HirProgram program;

    tamsayi temp_sayaci;
    tamsayi # Shadowing için unique id golge_sayaci;

    Liste<KapsamSeviyesi> kapsam_yigiti;

    tamsayi # Şu an işlenen HirIslev'in program.islevler içindeki indeksi mevcut_islev;
};

HirOlusturucu hir_olusturucu_kur(Liste<Dugum>& ast_arena) {
    HirOlusturucu o;
    o.ast_arena = ast_arena;
    o.temp_sayaci = 0;
    o.golge_sayaci = 0;
    o.mevcut_islev = -1;

    // Global kapsamı ekle

    KapsamSeviyesi k;
    liste_ekle(o.kapsam_yigiti, k);

    return o;
}

// Yeni bir geçici değişken (temp) adı üretir (örn: t1, t2)

metin get_temp(HirOlusturucu o) {
    o.temp_sayaci = o.temp_sayaci + 1;
    metin num;
    num = tamsayi_metne(o.temp_sayaci);
    return metin_birlestir("t", num);
}

tamsayi _kapsam_gir(HirOlusturucu o) {
    KapsamSeviyesi k;
    liste_ekle(o.kapsam_yigiti, k);
    return 0;
}

tamsayi _kapsam_cik(HirOlusturucu o) {
    tamsayi len = liste_uzunluk(o.kapsam_yigiti);
    if (len > 1) {
        // Son elemanı çıkar

        liste_sondan_cikar(o.kapsam_yigiti);
    }
    return 0;
}

// İsim çözümleme (Lexical rename haritasından arama)

metin _isim_coz(HirOlusturucu o, metin isim) {
    tamsayi i = liste_uzunluk(o.kapsam_yigiti) - 1;
    while (i >= 0) {
    KapsamSeviyesi k;
        k = liste_al(o.kapsam_yigiti, i);
    tamsayi j = 0;
    tamsayi len = liste_uzunluk(k.degiskenler);
        while (j < len) {
    KapsamDegiskeni d;
            d = liste_al(k.degiskenler, j);
            if (metin_esitmi(d.anahtar, isim) == 1) {
                return d.deger;
            }
            j = j + 1;
        }
        i = i - 1;
    }
    return isim;
}

// İsim mevcut kapsam dışında zaten var mı?

tamsayi _dis_kapsamda_var_mi(HirOlusturucu o, metin isim) {
    tamsayi i = liste_uzunluk(o.kapsam_yigiti) - 2;
    while (i >= 0) {
    KapsamSeviyesi k;
        k = liste_al(o.kapsam_yigiti, i);
    tamsayi j = 0;
    tamsayi len = liste_uzunluk(k.degiskenler);
        while (j < len) {
    KapsamDegiskeni d;
            d = liste_al(k.degiskenler, j);
            if (metin_esitmi(d.anahtar, isim) == 1) {
                return 1;
            }
            j = j + 1;
        }
        i = i - 1;
    }
    return 0;
}

metin _tanimla(HirOlusturucu o, metin isim) {
    tamsayi top_idx = liste_uzunluk(o.kapsam_yigiti) - 1;
    KapsamSeviyesi k;
    k = liste_al(o.kapsam_yigiti, top_idx);

    // Kendi scope'unda var mı bak

    tamsayi j = 0;
    tamsayi len = liste_uzunluk(k.degiskenler);
    tamsayi icinde_var = 0;
    while (j < len) {
    KapsamDegiskeni d;
        d = liste_al(k.degiskenler, j);
        if (metin_esitmi(d.anahtar, isim) == 1) {
            icinde_var = 1;
            break;
        }
        j = j + 1;
    }

    metin cozulmus;
    if (_dis_kapsamda_var_mi(o, isim) == 1 || icinde_var == 1) {
        o.golge_sayaci = o.golge_sayaci + 1;
    metin mangled = metin_birlestir(isim, "__s");
        mangled = metin_birlestir(mangled, tamsayi_metne(o.golge_sayaci));
        cozulmus = mangled;
    } else {
        cozulmus = isim;
    }

    KapsamDegiskeni yeni_d;
    yeni_d.anahtar = isim;
    yeni_d.deger = cozulmus;
    liste_ekle(k.degiskenler, yeni_d);
    liste_guncelle(o.kapsam_yigiti, top_idx, k);

    return cozulmus;
}

metin ziyaret_ifade(HirOlusturucu o, tamsayi dugum_id, tamsayi islev_idx) {
    Dugum d;
    d = liste_al(o.ast_arena, dugum_id);

    if (d.tip == AST_SAYI || d.tip == AST_GERCEK || d.tip == AST_METIN) {
        // Literal (Sabit)

        return d.metinsel_deger;
    } else if (d.tip == AST_KIMLIK) {
        // Değişken kullanımı (okuma)

        return _isim_coz(o, d.metinsel_deger);
    } else if (d.tip == AST_IKILI_OP) {
    metin sol_isim = ziyaret_ifade(o, d.sol_id, islev_idx);
    metin sag_isim = ziyaret_ifade(o, d.sag_id, islev_idx);
    metin temp = get_temp(o);

        // HirBinOp oluştur

    HirProgram prog = o.program;
    tamsayi h_id = hir_dugum_olustur(prog, HIR_IKILI_OP);
    HirDugum h_dugum = liste_al(prog.arena, h_id);
        h_dugum.hedef = temp;
        h_dugum.sol_isim = sol_isim;
        h_dugum.sag_isim = sag_isim;
        h_dugum.sabit_deger = d.metinsel_deger;
        liste_guncelle(prog.arena, h_id, h_dugum);

        // O anki işleve ekle

    HirIslev f = liste_al(prog.islevler, islev_idx);
        liste_ekle(f.govde, h_id);
        liste_guncelle(prog.islevler, islev_idx, f);

        o.program = prog;
        return temp;
    } else if (d.tip == AST_ATAMA) {
        // d.sol_id -> hedef (AST_KIMLIK)

        // d.sag_id -> atanacak değer

    metin atanan_isim = ziyaret_ifade(o, d.sag_id, islev_idx);

    Dugum sol_d = liste_al(o.ast_arena, d.sol_id);
    metin hedef_isim = _isim_coz(o, sol_d.metinsel_deger);

    HirProgram prog2 = o.program;
    tamsayi h_id = hir_dugum_olustur(prog2, HIR_ATAMA);
    HirDugum h_dugum = liste_al(prog2.arena, h_id);
        h_dugum.hedef = hedef_isim;
        h_dugum.sol_isim = atanan_isim;
        liste_guncelle(prog2.arena, h_id, h_dugum);

    HirIslev f2 = liste_al(prog2.islevler, islev_idx);
        liste_ekle(f2.govde, h_id);
        liste_guncelle(prog2.islevler, islev_idx, f2);

        o.program = prog2;
        return hedef_isim;
    }
    return "";
}

tamsayi ziyaret_global(HirOlusturucu o, tamsayi dugum_id) {
    Dugum d;
    d = liste_al(o.ast_arena, dugum_id);

    if (d.tip == AST_ISLEV) {
    HirIslev hir_islev;
        hir_islev.isim = d.metinsel_deger;
        hir_islev.parametreler = d.cocuklar;
        hir_islev.donus_tipi = d.veri_tipi;
        hir_islev.gpu_mu = 0; // TODO

    HirProgram prog3 = o.program;
        liste_ekle(prog3.islevler, hir_islev);
    tamsayi islev_idx = liste_uzunluk(prog3.islevler) - 1;
        o.mevcut_islev = islev_idx;
        o.program = prog3;

        _kapsam_gir(o);

        // Parametreleri tanımla (AST'deki formatı parse ederek isimleri almak gerek, şimdilik atlıyoruz)


        // Gövdeyi işle (AST_BLOK'un çocukları virgülle ayrılmış ID'ler)

        // Normalde string split ile parse etmeli. Şimdilik sembolik.

    tamsayi govde_id = d.govde_id;
    Dugum govde_dugum = liste_al(o.ast_arena, govde_id);

        // Gövdedeki her bir statement'ı ziyaret_ifade ile gez (metinsel "1,2,3" ayrıştırma gerekiyor)


        _kapsam_cik(o);
        o.mevcut_islev = -1;
    }
    return 0;
}