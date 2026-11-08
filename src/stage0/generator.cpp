#include "ykn_std.h"
// ============================================================

// Yukinary Self-Hosting Generator (Kod Ureticisi)

// ============================================================

// AST uzerinde gezerek gecerli C++ kodlari uretir.


struct UreticiDurumu {
    tamsayi girinti;
};

metin girinti_uret(tamsayi seviye) {
    metin sonuc = "";
    tamsayi i = 0;
    while (i < seviye) {
        sonuc = metin_birlestir(sonuc, "    ");
        i = i + 1;
    }
    return sonuc;
}

metin generator_visit_dugum(Liste<Dugum>& ast_arena, tamsayi dugum_id, UreticiDurumu durum) {
    if (dugum_id < 0 || dugum_id >= liste_uzunluk(ast_arena)) {
        return "";
    }

    Dugum n = liste_al(ast_arena, dugum_id);

    // 1: AST_SAYI

    if (n.tip == 1) {
        return n.metinsel_deger;
    }

    // 2: AST_GERCEK

    if (n.tip == 2) {
        return metin_birlestir(n.metinsel_deger, "f");
    }

    // 3: AST_METIN

    if (n.tip == 3) {
        return metin_birlestir("std::string(\"", metin_birlestir(n.metinsel_deger, "\")"));
    }

    // 4: AST_KIMLIK

    if (n.tip == 4) {
        return n.metinsel_deger;
    }

    // 5: AST_IKILI_OP

    if (n.tip == 5) {
    metin sol_kod = generator_visit_dugum(ast_arena, n.sol_id, durum);
    metin sag_kod = generator_visit_dugum(ast_arena, n.sag_id, durum);
    metin res = metin_birlestir("(", sol_kod);
        res = metin_birlestir(res, metin_birlestir(" ", n.metinsel_deger));
        res = metin_birlestir(res, metin_birlestir(" ", sag_kod));
        return metin_birlestir(res, ")");
    }

    // 6: AST_ATAMA

    if (n.tip == 6) {
    metin sol_kod_at = generator_visit_dugum(ast_arena, n.sol_id, durum);
    metin sag_kod_at = generator_visit_dugum(ast_arena, n.sag_id, durum);
    metin res_at = metin_birlestir(sol_kod_at, " = ");
        return metin_birlestir(res_at, sag_kod_at);
    }

    // 7: AST_DEGISKEN_TANIMI

    if (n.tip == 7) {
    metin c_tip = "auto";
        if (n.veri_tipi == "tamsayi") { c_tip = "int"; }
        if (n.veri_tipi == "metin") { c_tip = "std::string"; }
        if (n.veri_tipi == "gercek") { c_tip = "float"; }

    metin d_res = metin_birlestir(c_tip, metin_birlestir(" ", n.metinsel_deger));
        if (n.sag_id >= 0) {
    metin sag_kod_d = generator_visit_dugum(ast_arena, n.sag_id, durum);
            d_res = metin_birlestir(d_res, metin_birlestir(" = ", sag_kod_d));
        }
        return d_res;
    }

    // 15: AST_BLOK

    if (n.tip == 15) {
    Liste cocuklar_liste = metin_bol(n.cocuklar, ",");
    metin blok_kod = "{\n";
        durum.girinti = durum.girinti + 1;

    tamsayi l = liste_uzunluk(cocuklar_liste);
    tamsayi k = 0;

        while (k < l) {
    metin cocuk_str = liste_al(cocuklar_liste, k);
            if (cocuk_str != "") {
    tamsayi c_id = metni_tamsayiya(cocuk_str);
    metin satir_kod = generator_visit_dugum(ast_arena, c_id, durum);

                // Eger satir ise sonuna noktali virgul ekle

    Dugum c_dugum = liste_al(ast_arena, c_id);
                if (c_dugum.tip == 5 || c_dugum.tip == 6 || c_dugum.tip == 12) {
                    satir_kod = metin_birlestir(satir_kod, ";");
                }

                blok_kod = metin_birlestir(blok_kod, girinti_uret(durum.girinti));
                blok_kod = metin_birlestir(blok_kod, metin_birlestir(satir_kod, "\n"));
            }
            k = k + 1;
        }

        durum.girinti = durum.girinti - 1;
        blok_kod = metin_birlestir(blok_kod, girinti_uret(durum.girinti));
        blok_kod = metin_birlestir(blok_kod, "}");
        return blok_kod;
    }

    // 8: AST_EGER

    if (n.tip == 8) {
    metin kosul_kod = generator_visit_dugum(ast_arena, n.kosul_id, durum);
    metin govde_kod = generator_visit_dugum(ast_arena, n.govde_id, durum);

    metin if_kod = metin_birlestir("if (", metin_birlestir(kosul_kod, ") "));
        if_kod = metin_birlestir(if_kod, govde_kod);

        if (n.alternatif_id >= 0) {
    metin else_kod = generator_visit_dugum(ast_arena, n.alternatif_id, durum);
            if_kod = metin_birlestir(if_kod, metin_birlestir(" else ", else_kod));
        }
        return if_kod;
    }

    // 9: AST_DONGU (While Loop)

    if (n.tip == 9) {
    metin kosul_kod = generator_visit_dugum(ast_arena, n.dongu_kosul_id, durum);
    metin govde_kod = generator_visit_dugum(ast_arena, n.dongu_govde_id, durum);

    metin while_kod = metin_birlestir("while (", metin_birlestir(kosul_kod, ") "));
        while_kod = metin_birlestir(while_kod, govde_kod);
        return while_kod;
    }

    // 10: AST_HER (For Loop)

    if (n.tip == 10) {
    metin baslangic_kod = generator_visit_dugum(ast_arena, n.dongu_baslangic_id, durum);
    metin kosul_kod = generator_visit_dugum(ast_arena, n.dongu_kosul_id, durum);
    metin adim_kod = generator_visit_dugum(ast_arena, n.dongu_adim_id, durum);
    metin govde_kod = generator_visit_dugum(ast_arena, n.dongu_govde_id, durum);

    metin for_kod = metin_birlestir("for (", baslangic_kod);
        for_kod = metin_birlestir(for_kod, metin_birlestir("; ", kosul_kod));
        for_kod = metin_birlestir(for_kod, metin_birlestir("; ", adim_kod));
        for_kod = metin_birlestir(for_kod, metin_birlestir(") ", govde_kod));
        return for_kod;
    }

    // 20: AST_YAPI (Struct)

    if (n.tip == 20) {
    metin struct_kod = metin_birlestir("struct ", n.yapi_adi);
        struct_kod = metin_birlestir(struct_kod, " {\n");
        durum.girinti = durum.girinti + 1;

    Liste uyeler_liste = metin_bol(n.yapi_uyeleri, ",");
    tamsayi l = liste_uzunluk(uyeler_liste);
    tamsayi k = 0;

        while (k < l) {
    metin uye_str = liste_al(uyeler_liste, k);
            if (uye_str != "") {
    tamsayi uye_id = metni_tamsayiya(uye_str);
    metin uye_tanim_kod = generator_visit_dugum(ast_arena, uye_id, durum);
                struct_kod = metin_birlestir(struct_kod, girinti_uret(durum.girinti));
                struct_kod = metin_birlestir(struct_kod, metin_birlestir(uye_tanim_kod, ";\n"));
            }
            k = k + 1;
        }

        durum.girinti = durum.girinti - 1;
        struct_kod = metin_birlestir(struct_kod, girinti_uret(durum.girinti));
        struct_kod = metin_birlestir(struct_kod, "};\n");
        return struct_kod;
    }

    // 23: AST_ICE_AKTAR (Import)

    if (n.tip == 23) {
    metin modul_adi = n.metinsel_deger;
        // Basit bir C++ include olarak donusturuyoruz

        return metin_birlestir("//include \"", metin_birlestir(modul_adi, ".h\""));
    }

    // 21: AST_DENEME (Try/Catch/Finally)

    if (n.tip == 21) {
    metin try_kod = metin_birlestir("try ", generator_visit_dugum(ast_arena, n.govde_id, durum));

        if (n.alternatif_id >= 0) { // Catch blogu
    metin catch_bilgi = liste_al(ast_arena, n.alternatif_id).cocuklar; # Hata degiskeni ve tipi
    Liste catch_parametreleri = metin_bol(catch_bilgi, ":");
    metin hata_degiskeni = liste_al(catch_parametreleri, 0);
    metin hata_tipi = liste_al(catch_parametreleri, 1);

    metin catch_govde_kod = generator_visit_dugum(ast_arena, n.alternatif_id, durum);
            try_kod = metin_birlestir(try_kod, metin_birlestir(" catch (", metin_birlestir(hata_tipi, metin_birlestir(" ", hata_degiskeni))));
            try_kod = metin_birlestir(try_kod, metin_birlestir(") ", catch_govde_kod));
        }

        if (n.cocuklar != "" && metni_tamsayiya(n.cocuklar) >= 0) { // Finally blogu
    tamsayi finally_id = metni_tamsayiya(n.cocuklar);
    metin finally_govde_kod = generator_visit_dugum(ast_arena, finally_id, durum);
            try_kod = metin_birlestir(try_kod, metin_birlestir(" finally ", finally_govde_kod));
        }
        return try_kod;
    }

    return "";
}

tamsayi generator_calistir(Liste<Dugum>& ast_arena, tamsayi root_id, metin cikti_yolu) {
    UreticiDurumu durum;
    durum.girinti = 1; // main() ici oldugu icin 1 girinti

    metin cpp_kodu = "#include \"libykn_runtime.h\"\n\nint main() {\n";

    Dugum root = liste_al(ast_arena, root_id);
    Liste cocuklar_liste = metin_bol(root.cocuklar, ",");
    tamsayi l = liste_uzunluk(cocuklar_liste);
    auto GEN_MOV = 1;
    auto GEN_ADD = 2;
    auto GEN_SUB = 3;
    auto GEN_MUL = 4;
    auto GEN_DIV = 5;
    auto GEN_MOD = 6;
    auto GEN_SHL = 7;
    auto GEN_SHR = 8;
    auto GEN_CALL = 9;
    auto GEN_BR = 10;
    auto GEN_BR_EQ = 11;
    auto GEN_BR_NE = 12;
    auto GEN_LDR = 13;
    auto GEN_STR = 14;
    auto GEN_RET = 15;
    auto GEN_CMP = 16;
    auto GEN_FUSED_MADD = 17;
    auto GEN_SPILL_STORE = 18;
    auto GEN_SPILL_LOAD = 19;
    auto GEN_NOP = 20;
    tamsayi k = 0;

    while (k < l) {
    metin cocuk_str = liste_al(cocuklar_liste, k);
        if (cocuk_str != "") {
    tamsayi c_id = metni_tamsayiya(cocuk_str);
    metin satir_kod = generator_visit_dugum(ast_arena, c_id, durum);

    Dugum c_dugum = liste_al(ast_arena, c_id);
            if (c_dugum.tip == 5 || c_dugum.tip == 6 || c_dugum.tip == 12 || c_dugum.tip == 14 || c_dugum.tip == 7) {
                satir_kod = metin_birlestir(satir_kod, ";");
            }

            cpp_kodu = metin_birlestir(cpp_kodu, girinti_uret(durum.girinti));
            cpp_kodu = metin_birlestir(cpp_kodu, metin_birlestir(satir_kod, "\n"));
        }
        k = k + 1;
    }

    cpp_kodu = metin_birlestir(cpp_kodu, "    return 0;\n}\n");

    _ = dosyaya_yaz(cikti_yolu, cpp_kodu);

    return 1;
}