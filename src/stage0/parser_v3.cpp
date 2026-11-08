#include "ykn_std.h"
// ============================================================

// Yukinary Self-Hosting Parser (v3)

// ============================================================

// Tam özellikli self-hosting parser:

// - İf/else (eğer/yoksa)

// - While/for loops (döngü/her)

// - Function declarations (işlev)

// - Struct/enum (yapı/numaralandırma)

// - Import/use (içe_aktar/kullan)

// - Try/catch/finally (deneme/yakala/nihayet)

// - Pipeline (boru_hattı)

// - Pratt expression parsing

// - Error recovery


struct ParserDurumu {
    tamsayi poz;
    tamsayi hata_var;
    tamsayi  # iç içe geçme derinliği (infinite loop koruması) derinlik;
};

// ── Hata Yönetimi ──


tamsayi parser_hata(metin mesaj, tamsayi satir) {
    metin m1 = metin_birlestir("[Parser Hatasi] Satir ", tamsayi_metne(satir));
    metin m2 = metin_birlestir(m1, ": ");
    metin m3 = metin_birlestir(m2, mesaj);
    _ = sistem_komutu(metin_birlestir("echo ", m3));
    hata_firlatla(m3);
    return 1;
}

// ── Token Erişim ──


Jeton suanki_jeton(Liste<Jeton>& jetonlar, ParserDurumu durum) {
    tamsayi len = liste_uzunluk(jetonlar);
    if (durum.poz < len) {
        return liste_al(jetonlar, durum.poz);
    }
    Jeton eof;
    eof.tip = 99; // TIP_EOF
    return eof;
}

Jeton oncu_jeton(Liste<Jeton>& jetonlar, ParserDurumu durum, tamsayi offset) {
    tamsayi idx = durum.poz + offset;
    tamsayi len = liste_uzunluk(jetonlar);
    if (idx >= 0 && idx < len) {
        return liste_al(jetonlar, idx);
    }
    Jeton eof;
    eof.tip = 99;
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
    // Hata iyileştirme: bazen yanlış token geldiğinde atlayıp devam et

    metin msg = metin_birlestir("Beklenmeyen jeton. Beklenen tip: ", tamsayi_metne(beklenen_tip));
    msg = metin_birlestir(msg, metin_birlestir(", Gelen: ", tamsayi_metne(j.tip)));
    _ = parser_hata(msg, j.satir);
    durum.hata_var = 1;
    return 0;
}

// ── Pratt Parsing ──


tamsayi operator_onceligi(tamsayi tip) {
    if (tip == 34) { return 1; }       // ATAMA
    if (tip == 47 || tip == 48) { return 2; }  // VE_MANTIK, VEYA_MANTIK
    if (tip == 35 || tip == 36) { return 3; }  // ESIT_MI, ESIT_DEGIL
    if (tip >= 37 && tip <= 40) { return 4; }    // Karsilastirma
    if (tip == 30 || tip == 31) { return 5; }  // ARTI, EKSI
    if (tip == 32 || tip == 33) { return 6; }  // CARPI, BOLME
    if (tip == 46) { return 8; }                  // NOKTA (alan erisimi)
    if (tip == 49 || tip == 50) { return 7; }  // VE_BITI, VEYA_BITI
    return 0;
}

// Prefix: atomik ifadeler (sayı, metin, kimlik, parantez, !, -, ~)

tamsayi parse_prefix(Liste<Jeton>& jetonlar, ParserDurumu durum, Liste<Dugum>& ast_arena) {
    Jeton j = suanki_jeton(jetonlar, durum);
    Dugum n;
    n.id = liste_uzunluk(ast_arena);

    // Tamsayı

    if (j.tip == 10) {
        n.tip = 1; // AST_SAYI
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);
        liste_ekle(ast_arena, n);
        return n.id;
    }
    // Gerçek sayı

    if (j.tip == 11) {
        n.tip = 2; // AST_GERCEK
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);
        liste_ekle(ast_arena, n);
        return n.id;
    }
    // Metin

    if (j.tip == 12) {
        n.tip = 3; // AST_METIN
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);
        liste_ekle(ast_arena, n);
        return n.id;
    }
    // Kimlik / Fonksiyon çağrısı

    if (j.tip == 9) {
        n.tip = 4; // AST_KIMLIK
        n.metinsel_deger = j.deger;
        _ = ilerle(durum);

        // Fonksiyon çağrısı: foo(x, y)

    Jeton sn = suanki_jeton(jetonlar, durum);
        if (sn.tip == 40) { // TIP_SOL_PARANTEZ
            _ = ilerle(durum);
    Dugum cagri;
            cagri.id = liste_uzunluk(ast_arena) + 1;
            cagri.tip = 12; // AST_ISLEV_CAGRISI
            cagri.metinsel_deger = j.deger;
            cagri.cocuklar = "";

            sn = suanki_jeton(jetonlar, durum);
            while (sn.tip != 41 && sn.tip != 99) {
    tamsayi arg_id = parse_expression(jetonlar, durum, 0, ast_arena);
                cagri.cocuklar = metin_birlestir(cagri.cocuklar, metin_birlestir(tamsayi_metne(arg_id), ","));
                sn = suanki_jeton(jetonlar, durum);
                if (sn.tip == 44) { // VIRGUL
                    _ = ilerle(durum);
                }
                sn = suanki_jeton(jetonlar, durum);
            }
            _ = bekle(jetonlar, durum, 41); // ')'
            liste_ekle(ast_arena, n);
            liste_ekle(ast_arena, cagri);
            return cagri.id;
        }

        liste_ekle(ast_arena, n);
        return n.id;
    }
    // Parantezli ifade: (expr)

    if (j.tip == 40) { // TIP_SOL_PARANTEZ
        _ = ilerle(durum);
    tamsayi expr_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 41); // ')'
        return expr_id;
    }
    // Unary operators: !, -, ~

    if (j.tip == 48 || j.tip == 31 || j.tip == 52) { // DEGIL_MANTIK, EKSI, DEGIL_BITI
        _ = ilerle(durum);
    tamsayi operand_id = parse_prefix(jetonlar, durum, ast_arena);
    Dugum unary;
        unary.id = liste_uzunluk(ast_arena);
        unary.tip = 13; // AST_UNARY_OP
        unary.metinsel_deger = j.deger;
        unary.sag_id = operand_id;
        liste_ekle(ast_arena, unary);
        return unary.id;
    }

    _ = parser_hata(metin_birlestir("Beklenmeyen prefix: ", j.deger), j.satir);
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
    if (op_jeton.tip == 34) { // ATAMA
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

// ── Statement Parsing (Genişletilmiş) ──


tamsayi parse_statement(Liste<Jeton>& jetonlar, ParserDurumu durum, Liste<Dugum>& ast_arena) {
    // Derinlik kontrolü (infinite loop koruması)

    if (durum.derinlik > 1000) {
        _ = parser_hata("Parser derinlik limiti asildi (sonsuz dongu?)", 0);
        return -1;
    }
    durum.derinlik = durum.derinlik + 1;

    Jeton j = suanki_jeton(jetonlar, durum);
    tamsayi sonuc = -1;

    // ── değişken tanımı ──

    if (j.tip == 2) { // TIP_DEGISKEN
        _ = ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // KIMLIK

    metin tip_adi = "Bilinmiyor";
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 58) { // IKI_NOKTA
            _ = ilerle(durum);
    Jeton tmp_tip = suanki_jeton(jetonlar, durum);
            tip_adi = tmp_tip.deger;
            _ = ilerle(durum);
        }

    tamsayi sag_id = -1;
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 34) { // ATAMA
            _ = ilerle(durum);
            sag_id = parse_expression(jetonlar, durum, 0, ast_arena);
        }
        _ = bekle(jetonlar, durum, 59); // NOKTALI_VIRGUL

    Dugum n;
        n.id = liste_uzunluk(ast_arena);
        n.tip = 7; // AST_DEGISKEN_TANIMI
        n.metinsel_deger = isim_j.deger;
        n.veri_tipi = tip_adi;
        n.sag_id = sag_id;
        liste_ekle(ast_arena, n);
        sonuc = n.id;
    }
    // ── döndür ──

    else if (j.tip == 3) { // TIP_DONDUR
        _ = ilerle(durum);
    tamsayi expr_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 59);

    Dugum rn;
        rn.id = liste_uzunluk(ast_arena);
        rn.tip = 14; // AST_DONDUR
        rn.sag_id = expr_id;
        liste_ekle(ast_arena, rn);
        sonuc = rn.id;
    }
    // ── eğer/yoksa ──

    else if (j.tip == 4) { // TIP_EGER
        _ = ilerle(durum);
    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 5); // TIP_ISE

        // Govde bloğu

        _ = bekle(jetonlar, durum, 123); // '{'
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);

        // yoksa (else) veya yoksa eger (elseif)

    tamsayi yoksa_id = -1;
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 6) { // TIP_YOKSA
            _ = ilerle(durum);
            // yoksa eger mi kontrol et (lookahead)

            j = suanki_jeton(jetonlar, durum);
            if (j.tip == 4) { // TIP_EGER (elseif)
                yoksa_id = parse_statement(jetonlar, durum, ast_arena);
            } else j.tip == 123  { // '{' (else block)
                yoksa_id = parse_block(jetonlar, durum, ast_arena);
            }
        }

    Dugum in;
        in.id = liste_uzunluk(ast_arena);
        in.tip = 8; // AST_EGER
        in.kosul_id = kosul_id;
        in.govde_id = govde_id;
        in.yoksa_id = yoksa_id;
        liste_ekle(ast_arena, in);
        sonuc = in.id;
    }
    // ── döngü (while) ──

    else if (j.tip == 7) { // TIP_DONGU
        _ = ilerle(durum);
    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 123); // '{'
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);

    Dugum wh;
        wh.id = liste_uzunluk(ast_arena);
        wh.tip = 20; // AST_DONGU
        wh.kosul_id = kosul_id;
        wh.govde_id = govde_id;
        liste_ekle(ast_arena, wh);
        sonuc = wh.id;
    }
    // ── her (for-each) ──

    else if (j.tip == 13) { // TIP_HER
        _ = ilerle(durum);
        // her x in liste { ... }

    Jeton isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // KIMLIK

        j = suanki_jeton(jetonlar, durum);
        // 'in' anahtar kelimesi (ASCII fallback)

        _ = ilerle(durum);

    tamsayi koleksiyon_id = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 123); // '{'
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);

    Dugum fe;
        fe.id = liste_uzunluk(ast_arena);
        fe.tip = 21; // AST_HER
        fe.metinsel_deger = isim_j.deger;
        fe.koleksiyon_id = koleksiyon_id;
        fe.govde_id = govde_id;
        liste_ekle(ast_arena, fe);
        sonuc = fe.id;
    }
    // ── işlev (function declaration) ──

    else if (j.tip == 1) { // TIP_ISLEV
        _ = ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // KIMLIK

        _ = bekle(jetonlar, durum, 40); // '('
        // Parametreler

    metin params = "";
        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 41 && j.tip != 99) { // ')' || EOF
    Jeton p_isim = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // KIMLIK
            params = metin_birlestir(params, p_isim.deger);

            // Tip bilgisi var mı?

            j = suanki_jeton(jetonlar, durum);
            if (j.tip == 58) { // IKI_NOKTA
                _ = ilerle(durum);
    Jeton p_tip = suanki_jeton(jetonlar, durum);
                params = metin_birlestir(params, ":");
                params = metin_birlestir(params, p_tip.deger);
                _ = ilerle(durum);
            }

            j = suanki_jeton(jetonlar, durum);
            if (j.tip == 44) { // VIRGUL
                params = metin_birlestir(params, ",");
                _ = ilerle(durum);
            }
            j = suanki_jeton(jetonlar, durum);
        }
        _ = bekle(jetonlar, durum, 41); // ')'

        // Dönüş tipi

    metin donus_tipi = "Bilinmiyor";
        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 45) { // TIP_OK (->)
            _ = ilerle(durum);
    Jeton ret_tip = suanki_jeton(jetonlar, durum);
            donus_tipi = ret_tip.deger;
            _ = ilerle(durum);
        }

        _ = bekle(jetonlar, durum, 123); // '{'
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);

    Dugum fn;
        fn.id = liste_uzunluk(ast_arena);
        fn.tip = 16; // AST_ISLEV
        fn.metinsel_deger = isim_j.deger;
        fn.cocuklar = params;
        fn.veri_tipi = donus_tipi;
        fn.govde_id = govde_id;
        liste_ekle(ast_arena, fn);
        sonuc = fn.id;
    }
    // ── içe_aktar (import) ──

    else if (j.tip == 14) { // TIP_ICE_AKTAR
        _ = ilerle(durum);
    Jeton mod_path = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 12); // METIN_DEGERI
        _ = bekle(jetonlar, durum, 59); // ';'

    Dugum imp;
        imp.id = liste_uzunluk(ast_arena);
        imp.tip = 17; // AST_IMPORT
        imp.metinsel_deger = mod_path.deger;
        liste_ekle(ast_arena, imp);
        sonuc = imp.id;
    }
    // ── kullan (use) ──

    else if (j.tip == 15) { // TIP_KULLAN
        _ = ilerle(durum);
    Jeton mod_path = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // KIMLIK
        _ = bekle(jetonlar, durum, 59); // ';'

    Dugum us;
        us.id = liste_uzunluk(ast_arena);
        us.tip = 18; // AST_USE
        us.metinsel_deger = mod_path.deger;
        liste_ekle(ast_arena, us);
        sonuc = us.id;
    }
    // ── yapı (struct) ──

    else if (j.tip == 16) { // TIP_YAPI
        _ = ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // KIMLIK
        _ = bekle(jetonlar, durum, 123); // '{'

    metin fields = "";
        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) { // '}' || EOF
            // Alan adı

    Jeton alan = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // KIMLIK
            fields = metin_birlestir(fields, alan.deger);

            // Alan tipi

            j = suanki_jeton(jetonlar, durum);
            if (j.tip == 58) { // IKI_NOKTA
                _ = ilerle(durum);
    Jeton alan_tip = suanki_jeton(jetonlar, durum);
                fields = metin_birlestir(fields, ":");
                fields = metin_birlestir(fields, alan_tip.deger);
                _ = ilerle(durum);
            }
            _ = bekle(jetonlar, durum, 59); // ';'
            fields = metin_birlestir(fields, ",");
            j = suanki_jeton(jetonlar, durum);
        }
        _ = bekle(jetonlar, durum, 125); // '}'

    Dugum st;
        st.id = liste_uzunluk(ast_arena);
        st.tip = 19; // AST_YAPI
        st.metinsel_deger = isim_j.deger;
        st.cocuklar = fields;
        liste_ekle(ast_arena, st);
        sonuc = st.id;
    }
    // ── numaralandırma (enum) ──

    else if (j.tip == 17) { // TIP_NUMARALANDIRMA
        _ = ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);
        _ = bekle(jetonlar, durum, 9); // KIMLIK
        _ = bekle(jetonlar, durum, 123); // '{'

    metin values = "";
        j = suanki_jeton(jetonlar, durum);
        while (j.tip != 125 && j.tip != 99) {
    Jeton val = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // KIMLIK
            values = metin_birlestir(values, val.deger);

            // Değer ataması var mı?

            j = suanki_jeton(jetonlar, durum);
            if (j.tip == 34) { // ATAMA
                _ = ilerle(durum);
    Jeton deger = suanki_jeton(jetonlar, durum);
                values = metin_birlestir(values, "=");
                values = metin_birlestir(values, deger.deger);
                _ = ilerle(durum);
            }

            j = suanki_jeton(jetonlar, durum);
            if (j.tip == 44) { // VIRGUL
                _ = ilerle(durum);
            }
            j = suanki_jeton(jetonlar, durum);
        }
        _ = bekle(jetonlar, durum, 125); // '}'

    Dugum en;
        en.id = liste_uzunluk(ast_arena);
        en.tip = 22; // AST_NUMARALANDIRMA
        en.metinsel_deger = isim_j.deger;
        en.cocuklar = values;
        liste_ekle(ast_arena, en);
        sonuc = en.id;
    }
    // ── deneme/yakala/nihayet (try/catch/finally) ──

    else if (j.tip == 23) { // TIP_DENEME
        _ = ilerle(durum);
        _ = bekle(jetonlar, durum, 123); // '{'
    tamsayi try_id = parse_block(jetonlar, durum, ast_arena);

    tamsayi catch_id = -1;
    tamsayi finally_id = -1;

        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 24) { // TIP_YAKALA
            _ = ilerle(durum);
            _ = bekle(jetonlar, durum, 40); // '('
    Jeton err_var = suanki_jeton(jetonlar, durum);
            _ = bekle(jetonlar, durum, 9); // KIMLIK
            _ = bekle(jetonlar, durum, 41); // ')'
            _ = bekle(jetonlar, durum, 123); // '{'
            catch_id = parse_block(jetonlar, durum, ast_arena);

    Dugum ch;
            ch.id = liste_uzunluk(ast_arena);
            ch.tip = 24; // AST_YAKALA
            ch.metinsel_deger = err_var.deger;
            ch.govde_id = catch_id;
            liste_ekle(ast_arena, ch);
        }

        j = suanki_jeton(jetonlar, durum);
        if (j.tip == 25) { // TIP_NIHAYET
            _ = ilerle(durum);
            _ = bekle(jetonlar, durum, 123); // '{'
            finally_id = parse_block(jetonlar, durum, ast_arena);

    Dugum fi;
            fi.id = liste_uzunluk(ast_arena);
            fi.tip = 25; // AST_NIHAYET
            fi.govde_id = finally_id;
            liste_ekle(ast_arena, fi);
        }

    Dugum tc;
        tc.id = liste_uzunluk(ast_arena);
        tc.tip = 23; // AST_DENEME
        tc.govde_id = try_id;
        tc.yakala_id = catch_id;
        tc.nihayet_id = finally_id;
        liste_ekle(ast_arena, tc);
        sonuc = tc.id;
    }
    // ── durdur (break) ──

    else if (j.tip == 8) { // TIP_DURDUR
        _ = ilerle(durum);
        _ = bekle(jetonlar, durum, 59); // ';'
    Dugum br;
        br.id = liste_uzunluk(ast_arena);
        br.tip = 26; // AST_DURDUR
        liste_ekle(ast_arena, br);
        sonuc = br.id;
    }
    // ── devam_et (continue) ──

    else if (j.tip == 9 || j.tip == 10) { // TIP_DEVAM_ET
        _ = ilerle(durum);
        _ = bekle(jetonlar, durum, 59);
    Dugum ct;
        ct.id = liste_uzunluk(ast_arena);
        ct.tip = 27; // AST_DEVAM_ET
        liste_ekle(ast_arena, ct);
        sonuc = ct.id;
    }
    // ── boru_hattı (pipeline) ──

    else if (j.tip == 27) { // TIP_BORU_HATTI
        _ = ilerle(durum);
        _ = bekle(jetonlar, durum, 123); // '{'
    tamsayi pipe_id = parse_block(jetonlar, durum, ast_arena);

        // senkronize_et

        j = suanki_jeton(jetonlar, durum);
    tamsayi sync_id = -1;
        if (j.tip == 28) { // TIP_SENKRONIZE_ET
            _ = ilerle(durum);
            sync_id = parse_expression(jetonlar, durum, 0, ast_arena);
            _ = bekle(jetonlar, durum, 59);
        }

    Dugum pl;
        pl.id = liste_uzunluk(ast_arena);
        pl.tip = 28; // AST_BORU_HATTI
        pl.govde_id = pipe_id;
        pl.sync_id = sync_id;
        liste_ekle(ast_arena, pl);
        sonuc = pl.id;
    }
    // ── varsayılan: expression statement ──

    else {
    tamsayi expr = parse_expression(jetonlar, durum, 0, ast_arena);
        _ = bekle(jetonlar, durum, 59); // ';'
        sonuc = expr;
    }

    durum.derinlik = durum.derinlik - 1;
    return sonuc;
}

// ── Blok Parsing ──


tamsayi parse_block(Liste<Jeton>& jetonlar, ParserDurumu durum, Liste<Dugum>& ast_arena) {
    Dugum blok;
    blok.id = liste_uzunluk(ast_arena);
    blok.tip = 15; // AST_BLOK
    blok.cocuklar = "";
    liste_ekle(ast_arena, blok);

    Jeton j = suanki_jeton(jetonlar, durum);
    while (j.tip != 125 && j.tip != 99) { // '}' || EOF
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
        blok.cocuklar = metin_birlestir(blok.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
        j = suanki_jeton(jetonlar, durum);
    }
    _ = bekle(jetonlar, durum, 125); // '}'

    tamsayi root_id = blok.id;
    liste_sil(ast_arena, root_id);
    blok.id = liste_uzunluk(ast_arena);
    liste_ekle(ast_arena, blok);

    return blok.id;
}

// ── Ana Parser Fonksiyonu ──


tamsayi parser_cozumle(Liste<Jeton>& jetonlar, Liste<Dugum>& ast_arena) {
    ParserDurumu durum;
    durum.poz = 0;
    durum.hata_var = 0;
    durum.derinlik = 0;

    // Root blok

    Dugum root;
    root.tip = 15; // AST_BLOK
    root.id = liste_uzunluk(ast_arena);
    root.cocuklar = "";
    liste_ekle(ast_arena, root);
    tamsayi root_id = root.id;

    Jeton j = suanki_jeton(jetonlar, durum);
    while (j.tip != 99) { // TIP_EOF
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);
        root.cocuklar = metin_birlestir(root.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
        j = suanki_jeton(jetonlar, durum);
    }

    // Root'u güncelle

    liste_sil(ast_arena, root_id);
    root.id = liste_uzunluk(ast_arena);
    liste_ekle(ast_arena, root);

    return root.id;
}