#include "ykn_std.h"
// ============================================================

// Yukinary Self-Hosting Parser (v2)

// ============================================================

// Lexer tarafindan uretilen Jeton'lari okuyup, ast_arena listesine

// Dugum objeleri olarak kaydeder ve en ust seviye blok ID'sini dondurur.


struct ParserDurumu {
    tamsayi poz;
    tamsayi hata_var;
};

tamsayi parser_hata(metin mesaj, tamsayi satir) {
    metin m1 = metin_birlestir("[Parser Hatasi] Satir ", tamsayi_metne(satir));
    metin m2 = metin_birlestir(m1, ": ");
    metin m3 = metin_birlestir(m2, mesaj);
    _ = sistem_komutu(metin_birlestir("echo ", m3));
    hata_firlatla(m3);
    return 1;
}

// Jeton alma

Jeton suanki_jeton(Liste<Jeton>& jetonlar, ParserDurumu durum) {
    tamsayi len = liste_uzunluk(jetonlar);
    if (durum.poz < len) {
        return liste_al(jetonlar, durum.poz);
    }

    Jeton eof;
    eof.tip = 99; // TIP_EOF
    return eof;
}

tamsayi ilerle(ParserDurumu durum) {
    durum.poz = durum.poz + 1;
    return 1;
}

tamsayi bekle(Liste<Jeton>& jetonlar, ParserDurumu durum, tamsayi beklenen_tip) {
    Jeton j = suanki_jeton(jetonlar, durum);
    if (j.tip == beklenen_tip) {
        _ = ilerle(durum);
        return 1;
    }

    metin msg = metin_birlestir("Beklenmeyen jeton tipi. Beklenen: ", tamsayi_metne(beklenen_tip));
    _ = parser_hata(msg, j.satir);
    durum.hata_var = 1;
    return 0;
}

// --- Pratt Parsing Alt Yapisi ---


tamsayi operator_onceligi(tamsayi tip) {
    if (tip == 34) { return 1; } // TIP_ATAMA
    if (tip == 35 || tip == 36) { return 2; } // ESIT_MI, ESIT_DEGIL
    if (tip == 37 || tip == 38 || tip == 39 || tip == 40) { return 3; } // KUCUKTUR, BUYUKTUR, KUCUK_ESIT, BUYUK_ESIT
    if (tip == 30 || tip == 31) { return 4; } // ARTI, EKSI
    if (tip == 32 || tip == 33) { return 5; } // CARPI, BOLME
    if (tip == 46) { return 7; } // NOKTA (Alan Erisimi)
    return 0;
}

tamsayi parse_prefix(Liste<Jeton>& jetonlar, ParserDurumu durum, Liste<Dugum>& ast_arena) {
    Jeton j = suanki_jeton(jetonlar, durum);
    Dugum n;
    n.id = liste_uzunluk(ast_arena);

    if (j.tip == 10) { // TIP_TAMSAYI
        n.tip = 1; // AST_SAYI
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);
        liste_ekle(ast_arena, n);
        return n.id;
    }
    if (j.tip == 11) { // TIP_GERCEK_SAYI
        n.tip = 2; // AST_GERCEK
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);
        liste_ekle(ast_arena, n);
        return n.id;
    }
    if (j.tip == 12) { // TIP_METIN
        n.tip = 3; // AST_METIN
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);
        liste_ekle(ast_arena, n);
        return n.id;
    }
    if (j.tip == 9) { // TIP_KIMLIK
        n.tip = 4; // AST_KIMLIK
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);

        // Islev cagrisi mi? ( foo(x, y) )

    Jeton sn = suanki_jeton(jetonlar, durum);
        if (sn.tip == 40) { // TIP_SOL_PARANTEZ
            _ = ilerle(durum); // '(' atla
    Dugum cagri;
            cagri.id = liste_uzunluk(ast_arena) + 1;
            cagri.tip = 12; // AST_ISLEV_CAGRISI
            cagri.metinsel_deger = j.deger;
            cagri.cocuklar = "";

            while (sn.tip != 41 && sn.tip != 99) { // TIP_SAG_PARANTEZ, TIP_EOF
    tamsayi arg_id = parse_expression(jetonlar, durum, 0, ast_arena);
                cagri.cocuklar = metin_birlestir(cagri.cocuklar, metin_birlestir(tamsayi_metne(arg_id), ","));

                sn = suanki_jeton(jetonlar, durum);
                if (sn.tip == 44) { // TIP_VIRGUL
                    _ = ilerle(durum);
                }
                sn = suanki_jeton(jetonlar, durum);
            }
            _ = bekle(jetonlar, durum, 41); // ')'
            liste_ekle(ast_arena, n); // kimlik
            liste_ekle(ast_arena, cagri);
            return cagri.id;
        }

        liste_ekle(ast_arena, n);
        return n.id;
    }

    _ = parser_hata(metin_birlestir("Beklenmeyen prefix tokeni: ", j.deger), j.satir);
    durum.hata_var = 1;
    return -1;
}

tamsayi parse_infix(Liste<Jeton>& jetonlar, ParserDurumu durum, tamsayi sol_id, Liste<Dugum>& ast_arena) {
    Jeton op_jeton = suanki_jeton(jetonlar, durum);
    tamsayi op_oncelik = operator_onceligi(op_jeton.tip);
    _ = ilerle(durum);

    tamsayi sag_id = parse_expression(jetonlar, durum, op_oncelik, ast_arena);

    Dugum n;
    n.id = liste_uzunluk(ast_arena);
    if (op_jeton.tip == 34) { // TIP_ATAMA
        n.tip = 6; // AST_ATAMA
    } else {
        n.tip = 5; // AST_IKILI_OP
    }

    n.metinsel_deger = op_jeton.deger;
    n.sol_id = sol_id;
    n.sag_id = sag_id;
    liste_ekle(ast_arena, n);
    return n.id;
}

tamsayi parse_expression(Liste<Jeton>& jetonlar, ParserDurumu durum, tamsayi oncelik, Liste<Dugum>& ast_arena) {
    tamsayi sol_id = parse_prefix(jetonlar, durum, ast_arena);

    Jeton j = suanki_jeton(jetonlar, durum);
    tamsayi j_oncelik = operator_onceligi(j.tip);

    while (oncelik < j_oncelik) {
        sol_id = parse_infix(jetonlar, durum, sol_id, ast_arena);
        j = suanki_jeton(jetonlar, durum);
        j_oncelik = operator_onceligi(j.tip);
    }
    return sol_id;
}

tamsayi parse_statement(Liste<Jeton>& jetonlar, ParserDurumu durum, Liste<Dugum>& ast_arena) {
    Jeton j = suanki_jeton(jetonlar, durum);

    // degisken tanimi

    if (j.tip == 2) { // TIP_DEGISKEN
        _ = ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK

    metin tip_adi = "Bilinmiyor";
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 58) { // TIP_IKI_NOKTA
            _ = ilerle(durum);
    Jeton tmp_tip = suanki_jeton(jetonlar, durum);
            tip_adi = tmp_tip.deger;
            _ = ilerle(durum);
        }

    tamsayi sag_id = -1;
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 34) { // TIP_ATAMA
            _ = ilerle(durum);
            sag_id = parse_expression(jetonlar, durum, 0, ast_arena);
        }

        _ = bekle(jetonlar, durum, 59); // TIP_NOKTALI_VIRGUL

    Dugum n;
        n.id = liste_uzunluk(ast_arena);
        n.tip = 7; // AST_DEGISKEN_TANIMI
        n.metinsel_deger = isim_j.deger;
        n.veri_tipi = tip_adi;
        n.sag_id = sag_id;
        liste_ekle(ast_arena, n);
        return n.id;
    }

    // dondur (return)

    if (j.tip == 3) { // TIP_DONDUR
        _ = ilerle(durum);
    tamsayi expr_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 59); // TIP_NOKTALI_VIRGUL

    Dugum rn;
        rn.id = liste_uzunluk(ast_arena);
        rn.tip = 14; // AST_DONDUR
        rn.sag_id = expr_id;
        liste_ekle(ast_arena, rn);
        return rn.id;
    }

    // eger (if) - Basit bir eger blogu okuyalim

    if (j.tip == 4) { // TIP_EGER
        _ = ilerle(durum);
    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 5); // TIP_ISE

        _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'
    tamsayi govde_id = liste_uzunluk(ast_arena);
    Dugum govde;
        govde.id = govde_id;
        govde.tip = 15; // AST_BLOK
        govde.cocuklar = "";
        liste_ekle(ast_arena, govde); // Gecici olarak ekle

        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) { // '}'
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
            govde.cocuklar = metin_birlestir(govde.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
            j = suanki_jeton(jetonlar, durum);
        }

        // govdeyi guncelle

        // govde.id'si zaten govde_id olarak ayarlandi.

        // Cocuklar guncellendikten sonra dugumu tekrar listeye eklemeye gerek yok,

        // cunku liste_ekle zaten yapildi ve referans uzerinden guncelleme yeterli.

        // Ancak Yukinary'nin liste yapisi yerinde guncellemeyi desteklemiyorsa,

        // bu kisimda bir mekanizma olmali. Gecici olarak, liste_sil ve liste_ekle

        // yerine liste_guncelle varsayarak duzeltiyorum.

        liste_guncelle(ast_arena, govde_id, govde);

        _ = bekle(jetonlar, durum, 125); // '}'

        // yoksa (else) blogu

    tamsayi alternatif_id = -1;
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 5) { // TIP_YOKSA
            _ = ilerle(durum);
            _ = bekle(jetonlar, durum, 123); // '{'

            alternatif_id = liste_uzunluk(ast_arena);
    Dugum else_govde;
            else_govde.id = alternatif_id;
            else_govde.tip = 15; // AST_BLOK
            else_govde.cocuklar = "";
            liste_ekle(ast_arena, else_govde); // Gecici olarak ekle

            j = suanki_jeton(jetonlar, durum);
            while (j.tip != 125 && j.tip != 99) {
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
                else_govde.cocuklar = metin_birlestir(else_govde.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
                j = suanki_jeton(jetonlar, durum);
            }

            // else_govde.id'si zaten alternatif_id olarak ayarlandi.

            // Cocuklar guncellendikten sonra dugumu tekrar listeye eklemeye gerek yok,

            // cunku liste_ekle zaten yapildi ve referans uzerinden guncelleme yeterli.

            // Ancak Yukinary'nin liste yapisi yerinde guncellemeyi desteklemiyorsa,

            // bu kisimda bir mekanizma olmali. Gecici olarak, liste_sil ve liste_ekle

            // yerine liste_guncelle varsayarak duzeltiyorum.

            liste_guncelle(ast_arena, alternatif_id, else_govde); // Dugumu dogru ID ile guncelle)

            _ = bekle(jetonlar, durum, 125); // '}'
        }

    Dugum d;
        d.id = liste_uzunluk(ast_arena);
        d.tip = 8; // AST_EGER
        d.kosul_id = kosul_id;
        d.govde_id = govde.id;
        d.alternatif_id = alternatif_id;
        liste_ekle(ast_arena, d);
        return d.id;
    }

    // dongu (while)

    if (j.tip == 6) { // TIP_DONGU
        _ = ilerle(durum);
    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena);

        _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'
    tamsayi govde_id = liste_uzunluk(ast_arena);
    Dugum govde;
        govde.id = govde_id;
        govde.tip = 15; // AST_BLOK
        govde.cocuklar = "";
        liste_ekle(ast_arena, govde); // Gecici olarak ekle

        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) { // '}'
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
            govde.cocuklar = metin_birlestir(govde.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
            j = suanki_jeton(jetonlar, durum);
        }

        liste_guncelle(ast_arena, govde_id, govde);

        _ = bekle(jetonlar, durum, 125); // '}'

    Dugum d;
        d.id = liste_uzunluk(ast_arena);
        d.tip = 9; // AST_DONGU
        d.dongu_kosul_id = kosul_id;
        d.dongu_govde_id = govde.id;
        liste_ekle(ast_arena, d);
        return d.id;
    }

    // her (for) dongusu

    if (j.tip == 10) { // TIP_HER
        _ = ilerle(durum);
        _ = bekle(jetonlar, durum, 40); // TIP_SOL_PARANTEZ

    tamsayi baslangic_id = parse_statement(jetonlar, durum, ast_arena); # Baslangic ifadesi
        // parse_statement noktalı virgülü tüketir


    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena); # Kosul ifadesi
        _ = bekle(jetonlar, durum, 59); // TIP_NOKTALI_VIRGUL

    tamsayi adim_id = parse_expression(jetonlar, durum, 0, ast_arena); # Adim ifadesi
        _ = bekle(jetonlar, durum, 41); // TIP_SAG_PARANTEZ

        _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'
    tamsayi govde_id = liste_uzunluk(ast_arena);
    Dugum govde;
        govde.id = govde_id;
        govde.tip = 15; // AST_BLOK
        govde.cocuklar = "";
        liste_ekle(ast_arena, govde); // Gecici olarak ekle

        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) { // '}'
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
            govde.cocuklar = metin_birlestir(govde.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
            j = suanki_jeton(jetonlar, durum);
        }

        liste_guncelle(ast_arena, govde_id, govde);

        _ = bekle(jetonlar, durum, 125); // '}'

    Dugum d;
        d.id = liste_uzunluk(ast_arena);
        d.tip = 10; // AST_HER
        d.dongu_baslangic_id = baslangic_id;
        d.dongu_kosul_id = kosul_id;
        d.dongu_adim_id = adim_id;
        d.dongu_govde_id = govde.id;
        liste_ekle(ast_arena, d);
        return d.id;
    }

    // yapi (struct) tanimi

    if (j.tip == 7) { // TIP_YAPI
        _ = ilerle(durum);
    Jeton yapi_isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK (Yapi adi)

        _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'

    metin uyeler = "";
        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) { // '}'
    Jeton uye_isim_j = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK (Uye adi)
            _ = bekle(jetonlar, durum, 58); // TIP_IKI_NOKTA
    Jeton uye_tip_j = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK (Uye tipi)
            _ = bekle(jetonlar, durum, 59); // TIP_NOKTALI_VIRGUL

    Dugum uye_dugum;
            uye_dugum.id = liste_uzunluk(ast_arena);
            uye_dugum.tip = 7; // AST_DEGISKEN_TANIMI (Uye tanimi gibi)
            uye_dugum.metinsel_deger = uye_isim_j.deger;
            uye_dugum.veri_tipi = uye_tip_j.deger;
            liste_ekle(ast_arena, uye_dugum);
            uyeler = metin_birlestir(uyeler, metin_birlestir(tamsayi_metne(uye_dugum.id), ","));

            j = suanki_jeton(jetonlar, durum);
        }

        _ = bekle(jetonlar, durum, 125); // '}'

    Dugum n;
        n.id = liste_uzunluk(ast_arena);
        n.tip = 20; // AST_YAPI
        n.yapi_adi = yapi_isim_j.deger;
        n.yapi_uyeleri = uyeler;
        liste_ekle(ast_arena, n);
        return n.id;
    }

    // ice_aktar (import) ifadesi

    if (j.tip == 15) { // TIP_ICE_AKTAR
        _ = ilerle(durum);
    Jeton modul_adi_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK (Modul adi)
        _ = bekle(jetonlar, durum, 59); // TIP_NOKTALI_VIRGUL

    Dugum n;
        n.id = liste_uzunluk(ast_arena);
        n.tip = 23; // AST_ICE_AKTAR
        n.metinsel_deger = modul_adi_j.deger; // Modul adi
        liste_ekle(ast_arena, n);
        return n.id;
    }

    // deneme/yakala/nihayet (try/catch/finally) bloklari

    if (j.tip == 19) { // TIP_DENEME
        _ = ilerle(durum);
        _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'

    tamsayi try_govde_id = liste_uzunluk(ast_arena);
    Dugum try_govde;
        try_govde.id = try_govde_id;
        try_govde.tip = 15; // AST_BLOK
        try_govde.cocuklar = "";
        liste_ekle(ast_arena, try_govde); // Gecici olarak ekle

        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) { // '}'
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
            try_govde.cocuklar = metin_birlestir(try_govde.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
            j = suanki_jeton(jetonlar, durum);
        }
        liste_guncelle(ast_arena, try_govde_id, try_govde);
        _ = bekle(jetonlar, durum, 125); // '}'

    tamsayi catch_govde_id = -1;
    tamsayi finally_govde_id = -1;

        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 20) { // TIP_YAKALA
            _ = ilerle(durum);
            _ = bekle(jetonlar, durum, 40); // TIP_SOL_PARANTEZ
    Jeton hata_degiskeni_j = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK (Hata degiskeni adi)
            _ = bekle(jetonlar, durum, 58); // TIP_IKI_NOKTA
    Jeton hata_tipi_j = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // TIP_KIMLIK (Hata tipi)
            _ = bekle(jetonlar, durum, 41); // TIP_SAG_PARANTEZ
            _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'

            catch_govde_id = liste_uzunluk(ast_arena);
    Dugum catch_govde;
            catch_govde.id = catch_govde_id;
            catch_govde.tip = 15; // AST_BLOK
            catch_govde.cocuklar = metin_birlestir(hata_degiskeni_j.deger, metin_birlestir(":", hata_tipi_j.deger)); // Hata bilgisi
            liste_ekle(ast_arena, catch_govde); // Gecici olarak ekle

            j = suanki_jeton(jetonlar, durum);
            while (j.tip != 125 && j.tip != 99) { // '}'
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
                catch_govde.cocuklar = metin_birlestir(catch_govde.cocuklar, metin_birlestir(",", tamsayi_metne(stmt_id)));
                j = suanki_jeton(jetonlar, durum);
            }
            liste_guncelle(ast_arena, catch_govde_id, catch_govde);
            _ = bekle(jetonlar, durum, 125); // '}'
        }

        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 21) { // TIP_NIHAYET
            _ = ilerle(durum);
            _ = bekle(jetonlar, durum, 123); // TIP_SOL_KUSLU '{'

            finally_govde_id = liste_uzunluk(ast_arena);
    Dugum finally_govde;
            finally_govde.id = finally_govde_id;
            finally_govde.tip = 15; // AST_BLOK
            finally_govde.cocuklar = "";
            liste_ekle(ast_arena, finally_govde); // Gecici olarak ekle

            j = suanki_jeton(jetonlar, durum);
            while (j.tip != 125 && j.tip != 99) { // '}'
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
                finally_govde.cocuklar = metin_birlestir(finally_govde.cocuklar, metin_birlestir(",", tamsayi_metne(stmt_id)));
                j = suanki_jeton(jetonlar, durum);
            }
            liste_guncelle(ast_arena, finally_govde_id, finally_govde);
            _ = bekle(jetonlar, durum, 125); // '}'
        }

    Dugum n;
        n.id = liste_uzunluk(ast_arena);
        n.tip = 21; // AST_DENEME
        n.govde_id = try_govde_id;
        n.alternatif_id = catch_govde_id; // Catch blogu icin alternatif_id kullaniliyor
        n.cocuklar = tamsayi_metne(finally_govde_id); // Finally blogu icin cocuklar alani kullaniliyor
        liste_ekle(ast_arena, n);
        return n.id;
    }

    // Varsayilan olarak expression statement

    tamsayi expr = parse_expression(jetonlar, durum, 0, ast_arena);
    _ = bekle(jetonlar, durum, 59); // ';'
    return expr;
}

// Temel fonksiyon - gercek kodu parse eder

tamsayi parser_cozumle(Liste<Jeton>& jetonlar, Liste<Dugum>& ast_arena) {
    ParserDurumu durum;
    durum.poz = 0;
    durum.hata_var = 0;

    // Baslangic blogu olustur

    Dugum root;
    root.tip = 15; // AST_BLOK
    root.id = liste_uzunluk(ast_arena);
    root.cocuklar = "";
    liste_ekle(ast_arena, root);
    tamsayi root_id = root.id;

    // Tokenlari gez ve statementlari parse et

    Jeton j = suanki_jeton(jetonlar, durum);
    while (j.tip != 99) { // TIP_EOF
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
        root.cocuklar = metin_birlestir(root.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
        j = suanki_jeton(jetonlar, durum);
    }

    // Root'u arena listesinin basinda (ID=0) oldugu icin guncelle

    // (Su anki Yukinary liste_sil + liste_ekle ile guncelleme yapiyor)

    liste_sil(ast_arena, root_id);
    root.id = liste_uzunluk(ast_arena);
    liste_ekle(ast_arena, root);

    return root.id;
}