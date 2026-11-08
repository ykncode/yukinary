#include "ykn_std.h"
// Yukinary Self-Hosting Derleyici Driver (v2)

// Kullanim: yukic <kaynak.ykn> [-c] [-o <cikti>]

//

// Bu dosya, Yukinary'nin kendi kendini derleyebilmesi icin yazilmis

// on yuz (frontend) programidir.

// Adimlar: Lexer -> Parser -> TypeChecker -> Kod Uretimi


// include ast.ykn
// include lexer.ykn
// include parser_v3.ykn
// include type_checker.ykn
// include hir.ykn
// include hir_builder.ykn
// include cfg.ykn
// include ssa.ykn
// include optimizer.ykn
// include regalloc.ykn
// include mir.ykn
// include mir_passes.ykn
// include codegen.ykn

tamsayi kullanimi_goster() {
    _ = yazdir("YukiLLM Compiler (yukic) v1.0");
    _ = yazdir("Kullanim:");
    _ = yazdir("  yukic build <dosya.ykn>  - Derler ve binary olusturur");
    _ = yazdir("  yukic run <dosya.ykn>    - Derler ve hemen calistirir");
    return 1;
}

int _ykn_argc; char** _ykn_argv;

// JIT and REPL Support
extern "C" {
    int yukinary_jit_calistir(const unsigned char* makine_kodu, int boyut);
    // int ykn_repl_baslat();
}

int check_repl_mode(int argc, char** argv) {
    if (argc >= 2 && strcmp(argv[1], "repl") == 0) {
        printf("Starting REPL...\n");
        // ykn_repl_baslat(); // Called from YKN
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    _ykn_argc = argc; _ykn_argv = argv;
    tamsayi arg_s = argc_al();
    if (arg_s < 3) {
        return kullanimi_goster();
    }

    metin komut = argv_al(1);
    metin girdi_dosyasi = argv_al(2);

    if ((metin_esitmi(komut, "build") == 0) && (metin_esitmi(komut, "run") == 0)) {
        _ = yazdir("Hata: Bilinmeyen komut.");
        return kullanimi_goster();
    }

    _ = yazdir(metin_birlestir("YKN Derleniyor: ", girdi_dosyasi));
    metin kaynak = dosyadan_oku(girdi_dosyasi);
    // 1. Lexer & Parser

    Liste<Jeton> jetonlar;
    _ = tokenize_et(kaynak, jetonlar);
    Liste<Dugum> ast_arena;
    tamsayi root_id = parser_cozumle(jetonlar, ast_arena);

    // 2. Type Checker

    tamsayi tip_durum = tip_kontrol_baslat(ast_arena, root_id);

    // 3. HIR Oluşturma

    HirOlusturucu h_olusturucu = hir_olusturucu_kur(ast_arena);
    _ = ziyaret_global(h_olusturucu, root_id);

    // 4. CFG Oluşturma

    CfgOlusturucu cfg_o = cfg_olusturucu_kur(h_olusturucu.program);
    CfgProgram cfg_prog = cfg_olustur(cfg_o);

    // 5. SSA Dönüşümü

    SsaOlusturucu ssa_o = ssa_olusturucu_kur(cfg_prog);
    _ = ssa_uygula(ssa_o);

    // 6. Optimizasyon (PassManager)

    Optimizator opt_o = optimizator_kur(cfg_prog);
    _ = optimize_et(opt_o);

    // 7. RegAlloc

    RegAllocOlusturucu reg_o = regalloc_kur(cfg_prog);
    _ = regalloc_calistir(reg_o);

    // 8. MIR & Instruction Selection

    MirOlusturucu mir_o = mir_olusturucu_kur(cfg_prog);
    _ = mir_olustur(mir_o);

    KodUretici codegen_o = kod_uretici_kur(mir_o.mir_prog, "cpp");
    metin cxx_kodu = kod_uret(codegen_o);

    metin tmp_dosya = ".yukic_tmp.cpp";
    _ = dosyaya_yaz(tmp_dosya, cxx_kodu);

    _ = yazdir("Arka planda C++ ile makine koduna cevriliyor...");
    // Varsayilan olarak 'app.exe' uretir

    tamsayi derleme_sonucu = sistem_komutu("g++ -O3 -std=c++17 .yukic_tmp.cpp -o app.exe");

    if (derleme_sonucu != 0) {
        _ = yazdir("HATA: C++ derleme asamasinda hata olustu!");
        return 1;
    }

    _ = yazdir("Basariyla derlendi: app.exe");

    if (metin_esitmi(komut, "run") == 1) {
        _ = yazdir("--- CALISTIRILIYOR ---");
        _ = sistem_komutu(".\\app.exe");
    }

    return 0;
}