#include "ykn_std.h"
#include "cfg.cpp"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <queue>

// ============================================================
// Yukinary Optimizasyon Motoru (PassManager / Optimizer)
// ============================================================
// SSA formundaki CFG üzerinde çeşitli donanım ve yazılım bağımsız
// optimizasyonları uygular.
//
// Pass'ler:
// 1. Sabit Katlama (Constant Folding & Propagation)
// 2. Ölü Kod Elemesi (Dead Code Elimination - DCE)
// 3. Kernel Füzyonu (Kernel Fusion - ML Özel)
// 4. Döngü Bağımsız Kod Taşıma (LICM - Loop Invariant Code Motion)
// 5. Sabit Dallanma Budama & Ulaşılmaz Blok Elemesi (Branch Pruning)

#ifndef YKN_OPTIMIZATOR_DEFINED
#define YKN_OPTIMIZATOR_DEFINED
struct Optimizator {
    CfgProgram cfg_prog;
    tamsayi degisiklik_yapildi;
};
#endif

Optimizator optimizator_kur(CfgProgram cfg_prog) {
    Optimizator o;
    o.cfg_prog = cfg_prog;
    o.degisiklik_yapildi = 0;
    return o;
}

static bool _sayi_mi(const std::string& s) {
    if (s.empty()) return false;
    size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (start >= s.length()) return false;
    for (size_t i = start; i < s.length(); i++) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

// 1. Sabit Katlama ve Yayma (Constant Folding & Propagation)
tamsayi _pass_sabit_katlama(Optimizator o, CfgIslev f) {
    tamsayi katlanan = 0;
    std::map<std::string, long long> bilinen_sabitler;

    int blen = (int)liste_uzunluk(f.bloklar);
    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        int tlen = (int)liste_uzunluk(b.talimatlar);

        for (int ti = 0; ti < tlen; ti++) {
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id < 0 || instr_id >= (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) continue;

            HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);

            if (d.tip == 0) { // HIR_ATAMA
                if (!d.sabit_deger.empty() && _sayi_mi(d.sabit_deger)) {
                    try {
                        long long val = std::stoll(d.sabit_deger);
                        bilinen_sabitler[d.hedef] = val;
                    } catch (...) {}
                } else if (!d.sol_isim.empty() && bilinen_sabitler.find(d.sol_isim) != bilinen_sabitler.end()) {
                    long long val = bilinen_sabitler[d.sol_isim];
                    d.sabit_deger = std::to_string(val);
                    d.sol_isim = "";
                    bilinen_sabitler[d.hedef] = val;
                    liste_guncelle(o.cfg_prog.hir_prog.arena, instr_id, d);
                    o.degisiklik_yapildi = 1;
                    katlanan++;
                }
            } else if (d.tip == 1) { // HIR_IKILI_OP
                bool sol_biliniyor = false;
                bool sag_biliniyor = false;
                long long sol_val = 0;
                long long sag_val = 0;

                if (!d.sol_isim.empty() && bilinen_sabitler.find(d.sol_isim) != bilinen_sabitler.end()) {
                    sol_biliniyor = true;
                    sol_val = bilinen_sabitler[d.sol_isim];
                } else if (!d.sabit_deger.empty() && _sayi_mi(d.sabit_deger)) {
                    sol_biliniyor = true;
                    sol_val = std::stoll(d.sabit_deger);
                }

                if (!d.sag_isim.empty() && bilinen_sabitler.find(d.sag_isim) != bilinen_sabitler.end()) {
                    sag_biliniyor = true;
                    sag_val = bilinen_sabitler[d.sag_isim];
                }

                if (sol_biliniyor && sag_biliniyor) {
                    long long sonuc = 0;
                    bool hesaplandi = true;

                    switch (d.op) {
                        case 30: sonuc = sol_val + sag_val; break;
                        case 31: sonuc = sol_val - sag_val; break;
                        case 32: sonuc = sol_val * sag_val; break;
                        case 33: 
                            if (sag_val != 0) sonuc = sol_val / sag_val; 
                            else hesaplandi = false; 
                            break;
                        case 34: 
                            if (sag_val != 0) sonuc = sol_val % sag_val; 
                            else hesaplandi = false; 
                            break;
                        case 35: sonuc = (sol_val == sag_val) ? 1 : 0; break;
                        case 36: sonuc = (sol_val != sag_val) ? 1 : 0; break;
                        case 37: sonuc = (sol_val < sag_val) ? 1 : 0; break;
                        case 38: sonuc = (sol_val > sag_val) ? 1 : 0; break;
                        case 39: sonuc = (sol_val <= sag_val) ? 1 : 0; break;
                        case 40: sonuc = (sol_val >= sag_val) ? 1 : 0; break;
                        case 41: sonuc = sol_val << sag_val; break; // SHL
                        case 42: sonuc = sol_val >> sag_val; break; // SHR
                        case 43: sonuc = sol_val & sag_val; break;  // BITWISE AND
                        case 44: sonuc = sol_val | sag_val; break;  // BITWISE OR
                        case 45: sonuc = sol_val ^ sag_val; break;  // BITWISE XOR
                        default: hesaplandi = false; break;
                    }

                    if (hesaplandi) {
                        d.tip = 0; // HIR_ATAMA'ya indirge
                        d.sabit_deger = std::to_string(sonuc);
                        d.sol_isim = "";
                        d.sag_isim = "";
                        d.op = 0;
                        bilinen_sabitler[d.hedef] = sonuc;
                        liste_guncelle(o.cfg_prog.hir_prog.arena, instr_id, d);
                        o.degisiklik_yapildi = 1;
                        katlanan++;
                    }
                } else if (sol_biliniyor && !sag_biliniyor) {
                    // Cebirsel sadeleştirmeler (sol sabit)
                    if (d.op == 32 && sol_val == 0) { // 0 * x = 0
                        d.tip = 0;
                        d.sabit_deger = "0";
                        d.sol_isim = "";
                        d.sag_isim = "";
                        d.op = 0;
                        bilinen_sabitler[d.hedef] = 0;
                        liste_guncelle(o.cfg_prog.hir_prog.arena, instr_id, d);
                        o.degisiklik_yapildi = 1;
                        katlanan++;
                    }
                } else if (!sol_biliniyor && sag_biliniyor) {
                    // Cebirsel sadeleştirmeler (sağ sabit)
                    if (d.op == 32 && sag_val == 0) { // x * 0 = 0
                        d.tip = 0;
                        d.sabit_deger = "0";
                        d.sol_isim = "";
                        d.sag_isim = "";
                        d.op = 0;
                        bilinen_sabitler[d.hedef] = 0;
                        liste_guncelle(o.cfg_prog.hir_prog.arena, instr_id, d);
                        o.degisiklik_yapildi = 1;
                        katlanan++;
                    } else if (d.op == 30 && sag_val == 0) { // x + 0 = x
                        d.tip = 0;
                        d.sol_isim = d.sol_isim;
                        d.sag_isim = "";
                        d.op = 0;
                        liste_guncelle(o.cfg_prog.hir_prog.arena, instr_id, d);
                        o.degisiklik_yapildi = 1;
                        katlanan++;
                    }
                }
            }
        }
    }
    return katlanan;
}

// 2. Ölü Kod Elemesi (Dead Code Elimination - DCE)
tamsayi _pass_olu_kod_elemesi(Optimizator o, CfgIslev f) {
    tamsayi silinen = 0;

    std::map<std::string, int> kullanim_sayisi;
    int blen = (int)liste_uzunluk(f.bloklar);

    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        int tlen = (int)liste_uzunluk(b.talimatlar);
        for (int ti = 0; ti < tlen; ti++) {
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id < 0 || instr_id >= (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) continue;

            HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
            if (!d.sol_isim.empty()) kullanim_sayisi[d.sol_isim]++;
            if (!d.sag_isim.empty()) kullanim_sayisi[d.sag_isim]++;
            if (!d.kosul_isim.empty()) kullanim_sayisi[d.kosul_isim]++;
            if (d.tip == 2 || d.tip == 3) {
                if (!d.hedef.empty()) kullanim_sayisi[d.hedef]++;
            }
        }
    }

    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        Liste<tamsayi> yeni_talimatlar;
        int tlen = (int)liste_uzunluk(b.talimatlar);

        for (int ti = 0; ti < tlen; ti++) {
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id < 0 || instr_id >= (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) continue;

            HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);

            if ((d.tip == 0 || d.tip == 1) && !d.hedef.empty()) {
                if (kullanim_sayisi[d.hedef] == 0) {
                    silinen++;
                    o.degisiklik_yapildi = 1;
                    continue;
                }
            }
            liste_ekle(yeni_talimatlar, instr_id);
        }
        b.talimatlar = yeni_talimatlar;
        liste_guncelle(f.bloklar, bi, b);
    }
    return silinen;
}

// 3. Kernel Füzyonu (Kernel Fusion - ML Özel)
tamsayi _pass_kernel_fuzyonu(Optimizator o, CfgIslev f) {
    tamsayi fuzyon_sayisi = 0;
    int blen = (int)liste_uzunluk(f.bloklar);

    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        int tlen = (int)liste_uzunluk(b.talimatlar);
        if (tlen < 2) continue;

        for (int ti = 0; ti < tlen - 1; ti++) {
            tamsayi id1 = liste_al(b.talimatlar, ti);
            tamsayi id2 = liste_al(b.talimatlar, ti + 1);
            if (id1 < 0 || id2 < 0) continue;

            HirDugum d1 = liste_al(o.cfg_prog.hir_prog.arena, id1);
            HirDugum d2 = liste_al(o.cfg_prog.hir_prog.arena, id2);

            if (d1.tip == 14 && (d2.tip == 15 || d2.tip == 16)) {
                if (d2.sol_isim == d1.hedef) {
                    d1.tip = 17; // HIR_BIRLESTIRILMIS_OP
                    d1.hedef = d2.hedef;
                    d1.birlestirilmis_ops = (d2.tip == 15) ? "matmul+rmsnorm" : "matmul+softmax";
                    liste_guncelle(o.cfg_prog.hir_prog.arena, id1, d1);

                    d2.tip = 0;
                    d2.hedef = "";
                    liste_guncelle(o.cfg_prog.hir_prog.arena, id2, d2);

                    fuzyon_sayisi++;
                    o.degisiklik_yapildi = 1;
                }
            }
        }
    }
    return fuzyon_sayisi;
}

// 4. LICM (Loop Invariant Code Motion)
tamsayi _pass_licm(Optimizator o, CfgIslev f) {
    tamsayi tasinan = 0;
    int blen = (int)liste_uzunluk(f.bloklar);

    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        int slen = (int)liste_uzunluk(b.sonraki_bloklar);

        std::string back_target;
        for (int si = 0; si < slen; si++) {
            metin next_lbl = liste_al(b.sonraki_bloklar, si);
            for (int prev_i = 0; prev_i <= bi; prev_i++) {
                TemelBlok pb = liste_al(f.bloklar, prev_i);
                if (pb.etiket == next_lbl) {
                    back_target = next_lbl;
                    break;
                }
            }
            if (!back_target.empty()) break;
        }

        if (!back_target.empty()) {
            std::set<std::string> dongude_degisen;
            int tlen = (int)liste_uzunluk(b.talimatlar);
            for (int ti = 0; ti < tlen; ti++) {
                tamsayi instr_id = liste_al(b.talimatlar, ti);
                if (instr_id >= 0 && instr_id < (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) {
                    HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
                    if (!d.hedef.empty()) dongude_degisen.insert(d.hedef);
                }
            }

            int preheader_idx = -1;
            for (int pi = 0; pi < blen; pi++) {
                TemelBlok pb = liste_al(f.bloklar, pi);
                if (pb.etiket == back_target) {
                    int prev_len = (int)liste_uzunluk(pb.onceki_bloklar);
                    for (int ppi = 0; ppi < prev_len; ppi++) {
                        metin plbl = liste_al(pb.onceki_bloklar, ppi);
                        if (plbl != b.etiket) {
                            for (int k = 0; k < blen; k++) {
                                if (liste_al(f.bloklar, k).etiket == plbl) {
                                    preheader_idx = k;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }
            }

            if (preheader_idx >= 0) {
                TemelBlok pre_b = liste_al(f.bloklar, preheader_idx);
                Liste<tamsayi> yeni_govde_talimatlar;

                for (int ti = 0; ti < tlen; ti++) {
                    tamsayi instr_id = liste_al(b.talimatlar, ti);
                    if (instr_id >= 0 && instr_id < (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) {
                        HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
                        if ((d.tip == 0 || d.tip == 1) && !d.hedef.empty()) {
                            bool sol_degismiyor = (d.sol_isim.empty() || dongude_degisen.find(d.sol_isim) == dongude_degisen.end());
                            bool sag_degismiyor = (d.sag_isim.empty() || dongude_degisen.find(d.sag_isim) == dongude_degisen.end());
                            if (sol_degismiyor && sag_degismiyor) {
                                liste_ekle(pre_b.talimatlar, instr_id);
                                tasinan++;
                                o.degisiklik_yapildi = 1;
                                continue;
                            }
                        }
                    }
                    liste_ekle(yeni_govde_talimatlar, instr_id);
                }
                b.talimatlar = yeni_govde_talimatlar;
                liste_guncelle(f.bloklar, bi, b);
                liste_guncelle(f.bloklar, preheader_idx, pre_b);
            }
        }
    }
    return tasinan;
}

// 5. Sabit Dallanma Budama & Ulaşılmaz Blok Elemesi (Branch Pruning & Dead Block Elimination)
tamsayi _pass_dallanma_katlama(Optimizator o, CfgIslev f) {
    tamsayi budanan = 0;
    int blen = (int)liste_uzunluk(f.bloklar);
    if (blen <= 1) return 0;

    std::map<std::string, long long> bilinen_sabitler;
    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        int tlen = (int)liste_uzunluk(b.talimatlar);
        for (int ti = 0; ti < tlen; ti++) {
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id >= 0 && instr_id < (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) {
                HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
                if (d.tip == 0 && !d.hedef.empty() && !d.sabit_deger.empty() && _sayi_mi(d.sabit_deger)) {
                    try {
                        bilinen_sabitler[d.hedef] = std::stoll(d.sabit_deger);
                    } catch (...) {}
                }
            }
        }
    }

    // İki successor'ı olan (koşullu dallanma) blokları incele
    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        if (liste_uzunluk(b.sonraki_bloklar) == 2) {
            metin true_lbl = liste_al(b.sonraki_bloklar, 0);
            metin false_lbl = liste_al(b.sonraki_bloklar, 1);

            int tlen = (int)liste_uzunluk(b.talimatlar);
            std::string cond_var;
            for (int ti = tlen - 1; ti >= 0; ti--) {
                tamsayi instr_id = liste_al(b.talimatlar, ti);
                if (instr_id >= 0 && instr_id < (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) {
                    HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
                    if (!d.kosul_isim.empty()) {
                        cond_var = d.kosul_isim;
                        break;
                    }
                    if (!d.hedef.empty() && bilinen_sabitler.find(d.hedef) != bilinen_sabitler.end()) {
                        cond_var = d.hedef;
                        break;
                    }
                }
            }

            if (!cond_var.empty() && bilinen_sabitler.find(cond_var) != bilinen_sabitler.end()) {
                long long cond_val = bilinen_sabitler[cond_var];
                metin secilen = (cond_val != 0) ? true_lbl : false_lbl;
                metin elenen = (cond_val != 0) ? false_lbl : true_lbl;

                Liste<metin> yeni_succ;
                liste_ekle(yeni_succ, secilen);
                b.sonraki_bloklar = yeni_succ;
                liste_guncelle(f.bloklar, bi, b);

                // Elenen bloğun predecessor listesinden b.etiket'i çıkar
                for (int ki = 0; ki < blen; ki++) {
                    TemelBlok target_b = liste_al(f.bloklar, ki);
                    if (target_b.etiket == elenen) {
                        Liste<metin> yeni_prev;
                        int plen = (int)liste_uzunluk(target_b.onceki_bloklar);
                        for (int pi = 0; pi < plen; pi++) {
                            metin p_lbl = liste_al(target_b.onceki_bloklar, pi);
                            if (p_lbl != b.etiket) {
                                liste_ekle(yeni_prev, p_lbl);
                            }
                        }
                        target_b.onceki_bloklar = yeni_prev;
                        liste_guncelle(f.bloklar, ki, target_b);
                        break;
                    }
                }

                budanan++;
                o.degisiklik_yapildi = 1;
            }
        }
    }

    // Ulaşılabilirlik analizi (Reachability Analysis)
    std::set<std::string> ulasilabilir;
    std::queue<std::string> q;
    std::string start_label = f.giris_etiketi.empty() ? "giris" : f.giris_etiketi;
    q.push(start_label);
    ulasilabilir.insert(start_label);

    std::map<std::string, int> lbl_to_idx;
    for (int i = 0; i < blen; i++) {
        TemelBlok b = liste_al(f.bloklar, i);
        lbl_to_idx[b.etiket] = i;
    }

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();

        if (lbl_to_idx.find(curr) != lbl_to_idx.end()) {
            TemelBlok b = liste_al(f.bloklar, lbl_to_idx[curr]);
            int slen = (int)liste_uzunluk(b.sonraki_bloklar);
            for (int si = 0; si < slen; si++) {
                metin succ = liste_al(b.sonraki_bloklar, si);
                if (ulasilabilir.find(succ) == ulasilabilir.end()) {
                    ulasilabilir.insert(succ);
                    q.push(succ);
                }
            }
        }
    }

    // Ulaşılamayan blokların talimatlarını temizle (Dead Block Elimination)
    for (int i = 0; i < blen; i++) {
        TemelBlok b = liste_al(f.bloklar, i);
        if (ulasilabilir.find(b.etiket) == ulasilabilir.end()) {
            if (liste_uzunluk(b.talimatlar) > 0) {
                Liste<tamsayi> bos_talimatlar;
                b.talimatlar = bos_talimatlar;
                liste_guncelle(f.bloklar, i, b);
                budanan++;
                o.degisiklik_yapildi = 1;
            }
        }
    }

    return budanan;
}

tamsayi _optimize_islev(Optimizator o, tamsayi islev_idx) {
    tamsayi tur = 0;
    tamsayi maks_tur = 10;

    while (tur < maks_tur) {
        o.degisiklik_yapildi = 0;

        CfgProgram prog = o.cfg_prog;
        CfgIslev f = liste_al(prog.islevler, islev_idx);

        _pass_sabit_katlama(o, f);
        _pass_kernel_fuzyonu(o, f);
        _pass_licm(o, f);
        _pass_dallanma_katlama(o, f);
        _pass_olu_kod_elemesi(o, f);

        liste_guncelle(prog.islevler, islev_idx, f);
        o.cfg_prog = prog;

        if (o.degisiklik_yapildi == 0) {
            break;
        }

        tur = tur + 1;
    }

    return tur;
}

tamsayi optimize_et(Optimizator o) {
    tamsayi i = 0;
    CfgProgram prog = o.cfg_prog;
    tamsayi p_len = liste_uzunluk(prog.islevler);

    while (i < p_len) {
        _optimize_islev(o, i);
        i = i + 1;
    }

    return 0;
}