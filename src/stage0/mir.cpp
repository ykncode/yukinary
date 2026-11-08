#include "ykn_std.h"
// ============================================================

// Yukinary MIR (Machine Intermediate Representation) Veri Yapıları

// ============================================================

// Donanım mimarisine (örn. ARM64) özel instruction'ları (talimatları)

// temsil eden en düşük seviyeli IR.


    auto MIR_MOV = 100;
    auto MIR_ADD = 101;
    auto MIR_SUB = 102;
    auto MIR_MUL = 103;
    auto MIR_DIV = 104;
    auto MIR_MOD = 105;
    auto MIR_SHL = 106;
    auto MIR_SHR = 107;
    auto MIR_CALL = 108;
    auto MIR_BR = 109;
    auto MIR_BR_EQ = 110;
    auto MIR_BR_NE = 111;
    auto MIR_LDR = 112;
    auto MIR_STR = 113;
    auto MIR_FUSED_MADD = 114;
    auto MIR_SPILL = 115;

struct MirDugum {
    tamsayi id;
    tamsayi tip;

    tamsayi # Destination register no yazmac_hedef;
    tamsayi # Source 1 yazmac_kaynak1;
    tamsayi # Source 2 yazmac_kaynak2;

    metin    # Immediate value (Örn anlik_deger;
    metin         # Hedef etiket etiket;
};

struct MirBlok {
    metin etiket;
    Liste<tamsayi> # MirDugum id'leri talimatlar;
};

struct MirIslev {
    metin isim;
    Liste<MirBlok> bloklar;
};

struct MirProgram {
    Liste<MirIslev> islevler;
    Liste<MirDugum> arena;
};