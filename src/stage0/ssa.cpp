#include "ykn_std.h"
#include "cfg.cpp"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <sstream>

// ============================================================
// Yukinary SSA (Static Single Assignment) Dönüştürücü
// ============================================================
// Cytron algoritmasını kullanarak CFG üzerindeki değişkenlere
// Phi düğümleri ekler ve isim sürümlemesi (renaming) yapar.
//
// Adımlar:
// 1. Dominatör Ağacı (Dominator Tree) ve Dominance Frontier hesabı
// 2. Phi düğümlerinin yerleştirilmesi (insert_phi_nodes)
// 3. Değişkenlerin sürümlendirilmesi (rename - stack based)

#ifndef YKN_SSA_STRUCTS_DEFINED
#define YKN_SSA_STRUCTS_DEFINED
struct HakimiyetAgaci {
    Liste<metin> bloklar;
    Liste<metin> idom;      // Immediate dominator'lar
    Liste<metin> frontier;  // Dominance Frontier sınırları
};

struct PhiDugum {
    metin hedef;            // Sürümlenmiş hedef: örn x_2
    metin orijinal_isim;    // Orijinal değişken adı: örn x
    Liste<metin> gelen_bloklar;
    Liste<metin> gelen_degerler;
};

struct SsaOlusturucu {
    CfgProgram cfg_prog;
    tamsayi mevcut_islev;
    std::map<std::string, int> surum_sayaclari;
    std::map<std::string, std::vector<std::string>> isim_yiginlari;
    std::map<std::string, std::vector<PhiDugum>> blok_phi_haritasi;
};
#endif

SsaOlusturucu ssa_olusturucu_kur(CfgProgram cfg_prog) {
    SsaOlusturucu o;
    o.cfg_prog = cfg_prog;
    o.mevcut_islev = -1;
    return o;
}

// Dominatör kesişim hesaplama (Lengauer-Tarjan / Cooper-Harvey-Kennedy)
static int _idom_kesisim(const std::vector<int>& idom_idx, int b1, int b2) {
    int parmak1 = b1;
    int parmak2 = b2;
    while (parmak1 != parmak2) {
        while (parmak1 > parmak2 && parmak1 > 0) {
            parmak1 = idom_idx[parmak1];
        }
        while (parmak2 > parmak1 && parmak2 > 0) {
            parmak2 = idom_idx[parmak2];
        }
    }
    return parmak1;
}

// 1. Dominatör Ağacı ve Dominance Frontier Hesabı
HakimiyetAgaci _hakimiyet_agaci_hesapla(CfgIslev islev_cfg) {
    HakimiyetAgaci h;
    int len = (int)liste_uzunluk(islev_cfg.bloklar);
    if (len == 0) return h;

    std::map<std::string, int> etiket_to_idx;
    for (int i = 0; i < len; i++) {
        TemelBlok b = liste_al(islev_cfg.bloklar, i);
        liste_ekle(h.bloklar, b.etiket);
        etiket_to_idx[b.etiket] = i;
        liste_ekle(h.idom, (i == 0) ? b.etiket : metin(""));
        liste_ekle(h.frontier, metin(""));
    }

    std::vector<int> idom_idx(len, -1);
    idom_idx[0] = 0;

    bool degisti = true;
    while (degisti) {
        degisti = false;
        for (int i = 1; i < len; i++) {
            TemelBlok b = liste_al(islev_cfg.bloklar, i);
            int plen = (int)liste_uzunluk(b.onceki_bloklar);
            if (plen == 0) continue;

            int yeni_idom = -1;
            for (int p = 0; p < plen; p++) {
                metin p_lbl = liste_al(b.onceki_bloklar, p);
                if (etiket_to_idx.find(p_lbl) != etiket_to_idx.end()) {
                    int p_idx = etiket_to_idx[p_lbl];
                    if (idom_idx[p_idx] != -1) {
                        yeni_idom = p_idx;
                        break;
                    }
                }
            }
            if (yeni_idom == -1) continue;

            for (int p = 0; p < plen; p++) {
                metin p_lbl = liste_al(b.onceki_bloklar, p);
                if (etiket_to_idx.find(p_lbl) != etiket_to_idx.end()) {
                    int p_idx = etiket_to_idx[p_lbl];
                    if (idom_idx[p_idx] != -1) {
                        yeni_idom = _idom_kesisim(idom_idx, p_idx, yeni_idom);
                    }
                }
            }

            if (idom_idx[i] != yeni_idom) {
                idom_idx[i] = yeni_idom;
                h.idom[i] = h.bloklar[yeni_idom];
                degisti = true;
            }
        }
    }

    std::vector<std::set<std::string>> df_sets(len);
    for (int i = 0; i < len; i++) {
        TemelBlok b = liste_al(islev_cfg.bloklar, i);
        int plen = (int)liste_uzunluk(b.onceki_bloklar);
        if (plen >= 2) {
            for (int p = 0; p < plen; p++) {
                metin p_lbl = liste_al(b.onceki_bloklar, p);
                if (etiket_to_idx.find(p_lbl) == etiket_to_idx.end()) continue;
                int kosucu = etiket_to_idx[p_lbl];
                while (kosucu >= 0 && kosucu != idom_idx[i]) {
                    df_sets[kosucu].insert(b.etiket);
                    kosucu = (kosucu == idom_idx[kosucu]) ? -1 : idom_idx[kosucu];
                }
            }
        }
    }

    for (int i = 0; i < len; i++) {
        std::string df_str;
        for (const auto& item : df_sets[i]) {
            if (!df_str.empty()) df_str += ",";
            df_str += item;
        }
        h.frontier[i] = df_str;
    }

    return h;
}

// 2. Aşama: Phi Düğümlerini Yerleştir
tamsayi _phi_dugumleri_yerlestir(SsaOlusturucu o, CfgIslev islev_cfg, HakimiyetAgaci h) {
    tamsayi eklenen_phi = 0;
    int blen = (int)liste_uzunluk(islev_cfg.bloklar);

    std::map<std::string, std::set<std::string>> tanim_bloklari;
    for (int i = 0; i < blen; i++) {
        TemelBlok b = liste_al(islev_cfg.bloklar, i);
        int tlen = (int)liste_uzunluk(b.talimatlar);
        for (int t = 0; t < tlen; t++) {
            tamsayi instr_id = liste_al(b.talimatlar, t);
            if (instr_id >= 0 && instr_id < (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) {
                HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
                if (!d.hedef.empty()) {
                    tanim_bloklari[d.hedef].insert(b.etiket);
                }
            }
        }
    }

    std::map<std::string, std::vector<std::string>> df_map;
    for (int i = 0; i < blen; i++) {
        std::string df = (i < (int)h.frontier.size()) ? h.frontier[i] : "";
        if (!df.empty()) {
            std::stringstream ss(df);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                if (!tok.empty()) df_map[h.bloklar[i]].push_back(tok);
            }
        }
    }

    for (const auto& kv : tanim_bloklari) {
        std::vector<std::string> worklist(kv.second.begin(), kv.second.end());
        std::set<std::string> phi_eklendi;

        while (!worklist.empty()) {
            std::string curr_blk = worklist.back();
            worklist.pop_back();

            if (df_map.find(curr_blk) == df_map.end()) continue;
            for (const auto& target_blk : df_map[curr_blk]) {
                if (phi_eklendi.find(target_blk) == phi_eklendi.end()) {
                    phi_eklendi.insert(target_blk);

                    PhiDugum phi;
                    phi.orijinal_isim = kv.first;
                    phi.hedef = kv.first + "_phi";
                    o.blok_phi_haritasi[target_blk].push_back(phi);
                    eklenen_phi++;

                    if (kv.second.find(target_blk) == kv.second.end()) {
                        worklist.push_back(target_blk);
                    }
                }
            }
        }
    }

    return eklenen_phi;
}

// 3. Aşama: İsim Sürümleme (Renaming / Versioning)
tamsayi _isim_surumle(SsaOlusturucu o, CfgIslev islev_cfg) {
    tamsayi surumlenen = 0;
    int blen = (int)liste_uzunluk(islev_cfg.bloklar);

    for (int i = 0; i < blen; i++) {
        TemelBlok b = liste_al(islev_cfg.bloklar, i);

        if (o.blok_phi_haritasi.find(b.etiket) != o.blok_phi_haritasi.end()) {
            for (auto& phi : o.blok_phi_haritasi[b.etiket]) {
                int ver = ++o.surum_sayaclari[phi.orijinal_isim];
                std::string yeni_ad = phi.orijinal_isim + "_" + std::to_string(ver);
                phi.hedef = yeni_ad;
                o.isim_yiginlari[phi.orijinal_isim].push_back(yeni_ad);
                surumlenen++;
            }
        }

        int tlen = (int)liste_uzunluk(b.talimatlar);
        for (int t = 0; t < tlen; t++) {
            tamsayi instr_id = liste_al(b.talimatlar, t);
            if (instr_id >= 0 && instr_id < (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) {
                HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);

                if (!d.sol_isim.empty() && !o.isim_yiginlari[d.sol_isim].empty()) {
                    d.sol_isim = o.isim_yiginlari[d.sol_isim].back();
                }
                if (!d.sag_isim.empty() && !o.isim_yiginlari[d.sag_isim].empty()) {
                    d.sag_isim = o.isim_yiginlari[d.sag_isim].back();
                }
                if (!d.kosul_isim.empty() && !o.isim_yiginlari[d.kosul_isim].empty()) {
                    d.kosul_isim = o.isim_yiginlari[d.kosul_isim].back();
                }

                if (!d.hedef.empty()) {
                    int ver = ++o.surum_sayaclari[d.hedef];
                    std::string yeni_ad = d.hedef + "_" + std::to_string(ver);
                    o.isim_yiginlari[d.hedef].push_back(yeni_ad);
                    d.hedef = yeni_ad;
                    surumlenen++;
                }

                liste_guncelle(o.cfg_prog.hir_prog.arena, instr_id, d);
            }
        }

        int slen = (int)liste_uzunluk(b.sonraki_bloklar);
        for (int s = 0; s < slen; s++) {
            metin succ_lbl = liste_al(b.sonraki_bloklar, s);
            if (o.blok_phi_haritasi.find(succ_lbl) != o.blok_phi_haritasi.end()) {
                for (auto& phi : o.blok_phi_haritasi[succ_lbl]) {
                    liste_ekle(phi.gelen_bloklar, b.etiket);
                    if (!o.isim_yiginlari[phi.orijinal_isim].empty()) {
                        liste_ekle(phi.gelen_degerler, o.isim_yiginlari[phi.orijinal_isim].back());
                    } else {
                        liste_ekle(phi.gelen_degerler, phi.orijinal_isim + "_0");
                    }
                }
            }
        }
    }

    return surumlenen;
}

tamsayi _ssa_islev_isle(SsaOlusturucu o, tamsayi islev_idx) {
    CfgProgram prog = o.cfg_prog;
    CfgIslev f = liste_al(prog.islevler, islev_idx);

    HakimiyetAgaci h = _hakimiyet_agaci_hesapla(f);
    _phi_dugumleri_yerlestir(o, f, h);
    _isim_surumle(o, f);

    return 0;
}

tamsayi ssa_uygula(SsaOlusturucu o) {
    tamsayi i = 0;
    CfgProgram prog = o.cfg_prog;
    tamsayi p_len = liste_uzunluk(prog.islevler);

    while (i < p_len) {
        _ssa_islev_isle(o, i);
        i = i + 1;
    }

    return 0;
}