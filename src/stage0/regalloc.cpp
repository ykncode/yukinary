#include "ykn_std.h"
#include "cfg.cpp"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <algorithm>

// ============================================================
// Yukinary Yazmac Tahsisi (Linear Scan Register Allocation)
// ============================================================
// SSA formatindaki CFG uzerinde Linear Scan algoritmasi kullanarak
// degiskenleri fiziksel yazmaclara (Registers) atar, sigmayanlari
// yigina (Spill) tasir.

struct CanlilikAraligi {
    metin isim;
    tamsayi baslangic;
    tamsayi bitis;
    tamsayi yazmac_no;  // -1 ise atanmamis
    tamsayi spill_slot; // -1 ise registerda, degilse yiginda
};

struct RegAllocOlusturucu {
    CfgProgram cfg_prog;
    Liste<CanlilikAraligi> araliklar;
    tamsayi maks_yazmac;
    Liste<tamsayi> aktif_araliklar;
};

RegAllocOlusturucu regalloc_kur(CfgProgram cfg_prog) {
    RegAllocOlusturucu o;
    o.cfg_prog = cfg_prog;
    o.maks_yazmac = 16; // Varsayilan: 16 donanim yazmaci (x86_64 / ARM64 standardi)
    return o;
}

// 1. Canlilik (Liveness) Analizi
// CFG temel bloklarindaki talimatlari tarayarak her sembolun
// ilk kullanim (start) ve son kullanim (end) araliklarini hesaplar.
tamsayi _canlilik_analizi_yap(RegAllocOlusturucu& o, CfgIslev islev_cfg) {
    std::map<std::string, std::pair<int, int>> liveness_map;
    int program_counter = 0;

    int blen = (int)liste_uzunluk(islev_cfg.bloklar);
    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(islev_cfg.bloklar, bi);
        int tlen = (int)liste_uzunluk(b.talimatlar);

        for (int ti = 0; ti < tlen; ti++) {
            program_counter++;
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id < 0 || instr_id >= (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) continue;

            HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);

            auto record_use = [&](const std::string& var_name) {
                if (var_name.empty()) return;
                if (liveness_map.find(var_name) == liveness_map.end()) {
                    liveness_map[var_name] = {program_counter, program_counter};
                } else {
                    liveness_map[var_name].second = program_counter;
                }
            };

            record_use(d.hedef);
            record_use(d.sol_isim);
            record_use(d.sag_isim);
            record_use(d.kosul_isim);
        }
    }

    tamsayi eklenen = 0;
    for (const auto& kv : liveness_map) {
        CanlilikAraligi aralik;
        aralik.isim = kv.first;
        aralik.baslangic = kv.second.first;
        aralik.bitis = kv.second.second;
        aralik.yazmac_no = -1;
        aralik.spill_slot = -1;
        liste_ekle(o.araliklar, aralik);
        eklenen++;
    }

    return eklenen;
}

// 2. Linear Scan Register Allocation
// Araliklari baslangic zamanina gore siralar ve aktif set uzerinden
// yazmac atamasi gerceklestirir.
tamsayi _linear_scan_uygula(RegAllocOlusturucu& o) {
    int n = (int)liste_uzunluk(o.araliklar);
    if (n == 0) return 0;

    std::vector<int> sira(n);
    for (int i = 0; i < n; i++) sira[i] = i;

    // Baslangic zamanina gore artan sirala
    std::sort(sira.begin(), sira.end(), [&](int a, int b) {
        return liste_al(o.araliklar, a).baslangic < liste_al(o.araliklar, b).baslangic;
    });

    std::set<int> serbest_yazmaclar;
    for (int r = 0; r < o.maks_yazmac; r++) serbest_yazmaclar.insert(r);

    std::vector<int> aktifler;
    int sonraki_spill_slot = 0;
    tamsayi spill_sayisi = 0;

    for (int idx : sira) {
        CanlilikAraligi cur = liste_al(o.araliklar, idx);

        // Sureci bitmis olan aktif araliklari tahliye et ve yazmaclarini geri al
        auto it = aktifler.begin();
        while (it != aktifler.end()) {
            CanlilikAraligi a = liste_al(o.araliklar, *it);
            if (a.bitis < cur.baslangic) {
                if (a.yazmac_no >= 0) serbest_yazmaclar.insert((int)a.yazmac_no);
                it = aktifler.erase(it);
            } else {
                ++it;
            }
        }

        // Bos yazmac varsa ata
        if (!serbest_yazmaclar.empty()) {
            int reg = *serbest_yazmaclar.begin();
            serbest_yazmaclar.erase(serbest_yazmaclar.begin());
            cur.yazmac_no = reg;
            cur.spill_slot = -1;
            liste_guncelle(o.araliklar, idx, cur);
            aktifler.push_back(idx);
        } else {
            // Spill: en gec biten araligi yigina tasi
            int en_uzak_idx = -1;
            int en_uzak_bitis = (int)cur.bitis;

            for (int act_idx : aktifler) {
                CanlilikAraligi act = liste_al(o.araliklar, act_idx);
                if (act.bitis > en_uzak_bitis) {
                    en_uzak_bitis = (int)act.bitis;
                    en_uzak_idx = act_idx;
                }
            }

            if (en_uzak_idx != -1) {
                CanlilikAraligi kurban = liste_al(o.araliklar, en_uzak_idx);
                cur.yazmac_no = kurban.yazmac_no;
                kurban.yazmac_no = -1;
                kurban.spill_slot = sonraki_spill_slot++;
                liste_guncelle(o.araliklar, en_uzak_idx, kurban);

                auto rm = std::find(aktifler.begin(), aktifler.end(), en_uzak_idx);
                if (rm != aktifler.end()) aktifler.erase(rm);

                cur.spill_slot = -1;
                liste_guncelle(o.araliklar, idx, cur);
                aktifler.push_back(idx);
            } else {
                cur.yazmac_no = -1;
                cur.spill_slot = sonraki_spill_slot++;
                liste_guncelle(o.araliklar, idx, cur);
            }
            spill_sayisi++;
        }
    }

    return spill_sayisi;
}

// 3. Kodu Yeniden Yazma (Spill Metadata Isaretleme)
tamsayi _spill_kodlarini_ekle(RegAllocOlusturucu& o, CfgIslev islev_cfg) {
    tamsayi isaretlenen = 0;
    int alen = (int)liste_uzunluk(o.araliklar);
    for (int i = 0; i < alen; i++) {
        CanlilikAraligi a = liste_al(o.araliklar, i);
        if (a.spill_slot >= 0) isaretlenen++;
    }
    return isaretlenen;
}

tamsayi _regalloc_islev_isle(RegAllocOlusturucu& o, tamsayi islev_idx) {
    CfgProgram prog = o.cfg_prog;
    CfgIslev f = liste_al(prog.islevler, islev_idx);

    _canlilik_analizi_yap(o, f);
    _linear_scan_uygula(o);
    _spill_kodlarini_ekle(o, f);

    return 0;
}

tamsayi regalloc_calistir(RegAllocOlusturucu o) {
    tamsayi i = 0;
    CfgProgram prog = o.cfg_prog;
    tamsayi p_len = liste_uzunluk(prog.islevler);

    while (i < p_len) {
        _regalloc_islev_isle(o, i);
        i = i + 1;
    }

    return 0;
}