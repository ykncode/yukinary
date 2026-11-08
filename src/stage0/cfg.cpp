#include "ykn_std.h"
#include "hir.cpp"
#include <sstream>

// ============================================================
// Yukinary CFG (Kontrol Akış Grafiği) Oluşturucu
// ============================================================
// HIR programını alıp temel bloklara (Basic Blocks) ve bunlar arasındaki
// kontrol akışı kenarlarına (Edges) böler.
//
// CFG, SSA dönüşümü, canlılık analizi (Liveness Analysis) ve
// yazmaç tahsisi (Register Allocation) için temel veri yapısıdır.

// Temel Blok: Doğrusal (dallanmasız) çalışan talimatlar dizisi.
struct TemelBlok {
    metin etiket;
    Liste<tamsayi> talimatlar; // HIR_Dugum ID'leri

    // Sonraki ve önceki blokların etiket isimleri
    Liste<metin> sonraki_bloklar;
    Liste<metin> onceki_bloklar;
};

// CFG formundaki bir işlev
struct CfgIslev {
    metin isim;
    Liste<TemelBlok> bloklar; // Tüm bloklar burada tutulur
    metin giris_etiketi;
};

// Tüm programın CFG formu
struct CfgProgram {
    HirProgram hir_prog;
    Liste<CfgIslev> islevler;
};

struct CfgOlusturucu {
    HirProgram hir_prog;
    CfgProgram cfg_prog;
    tamsayi blok_sayaci;
};

CfgOlusturucu cfg_olusturucu_kur(HirProgram hir_prog) {
    CfgOlusturucu o;
    o.hir_prog = hir_prog;
    o.blok_sayaci = 0;
    return o;
}

metin _yeni_etiket(CfgOlusturucu o) {
    o.blok_sayaci = o.blok_sayaci + 1;
    metin lbl = metin_birlestir("B", tamsayi_metne(o.blok_sayaci));
    return lbl;
}

// Metin olarak virgülle ayrılmış ID'leri listeye çeviren (şimdilik kaba) bir yardımcı 

Liste<tamsayi> _ayristir_id_listesi(metin idler) {
    Liste<tamsayi> sonuc;
    std::stringstream ss(idler);
    std::string token;
    while (std::getline(ss, token, ',')) {
        size_t start = token.find_first_not_of(" \t\r\n");
        size_t end = token.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            std::string trimmed = token.substr(start, end - start + 1);
            if (!trimmed.empty()) {
                try {
                    liste_ekle(sonuc, (tamsayi)std::stoll(trimmed));
                } catch (...) {}
            }
        }
    }
    return sonuc;
}

// Blokları bulmak için indeks döndürür

tamsayi _blok_bul_idx(CfgIslev cfg_fn_nesnesi, metin etiket) {
    tamsayi i = 0;
    tamsayi len = liste_uzunluk(cfg_fn_nesnesi.bloklar);
    while (i < len) {
    TemelBlok b = liste_al(cfg_fn_nesnesi.bloklar, i);
        if (metin_esitmi(b.etiket, etiket) == 1) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

CfgIslev _cfg_islev_olustur(CfgOlusturucu o, HirIslev hir_fn) {
    o.blok_sayaci = 0;

    CfgIslev cfg_fn;
    cfg_fn.isim = hir_fn.isim;
    cfg_fn.giris_etiketi = "giris";

    TemelBlok baslangic_blok;
    baslangic_blok.etiket = "giris";
    liste_ekle(cfg_fn.bloklar, baslangic_blok);

    metin mevcut_etiket = "giris";

    // RECURSIVE EMIT SİMÜLASYONU (Düz liste olarak geziyoruz, aslında recursive olmalı)

    // Şimdilik sadece basit talimatları ve IF/WHILE temel çatısını ekliyoruz.


    tamsayi i = 0;
    tamsayi len = liste_uzunluk(hir_fn.govde);

    while (i < len) {
    tamsayi instr_id = liste_al(hir_fn.govde, i);
    HirProgram prog = o.hir_prog;
    HirDugum d = liste_al(prog.arena, instr_id);

    tamsayi b_idx = _blok_bul_idx(cfg_fn, mevcut_etiket);
    TemelBlok mevcut_blok = liste_al(cfg_fn.bloklar, b_idx);

        if (d.tip == HIR_EGER) {
            // True, False ve Merge etiketleri üret
            metin dogru_etiket = _yeni_etiket(o);
            metin yanlis_etiket = _yeni_etiket(o);
            metin merge_etiket = _yeni_etiket(o);

            // Mevcut bloktan dallan
            liste_ekle(mevcut_blok.sonraki_bloklar, dogru_etiket);
            liste_ekle(mevcut_blok.sonraki_bloklar, yanlis_etiket);
            liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);

            // Doğru bloğu ve talimatları
            TemelBlok dogru_blok;
            dogru_blok.etiket = dogru_etiket;
            Liste<tamsayi> dogru_ids = _ayristir_id_listesi(d.dogru_govde);
            for (size_t di = 0; di < liste_uzunluk(dogru_ids); di++) {
                liste_ekle(dogru_blok.talimatlar, liste_al(dogru_ids, di));
            }
            liste_ekle(dogru_blok.sonraki_bloklar, merge_etiket);
            liste_ekle(cfg_fn.bloklar, dogru_blok);

            // Yanlış bloğu ve talimatları
            TemelBlok yanlis_blok;
            yanlis_blok.etiket = yanlis_etiket;
            Liste<tamsayi> yanlis_ids = _ayristir_id_listesi(d.yanlis_govde);
            for (size_t yi = 0; yi < liste_uzunluk(yanlis_ids); yi++) {
                liste_ekle(yanlis_blok.talimatlar, liste_al(yanlis_ids, yi));
            }
            liste_ekle(yanlis_blok.sonraki_bloklar, merge_etiket);
            liste_ekle(cfg_fn.bloklar, yanlis_blok);

            // Merge bloğu
            TemelBlok merge_blok;
            merge_blok.etiket = merge_etiket;
            liste_ekle(cfg_fn.bloklar, merge_blok);

            mevcut_etiket = merge_etiket;

        } else if (d.tip == HIR_DONGU) {
            metin baslik_etiket = _yeni_etiket(o);
            metin govde_etiket = _yeni_etiket(o);
            metin cikis_etiket = _yeni_etiket(o);

            liste_ekle(mevcut_blok.sonraki_bloklar, baslik_etiket);
            liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);

            // Başlık bloğu
            TemelBlok baslik_blok;
            baslik_blok.etiket = baslik_etiket;
            liste_ekle(baslik_blok.sonraki_bloklar, govde_etiket);
            liste_ekle(baslik_blok.sonraki_bloklar, cikis_etiket);
            liste_ekle(cfg_fn.bloklar, baslik_blok);

            // Gövde bloğu ve talimatları
            TemelBlok govde_blok;
            govde_blok.etiket = govde_etiket;
            Liste<tamsayi> govde_ids = _ayristir_id_listesi(d.dongu_govde);
            for (size_t gi = 0; gi < liste_uzunluk(govde_ids); gi++) {
                liste_ekle(govde_blok.talimatlar, liste_al(govde_ids, gi));
            }
            liste_ekle(govde_blok.sonraki_bloklar, baslik_etiket); // Back-edge
            liste_ekle(cfg_fn.bloklar, govde_blok);

            // Çıkış bloğu
            TemelBlok cikis_blok;
            cikis_blok.etiket = cikis_etiket;
            liste_ekle(cfg_fn.bloklar, cikis_blok);

            mevcut_etiket = cikis_etiket;
        } else if (d.tip == HIR_DONDUR) {
            // Fonksiyon dönüşü — mevcut bloğu sonlandırır (fallthrough yok)
            liste_ekle(mevcut_blok.talimatlar, instr_id);
            liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);
            if (i + 1 < len) {
                metin sonraki_etiket = _yeni_etiket(o);
                TemelBlok sonraki_blok;
                sonraki_blok.etiket = sonraki_etiket;
                liste_ekle(cfg_fn.bloklar, sonraki_blok);
                mevcut_etiket = sonraki_etiket;
            }
        } else {
            // Sade talimat
            liste_ekle(mevcut_blok.talimatlar, instr_id);
            liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);
        }

        i = i + 1;
    }

    // Predecessor (Önceki) kenarlarını bağla

    tamsayi bi = 0;
    tamsayi blen = liste_uzunluk(cfg_fn.bloklar);
    while (bi < blen) {
    TemelBlok b = liste_al(cfg_fn.bloklar, bi);
    tamsayi si = 0;
    tamsayi slen = liste_uzunluk(b.sonraki_bloklar);
        while (si < slen) {
    metin succ_lbl = liste_al(b.sonraki_bloklar, si);
    tamsayi succ_idx = _blok_bul_idx(cfg_fn, succ_lbl);
            if (succ_idx != -1) {
    TemelBlok succ_b = liste_al(cfg_fn.bloklar, succ_idx);

                // Zaten var mı kontrol et

    tamsayi pi = 0;
    tamsayi plen = liste_uzunluk(succ_b.onceki_bloklar);
    tamsayi var_mi = 0;
                while (pi < plen) {
    metin p_lbl = liste_al(succ_b.onceki_bloklar, pi);
                    if (metin_esitmi(p_lbl, b.etiket) == 1) {
                        var_mi = 1;
                        break;
                    }
                    pi = pi + 1;
                }

                if (var_mi == 0) {
                    liste_ekle(succ_b.onceki_bloklar, b.etiket);
                    liste_guncelle(cfg_fn.bloklar, succ_idx, succ_b);
                }
            }
            si = si + 1;
        }
        bi = bi + 1;
    }

    return cfg_fn;
}

CfgProgram cfg_olustur(CfgOlusturucu o) {
    tamsayi pi = 0;
    HirProgram hprog = o.hir_prog;
    tamsayi p_len = liste_uzunluk(hprog.islevler);
    o.cfg_prog.hir_prog = hprog;
    while (pi < p_len) {
        HirIslev f = liste_al(hprog.islevler, pi);
        CfgIslev cfg_f = _cfg_islev_olustur(o, f);
        liste_ekle(o.cfg_prog.islevler, cfg_f);
        pi = pi + 1;
    }
    return o.cfg_prog;
}