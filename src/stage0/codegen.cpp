#include "ykn_std.h"
// ============================================================

// Yukinary Kod Üretici (Code Generator / Emitter)

// ============================================================

// MIR (Machine IR) formatındaki programı C++ koduna veya doğrudan

// Assembly dosyasına (.s) dönüştürür.

//

// Hedef: C++ (Transpilation) veya ARM64 Assembly


// include mir.ykn

struct KodUretici {
    MirProgram mir_prog;
    metin    # "cpp" veya "asm" hedef_dil;
    metin   # Üretilen kod metni cikti_kodu;
};

KodUretici kod_uretici_kur(MirProgram mir_prog, metin hedef) {
    KodUretici o;
    o.mir_prog = mir_prog;
    o.hedef_dil = hedef;
    o.cikti_kodu = "";
    return o;
}

// C++ Kodu Üretimi

metin _cpp_kodu_uret(KodUretici o) {
    // 1. Başlıklar (Headers) ve importlar

    metin basliklar = "#include <iostream>\n#include <vector>\n\n";
    o.cikti_kodu = metin_birlestir(o.cikti_kodu, basliklar);

    // 2. Extern tanımlamaları (matris_carp vb.)

    metin extern_tanimlar = "extern \"C\" void matris_carp();\n\n";
    o.cikti_kodu = metin_birlestir(o.cikti_kodu, extern_tanimlar);

    // 3. İşlevleri gez (main wrapper vb.)

    MirProgram m_prog = o.mir_prog;
    tamsayi p_len = liste_uzunluk(m_prog.islevler);
    tamsayi i = 0;
    while (i < p_len) {
    MirIslev f = liste_al(m_prog.islevler, i);

        // Fonksiyon imzasını yazdır (Örn: void fn_adi() { )

    metin fn_imza = metin_birlestir("void ", f.isim);
        fn_imza = metin_birlestir(fn_imza, "() {\n");
        o.cikti_kodu = metin_birlestir(o.cikti_kodu, fn_imza);

        // Fonksiyon kapat

        o.cikti_kodu = metin_birlestir(o.cikti_kodu, "}\n\n");
        i = i + 1;
    }
    return o.cikti_kodu;
}

metin kod_uret(KodUretici o) {
    if (metin_esitmi(o.hedef_dil, "cpp") == 1) {
        o.cikti_kodu = _cpp_kodu_uret(o);
    } else {
        // assembly uret

    }
    return o.cikti_kodu;
}