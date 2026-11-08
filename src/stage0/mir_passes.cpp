#include "ykn_std.h"
#include <map>
#include <vector>
#include <string>
#include <algorithm>

// ============================================================
// Yukinary MIR Gecisleri (Instruction Selection, Peephole & Scheduling)
// ============================================================

#ifndef YKN_MIR_TYPES_DEFINED
#define YKN_MIR_TYPES_DEFINED

constexpr int MIR_MOV = 100;
constexpr int MIR_ADD = 101;
constexpr int MIR_SUB = 102;
constexpr int MIR_MUL = 103;
constexpr int MIR_DIV = 104;
constexpr int MIR_MOD = 105;
constexpr int MIR_SHL = 106;
constexpr int MIR_SHR = 107;
constexpr int MIR_CALL = 108;
constexpr int MIR_BR = 109;
constexpr int MIR_BR_EQ = 110;
constexpr int MIR_BR_NE = 111;
constexpr int MIR_LDR = 112;
constexpr int MIR_STR = 113;
constexpr int MIR_FUSED_MADD = 114;
constexpr int MIR_SPILL = 115;

struct HirDugum {
    tamsayi id; tamsayi tip; metin hedef; metin sol_isim; metin sag_isim; metin kosul_isim;
    tamsayi op; metin etiket_adi; metin dogru_hedef; metin yanlis_hedef; metin islev_adi;
    metin birlestirilmis_ops; metin sabit_deger;
};
struct HirIslev { metin isim; Liste<tamsayi> govde; };
struct HirProgram { Liste<HirIslev> islevler; Liste<HirDugum> arena; };
struct TemelBlok { metin etiket; Liste<tamsayi> talimatlar; Liste<metin> sonraki_bloklar; Liste<metin> onceki_bloklar; };
struct CfgIslev { metin isim; Liste<TemelBlok> bloklar; metin giris_etiketi; };
struct CfgProgram { HirProgram hir_prog; Liste<CfgIslev> islevler; };

struct MirDugum {
    tamsayi id; tamsayi tip; tamsayi yazmac_hedef; tamsayi yazmac_kaynak1; tamsayi yazmac_kaynak2;
    metin anlik_deger; metin etiket;
};
struct MirBlok { metin etiket; Liste<tamsayi> talimatlar; };
struct MirIslev { metin isim; Liste<MirBlok> bloklar; };
struct MirProgram { Liste<MirIslev> islevler; Liste<MirDugum> arena; };
#endif

struct MirOlusturucu {
    CfgProgram cfg_prog;
    MirProgram mir_prog;
};

MirOlusturucu mir_olusturucu_kur(CfgProgram cfg_prog) {
    MirOlusturucu o;
    o.cfg_prog = cfg_prog;
    return o;
}

// 1. Instruction Selection (ISel)
// HIR dugumlerini hedef makine talimatlarina (MIR) esler.
tamsayi _talimat_secimi_yap(MirOlusturucu o, CfgIslev f, MirIslev& mir_f) {
    tamsayi uretilen = 0;
    int blen = (int)liste_uzunluk(f.bloklar);

    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        MirBlok mb;
        mb.etiket = b.etiket;

        int tlen = (int)liste_uzunluk(b.talimatlar);
        for (int ti = 0; ti < tlen; ti++) {
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id < 0 || instr_id >= (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) continue;

            HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
            MirDugum md;
            md.id = (tamsayi)liste_uzunluk(o.mir_prog.arena);
            md.yazmac_hedef = 0;
            md.yazmac_kaynak1 = 0;
            md.yazmac_kaynak2 = 0;

            if (d.tip == 0) { // HIR_ATAMA
                md.tip = MIR_MOV;
                md.anlik_deger = d.sabit_deger;
                md.etiket = d.hedef;
            } else if (d.tip == 1) { // HIR_IKILI_OP
                switch (d.op) {
                    case 30: md.tip = MIR_ADD; break; // +
                    case 31: md.tip = MIR_SUB; break; // -
                    case 32: md.tip = MIR_MUL; break; // *
                    case 33: md.tip = MIR_DIV; break; // /
                    case 41: md.tip = MIR_MOD; break; // %
                    default: md.tip = MIR_ADD; break;
                }
                md.anlik_deger = d.sabit_deger;
                md.etiket = d.hedef;
            } else if (d.tip == 10) { // HIR_CAGRI
                md.tip = MIR_CALL;
                md.etiket = d.islev_adi;
            } else if (d.tip == 2) { // HIR_DALLAN
                md.tip = MIR_BR;
                md.etiket = d.dogru_hedef;
            } else {
                md.tip = MIR_MOV;
                md.anlik_deger = d.sabit_deger;
                md.etiket = d.hedef;
            }

            liste_ekle(o.mir_prog.arena, md);
            liste_ekle(mb.talimatlar, md.id);
            uretilen++;
        }

        liste_ekle(mir_f.bloklar, mb);
    }

    return uretilen;
}

// 2. Peephole Optimizasyonlari
// Donanim seviyesi sadeleştirmeler:
// x = y + 0 -> x = y
// x = y * 1 -> x = y
// x = y * 0 -> x = 0
// x = y * 2 -> x = y << 1
// x = y - 0 -> x = y
// x = y / 1 -> x = y
tamsayi _peephole_optimizasyon_yap(MirOlusturucu o, MirIslev& mir_f) {
    tamsayi optimize_edilen = 0;
    int blen = (int)liste_uzunluk(mir_f.bloklar);

    for (int bi = 0; bi < blen; bi++) {
        MirBlok mb = liste_al(mir_f.bloklar, bi);
        int tlen = (int)liste_uzunluk(mb.talimatlar);

        for (int ti = 0; ti < tlen; ti++) {
            tamsayi mid = liste_al(mb.talimatlar, ti);
            if (mid < 0 || mid >= (tamsayi)liste_uzunluk(o.mir_prog.arena)) continue;

            MirDugum md = liste_al(o.mir_prog.arena, mid);

            // x + 0 -> x = y
            if (md.tip == MIR_ADD && md.anlik_deger == "0") {
                md.tip = MIR_MOV;
                md.anlik_deger = "";
                liste_guncelle(o.mir_prog.arena, mid, md);
                optimize_edilen++;
            }
            // x - 0 -> x = y
            else if (md.tip == MIR_SUB && md.anlik_deger == "0") {
                md.tip = MIR_MOV;
                md.anlik_deger = "";
                liste_guncelle(o.mir_prog.arena, mid, md);
                optimize_edilen++;
            }
            // x * 1 -> x = y
            else if (md.tip == MIR_MUL && md.anlik_deger == "1") {
                md.tip = MIR_MOV;
                md.anlik_deger = "";
                liste_guncelle(o.mir_prog.arena, mid, md);
                optimize_edilen++;
            }
            // x * 0 -> 0
            else if (md.tip == MIR_MUL && md.anlik_deger == "0") {
                md.tip = MIR_MOV;
                md.anlik_deger = "0";
                liste_guncelle(o.mir_prog.arena, mid, md);
                optimize_edilen++;
            }
            // x * 2 -> x << 1 (Strength reduction)
            else if (md.tip == MIR_MUL && md.anlik_deger == "2") {
                md.tip = MIR_SHL;
                md.anlik_deger = "1";
                liste_guncelle(o.mir_prog.arena, mid, md);
                optimize_edilen++;
            }
            // x / 1 -> x = y
            else if (md.tip == MIR_DIV && md.anlik_deger == "1") {
                md.tip = MIR_MOV;
                md.anlik_deger = "";
                liste_guncelle(o.mir_prog.arena, mid, md);
                optimize_edilen++;
            }
        }
    }

    return optimize_edilen;
}

// 3. Instruction Scheduling (Zamanlama)
// Komutlar arasi veri bagimliliklarini koruyarak boru hatti (pipeline)
// gecikmelerini minimize etmek uzere siralar.
tamsayi _zamanlama_yap(MirOlusturucu o, MirIslev& mir_f) {
    tamsayi yeniden_siralanan = 0;
    int blen = (int)liste_uzunluk(mir_f.bloklar);

    for (int bi = 0; bi < blen; bi++) {
        MirBlok mb = liste_al(mir_f.bloklar, bi);
        int tlen = (int)liste_uzunluk(mb.talimatlar);
        if (tlen <= 2) continue;

        // Bagimsiz yukleme/atama talimatlarini one alarak
        // ALU komutlarinin operand gecikmesini gizle
        Liste<tamsayi> planli_talimatlar;
        std::vector<tamsayi> yuklemeler;
        std::vector<tamsayi> hesaplar;
        std::vector<tamsayi> dallanmalar;

        for (int ti = 0; ti < tlen; ti++) {
            tamsayi mid = liste_al(mb.talimatlar, ti);
            if (mid < 0 || mid >= (tamsayi)liste_uzunluk(o.mir_prog.arena)) continue;
            MirDugum md = liste_al(o.mir_prog.arena, mid);

            if (md.tip == MIR_MOV || md.tip == MIR_LDR) {
                yuklemeler.push_back(mid);
            } else if (md.tip == MIR_BR || md.tip == MIR_BR_EQ || md.tip == MIR_BR_NE) {
                dallanmalar.push_back(mid);
            } else {
                hesaplar.push_back(mid);
            }
        }

        for (auto id : yuklemeler) liste_ekle(planli_talimatlar, id);
        for (auto id : hesaplar) liste_ekle(planli_talimatlar, id);
        for (auto id : dallanmalar) liste_ekle(planli_talimatlar, id);

        mb.talimatlar = planli_talimatlar;
        liste_guncelle(mir_f.bloklar, bi, mb);
        yeniden_siralanan += tlen;
    }

    return yeniden_siralanan;
}

tamsayi _mir_islev_isle(MirOlusturucu o, tamsayi islev_idx) {
    CfgProgram prog = o.cfg_prog;
    CfgIslev f = liste_al(prog.islevler, islev_idx);

    MirIslev mir_f;
    mir_f.isim = f.isim;

    _talimat_secimi_yap(o, f, mir_f);
    _peephole_optimizasyon_yap(o, mir_f);
    _zamanlama_yap(o, mir_f);

    liste_ekle(o.mir_prog.islevler, mir_f);
    return 0;
}

tamsayi mir_olustur(MirOlusturucu o) {
    tamsayi i = 0;
    CfgProgram prog = o.cfg_prog;
    tamsayi p_len = liste_uzunluk(prog.islevler);

    while (i < p_len) {
        _mir_islev_isle(o, i);
        i = i + 1;
    }

    return 0;
}