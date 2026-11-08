#include "ykn_std.h"
struct Dugum {
    tamsayi id;
    tamsayi ust_kapsam;
    tamsayi yoksa_id;
    tamsayi koleksiyon_id;
    tamsayi yakala_id;
    tamsayi nihayet_id;
    tamsayi sync_id;
    tamsayi tip;
    metin metinsel_deger;
    metin veri_tipi;
    tamsayi sol_id;
    tamsayi sag_id;
    tamsayi kosul_id;
    tamsayi govde_id;
    tamsayi alternatif_id;
    tamsayi dongu_kosul_id;
    tamsayi dongu_govde_id;
    tamsayi dongu_baslangic_id;
    tamsayi dongu_bitis_id;
    tamsayi dongu_adim_id;
    metin yapi_adi;
    metin yapi_uyeleri;
    metin cocuklar;
};
struct Jeton {
    tamsayi tip;
    metin deger;
    tamsayi satir;
    tamsayi sutun;
};
struct ParserDurumu {
    tamsayi poz;
    tamsayi hata_var;
    tamsayi derinlik;
};
struct TipKontrolDurumu {
    tamsayi hata_var;
};
struct Sembol {
    metin isim;
    metin tip;
};
struct KapsamYigiti {
    Liste<Liste<Sembol>> kapsamlar;
    tamsayi mevcut_seviye;
};

Liste<Dugum>* g_ast_arena = nullptr;
tamsayi g_root_id = -1;

struct HirDugum {
    tamsayi id;
    tamsayi tip;
    metin hedef;
    metin sol_isim;
    metin sag_isim;
    metin kosul_isim;
    tamsayi op;
    metin etiket_adi;
    metin dogru_hedef;
    metin yanlis_hedef;
    metin islev_adi;
    metin birlestirilmis_ops;
    metin sabit_deger;
    metin cocuklar;
    metin dogru_govde;
    metin yanlis_govde;
    metin dongu_govde;
    metin dongu_kosul_ifadeleri;
};
struct HirIslev {
    metin isim;
    metin parametreler;
    metin donus_tipi;
    tamsayi gpu_mu;
    Liste<tamsayi> govde;
};
struct HirProgram {
    Liste<HirIslev> islevler;
    Liste<HirDugum> arena;
};
struct KapsamDegiskeni {
    metin anahtar;
    metin deger;
};
struct KapsamSeviyesi {
    Liste<KapsamDegiskeni> degiskenler;
};
struct HirOlusturucu {
    Liste<Dugum> ast_arena;
    HirProgram program;
    tamsayi temp_sayaci;
    tamsayi golge_sayaci;
    Liste<KapsamSeviyesi> kapsam_yigiti;
    tamsayi mevcut_islev;
};
struct TemelBlok {
    metin etiket;
    Liste<tamsayi> talimatlar;
    Liste<metin> sonraki_bloklar;
    Liste<metin> onceki_bloklar;
};
struct CfgIslev {
    metin isim;
    Liste<TemelBlok> bloklar;
    metin giris_etiketi;
};
struct CfgProgram {
    Liste<CfgIslev> islevler;
    HirProgram hir_prog;
};
struct CfgOlusturucu {
    HirProgram hir_prog;
    CfgProgram cfg_prog;
    tamsayi blok_sayaci;
};
struct HakimiyetAgaci {
    Liste<metin> bloklar;
    Liste<metin> idom;
    Liste<metin> frontier;
};
struct PhiDugum {
    metin hedef;
    metin orijinal_isim;
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
struct Optimizator {
    CfgProgram cfg_prog;
    tamsayi degisiklik_yapildi;
};
struct CanlilikAraligi {
    metin isim;
    tamsayi baslangic;
    tamsayi bitis;
    tamsayi yazmac_no;
    tamsayi spill_slot;
};
struct RegAllocOlusturucu {
    CfgProgram cfg_prog;
    Liste<CanlilikAraligi> araliklar;
    tamsayi maks_yazmac;
    Liste<tamsayi> aktif_araliklar;
};
struct MirDugum {
    tamsayi id;
    tamsayi tip;
    tamsayi yazmac_hedef;
    tamsayi yazmac_kaynak1;
    tamsayi yazmac_kaynak2;
    metin anlik_deger;
    metin etiket;
};
struct MirBlok {
    metin etiket;
    Liste<tamsayi> talimatlar;
};
struct MirIslev {
    metin isim;
    Liste<MirBlok> bloklar;
};
struct MirProgram {
    Liste<MirIslev> islevler;
    Liste<MirDugum> arena;
};
struct MirOlusturucu {
    CfgProgram cfg_prog;
    MirProgram mir_prog;
};
struct KodUretici {
    MirProgram mir_prog;
    metin hedef_dil;
    metin cikti_kodu;
};
struct LocaleKelime {
    tamsayi token_id;
    metin yerel_isim;
};
tamsayi harf_mi(tamsayi k);
tamsayi rakam_mi(tamsayi k);
tamsayi bosluk_mi(tamsayi k);
tamsayi alt_cizgi_mi(tamsayi k);
tamsayi kimlik_karakter_mi(tamsayi k);
tamsayi anahtar_kelime_tanimla(metin isim);
tamsayi jeton_ekle(Liste<Jeton>& jetonlar, tamsayi tip, metin deger, tamsayi sat, tamsayi sut);
tamsayi tokenize_et(metin kaynak, Liste<Jeton>& jetonlar);
tamsayi test_lexer();
tamsayi parser_hata(metin mesaj, tamsayi satir);
Jeton suanki_jeton(Liste<Jeton>& jetonlar, ParserDurumu& durum);
Jeton oncu_jeton(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi offset);
tamsayi ilerle(ParserDurumu& durum);
tamsayi bekle(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi beklenen_tip);
tamsayi operator_onceligi(tamsayi tip);
tamsayi parse_prefix(Liste<Jeton>& jetonlar, ParserDurumu& durum, Liste<Dugum>& ast_arena);
tamsayi parse_infix(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi sol_id, Liste<Dugum>& ast_arena);
tamsayi parse_expression(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi oncelik, Liste<Dugum>& ast_arena);
tamsayi parse_statement(Liste<Jeton>& jetonlar, ParserDurumu& durum, Liste<Dugum>& ast_arena);
tamsayi parse_block(Liste<Jeton>& jetonlar, ParserDurumu& durum, Liste<Dugum>& ast_arena);
tamsayi parser_cozumle(Liste<Jeton>& jetonlar, Liste<Dugum>& ast_arena);
tamsayi kapsam_yigiti_ilk_kur();
tamsayi kapsam_ekle(KapsamYigiti& yigit);
tamsayi kapsam_cikar(KapsamYigiti& yigit);
tamsayi sembol_ekle_kapsam(KapsamYigiti& yigit, metin isim, metin tip);
metin sembol_bul_kapsam(KapsamYigiti yigit, metin isim);
tamsayi sembol_ekle(metin isim, metin tip);
metin sembol_bul(metin isim);
tamsayi tip_hatasi(metin mesaj, tamsayi dugum_id);
tamsayi dugum_kontrol_et(Liste<Dugum>& ast_arena, tamsayi dugum_id, TipKontrolDurumu durum);
tamsayi tip_kontrol_baslat(Liste<Dugum>& ast_arena, tamsayi root_id);
tamsayi hir_dugum_olustur(HirProgram program, tamsayi tip);
HirOlusturucu hir_olusturucu_kur(Liste<Dugum>& ast_arena);
metin get_temp(HirOlusturucu& o);
tamsayi _kapsam_gir(HirOlusturucu& o);
tamsayi _kapsam_cik(HirOlusturucu& o);
metin _isim_coz(HirOlusturucu o, metin isim);
tamsayi _dis_kapsamda_var_mi(HirOlusturucu o, metin isim);
metin _tanimla(HirOlusturucu o, metin isim);
metin ziyaret_ifade(HirOlusturucu o, tamsayi dugum_id, tamsayi islev_idx);
tamsayi ziyaret_global(HirOlusturucu o, tamsayi dugum_id);
CfgOlusturucu cfg_olusturucu_kur(HirProgram hir_prog);
metin _yeni_etiket(CfgOlusturucu o);
Liste<tamsayi> _ayristir_id_listesi(metin idler);
tamsayi _blok_bul_idx(CfgIslev cfg_fn_nesnesi, metin etiket);
CfgIslev _cfg_islev_olustur(CfgOlusturucu o, HirIslev hir_fn);
CfgProgram cfg_olustur(CfgOlusturucu o);
SsaOlusturucu ssa_olusturucu_kur(CfgProgram cfg_prog);
HakimiyetAgaci _hakimiyet_agaci_hesapla(CfgIslev islev_cfg);
tamsayi _phi_dugumleri_yerlestir(SsaOlusturucu o, CfgIslev islev_cfg, HakimiyetAgaci h);
tamsayi _isim_surumle(SsaOlusturucu o, CfgIslev islev_cfg);
tamsayi _ssa_islev_isle(SsaOlusturucu o, tamsayi islev_idx);
tamsayi ssa_uygula(SsaOlusturucu o);
Optimizator optimizator_kur(CfgProgram cfg_prog);
tamsayi _pass_sabit_katlama(Optimizator o, CfgIslev f);
tamsayi _pass_olu_kod_elemesi(Optimizator o, CfgIslev f);
tamsayi _pass_kernel_fuzyonu(Optimizator o, CfgIslev f);
tamsayi _pass_licm(Optimizator o, CfgIslev f);
tamsayi _optimize_islev(Optimizator o, tamsayi islev_idx);
tamsayi optimize_et(Optimizator o);
RegAllocOlusturucu regalloc_kur(CfgProgram cfg_prog);
tamsayi _canlilik_analizi_yap(RegAllocOlusturucu o, CfgIslev islev_cfg);
tamsayi _linear_scan_uygula(RegAllocOlusturucu o);
tamsayi _spill_kodlarini_ekle(RegAllocOlusturucu o, CfgIslev islev_cfg);
tamsayi _regalloc_islev_isle(RegAllocOlusturucu o, tamsayi islev_idx);
tamsayi regalloc_calistir(RegAllocOlusturucu o);
MirOlusturucu mir_olusturucu_kur(CfgProgram cfg_prog);
tamsayi _talimat_secimi_yap(MirOlusturucu o, CfgIslev f);
tamsayi _peephole_optimizasyon_yap(MirOlusturucu o, CfgIslev f);
tamsayi _zamanlama_yap(MirOlusturucu o, CfgIslev f);
tamsayi _mir_islev_isle(MirOlusturucu o, tamsayi islev_idx);
tamsayi mir_olustur(MirOlusturucu o);
KodUretici kod_uretici_kur(MirProgram mir_prog, metin hedef);
metin _cpp_kodu_uret(KodUretici o);
metin _mir_reg_isim(tamsayi idx);
metin _asm_kodu_uret(KodUretici o);
metin kod_uret(KodUretici o);
metin onislemci_ic(metin dosya_yolu, Liste<metin>& ziyaret_edilenler);
metin onislemci_calistir(metin dosya_yolu);
tamsayi locale_kelime_ekle(tamsayi t_id, metin y_isim);
tamsayi temizle_ve_ekle(tamsayi token_id, metin val_part);
tamsayi locale_yukle(metin dil_kodu);
tamsayi locale_anahtar_kelime(metin isim);
tamsayi locale_hedef_kelime_ekle(tamsayi t_id, metin y_isim);
tamsayi hedef_temizle_ve_ekle(tamsayi token_id, metin val_part);
tamsayi locale_hedef_yukle(metin dil_kodu);
metin locale_hedef_getir(tamsayi t_id);
tamsayi _hedef_dizin_olustur(metin repo_yolu);
metin _repo_url_to_raw(metin repo_path);
tamsayi bagimliliklari_coz(metin kaynak);
tamsayi kullanimi_goster();
// --- START main.ykn ---
// --- START ast.ykn ---
    auto AST_BOS = 0;;
    auto AST_SAYI = 1;;
    auto AST_GERCEK = 2;;
    auto AST_METIN = 3;;
    auto AST_KIMLIK = 4;;
    auto AST_IKILI_OP = 5;;
    auto AST_ATAMA = 6;;
    auto AST_DEGISKEN_TANIMI = 7;;
    auto AST_EGER = 8;;
    auto AST_DONGU = 9;;
    auto AST_HER = 10;;
    auto AST_ISLEV = 11;;
    auto AST_ISLEV_CAGRISI = 12;;
    auto AST_METOD_CAGRISI = 13;;
    auto AST_DONDUR = 14;;
    auto AST_BLOK = 15;;
    auto AST_DURDUR = 16;;
    auto AST_DEVAM_ET = 17;;
    auto AST_ALAN_ERISIMI = 18;;
    auto AST_INDEKS_ERISIMI = 19;;
    auto AST_YAPI = 20;;
    auto AST_ICE_AKTAR = 23;;
    auto AST_DENEME = 21;;
    auto AST_FIRLATMA = 22;;
// --- END ast.ykn ---
// --- START lexer.ykn ---
    auto TIP_ISLEV = 1;;
    auto TIP_DEGISKEN = 2;;
    auto TIP_DONDUR = 3;;
    auto TIP_EGER = 4;;
    auto TIP_ISE = 5;;
    auto TIP_YOKSA = 6;;
    auto TIP_DONGU = 7;;
    auto TIP_HER = 8;;
    auto TIP_KIMLIK = 9;;
    auto TIP_TAMSAYI = 10;;
    auto TIP_GERCEK_SAYI = 11;;
    auto TIP_METIN = 12;;
    auto TIP_YAPI = 13;;
    auto TIP_NUMARALANDIRMA = 14;;
    auto TIP_ICE_AKTAR = 15;;
    auto TIP_BORU_HATTI = 16;;
    auto TIP_DURDUR = 17;;
    auto TIP_DEVAM_ET = 18;;
    auto TIP_DENEME = 19;;
    auto TIP_YAKALA = 20;;
    auto TIP_NIHAYET = 21;;
    auto TIP_FIRLATMA = 22;;
    auto TIP_KULLAN = 115;;
    auto TIP_SENKRONIZE_ET = 128;;
    auto TIP_ARTI = 30;;
    auto TIP_EKSI = 31;;
    auto TIP_CARPI = 32;;
    auto TIP_BOLME = 33;;
    auto TIP_ATAMA = 34;;
    auto TIP_ESIT_MI = 35;;
    auto TIP_ESIT_DEGIL = 36;;
    auto TIP_KUCUKTUR = 37;;
    auto TIP_BUYUKTUR = 38;;
    auto TIP_KUCUK_ESIT = 39;;
    auto TIP_BUYUK_ESIT = 40;;
    auto TIP_MOD = 41;;
    auto TIP_SOL_PARANTEZ = 50;;
    auto TIP_SAG_PARANTEZ = 51;;
    auto TIP_SOL_KUSLU = 52;;
    auto TIP_SAG_KUSLU = 53;;
    auto TIP_SOL_KOSELI = 54;;
    auto TIP_SAG_KOSELI = 55;;
    auto TIP_IKI_NOKTA = 56;;
    auto TIP_NOKTALI_VIRGUL = 57;;
    auto TIP_VIRGUL = 58;;
    auto TIP_OK = 59;;
    auto TIP_VE_MANTIK = 60;;
    auto TIP_VEYA_MANTIK = 61;;
    auto TIP_NOKTA = 62;;
    auto TIP_EOF = 99;;
tamsayi harf_mi(tamsayi k) {
if (k > 64) {
if (k < 91) { return 1; }
}
if (k > 96) {
if (k < 123) { return 1; }
}
if (k > 127) {
return 1;
}
return 0;
}
tamsayi rakam_mi(tamsayi k) {
if (k > 47) {
if (k < 58) { return 1; }
}
return 0;
}
tamsayi bosluk_mi(tamsayi k) {
if (k == 32) { return 1; }
if (k == 9) { return 1; }
if (k == 13) { return 1; }
if (k == 10) { return 1; }
return 0;
}
tamsayi alt_cizgi_mi(tamsayi k) {
if (k == 95) { return 1; }
return 0;
}
tamsayi kimlik_karakter_mi(tamsayi k) {
if (harf_mi(k) == 1) { return 1; }
if (rakam_mi(k) == 1) { return 1; }
if (alt_cizgi_mi(k) == 1) { return 1; }
return 0;
}
tamsayi anahtar_kelime_tanimla(metin isim) {
    tamsayi token_id = locale_anahtar_kelime(isim);;
if (token_id != 9) {
return token_id;
}
if (metin_esitmi(isim, "islev") == 1) { return TIP_ISLEV; }
if (metin_esitmi(isim, "isv") == 1) { return TIP_ISLEV; }
if (metin_esitmi(isim, "degisken") == 1) { return TIP_DEGISKEN; }
if (metin_esitmi(isim, "dondu") == 1) { return TIP_DONDUR; }
if (metin_esitmi(isim, "eger") == 1) { return TIP_EGER; }
if (metin_esitmi(isim, "yoksa") == 1) { return TIP_YOKSA; }
if (metin_esitmi(isim, "ise") == 1) { return TIP_ISE; }
if (metin_esitmi(isim, "dongu") == 1) { return TIP_DONGU; }
if (metin_esitmi(isim, "her") == 1) { return TIP_HER; }
if (metin_esitmi(isim, "yapi") == 1) { return TIP_YAPI; }
if (metin_esitmi(isim, "numaralandirma") == 1) { return TIP_NUMARALANDIRMA; }
if (metin_esitmi(isim, "durdur") == 1) { return 17; }
if (metin_esitmi(isim, "break") == 1) { return 17; }
if (metin_esitmi(isim, "devam_et") == 1) { return 18; }
if (metin_esitmi(isim, "continue") == 1) { return 18; }
if (metin_esitmi(isim, "deneme") == 1) { return 19; }
if (metin_esitmi(isim, "yakala") == 1) { return 20; }
if (metin_esitmi(isim, "nihayet") == 1) { return 21; }
if (metin_esitmi(isim, "firlatma") == 1) { return 22; }
if (metin_esitmi(isim, "ve") == 1) { return TIP_VE_MANTIK; }
if (metin_esitmi(isim, "veya") == 1) { return TIP_VEYA_MANTIK; }
return TIP_KIMLIK;
}
tamsayi jeton_ekle(Liste<Jeton>& jetonlar, tamsayi tip, metin deger, tamsayi sat, tamsayi sut) {
    Jeton j;
j.tip   = tip;
j.deger = deger;
j.satir = sat;
j.sutun = sut;
liste_ekle(jetonlar, j);
return 0;
}
tamsayi tokenize_et(metin kaynak, Liste<Jeton>& jetonlar) {
    tamsayi poz = 0;;
    tamsayi sat = 1;;
    tamsayi sut = 1;;
    tamsayi uzunluk;
uzunluk = metin_uzunlugu(kaynak);
while (poz < uzunluk) {
    tamsayi c;
c = karakter_kodu(karakter_al(kaynak, poz));
if (bosluk_mi(c) == 1) {
if (c == 10) {
sat = sat + 1;
sut = 1;
} else {
sut = sut + 1;
}
poz = poz + 1;
continue;
}
if (c == 35) {
while (poz < uzunluk && karakter_kodu(karakter_al(kaynak, poz)) != 10) {
poz = poz + 1;
}
continue;
}
    tamsayi c2 = 0;;
if (poz + 1 < uzunluk) {
c2 = karakter_kodu(karakter_al(kaynak, poz + 1));
}
if (c == 47 && c2 == 47) {
while (poz < uzunluk && karakter_kodu(karakter_al(kaynak, poz)) != 10) {
poz = poz + 1;
}
continue;
}
if (c == 34) {
poz = poz + 1;
sut = sut + 1;
    metin metin_ic;
metin_ic = "";
    tamsayi basti_sut = sut;;
while (poz < uzunluk) {
    tamsayi cc;
cc = karakter_kodu(karakter_al(kaynak, poz));
if (cc == 34) {
poz = poz + 1;
sut = sut + 1;
break;
}
if (cc == 92 && poz + 1 < uzunluk) {
poz = poz + 1;
    tamsayi esc;
esc = karakter_kodu(karakter_al(kaynak, poz));
if (esc == 110) { metin_ic = metin_birlestir(metin_ic, "\n"); }
else if (esc == 116) { metin_ic = metin_birlestir(metin_ic, "\t"); }
else if (esc == 34) { metin_ic = metin_birlestir(metin_ic, "\""); }
else if (esc == 92) { metin_ic = metin_birlestir(metin_ic, "\\"); }
else {
metin_ic = metin_birlestir(metin_ic, "\\");
metin_ic = metin_birlestir(metin_ic, karakter(esc));
}
poz = poz + 1;
sut = sut + 1;
} else {
metin_ic = metin_birlestir(metin_ic, karakter(cc));
poz = poz + 1;
sut = sut + 1;
}
}
jeton_ekle(jetonlar, TIP_METIN, metin_ic, sat, basti_sut);
continue;
}
if (harf_mi(c) == 1 || alt_cizgi_mi(c) == 1) {
    tamsayi basla = poz;;
    tamsayi basla_sut = sut;;
while (poz < uzunluk) {
    tamsayi kk;
kk = karakter_kodu(karakter_al(kaynak, poz));
if (kimlik_karakter_mi(kk) == 1) {
poz = poz + 1;
sut = sut + 1;
} else {
break;
}
}
    metin isim;
isim = alt_metin(kaynak, basla, poz);
    tamsayi tip_sayi;
tip_sayi = anahtar_kelime_tanimla(isim);
jeton_ekle(jetonlar, tip_sayi, isim, sat, basla_sut);
continue;
}
if (rakam_mi(c) == 1) {
    tamsayi basla = poz;;
    tamsayi basla_sut = sut;;
    tamsayi nokta_goruldu = 0;;
while (poz < uzunluk) {
    tamsayi kk;
kk = karakter_kodu(karakter_al(kaynak, poz));
if (rakam_mi(kk) == 1 || kk == 120 || kk == 88 || (kk >= 97 && kk <= 102) || (kk >= 65 && kk <= 70)) {
poz = poz + 1;
sut = sut + 1;
} else if (kk == 46) {
nokta_goruldu = 1;
poz = poz + 1;
sut = sut + 1;
} else {
break;
}
}
    metin sayi_metin;
sayi_metin = alt_metin(kaynak, basla, poz);
if (nokta_goruldu == 1) {
jeton_ekle(jetonlar, TIP_GERCEK_SAYI, sayi_metin, sat, basla_sut);
} else {
jeton_ekle(jetonlar, TIP_TAMSAYI, sayi_metin, sat, basla_sut);
}
continue;
}
if (c == 61 && c2 == 61) {
jeton_ekle(jetonlar, TIP_ESIT_MI, "==", sat, sut);
poz = poz + 2; sut = sut + 2;
continue;
}
if (c == 33 && c2 == 61) {
jeton_ekle(jetonlar, TIP_ESIT_DEGIL, "!=", sat, sut);
poz = poz + 2; sut = sut + 2;
continue;
}
if (c == 38) {
if (poz + 1 < uzunluk && karakter_kodu(karakter_al(kaynak, poz + 1)) == 38) {
jeton_ekle(jetonlar, TIP_VE_MANTIK, "&&", sat, sut);
poz = poz + 2; sut = sut + 2; continue;
} else {
jeton_ekle(jetonlar, 80, "&", sat, sut);
poz = poz + 1; sut = sut + 1; continue;
}
}
if (c == 45 && c2 == 62) {
jeton_ekle(jetonlar, TIP_OK, "->", sat, sut);
poz = poz + 2; sut = sut + 2;
continue;
}
if (c == 60 && c2 == 61) {
jeton_ekle(jetonlar, TIP_KUCUK_ESIT, "<=", sat, sut);
poz = poz + 2; sut = sut + 2;
continue;
}
if (c == 62 && c2 == 61) {
jeton_ekle(jetonlar, TIP_BUYUK_ESIT, ">=", sat, sut);
poz = poz + 2; sut = sut + 2;
continue;
}
if (c == 60 && c2 == 60) { jeton_ekle(jetonlar, 84, "<<", sat, sut); poz = poz + 2; sut = sut + 2; continue; }
if (c == 62 && c2 == 62) { jeton_ekle(jetonlar, 85, ">>", sat, sut); poz = poz + 2; sut = sut + 2; continue; }
if (c == 124) {
    if (c2 == 124) { jeton_ekle(jetonlar, TIP_VEYA_MANTIK, "||", sat, sut); poz = poz + 2; sut = sut + 2; continue; }
    jeton_ekle(jetonlar, 81, "|", sat, sut); poz = poz + 1; sut = sut + 1; continue;
}
if (c == 94) { jeton_ekle(jetonlar, 82, "^", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 126) { jeton_ekle(jetonlar, 83, "~", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 43) { jeton_ekle(jetonlar, TIP_ARTI, "+", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 45) { jeton_ekle(jetonlar, TIP_EKSI, "-", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 42) { jeton_ekle(jetonlar, TIP_CARPI, "*", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 47) { jeton_ekle(jetonlar, TIP_BOLME, "/", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 37) { jeton_ekle(jetonlar, TIP_MOD, "%", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 61) { jeton_ekle(jetonlar, TIP_ATAMA, "=", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 60) { jeton_ekle(jetonlar, TIP_KUCUKTUR, "<", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 62) { jeton_ekle(jetonlar, TIP_BUYUKTUR, ">", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 40) { jeton_ekle(jetonlar, TIP_SOL_PARANTEZ, "(", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 41) { jeton_ekle(jetonlar, TIP_SAG_PARANTEZ, ")", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 123) { jeton_ekle(jetonlar, TIP_SOL_KUSLU, "{", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 125) { jeton_ekle(jetonlar, TIP_SAG_KUSLU, "}", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 91) { jeton_ekle(jetonlar, TIP_SOL_KOSELI, "[", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 93) { jeton_ekle(jetonlar, TIP_SAG_KOSELI, "]", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 58) { jeton_ekle(jetonlar, TIP_IKI_NOKTA, ":", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 59) { jeton_ekle(jetonlar, TIP_NOKTALI_VIRGUL, ";", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 44) { jeton_ekle(jetonlar, TIP_VIRGUL, ",", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
if (c == 46) { jeton_ekle(jetonlar, TIP_NOKTA, ".", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
poz = poz + 1;
sut = sut + 1;
}
jeton_ekle(jetonlar, TIP_EOF, "", sat, sut);
return liste_uzunluk(jetonlar);
}
tamsayi test_lexer() {
    metin kaynak;
kaynak = "islev topla(a: tamsayi) -> tamsayi { dondu a + 7; }";
    Liste<Jeton> jetonlar;
    tamsayi n;
n = tokenize_et(kaynak, jetonlar);
yazdir(n);
return 0;
}
// --- END lexer.ykn ---
// --- START parser_v3.ykn ---
tamsayi parser_hata(metin mesaj, tamsayi satir) {
    metin m1 = metin_birlestir("[Parser Uyarisi] Satir ", tamsayi_metne(satir));;
    metin m2 = metin_birlestir(m1, ": ");;
    metin m3 = metin_birlestir(m2, mesaj);;
sistem_komutu(metin_birlestir("echo ", m3));
return 1;
}
Jeton suanki_jeton(Liste<Jeton>& jetonlar, ParserDurumu& durum) {
    tamsayi len = liste_uzunluk(jetonlar);;
if (durum.poz < len) {
return liste_al(jetonlar, durum.poz);
}
    Jeton eof;
eof.tip = 99;
return eof;
}
Jeton oncu_jeton(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi offset) {
    tamsayi idx = durum.poz + offset;;
    tamsayi len = liste_uzunluk(jetonlar);;
if (idx >= 0 && idx < len) {
return liste_al(jetonlar, idx);
}
    Jeton eof;
eof.tip = 99;
return eof;
}
tamsayi ilerle(ParserDurumu& durum) {
durum.poz = durum.poz + 1;
return 1;
}
tamsayi bekle(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi beklenen_tip) {
    Jeton j = suanki_jeton(jetonlar, durum);;
if (j.tip == beklenen_tip) {
ilerle(durum);
return 1;
}
    metin msg = metin_birlestir("Beklenmeyen jeton. Beklenen tip: ", tamsayi_metne(beklenen_tip));;
msg = metin_birlestir(msg, metin_birlestir(", Gelen: ", tamsayi_metne(j.tip)));
parser_hata(msg, j.satir);
durum.hata_var = 1;
ilerle(durum); // Sonsuz donguyu onle: beklenmeyen tokeni atla
return 0;
}
tamsayi operator_onceligi(tamsayi tip) {
if (tip == TIP_ATAMA) { return 1; }
if (tip == TIP_VE_MANTIK || tip == TIP_VEYA_MANTIK) { return 2; }
if (tip == TIP_ESIT_MI || tip == TIP_ESIT_DEGIL) { return 3; }
if (tip >= TIP_KUCUKTUR && tip <= TIP_BUYUK_ESIT) { return 4; }
if (tip == TIP_ARTI || tip == TIP_EKSI) { return 5; }
if (tip == TIP_CARPI || tip == TIP_BOLME || tip == TIP_MOD) { return 6; }
if (tip == 80 || tip == 81 || tip == 82 || tip == 84 || tip == 85) { return 5; }
if (tip == TIP_NOKTA) { return 8; }
if (tip == 54) { return 9; }
return 0;
}
tamsayi parse_prefix(Liste<Jeton>& jetonlar, ParserDurumu& durum, Liste<Dugum>& ast_arena) {
    Jeton j = suanki_jeton(jetonlar, durum);;
    Dugum n;
n.id = liste_uzunluk(ast_arena);
if (j.tip == TIP_TAMSAYI) {
n.tip = 1;
n.metinsel_deger = j.deger;
ilerle(durum);
liste_ekle(ast_arena, n);
return n.id;
}
if (j.tip == TIP_GERCEK_SAYI) {
n.tip = 2;
n.metinsel_deger = j.deger;
ilerle(durum);
liste_ekle(ast_arena, n);
return n.id;
}
if (j.tip == TIP_METIN) {
n.tip = 3;
n.metinsel_deger = j.deger;
ilerle(durum);
liste_ekle(ast_arena, n);
return n.id;
}
if (j.tip == TIP_KIMLIK) {
n.tip = 4;
n.metinsel_deger = j.deger;
ilerle(durum);
    Jeton sn = suanki_jeton(jetonlar, durum);;
if (sn.tip == TIP_SOL_PARANTEZ) {
ilerle(durum);
    Dugum cagri;
cagri.tip = 12;
cagri.metinsel_deger = j.deger;
cagri.cocuklar = "";
sn = suanki_jeton(jetonlar, durum);
while (sn.tip != TIP_SAG_PARANTEZ && sn.tip != TIP_EOF) {
    tamsayi arg_id = parse_expression(jetonlar, durum, 0, ast_arena);;
cagri.cocuklar = metin_birlestir(cagri.cocuklar, metin_birlestir(tamsayi_metne(arg_id), ","));
sn = suanki_jeton(jetonlar, durum);
if (sn.tip == TIP_VIRGUL) {
ilerle(durum);
}
sn = suanki_jeton(jetonlar, durum);
}
bekle(jetonlar, durum, TIP_SAG_PARANTEZ);
n.id = liste_uzunluk(ast_arena);
liste_ekle(ast_arena, n);
cagri.id = liste_uzunluk(ast_arena);
liste_ekle(ast_arena, cagri);
return cagri.id;
}
n.id = liste_uzunluk(ast_arena);
liste_ekle(ast_arena, n);
return n.id;
}
if (j.tip == TIP_SOL_PARANTEZ) {
ilerle(durum);
    tamsayi expr_id = parse_expression(jetonlar, durum, 0, ast_arena);;
bekle(jetonlar, durum, TIP_SAG_PARANTEZ);
return expr_id;
}
if (j.tip == 48 || j.tip == TIP_EKSI || j.tip == 52 || j.tip == 80) {
ilerle(durum);
    tamsayi operand_id = parse_prefix(jetonlar, durum, ast_arena);;
    Dugum unary;
unary.tip = 13;
unary.metinsel_deger = j.deger;
unary.sag_id = operand_id;
unary.id = liste_uzunluk(ast_arena);
liste_ekle(ast_arena, unary);
return unary.id;
}
parser_hata(metin_birlestir("Beklenmeyen prefix: ", j.deger), j.satir);
ilerle(durum); // hata durumunda token atla, sonsuz donguyu onle
durum.hata_var = 1;
return -1;
}
tamsayi parse_infix(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi sol_id, Liste<Dugum>& ast_arena) {
    Jeton op_jeton = suanki_jeton(jetonlar, durum);;
    tamsayi op_oncelik = operator_onceligi(op_jeton.tip);;
ilerle(durum);
    tamsayi sag_id = -1;;
    if (op_jeton.tip == 54) { sag_id = parse_expression(jetonlar, durum, 0, ast_arena); bekle(jetonlar, durum, 55); }
    else { sag_id = parse_expression(jetonlar, durum, op_oncelik, ast_arena); }
    Dugum n;
n.id = liste_uzunluk(ast_arena);
if (op_jeton.tip == TIP_ATAMA) {
n.tip = 6;
} else {
n.tip = 5;
}
n.metinsel_deger = op_jeton.deger;
n.sol_id = sol_id;
n.sag_id = sag_id;
liste_ekle(ast_arena, n);
return n.id;
}
tamsayi parse_expression(Liste<Jeton>& jetonlar, ParserDurumu& durum, tamsayi oncelik, Liste<Dugum>& ast_arena) {
    tamsayi sol_id = parse_prefix(jetonlar, durum, ast_arena);;
    Jeton j = suanki_jeton(jetonlar, durum);;
    tamsayi j_oncelik = operator_onceligi(j.tip);;
while (oncelik < j_oncelik) {
sol_id = parse_infix(jetonlar, durum, sol_id, ast_arena);
j = suanki_jeton(jetonlar, durum);
j_oncelik = operator_onceligi(j.tip);
}
return sol_id;
}
tamsayi parse_statement(Liste<Jeton>& jetonlar, ParserDurumu& durum, Liste<Dugum>& ast_arena) {
if (durum.derinlik > 500) {
// Cok derin veya dongu tespiti - kalan token'i atla ve geri don
ilerle(durum);
durum.derinlik = 0;
return -1;
}
if (durum.hata_var > 100) {
// Cok fazla hata - bir sonraki isv/yapi/degisken'e atla
    Jeton j = suanki_jeton(jetonlar, durum);;
while (j.tip != TIP_EOF && j.tip != TIP_ISLEV && j.tip != TIP_YAPI && j.tip != TIP_DEGISKEN) {
ilerle(durum);
j = suanki_jeton(jetonlar, durum);
}
durum.hata_var = 0;
return -1;
}
durum.derinlik = durum.derinlik + 1;
    Jeton j = suanki_jeton(jetonlar, durum);;
    tamsayi sonuc = -1;;
if (j.tip == TIP_DEGISKEN) {
ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
    metin tip_adi = "Bilinmiyor";;
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_IKI_NOKTA) {
ilerle(durum);
    metin tip_adi_str = "";
    while (suanki_jeton(jetonlar, durum).tip == 32) { tip_adi_str = metin_birlestir(tip_adi_str, "*"); ilerle(durum); }
    Jeton tmp_tip = suanki_jeton(jetonlar, durum);;
    tip_adi_str = metin_birlestir(tip_adi_str, tmp_tip.deger);
tip_adi = tip_adi_str;
ilerle(durum);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_KUCUKTUR) {
ilerle(durum);
    Jeton gen_tip = suanki_jeton(jetonlar, durum);;
tip_adi = metin_birlestir(tip_adi, "<");
tip_adi = metin_birlestir(tip_adi, gen_tip.deger);
ilerle(durum);
bekle(jetonlar, durum, TIP_BUYUKTUR);
tip_adi = metin_birlestir(tip_adi, ">");
}
}
    tamsayi sag_id = -1;;
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_ATAMA) {
ilerle(durum);
sag_id = parse_expression(jetonlar, durum, 0, ast_arena);
}
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
    Dugum n;
n.id = liste_uzunluk(ast_arena);
n.tip = 7;
n.metinsel_deger = isim_j.deger;
n.veri_tipi = tip_adi;
n.sag_id = sag_id;
liste_ekle(ast_arena, n);
sonuc = n.id;
}
else if (j.tip == TIP_DONDUR) {
ilerle(durum);
    tamsayi expr_id = -1;;
    if (suanki_jeton(jetonlar, durum).tip != 57) {
        expr_id = parse_expression(jetonlar, durum, 0, ast_arena);
    }
bekle(jetonlar, durum, 57);
    Dugum rn;
rn.id = liste_uzunluk(ast_arena);
rn.tip = 14;
rn.sag_id = expr_id;
liste_ekle(ast_arena, rn);
sonuc = rn.id;
}
else if (j.tip == TIP_EGER) {
ilerle(durum);
    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena);;
bekle(jetonlar, durum, TIP_ISE);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);;
    tamsayi yoksa_id = -1;;
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_YOKSA) {
ilerle(durum);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_EGER) {
yoksa_id = parse_statement(jetonlar, durum, ast_arena);
} else if (j.tip == TIP_SOL_KUSLU) {
bekle(jetonlar, durum, TIP_SOL_KUSLU);
yoksa_id = parse_block(jetonlar, durum, ast_arena);
}
}
    Dugum in;
in.id = liste_uzunluk(ast_arena);
in.tip = 8;
in.kosul_id = kosul_id;
in.govde_id = govde_id;
in.yoksa_id = yoksa_id;
liste_ekle(ast_arena, in);
sonuc = in.id;
}
else if (j.tip == TIP_DONGU) {
ilerle(durum);
    tamsayi kosul_id = parse_expression(jetonlar, durum, 0, ast_arena);;
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);;
    Dugum wh;
wh.id = liste_uzunluk(ast_arena);
wh.tip = 20;
wh.kosul_id = kosul_id;
wh.govde_id = govde_id;
liste_ekle(ast_arena, wh);
sonuc = wh.id;
}
else if (j.tip == TIP_HER) {
ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
j = suanki_jeton(jetonlar, durum);
ilerle(durum);
    tamsayi koleksiyon_id = parse_expression(jetonlar, durum, 0, ast_arena);;
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);;
    Dugum fe;
fe.id = liste_uzunluk(ast_arena);
fe.tip = 21;
fe.metinsel_deger = isim_j.deger;
fe.koleksiyon_id = koleksiyon_id;
fe.govde_id = govde_id;
liste_ekle(ast_arena, fe);
sonuc = fe.id;
}
else if (j.tip == TIP_ISLEV) {
ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
bekle(jetonlar, durum, TIP_SOL_PARANTEZ);
    metin params = "";;
j = suanki_jeton(jetonlar, durum);
while (j.tip != TIP_SAG_PARANTEZ && j.tip != TIP_EOF) {
    Jeton p_isim = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
params = metin_birlestir(params, p_isim.deger);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_IKI_NOKTA) {
ilerle(durum);
    metin p_tip_str = "";
    while (suanki_jeton(jetonlar, durum).tip == 32) { p_tip_str = metin_birlestir(p_tip_str, "*"); ilerle(durum); }
    Jeton p_tip = suanki_jeton(jetonlar, durum);;
    p_tip_str = metin_birlestir(p_tip_str, p_tip.deger);
params = metin_birlestir(params, ":");
params = metin_birlestir(params, p_tip_str);
ilerle(durum);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_KUCUKTUR) {
ilerle(durum);
    Jeton p_gen_tip = suanki_jeton(jetonlar, durum);;
params = metin_birlestir(params, "<");
params = metin_birlestir(params, p_gen_tip.deger);
ilerle(durum);
bekle(jetonlar, durum, TIP_BUYUKTUR);
params = metin_birlestir(params, ">");
}
}
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_VIRGUL) {
params = metin_birlestir(params, ",");
ilerle(durum);
}
j = suanki_jeton(jetonlar, durum);
}
bekle(jetonlar, durum, TIP_SAG_PARANTEZ);
    metin donus_tipi = "Bilinmiyor";;
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_OK) {
ilerle(durum);
    metin donus_tipi_str = "";
    while (suanki_jeton(jetonlar, durum).tip == 32) { donus_tipi_str = metin_birlestir(donus_tipi_str, "*"); ilerle(durum); }
    Jeton ret_tip = suanki_jeton(jetonlar, durum);;
    donus_tipi_str = metin_birlestir(donus_tipi_str, ret_tip.deger);
donus_tipi = donus_tipi_str;
ilerle(durum);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_KUCUKTUR) {
ilerle(durum);
    Jeton r_gen_tip = suanki_jeton(jetonlar, durum);;
donus_tipi = metin_birlestir(donus_tipi, "<");
donus_tipi = metin_birlestir(donus_tipi, r_gen_tip.deger);
ilerle(durum);
bekle(jetonlar, durum, TIP_BUYUKTUR);
donus_tipi = metin_birlestir(donus_tipi, ">");
}
}
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    tamsayi govde_id = parse_block(jetonlar, durum, ast_arena);;
    Dugum fn;
fn.id = liste_uzunluk(ast_arena);
fn.tip = 16;
fn.metinsel_deger = isim_j.deger;
fn.cocuklar = params;
fn.veri_tipi = donus_tipi;
fn.govde_id = govde_id;
liste_ekle(ast_arena, fn);
sonuc = fn.id;
}
else if (j.tip == TIP_ICE_AKTAR) {
ilerle(durum);
    Jeton mod_path = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, 12);
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
    Dugum imp;
imp.id = liste_uzunluk(ast_arena);
imp.tip = 17;
imp.metinsel_deger = mod_path.deger;
liste_ekle(ast_arena, imp);
sonuc = imp.id;
}
else if (j.tip == TIP_KULLAN) {
ilerle(durum);
    Jeton mod_path = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
    Dugum us;
us.id = liste_uzunluk(ast_arena);
us.tip = 18;
us.metinsel_deger = mod_path.deger;
liste_ekle(ast_arena, us);
sonuc = us.id;
}
else if (j.tip == TIP_YAPI) {
ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    metin fields = "";;
j = suanki_jeton(jetonlar, durum);
while (j.tip != TIP_SAG_KUSLU && j.tip != TIP_EOF) {
    Jeton alan = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
fields = metin_birlestir(fields, alan.deger);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_IKI_NOKTA) {
ilerle(durum);
    metin alan_tip_str = "";
    while (suanki_jeton(jetonlar, durum).tip == 32) { alan_tip_str = metin_birlestir(alan_tip_str, "*"); ilerle(durum); }
    Jeton alan_tip = suanki_jeton(jetonlar, durum);;
    alan_tip_str = metin_birlestir(alan_tip_str, alan_tip.deger);
fields = metin_birlestir(fields, ":");
fields = metin_birlestir(fields, alan_tip_str);
ilerle(durum);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_KUCUKTUR) {
ilerle(durum);
    Jeton alan_gen = suanki_jeton(jetonlar, durum);;
fields = metin_birlestir(fields, "<");
fields = metin_birlestir(fields, alan_gen.deger);
ilerle(durum);
bekle(jetonlar, durum, TIP_BUYUKTUR);
fields = metin_birlestir(fields, ">");
}
}
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
fields = metin_birlestir(fields, ",");
j = suanki_jeton(jetonlar, durum);
}
bekle(jetonlar, durum, TIP_SAG_KUSLU);
    Dugum st;
st.id = liste_uzunluk(ast_arena);
st.tip = 19;
st.metinsel_deger = isim_j.deger;
st.cocuklar = fields;
liste_ekle(ast_arena, st);
sonuc = st.id;
}
else if (j.tip == TIP_NUMARALANDIRMA) {
ilerle(durum);
    Jeton isim_j = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    metin values = "";;
j = suanki_jeton(jetonlar, durum);
while (j.tip != TIP_SAG_KUSLU && j.tip != TIP_EOF) {
    Jeton val = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
values = metin_birlestir(values, val.deger);
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_ATAMA) {
ilerle(durum);
    Jeton deger = suanki_jeton(jetonlar, durum);;
values = metin_birlestir(values, "=");
values = metin_birlestir(values, deger.deger);
ilerle(durum);
}
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_VIRGUL) {
ilerle(durum);
}
j = suanki_jeton(jetonlar, durum);
}
bekle(jetonlar, durum, TIP_SAG_KUSLU);
    Dugum en;
en.id = liste_uzunluk(ast_arena);
en.tip = 22;
en.metinsel_deger = isim_j.deger;
en.cocuklar = values;
liste_ekle(ast_arena, en);
sonuc = en.id;
}
else if (j.tip == TIP_DENEME) {
ilerle(durum);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    tamsayi try_id = parse_block(jetonlar, durum, ast_arena);;
    tamsayi catch_id = -1;;
    tamsayi finally_id = -1;;
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_YAKALA) {
ilerle(durum);
bekle(jetonlar, durum, TIP_SOL_PARANTEZ);
    Jeton err_var = suanki_jeton(jetonlar, durum);;
bekle(jetonlar, durum, TIP_KIMLIK);
bekle(jetonlar, durum, TIP_SAG_PARANTEZ);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
catch_id = parse_block(jetonlar, durum, ast_arena);
    Dugum ch;
ch.id = liste_uzunluk(ast_arena);
ch.tip = 24;
ch.metinsel_deger = err_var.deger;
ch.govde_id = catch_id;
liste_ekle(ast_arena, ch);
}
j = suanki_jeton(jetonlar, durum);
if (j.tip == TIP_NIHAYET) {
ilerle(durum);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
finally_id = parse_block(jetonlar, durum, ast_arena);
    Dugum fi;
fi.id = liste_uzunluk(ast_arena);
fi.tip = 25;
fi.govde_id = finally_id;
liste_ekle(ast_arena, fi);
}
    Dugum tc;
tc.id = liste_uzunluk(ast_arena);
tc.tip = 23;
tc.govde_id = try_id;
tc.yakala_id = catch_id;
tc.nihayet_id = finally_id;
liste_ekle(ast_arena, tc);
sonuc = tc.id;
}
else if (j.tip == TIP_DURDUR) {
ilerle(durum);
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
    Dugum br;
br.id = liste_uzunluk(ast_arena);
br.tip = 26;
liste_ekle(ast_arena, br);
sonuc = br.id;
}
else if (j.tip == TIP_DEVAM_ET) {
ilerle(durum);
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
    Dugum ct;
ct.id = liste_uzunluk(ast_arena);
ct.tip = 27;
liste_ekle(ast_arena, ct);
sonuc = ct.id;
}
else if (j.tip == TIP_BORU_HATTI) {
ilerle(durum);
bekle(jetonlar, durum, TIP_SOL_KUSLU);
    tamsayi pipe_id = parse_block(jetonlar, durum, ast_arena);;
j = suanki_jeton(jetonlar, durum);
    tamsayi sync_id = -1;;
if (j.tip == TIP_SENKRONIZE_ET) {
ilerle(durum);
sync_id = parse_expression(jetonlar, durum, 0, ast_arena);
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
}
    Dugum pl;
pl.id = liste_uzunluk(ast_arena);
pl.tip = 28;
pl.govde_id = pipe_id;
pl.sync_id = sync_id;
liste_ekle(ast_arena, pl);
sonuc = pl.id;
}
else {
    tamsayi expr = parse_expression(jetonlar, durum, 0, ast_arena);;
if (expr < 0) {
durum.derinlik = durum.derinlik - 1;
return -1;
}
durum.hata_var = 0; // her statement bitti, hata sayacini sifirla
bekle(jetonlar, durum, TIP_NOKTALI_VIRGUL);
sonuc = expr;
}
durum.derinlik = durum.derinlik - 1;
return sonuc;
}
tamsayi parse_block(Liste<Jeton>& jetonlar, ParserDurumu& durum, Liste<Dugum>& ast_arena) {
    Dugum blok;
blok.id = liste_uzunluk(ast_arena);
blok.tip = 15;
blok.cocuklar = "";
liste_ekle(ast_arena, blok);
    Jeton j = suanki_jeton(jetonlar, durum);;
    tamsayi guvenli_adim = 0;;
while (j.tip != TIP_SAG_KUSLU && j.tip != TIP_EOF) {
    tamsayi poz_once = durum.poz;;
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);;
    if (durum.poz == poz_once) { ilerle(durum); durum.hata_var = 0; } // ilerleme olmadiysa token atla
blok.cocuklar = metin_birlestir(blok.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
j = suanki_jeton(jetonlar, durum);
}
  bekle(jetonlar, durum, TIP_SAG_KUSLU);
  tamsayi root_id = blok.id;
  ast_arena[root_id] = blok;
  return root_id;
}
tamsayi parser_cozumle(Liste<Jeton>& jetonlar, Liste<Dugum>& ast_arena) {
    ParserDurumu durum;
durum.poz = 0;
durum.hata_var = 0;
durum.derinlik = 0;
    Dugum root;
root.tip = 15;
root.id = liste_uzunluk(ast_arena);
root.cocuklar = "";
liste_ekle(ast_arena, root);
    tamsayi root_id = root.id;;
    Jeton j = suanki_jeton(jetonlar, durum);;
while (j.tip != TIP_EOF) {
    tamsayi poz_once_g = durum.poz;;
    tamsayi stmt_id = parse_statement(jetonlar, durum, ast_arena);;
    if (durum.poz == poz_once_g) { ilerle(durum); durum.hata_var = 0; } // ilerleme olmadiysa atla
root.cocuklar = metin_birlestir(root.cocuklar, metin_birlestir(tamsayi_metne(stmt_id), ","));
  j = suanki_jeton(jetonlar, durum);
  }
  ast_arena[root_id] = root;
  return root_id;
}
// --- END parser_v3.ykn ---
// --- START type_checker.ykn ---
    KapsamYigiti kapsam_yigiti;
    Liste<Sembol> sembol_tablosu;
tamsayi kapsam_yigiti_ilk_kur() {
    Liste<Sembol> ilk_kapsam;
liste_ekle(kapsam_yigiti.kapsamlar, ilk_kapsam);
kapsam_yigiti.mevcut_seviye = 0;
sembol_tablosu = liste_al(kapsam_yigiti.kapsamlar, 0);
return 1;
}
tamsayi kapsam_ekle(KapsamYigiti& yigit) {
    Liste<Sembol> yeni_kapsam;
    yigit.kapsamlar.push_back(yeni_kapsam);
    yigit.mevcut_seviye = yigit.mevcut_seviye + 1;
    return 1;
}
tamsayi kapsam_cikar(KapsamYigiti& yigit) {
    if (yigit.kapsamlar.size() > 1) {
        yigit.kapsamlar.pop_back();
        if (yigit.mevcut_seviye > 0) {
            yigit.mevcut_seviye = yigit.mevcut_seviye - 1;
        }
    }
    return 1;
}
tamsayi sembol_ekle_kapsam(KapsamYigiti& yigit, metin isim, metin tip) {
    tamsayi sev = yigit.mevcut_seviye;
    Sembol s;
    s.isim = isim;
    s.tip = tip;
    yigit.kapsamlar[sev].push_back(s);
    return 1;
}
metin sembol_bul_kapsam(KapsamYigiti yigit, metin isim) {
    tamsayi sev = yigit.mevcut_seviye;;
while (sev >= 0) {
    Liste<Sembol> kapsam = liste_al(yigit.kapsamlar, sev);;
    tamsayi l = liste_uzunluk(kapsam);;
    tamsayi i = 0;;
while (i < l) {
    Sembol s = liste_al(kapsam, i);;
if (s.isim == isim) {
return s.tip;
}
i = i + 1;
}
sev = sev - 1;
}
return "Bilinmiyor";
}
tamsayi sembol_ekle(metin isim, metin tip) {
if (liste_uzunluk(kapsam_yigiti.kapsamlar) == 0) {
kapsam_yigiti_ilk_kur();
}
return sembol_ekle_kapsam(kapsam_yigiti, isim, tip);
}
metin sembol_bul(metin isim) {
if (liste_uzunluk(kapsam_yigiti.kapsamlar) == 0) {
kapsam_yigiti_ilk_kur();
}
return sembol_bul_kapsam(kapsam_yigiti, isim);
}
tamsayi tip_hatasi(metin mesaj, tamsayi dugum_id) {
    metin m1 = metin_birlestir("[Tip Uyarisi] Dugum ID ", tamsayi_metne(dugum_id));;
    metin m2 = metin_birlestir(m1, ": ");;
    metin m3 = metin_birlestir(m2, mesaj);;
sistem_komutu(metin_birlestir("echo ", m3));
return 1;
}
tamsayi dugum_kontrol_et(Liste<Dugum>& ast_arena, tamsayi dugum_id, TipKontrolDurumu durum) {
if (dugum_id < 0 || dugum_id >= liste_uzunluk(ast_arena)) {
return 1;
}
    Dugum n = liste_al(ast_arena, dugum_id);;
if (n.tip == 1 || n.tip == 2 || n.tip == 3) {
return 1;
}
if (n.tip == 4) {
    metin tip_adi = sembol_bul(n.metinsel_deger);;
if (tip_adi == "Bilinmiyor") {
tip_hatasi(metin_birlestir("Tanimlanmamis degisken: ", n.metinsel_deger), dugum_id);
durum.hata_var = 1;
}
return 1;
}
if (n.tip == 5) {
dugum_kontrol_et(ast_arena, n.sol_id, durum);
dugum_kontrol_et(ast_arena, n.sag_id, durum);
return 1;
}
if (n.tip == 6) {
dugum_kontrol_et(ast_arena, n.sol_id, durum);
dugum_kontrol_et(ast_arena, n.sag_id, durum);
return 1;
}
if (n.tip == 7) {
if (n.sag_id >= 0) {
dugum_kontrol_et(ast_arena, n.sag_id, durum);
}
sembol_ekle(n.metinsel_deger, n.veri_tipi);
return 1;
}
if (n.tip == 15) {
kapsam_ekle(kapsam_yigiti);
    Liste<metin> cocuklar_liste = metin_bol(n.cocuklar, ",");;
    tamsayi l = liste_uzunluk(cocuklar_liste);;
    tamsayi k = 0;;
while (k < l) {
    metin cocuk_str = liste_al(cocuklar_liste, k);;
if (cocuk_str != "") {
dugum_kontrol_et(ast_arena, metni_tamsayiya(cocuk_str), durum);
}
k = k + 1;
}
kapsam_cikar(kapsam_yigiti);
return 1;
}
if (n.tip == 8) {
dugum_kontrol_et(ast_arena, n.kosul_id, durum);
kapsam_ekle(kapsam_yigiti);
dugum_kontrol_et(ast_arena, n.govde_id, durum);
kapsam_cikar(kapsam_yigiti);
if (n.alternatif_id >= 0) {
kapsam_ekle(kapsam_yigiti);
dugum_kontrol_et(ast_arena, n.alternatif_id, durum);
kapsam_cikar(kapsam_yigiti);
}
return 1;
}
if (n.tip == 9) {
dugum_kontrol_et(ast_arena, n.dongu_kosul_id, durum);
kapsam_ekle(kapsam_yigiti);
dugum_kontrol_et(ast_arena, n.dongu_govde_id, durum);
kapsam_cikar(kapsam_yigiti);
return 1;
}
if (n.tip == 10) {
dugum_kontrol_et(ast_arena, n.dongu_baslangic_id, durum);
dugum_kontrol_et(ast_arena, n.dongu_kosul_id, durum);
dugum_kontrol_et(ast_arena, n.dongu_adim_id, durum);
kapsam_ekle(kapsam_yigiti);
dugum_kontrol_et(ast_arena, n.dongu_govde_id, durum);
kapsam_cikar(kapsam_yigiti);
return 1;
}
if (n.tip == 20) {
    Liste<metin> uyeler_liste = metin_bol(n.yapi_uyeleri, ",");;
    tamsayi l = liste_uzunluk(uyeler_liste);;
    tamsayi k = 0;;
while (k < l) {
    metin uye_str = liste_al(uyeler_liste, k);;
if (uye_str != "") {
dugum_kontrol_et(ast_arena, metni_tamsayiya(uye_str), durum);
}
k = k + 1;
}
return 1;
}
if (n.tip == 23) {
return 1;
}
if (n.tip == 21) {
dugum_kontrol_et(ast_arena, n.govde_id, durum);
if (n.alternatif_id >= 0) {
dugum_kontrol_et(ast_arena, n.alternatif_id, durum);
}
if (n.cocuklar != "" && metni_tamsayiya(n.cocuklar) >= 0) {
dugum_kontrol_et(ast_arena, metni_tamsayiya(n.cocuklar), durum);
}
return 1;
}
return 1;
}
tamsayi tip_kontrol_baslat(Liste<Dugum>& ast_arena, tamsayi root_id) {
    TipKontrolDurumu durum;
durum.hata_var = 0;
if (liste_uzunluk(kapsam_yigiti.kapsamlar) > 0) {
    tamsayi l2 = liste_uzunluk(kapsam_yigiti.kapsamlar);;
while (l2 > 0) {
liste_sondan_cikar(kapsam_yigiti.kapsamlar);
l2 = l2 - 1;
}
}
kapsam_yigiti_ilk_kur();
dugum_kontrol_et(ast_arena, root_id, durum);
if (durum.hata_var == 1) {
return 0;
}
return 1;
}
// --- END type_checker.ykn ---
// --- START hir.ykn ---
    auto HIR_ATAMA = 0;;
    auto HIR_IKILI_OP = 1;;
    auto HIR_CAGRI = 2;;
    auto HIR_DONDUR = 3;;
    auto HIR_EGER = 4;;
    auto HIR_DONGU = 5;;
    auto HIR_HER = 6;;
    auto HIR_BORU_HATTI = 7;;
    auto HIR_SENKRONIZE_ET = 8;;
    auto HIR_DURDUR = 9;;
    auto HIR_DEVAM_ET = 10;;
    auto HIR_ETIKET = 11;;
    auto HIR_GOTO = 12;;
    auto HIR_KARSILASTIR_GOTO = 13;;
    auto HIR_MATRIS_CARP = 14;;
    auto HIR_RMSNORM = 15;;
    auto HIR_SOFTMAX = 16;;
    auto HIR_BIRLESTIRILMIS_OP = 17;;
    auto HIR_SPILL_YAZ = 18;;
    auto HIR_SPILL_OKU = 19;;
    auto HIR_BITNET_CARP = 20;;
tamsayi hir_dugum_olustur(HirProgram program, tamsayi tip) {
    HirDugum d;
d.id = liste_uzunluk(program.arena);
d.tip = tip;
d.hedef = "";
d.sol_isim = "";
d.sag_isim = "";
d.kosul_isim = "";
d.etiket_adi = "";
d.dogru_hedef = "";
d.yanlis_hedef = "";
d.islev_adi = "";
d.birlestirilmis_ops = "";
d.sabit_deger = "";
d.cocuklar = "";
d.dogru_govde = "";
d.yanlis_govde = "";
d.dongu_govde = "";
d.dongu_kosul_ifadeleri = "";
liste_ekle(program.arena, d);
return d.id;
}
// --- END hir.ykn ---
// --- START hir_builder.ykn ---


HirOlusturucu hir_olusturucu_kur(Liste<Dugum>& ast_arena) {
    HirOlusturucu o;
o.ast_arena = ast_arena;
o.temp_sayaci = 0;
o.golge_sayaci = 0;
o.mevcut_islev = -1;
    KapsamSeviyesi k;
liste_ekle(o.kapsam_yigiti, k);
return o;
}
metin get_temp(HirOlusturucu& o) {
    o.temp_sayaci = o.temp_sayaci + 1;
    return "t" + std::to_string(o.temp_sayaci);
}
tamsayi _kapsam_gir(HirOlusturucu& o) {
    KapsamSeviyesi k;
    o.kapsam_yigiti.push_back(k);
    return 0;
}
tamsayi _kapsam_cik(HirOlusturucu& o) {
    if (o.kapsam_yigiti.size() > 1) {
        o.kapsam_yigiti.pop_back();
    }
    return 0;
}
metin _isim_coz(HirOlusturucu o, metin isim) {
    tamsayi i = liste_uzunluk(o.kapsam_yigiti) - 1;;
while (i >= 0) {
    KapsamSeviyesi k;
k = liste_al(o.kapsam_yigiti, i);
    tamsayi j = 0;;
    tamsayi len = liste_uzunluk(k.degiskenler);;
while (j < len) {
    KapsamDegiskeni d;
d = liste_al(k.degiskenler, j);
if (metin_esitmi(d.anahtar, isim) == 1) {
return d.deger;
}
j = j + 1;
}
i = i - 1;
}
return isim;
}
tamsayi _dis_kapsamda_var_mi(HirOlusturucu o, metin isim) {
    tamsayi i = liste_uzunluk(o.kapsam_yigiti) - 2;;
while (i >= 0) {
    KapsamSeviyesi k;
k = liste_al(o.kapsam_yigiti, i);
    tamsayi j = 0;;
    tamsayi len = liste_uzunluk(k.degiskenler);;
while (j < len) {
    KapsamDegiskeni d;
d = liste_al(k.degiskenler, j);
if (metin_esitmi(d.anahtar, isim) == 1) {
return 1;
}
j = j + 1;
}
i = i - 1;
}
return 0;
}
metin _tanimla(HirOlusturucu& o, metin isim) {
    tamsayi top_idx = o.kapsam_yigiti.size() - 1;
    KapsamSeviyesi& k = o.kapsam_yigiti[top_idx];
    bool icinde_var = false;
    for (auto& d : k.degiskenler) {
        if (d.anahtar == isim) { icinde_var = true; break; }
    }
    metin cozulmus;
    if (_dis_kapsamda_var_mi(o, isim) == 1 || icinde_var) {
        o.golge_sayaci++;
        cozulmus = isim + "__s" + std::to_string(o.golge_sayaci);
    } else {
        cozulmus = isim;
    }
    KapsamDegiskeni yeni_d;
    yeni_d.anahtar = isim;
    yeni_d.deger = cozulmus;
    k.degiskenler.push_back(yeni_d);
    return cozulmus;
}
metin ziyaret_ifade(HirOlusturucu o, tamsayi dugum_id, tamsayi islev_idx) {
    Dugum d;
d = liste_al(o.ast_arena, dugum_id);
if (d.tip == AST_SAYI || d.tip == AST_GERCEK || d.tip == AST_METIN) {
return d.metinsel_deger;
} else if (d.tip == AST_KIMLIK) {
return _isim_coz(o, d.metinsel_deger);
} else if (d.tip == AST_IKILI_OP) {
    metin sol_isim = ziyaret_ifade(o, d.sol_id, islev_idx);;
    metin sag_isim = ziyaret_ifade(o, d.sag_id, islev_idx);;
    metin temp = get_temp(o);;
    HirProgram prog = o.program;;
    tamsayi h_id = hir_dugum_olustur(prog, HIR_IKILI_OP);;
    HirDugum h_dugum = liste_al(prog.arena, h_id);;
h_dugum.hedef = temp;
h_dugum.sol_isim = sol_isim;
h_dugum.sag_isim = sag_isim;
h_dugum.sabit_deger = d.metinsel_deger;
liste_guncelle(prog.arena, h_id, h_dugum);
    HirIslev f = liste_al(prog.islevler, islev_idx);;
liste_ekle(f.govde, h_id);
liste_guncelle(prog.islevler, islev_idx, f);
o.program = prog;
return temp;
} else if (d.tip == AST_ATAMA) {
    metin atanan_isim = ziyaret_ifade(o, d.sag_id, islev_idx);;
    Dugum sol_d = liste_al(o.ast_arena, d.sol_id);;
    metin hedef_isim = _isim_coz(o, sol_d.metinsel_deger);;
    HirProgram prog2 = o.program;;
    tamsayi h_id = hir_dugum_olustur(prog2, HIR_ATAMA);;
    HirDugum h_dugum = liste_al(prog2.arena, h_id);;
h_dugum.hedef = hedef_isim;
h_dugum.sol_isim = atanan_isim;
liste_guncelle(prog2.arena, h_id, h_dugum);
    HirIslev f2 = liste_al(prog2.islevler, islev_idx);;
liste_ekle(f2.govde, h_id);
liste_guncelle(prog2.islevler, islev_idx, f2);
o.program = prog2;
return hedef_isim;
}
return "";
}
tamsayi ziyaret_global(HirOlusturucu o, tamsayi dugum_id) {
    Dugum d;
d = liste_al(o.ast_arena, dugum_id);
if (d.tip == AST_ISLEV) {
    HirIslev hir_islev;
hir_islev.isim = d.metinsel_deger;
hir_islev.parametreler = d.cocuklar;
hir_islev.donus_tipi = d.veri_tipi;
hir_islev.gpu_mu = 0;
    HirProgram prog3 = o.program;;
liste_ekle(prog3.islevler, hir_islev);
    tamsayi islev_idx = liste_uzunluk(prog3.islevler) - 1;;
o.mevcut_islev = islev_idx;
o.program = prog3;
_kapsam_gir(o);
    tamsayi govde_id = d.govde_id;;
    Dugum govde_dugum = liste_al(o.ast_arena, govde_id);;
_kapsam_cik(o);
o.mevcut_islev = -1;
}
return 0;
}
// --- END hir_builder.ykn ---
// --- START cfg.ykn ---

CfgOlusturucu cfg_olusturucu_kur(HirProgram hir_prog) {
    CfgOlusturucu o;
o.hir_prog = hir_prog;
o.blok_sayaci = 0;
return o;
}
metin _yeni_etiket(CfgOlusturucu o) {
o.blok_sayaci = o.blok_sayaci + 1;
    metin lbl = metin_birlestir("B", tamsayi_metne(o.blok_sayaci));;
return lbl;
}
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
tamsayi _blok_bul_idx(CfgIslev cfg_fn_nesnesi, metin etiket) {
    tamsayi i = 0;;
    tamsayi len = liste_uzunluk(cfg_fn_nesnesi.bloklar);;
while (i < len) {
    TemelBlok b = liste_al(cfg_fn_nesnesi.bloklar, i);;
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
    metin mevcut_etiket = "giris";;
    tamsayi i = 0;;
    tamsayi len = liste_uzunluk(hir_fn.govde);;
while (i < len) {
    tamsayi instr_id = liste_al(hir_fn.govde, i);;
    HirProgram prog = o.hir_prog;;
    HirDugum d = liste_al(prog.arena, instr_id);;
    tamsayi b_idx = _blok_bul_idx(cfg_fn, mevcut_etiket);;
    TemelBlok mevcut_blok = liste_al(cfg_fn.bloklar, b_idx);;
if (d.tip == HIR_EGER) {
    metin dogru_etiket = _yeni_etiket(o);;
    metin yanlis_etiket = _yeni_etiket(o);;
    metin merge_etiket = _yeni_etiket(o);;
liste_ekle(mevcut_blok.sonraki_bloklar, dogru_etiket);
liste_ekle(mevcut_blok.sonraki_bloklar, yanlis_etiket);
liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);
    TemelBlok dogru_blok;
dogru_blok.etiket = dogru_etiket;
    Liste<tamsayi> dogru_ids = _ayristir_id_listesi(d.dogru_govde);
    for (size_t di = 0; di < (size_t)liste_uzunluk(dogru_ids); di++) {
        liste_ekle(dogru_blok.talimatlar, liste_al(dogru_ids, di));
    }
liste_ekle(dogru_blok.sonraki_bloklar, merge_etiket);
liste_ekle(cfg_fn.bloklar, dogru_blok);
    TemelBlok yanlis_blok;
yanlis_blok.etiket = yanlis_etiket;
    Liste<tamsayi> yanlis_ids = _ayristir_id_listesi(d.yanlis_govde);
    for (size_t yi = 0; yi < (size_t)liste_uzunluk(yanlis_ids); yi++) {
        liste_ekle(yanlis_blok.talimatlar, liste_al(yanlis_ids, yi));
    }
liste_ekle(yanlis_blok.sonraki_bloklar, merge_etiket);
liste_ekle(cfg_fn.bloklar, yanlis_blok);
    TemelBlok merge_blok;
merge_blok.etiket = merge_etiket;
liste_ekle(cfg_fn.bloklar, merge_blok);
mevcut_etiket = merge_etiket;
} else if (d.tip == HIR_DONGU) {
    metin baslik_etiket = _yeni_etiket(o);;
    metin govde_etiket = _yeni_etiket(o);;
    metin cikis_etiket = _yeni_etiket(o);;
liste_ekle(mevcut_blok.sonraki_bloklar, baslik_etiket);
liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);
    TemelBlok baslik_blok;
baslik_blok.etiket = baslik_etiket;
liste_ekle(baslik_blok.sonraki_bloklar, govde_etiket);
liste_ekle(baslik_blok.sonraki_bloklar, cikis_etiket);
liste_ekle(cfg_fn.bloklar, baslik_blok);
    TemelBlok govde_blok;
govde_blok.etiket = govde_etiket;
    Liste<tamsayi> govde_ids = _ayristir_id_listesi(d.dongu_govde);
    for (size_t gi = 0; gi < (size_t)liste_uzunluk(govde_ids); gi++) {
        liste_ekle(govde_blok.talimatlar, liste_al(govde_ids, gi));
    }
liste_ekle(govde_blok.sonraki_bloklar, baslik_etiket);
liste_ekle(cfg_fn.bloklar, govde_blok);
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
liste_ekle(mevcut_blok.talimatlar, instr_id);
liste_guncelle(cfg_fn.bloklar, b_idx, mevcut_blok);
}
i = i + 1;
}
    tamsayi bi = 0;;
    tamsayi blen = liste_uzunluk(cfg_fn.bloklar);;
while (bi < blen) {
    TemelBlok b = liste_al(cfg_fn.bloklar, bi);;
    tamsayi si = 0;;
    tamsayi slen = liste_uzunluk(b.sonraki_bloklar);;
while (si < slen) {
    metin succ_lbl = liste_al(b.sonraki_bloklar, si);;
    tamsayi succ_idx = _blok_bul_idx(cfg_fn, succ_lbl);;
if (succ_idx != -1) {
    TemelBlok succ_b = liste_al(cfg_fn.bloklar, succ_idx);;
    tamsayi pi = 0;;
    tamsayi plen = liste_uzunluk(succ_b.onceki_bloklar);;
    tamsayi var_mi = 0;;
while (pi < plen) {
    metin p_lbl = liste_al(succ_b.onceki_bloklar, pi);;
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
    tamsayi pi = 0;;
    HirProgram prog = o.hir_prog;;
    tamsayi p_len = liste_uzunluk(prog.islevler);;
while (pi < p_len) {
    HirIslev f = liste_al(prog.islevler, pi);;
    CfgIslev cfg_f = _cfg_islev_olustur(o, f);;
    CfgProgram prog = o.cfg_prog;;
liste_ekle(prog.islevler, cfg_f);
o.cfg_prog = prog;
pi = pi + 1;
}
o.cfg_prog.hir_prog = o.hir_prog;
return o.cfg_prog;
}
// --- END cfg.ykn ---
// --- START ssa.ykn ---

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
// --- END ssa.ykn ---
// --- START optimizer.ykn ---

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

    std::set<std::string> ulasilabilir;
    std::vector<std::string> worklist;
    std::string start_label = f.giris_etiketi.empty() ? "giris" : f.giris_etiketi;
    worklist.push_back(start_label);
    ulasilabilir.insert(start_label);

    std::map<std::string, int> lbl_to_idx;
    for (int i = 0; i < blen; i++) {
        TemelBlok b = liste_al(f.bloklar, i);
        lbl_to_idx[b.etiket] = i;
    }

    while (!worklist.empty()) {
        std::string curr = worklist.back();
        worklist.pop_back();

        if (lbl_to_idx.find(curr) != lbl_to_idx.end()) {
            TemelBlok b = liste_al(f.bloklar, lbl_to_idx[curr]);
            int slen = (int)liste_uzunluk(b.sonraki_bloklar);
            for (int si = 0; si < slen; si++) {
                metin succ = liste_al(b.sonraki_bloklar, si);
                if (ulasilabilir.find(succ) == ulasilabilir.end()) {
                    ulasilabilir.insert(succ);
                    worklist.push_back(succ);
                }
            }
        }
    }

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
// --- END optimizer.ykn ---
// --- START regalloc.ykn ---

RegAllocOlusturucu regalloc_kur(CfgProgram cfg_prog) {
    RegAllocOlusturucu o;
o.cfg_prog = cfg_prog;
o.maks_yazmac = 64;
return o;
}
tamsayi _canlilik_analizi_yap(RegAllocOlusturucu o, CfgIslev islev_cfg) {
    tamsayi bi = 0;;
    tamsayi blen = liste_uzunluk(islev_cfg.bloklar);;
while (bi < blen) {
    TemelBlok blok = liste_al(islev_cfg.bloklar, bi);;
    tamsayi ti = 0;;
    tamsayi tlen = liste_uzunluk(blok.talimatlar);;
while (ti < tlen) {
    tamsayi instr_id = liste_al(blok.talimatlar, ti);;
    HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);;
ti = ti + 1;
}
bi = bi + 1;
}
return 0;
}
tamsayi _linear_scan_uygula(RegAllocOlusturucu o) {
    tamsayi num_aralik = liste_uzunluk(o.araliklar);;
    tamsayi i = 0;;
    tamsayi MAX_REG = 10;;
while (i < num_aralik) {
    CanlilikAraligi aralik = liste_al(o.araliklar, i);;
    tamsayi aktif_sayisi = liste_uzunluk(o.aktif_araliklar);;
if (aktif_sayisi >= MAX_REG) {
aralik.spill_slot = i;
aralik.yazmac_no = -1;
} else {
aralik.yazmac_no = aktif_sayisi;
aralik.spill_slot = -1;
liste_ekle(o.aktif_araliklar, i);
}
liste_guncelle(o.araliklar, i, aralik);
i = i + 1;
}
return 0;
}
tamsayi _spill_kodlarini_ekle(RegAllocOlusturucu o, CfgIslev islev_cfg) {
return 0;
}
tamsayi _regalloc_islev_isle(RegAllocOlusturucu o, tamsayi islev_idx) {
    CfgProgram prog = o.cfg_prog;;
    CfgIslev f = liste_al(prog.islevler, islev_idx);;
_canlilik_analizi_yap(o, f);
_linear_scan_uygula(o);
_spill_kodlarini_ekle(o, f);
return 0;
}
tamsayi regalloc_calistir(RegAllocOlusturucu o) {
    tamsayi i = 0;;
    CfgProgram prog = o.cfg_prog;;
    tamsayi p_len = liste_uzunluk(prog.islevler);;
while (i < p_len) {
_regalloc_islev_isle(o, i);
i = i + 1;
}
return 0;
}
// --- END regalloc.ykn ---
// --- START mir.ykn ---
    auto MIR_MOV = 100;;
    auto MIR_ADD = 101;;
    auto MIR_SUB = 102;;
    auto MIR_MUL = 103;;
    auto MIR_DIV = 104;;
    auto MIR_MOD = 105;;
    auto MIR_SHL = 106;;
    auto MIR_SHR = 107;;
    auto MIR_CALL = 108;;
    auto MIR_BR = 109;;
    auto MIR_BR_EQ = 110;;
    auto MIR_BR_NE = 111;;
    auto MIR_LDR = 112;;
    auto MIR_STR = 113;;
    auto MIR_FUSED_MADD = 114;;
    auto MIR_SPILL = 115;;
// --- END mir.ykn ---
// --- START mir_passes.ykn ---


MirOlusturucu mir_olusturucu_kur(CfgProgram cfg_prog) {
    MirOlusturucu o;
    o.cfg_prog = cfg_prog;
    return o;
}
tamsayi _talimat_secimi_yap(MirOlusturucu o, CfgIslev f) {
    tamsayi uretilen = 0;
    int blen = (int)liste_uzunluk(f.bloklar);
    for (int bi = 0; bi < blen; bi++) {
        TemelBlok b = liste_al(f.bloklar, bi);
        int tlen = (int)liste_uzunluk(b.talimatlar);
        for (int ti = 0; ti < tlen; ti++) {
            tamsayi instr_id = liste_al(b.talimatlar, ti);
            if (instr_id < 0 || instr_id >= (tamsayi)liste_uzunluk(o.cfg_prog.hir_prog.arena)) continue;
            HirDugum d = liste_al(o.cfg_prog.hir_prog.arena, instr_id);
            MirDugum md;
            md.id = (tamsayi)liste_uzunluk(o.mir_prog.arena);
            md.yazmac_hedef = 0; md.yazmac_kaynak1 = 0; md.yazmac_kaynak2 = 0;
            if (d.tip == 0) { md.tip = MIR_MOV; md.anlik_deger = d.sabit_deger; md.etiket = d.hedef; }
            else if (d.tip == 1) {
                if (d.op == 30) md.tip = MIR_ADD;
                else if (d.op == 31) md.tip = MIR_SUB;
                else if (d.op == 32) md.tip = MIR_MUL;
                else if (d.op == 33) md.tip = MIR_DIV;
                else md.tip = MIR_ADD;
                md.anlik_deger = d.sabit_deger; md.etiket = d.hedef;
            } else if (d.tip == 10) { md.tip = MIR_CALL; md.etiket = d.islev_adi; }
            else if (d.tip == 2) { md.tip = MIR_BR; md.etiket = d.dogru_hedef; }
            else { md.tip = MIR_MOV; md.anlik_deger = d.sabit_deger; md.etiket = d.hedef; }
            liste_ekle(o.mir_prog.arena, md);
            uretilen++;
        }
    }
    return uretilen;
}
tamsayi _peephole_optimizasyon_yap(MirOlusturucu o, CfgIslev f) {
    tamsayi optimize_edilen = 0;
    int mlen = (int)liste_uzunluk(o.mir_prog.arena);
    for (int mi = 0; mi < mlen; mi++) {
        MirDugum md = liste_al(o.mir_prog.arena, mi);
        if (md.tip == MIR_ADD && md.anlik_deger == "0") {
            md.tip = MIR_MOV; md.anlik_deger = "";
            liste_guncelle(o.mir_prog.arena, mi, md);
            optimize_edilen++;
        } else if (md.tip == MIR_MUL && md.anlik_deger == "1") {
            md.tip = MIR_MOV; md.anlik_deger = "";
            liste_guncelle(o.mir_prog.arena, mi, md);
            optimize_edilen++;
        } else if (md.tip == MIR_MUL && md.anlik_deger == "0") {
            md.tip = MIR_MOV; md.anlik_deger = "0";
            liste_guncelle(o.mir_prog.arena, mi, md);
            optimize_edilen++;
        } else if (md.tip == MIR_MUL && md.anlik_deger == "2") {
            md.tip = MIR_SHL; md.anlik_deger = "1";
            liste_guncelle(o.mir_prog.arena, mi, md);
            optimize_edilen++;
        }
    }
    return optimize_edilen;
}
tamsayi _zamanlama_yap(MirOlusturucu o, CfgIslev f) {
    return (tamsayi)liste_uzunluk(o.mir_prog.arena);
}
tamsayi _mir_islev_isle(MirOlusturucu o, tamsayi islev_idx) {
    CfgProgram prog = o.cfg_prog;
    CfgIslev f = liste_al(prog.islevler, islev_idx);
    _talimat_secimi_yap(o, f);
    _peephole_optimizasyon_yap(o, f);
    _zamanlama_yap(o, f);
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
// --- END mir_passes.ykn ---
// --- START codegen.ykn ---

KodUretici kod_uretici_kur(MirProgram mir_prog, metin hedef) {
    KodUretici o;
o.mir_prog = mir_prog;
o.hedef_dil = hedef;
o.cikti_kodu = "";
return o;
}
Liste<tamsayi> ayristir_id_cpp(metin idler) {
    Liste<tamsayi> sonuc;
    std::string s = idler;
    size_t pos = 0;
    while((pos = s.find(",")) != std::string::npos) {
        if(pos > 0) {
            long long val = std::strtoll(s.substr(0, pos).c_str(), nullptr, 10);
            sonuc.push_back(val);
        }
        s.erase(0, pos + 1);
    }
    if(s.length() > 0) {
        long long val = std::strtoll(s.c_str(), nullptr, 10);
        sonuc.push_back(val);
    }
    return sonuc;
}


// Global set of struct names (populated during struct definition emission)
static std::set<std::string> g_struct_names;

// Helper: true if ykn base type string names a user-defined struct
static bool _ykn_is_primitive_base(const std::string& t) {
    return (t == "u8" || t == "u16" || t == "u32" || t == "u64" ||
            t == "i8" || t == "i16" || t == "i32" || t == "i64" ||
            t == "tamsayi" || t == "sayi" || t == "gercek" || t == "f64" || t == "f32" ||
            t == "bool" || t == "mantik" || t == "metin" ||
            t == "hic" || t == "void" || t == "Bilinmiyor");
}

    // Convert a YKN type string to a C++ type string.
std::string ykn_tip_to_cpp(const std::string& ykn) {
    std::string t = ykn;
    while (!t.empty() && t[0] == ' ') t = t.substr(1);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (t.empty()) return "long long";
    // pointer prefix -> recurse and append *
    if (t[0] == '*') {
        return ykn_tip_to_cpp(t.substr(1)) + "*";
    }
    if (t == "tamsayi" || t == "sayi") return "long long";
    if (t == "u8")  return "char";
    if (t == "u16") return "unsigned short";
    if (t == "u32") return "unsigned int";
    if (t == "u64") return "unsigned long long";
    if (t == "i8")  return "signed char";
    if (t == "i16") return "short";
    if (t == "i32") return "int";
    if (t == "i64") return "long long";
    if (t == "gercek" || t == "f64") return "double";
    if (t == "f32") return "float";
    if (t == "bool" || t == "mantik") return "bool";
    if (t == "metin") return "std::string";
    if (t == "hic" || t == "void" || t == "Bilinmiyor") return "void";
    
    // User-defined struct: In C++, we use the name directly for the struct type.
    // If it's used as a value, it's 'T', if it's used as a pointer, it's 'T*'.
    // The recursive call above handles the '*' case.
    return t;
}

metin ast_to_cpp(tamsayi id) {
    if (id < 0 || g_ast_arena == nullptr) return "";
    if (id >= g_ast_arena->size()) return "";
    Dugum n = (*g_ast_arena)[id];
    
    if (n.tip == 1 || n.tip == 2) { return n.metinsel_deger; } // Sayi
      if (n.tip == 3) { 
          metin m = n.metinsel_deger;
          metin k = "";
          for(size_t i=0; i<m.length(); ++i) {
              if (m[i] == '\n') k += "\\n";
              else if (m[i] == '\t') k += "\\t";
              else if (m[i] == '\r') k += "\\r";
              else if (m[i] == '\"') k += "\\\"";
              else if (m[i] == '\\') k += "\\\\";
              else k += m[i];
          }
          return "\"" + k + "\""; 
      } // Metin
    if (n.tip == 4) { 
        if (n.metinsel_deger == "dogru") return "true";
        if (n.metinsel_deger == "yanlis") return "false";
        // If this identity is a pointer to a struct, YKN_DOT will handle it.
        // If it's used as a function argument, it's just the name.
        return n.metinsel_deger; 
    } // Kimlik
      if (n.tip == 5) { // Ikili Op
          if (n.metinsel_deger == "[") {
              // In YKN, x[i] depends on type. If it's *Jeton, it should be Jeton-based.
              // But unified compiler treats all as byte-based?
              // Let's try to detect if it's jetonlar[i]
              metin sol = ast_to_cpp(n.sol_id);
              metin idx = ast_to_cpp(n.sag_id);
              // In YKN v4, most pointers like jetonlar, tablo, kapsamlar are struct pointers.
              // We should only cast to char* if it's likely a byte buffer (kaynak, bellek).
              if (sol.find("kaynak") != std::string::npos || sol.find("bellek") != std::string::npos || sol.find("buffer") != std::string::npos) {
                  return "((char*)(" + sol + "))[" + idx + "]";
              }
              // To prevent error: expected unqualified-id before '(' token in C++ when expanded via YKN_DOT(obj, (tablo)[indeks])
              // We remove the parenthesis around sol if it doesn't seem to need it, or just rely on tight binding.
              // We will just do sol + "[" + idx + "]" for now. If it has operators, maybe add parens, but typical case is array name.
              return sol + "[" + idx + "]";
          } else if (n.metinsel_deger == ".") {
              // Check if right side is itself an array index (a.b[i] pattern)
              Dugum sag_dugum = (*g_ast_arena)[n.sag_id];
              if (sag_dugum.tip == 5 && sag_dugum.metinsel_deger == "[") {
                  // a.b[i] -> (_ykn_ref(a).b)[i]
                  metin field = ast_to_cpp(sag_dugum.sol_id);
                  metin idx   = ast_to_cpp(sag_dugum.sag_id);
                  metin sol   = ast_to_cpp(n.sol_id);
                  return "(_ykn_ref(" + sol + ")." + field + ")[" + idx + "]";
              }
              metin sag = ast_to_cpp(n.sag_id);
              if (sag == "ptr") {
                  metin sol = ast_to_cpp(n.sol_id);
                  if (sol.length() > 0 && sol[0] == '"') return "(char*)(" + sol + ")";
                  return "(char*)(" + sol + ").c_str()";
              }
              return "YKN_DOT(" + ast_to_cpp(n.sol_id) + ", " + sag + ")";
          } else if (n.metinsel_deger == "+") {
              metin sol = ast_to_cpp(n.sol_id);
              if (sol.find("kaynak") != std::string::npos || sol.find("bellek") != std::string::npos || sol.find("buffer") != std::string::npos) {
                  return "(char*)((char*)(" + sol + ") + " + ast_to_cpp(n.sag_id) + ")";
              }
              return "(" + sol + " + " + ast_to_cpp(n.sag_id) + ")";
          } else {
                std::string op = n.metinsel_deger;
                if(op == "ve") op = "&&";
                if(op == "veya") op = "||";
                metin sol = ast_to_cpp(n.sol_id);
                metin sag = ast_to_cpp(n.sag_id);
                if(op == "==") {
                    if (sag == "0") return "(!(" + sol + "))";
                    if (sol == "0") return "(!(" + sag + "))";
                }
                if(op == "!=") {
                    if (sag == "0") return "((" + sol + ") != 0)";
                    if (sol == "0") return "((" + sag + ") != 0)";
                }
                return "(" + sol + " " + op + " " + sag + ")";
          }
      }
      if (n.tip == 6) { return ast_to_cpp(n.sol_id) + " = " + ast_to_cpp(n.sag_id); } // Atama
    if (n.tip == 7) { // Degisken Tanimi
        std::string vname = n.metinsel_deger;
        std::string vtype = n.veri_tipi;
        std::string cpp_type = ykn_tip_to_cpp(vtype);
        if (cpp_type == "void") cpp_type = "auto";
        if (n.sag_id >= 0) {
            std::string rhs = ast_to_cpp(n.sag_id);
            if (cpp_type != "auto" && cpp_type.find('*') != std::string::npos) {
                return cpp_type + " " + vname + " = (" + cpp_type + ")(" + rhs + ")";
            }
            return cpp_type + " " + vname + " = " + rhs;
        } else {
              // No initializer
              std::string cpp_t = ykn_tip_to_cpp(vtype);
              if (!cpp_t.empty() && cpp_t.back() == '*') {
                  return cpp_t + " " + vname + " = nullptr";
              }
            if (g_struct_names.count(vtype)) {
                return cpp_t + " " + vname + "; memset(&" + vname + ", 0, sizeof(" + cpp_t + "))";
            }
            if (vtype == "LexerState") {
                return "LexerState " + vname + "; memset(&" + vname + ", 0, sizeof(LexerState))";
            }
              return cpp_t + " " + vname + " = 0";
        }
    }
    if (n.tip == 8) { // Eger
        metin c = "if (" + ast_to_cpp(n.kosul_id) + ") " + ast_to_cpp(n.govde_id);
        if (n.yoksa_id >= 0) { c += " else " + ast_to_cpp(n.yoksa_id); }
        return c;
    }
    if (n.tip == 20) { // Dongu
        metin cond = (n.kosul_id >= 0) ? ast_to_cpp(n.kosul_id) : "true";
        return "while (" + cond + ") " + ast_to_cpp(n.govde_id);
    }
    if (n.tip == 12) { // Islev Cagrisi
        metin c = n.metinsel_deger + "(";
        Liste<tamsayi> args = ayristir_id_cpp(n.cocuklar);
        for(size_t i=0; i<args.size(); ++i) {
            metin arg_str = ast_to_cpp(args[i]);
            // If the argument is a Kimlik and we know it's a struct type, 
            // but it's being passed to a function that expects a pointer,
            // we might need to take its address. However, YKN usually 
            // uses explicit pointers. The current issue is struct** vs struct*.
            c += arg_str;
            if(i < args.size()-1) c += ", ";
        }
        return c + ")";
    }
    if (n.tip == 13) { return n.metinsel_deger + ast_to_cpp(n.sag_id); } // Unary
    if (n.tip == 14) {
        if (n.sag_id < 0) return "return";
        metin ret_val = ast_to_cpp(n.sag_id);
        // If the return value is a Kimlik and it's a struct, we might need to return it.
        // But in YKN, structs are usually handled as pointers.
        return "return " + ret_val;
    } // Dondur
    if (n.tip == 15) { // Blok
        metin c = "{\n";
        Liste<tamsayi> stmts = ayristir_id_cpp(n.cocuklar);
        for(size_t i=0; i<stmts.size(); ++i) {
            c += ast_to_cpp(stmts[i]) + ";\n";
        }
        return c + "}\n";
    }
    if (n.tip == 18) { // Alan Erisimi
        if (n.metinsel_deger == "ptr") {
            metin sol = ast_to_cpp(n.sol_id);
            if (sol.length() > 0 && sol[0] == '"') return "(char*)(" + sol + ")";
            // If it's a std::string, use .c_str()
            return "(char*)(" + sol + ").c_str()";
        }
        return "YKN_DOT(" + ast_to_cpp(n.sol_id) + ", " + n.metinsel_deger + ")";
    }
    if (n.tip == 16) { // Islev
          if (n.metinsel_deger == "bellek_tahsis_et" || n.metinsel_deger == "ekrana_yaz" || 
              n.metinsel_deger == "dosya_oku" || n.metinsel_deger == "diske_bas_ve_kapat") {
              return ""; 
          }
          metin ret_tip;
          if (n.veri_tipi == "") ret_tip = "void";
          else ret_tip = ykn_tip_to_cpp(n.veri_tipi);
          if (n.metinsel_deger == "ana") ret_tip = "extern \"C\" long long"; // C++ entry
          metin c = ret_tip + " " + n.metinsel_deger + "(";
        
        std::string p_str = n.cocuklar;
        size_t p_pos = 0;
        std::vector<std::string> params;
        while((p_pos = p_str.find(",")) != std::string::npos) {
            if(p_pos > 0) params.push_back(p_str.substr(0, p_pos));
            p_str.erase(0, p_pos + 1);
        }
        if(p_str.length() > 0) params.push_back(p_str);
        
        for(size_t i=0; i<params.size(); ++i) {
            std::string p = params[i];
            // trim
            while (!p.empty() && p[0] == ' ') p = p.substr(1);
            while (!p.empty() && p.back() == ' ') p.pop_back();
            if (p.empty()) continue;
            size_t c_pos = p.find(":");
            std::string p_isim = (c_pos != std::string::npos) ? p.substr(0, c_pos) : p;
            std::string p_tip_ykn = (c_pos != std::string::npos) ? p.substr(c_pos+1) : "tamsayi";
            // trim
            while (!p_isim.empty() && p_isim[0]==' ') p_isim=p_isim.substr(1);
            while (!p_isim.empty() && p_isim.back()==' ') p_isim.pop_back();
            while (!p_tip_ykn.empty() && p_tip_ykn[0]==' ') p_tip_ykn=p_tip_ykn.substr(1);
            while (!p_tip_ykn.empty() && p_tip_ykn.back()==' ') p_tip_ykn.pop_back();
            std::string cpp_tip = ykn_tip_to_cpp(p_tip_ykn);
            c += cpp_tip + " " + p_isim;
            if(i < params.size()-1) c += ", ";
        }
        c += ") " + ast_to_cpp(n.govde_id) + "\n";
        return c;
    }
    if (n.tip == 19) { // Yapi (Struct)
        metin c = "struct " + n.metinsel_deger + " {\n";
        std::string f_str = n.cocuklar;
        size_t f_pos = 0;
        while ((f_pos = f_str.find(",")) != std::string::npos) {
            std::string field = f_str.substr(0, f_pos);
            f_str.erase(0, f_pos + 1);
            while (!field.empty() && field[0]==' ') field=field.substr(1);
            while (!field.empty() && field.back()==' ') field.pop_back();
            if (field.empty()) continue;
            size_t colon = field.find(":");
            std::string f_isim = (colon != std::string::npos) ? field.substr(0, colon) : field;
            std::string f_tip_ykn = (colon != std::string::npos) ? field.substr(colon+1) : "tamsayi";
            while (!f_isim.empty() && f_isim[0]==' ') f_isim=f_isim.substr(1);
            while (!f_isim.empty() && f_isim.back()==' ') f_isim.pop_back();
            while (!f_tip_ykn.empty() && f_tip_ykn[0]==' ') f_tip_ykn=f_tip_ykn.substr(1);
            while (!f_tip_ykn.empty() && f_tip_ykn.back()==' ') f_tip_ykn.pop_back();
            if (f_isim.empty()) continue;
            std::string cpp_tip = ykn_tip_to_cpp(f_tip_ykn);
            c += "    " + cpp_tip + " " + f_isim + ";\n";
        }
        return c + "};\n";
    }
    if (n.tip == 26) { return "break"; }
    if (n.tip == 27) { return "continue"; }
    return "";
}

metin _cpp_kodu_uret(KodUretici o) {
    metin c = "#include <iostream>\n#include <vector>\n#include <string>\n#include <cstdlib>\n#include <cstring>\n\n";
    c += "using tamsayi = long long;\nusing metin = std::string;\n\n";
    c += "extern \"C\" void matris_carp();\n\n";
    c += "template<typename T> inline T& _ykn_ref(T& x) { return x; }\n";
    c += "template<typename T> inline T& _ykn_ref(T* x) { return *x; }\n";
    c += "template<typename T> inline T& _ykn_ref(T** x) { return **x; }\n";
    c += "#define YKN_DOT(obj, member) (_ykn_ref(obj).member)\n\n";
    
    // YKN std mappings
    c += "inline tamsayi yazdir(const metin& m) { std::cout << m << std::endl; return 0; }\n";
    c += "inline tamsayi yazdir(tamsayi n) { std::cout << n << std::endl; return 0; }\n";
    c += "inline metin metin_birlestir(const metin& a, const metin& b) { return a + b; }\n";
    c += "inline metin tamsayi_metne(tamsayi n) { return std::to_string(n); }\n";
    c += "inline tamsayi argc_al() { extern int _ykn_argc; return _ykn_argc; }\n";
    c += "inline metin argv_al(tamsayi i) { extern int _ykn_argc; extern char** _ykn_argv; if(i<_ykn_argc) return _ykn_argv[i]; return \"\"; }\n";
    c += "inline char* bellek_tahsis_et(long long boyut) { return (char*)calloc(1, boyut); }\n";
    c += "inline long long ekrana_yaz(const char* m) { std::cout << m; return 0; }\n";
    c += "inline long long dosya_oku(const char* yol, char* buf, long long n) { FILE* f = fopen(yol, \"rb\"); if(!f) return 0; long long r = fread(buf, 1, n, f); fclose(f); return r; }\n";
    c += "inline long long diske_bas_ve_kapat(const char* yol, char* buf, long long n) { FILE* f = fopen(yol, \"wb\"); if(!f) return 0; long long w = fwrite(buf, 1, n, f); fclose(f); return w; }\n\n";
    
    metin global_decls = "";
    metin ana_body = "";
      if (g_ast_arena && g_root_id >= 0) {
          Dugum root = (*g_ast_arena)[g_root_id];
          Liste<tamsayi> globals = ayristir_id_cpp(root.cocuklar);
          
          std::set<std::string> emitted_struct_fwd;
          for(size_t i=0; i<globals.size(); ++i) {
              Dugum gn = (*g_ast_arena)[globals[i]];
              if (gn.tip == 19) {
                  if (emitted_struct_fwd.count(gn.metinsel_deger) == 0) {
                      emitted_struct_fwd.insert(gn.metinsel_deger);
                      global_decls += "struct " + gn.metinsel_deger + ";\n";
                  }
              }
          }

          std::set<std::string> emitted_structs;
          for(size_t i=0; i<globals.size(); ++i) {
              Dugum gn = (*g_ast_arena)[globals[i]];
              if (gn.tip == 19) {
                  g_struct_names.insert(gn.metinsel_deger); // register struct name
                  if (emitted_structs.count(gn.metinsel_deger) == 0) {
                      emitted_structs.insert(gn.metinsel_deger);
                      global_decls += ast_to_cpp(globals[i]) + "\n";
                  }
              }
          }
          std::set<std::string> emitted_fn_decls;
          for(size_t i=0; i<globals.size(); ++i) {
              Dugum gn = (*g_ast_arena)[globals[i]];
              if (gn.tip == 16) {
                  if (emitted_fn_decls.count(gn.metinsel_deger) == 0) {
                      emitted_fn_decls.insert(gn.metinsel_deger);
                      metin ret_tip;
                      if (gn.veri_tipi == "") ret_tip = "void";
                      else ret_tip = ykn_tip_to_cpp(gn.veri_tipi);
                      if (gn.metinsel_deger == "ana") ret_tip = "extern \"C\" long long";
                      
                        metin decl = ret_tip + " " + gn.metinsel_deger + "(";
                        std::string p_str = gn.cocuklar;
                        size_t p_pos = 0;
                        std::vector<std::string> params;
                        while((p_pos = p_str.find(",")) != std::string::npos) {
                            if(p_pos > 0) params.push_back(p_str.substr(0, p_pos));
                            p_str.erase(0, p_pos + 1);
                        }
                        if(p_str.length() > 0) params.push_back(p_str);
                        
                        for(size_t j=0; j<params.size(); ++j) {
                            std::string p = params[j];
                            while (!p.empty() && p[0]==' ') p=p.substr(1);
                            while (!p.empty() && p.back()==' ') p.pop_back();
                            if (p.empty()) continue;
                            size_t c_pos = p.find(":");
                            std::string p_isim = (c_pos != std::string::npos) ? p.substr(0, c_pos) : p;
                            std::string p_tip_ykn = (c_pos != std::string::npos) ? p.substr(c_pos+1) : "tamsayi";
                            while (!p_isim.empty() && p_isim[0]==' ') p_isim=p_isim.substr(1);
                            while (!p_isim.empty() && p_isim.back()==' ') p_isim.pop_back();
                            while (!p_tip_ykn.empty() && p_tip_ykn[0]==' ') p_tip_ykn=p_tip_ykn.substr(1);
                            while (!p_tip_ykn.empty() && p_tip_ykn.back()==' ') p_tip_ykn.pop_back();
                            std::string cpp_tip = ykn_tip_to_cpp(p_tip_ykn);
                            decl += cpp_tip + " " + p_isim;
                            if(j < params.size()-1) decl += ", ";
                        }
                        decl += ");\n";
                      if (gn.metinsel_deger != "bellek_tahsis_et" && gn.metinsel_deger != "ekrana_yaz" && 
                          gn.metinsel_deger != "dosya_oku" && gn.metinsel_deger != "diske_bas_ve_kapat") {
                          global_decls += decl;
                      }
                  }
              }
          }
          // deduplicate non-function/non-struct globals -> emit at global scope
          std::set<std::string> emitted_globals;
          for(size_t i=0; i<globals.size(); ++i) {
              Dugum gn = (*g_ast_arena)[globals[i]];
              if (gn.tip == 16) {
                  std::string fn_code = ast_to_cpp(globals[i]);
                  std::string fn_name = gn.metinsel_deger;
                  if (emitted_globals.count(fn_name) == 0) {
                      emitted_globals.insert(fn_name);
                      global_decls += fn_code + "\n";
                  }
              } else if (gn.tip == 7 || gn.tip != 19) {
                  std::string stmt = ast_to_cpp(globals[i]);
                  std::string key = gn.metinsel_deger;
                  if (key.empty()) {
                      key = stmt;
                      size_t eq = stmt.find('=');
                      size_t sp = stmt.find(' ');
                      if (eq != std::string::npos && sp != std::string::npos && sp < eq) {
                          key = stmt.substr(sp+1, eq-sp-1);
                          while (!key.empty() && key[0]==' ') key=key.substr(1);
                          while (!key.empty() && key.back()==' ') key.pop_back();
                      }
                  }
                  if (emitted_globals.count(key) == 0) {
                      emitted_globals.insert(key);
                      if (!stmt.empty() && stmt.back() != '}' && stmt.back() != ';') stmt += ";";
                      global_decls += stmt + "\n";
                  }
              }
          }
      }
    if (ana_body != "") {
        c += "\nextern \"C\" long long ana(long long argc, char** argv) {\n";
        c += ana_body;
        c += "return 0;\n}\n";
    }
    c += global_decls;
    c += "\nint main(int argc, char** argv) {\n";
    c += "    extern int _ykn_argc; extern char** _ykn_argv;\n";
    c += "    _ykn_argc = argc; _ykn_argv = argv;\n";
    c += "    return (int)ana((long long)argc, argv);\n";
    c += "}\n";
    c += "int _ykn_argc; char** _ykn_argv;\n";
    o.cikti_kodu = c;
    return o.cikti_kodu;
}
metin _mir_reg_isim(tamsayi idx) {
if (idx == 0) { return "r8"; }
if (idx == 1) { return "r9"; }
if (idx == 2) { return "r10"; }
if (idx == 3) { return "r11"; }
if (idx == 4) { return "r12"; }
if (idx == 5) { return "r13"; }
if (idx == 6) { return "r14"; }
if (idx == 7) { return "r15"; }
if (idx == 8) { return "rbx"; }
if (idx == 9) { return "rcx"; }
return "rax";
}
metin _asm_kodu_uret(KodUretici o) {
    metin asm_kodu = ".data\n\n.text\n.global main\n\n";;
o.cikti_kodu = metin_birlestir(o.cikti_kodu, asm_kodu);
    MirProgram m_prog = o.mir_prog;;
    tamsayi p_len = liste_uzunluk(m_prog.islevler);;
    tamsayi i = 0;;
while (i < p_len) {
    MirIslev f = liste_al(m_prog.islevler, i);;
    metin fn_imza = metin_birlestir(f.isim, ":\n");;
fn_imza = metin_birlestir(fn_imza, "    push rbp\n");
fn_imza = metin_birlestir(fn_imza, "    mov rbp, rsp\n");
o.cikti_kodu = metin_birlestir(o.cikti_kodu, fn_imza);
    tamsayi b_len = liste_uzunluk(f.bloklar);;
    tamsayi bi = 0;;
while (bi < b_len) {
    MirBlok b = liste_al(f.bloklar, bi);;
o.cikti_kodu = metin_birlestir(o.cikti_kodu, metin_birlestir(b.etiket, ":\n"));
    tamsayi t_len = liste_uzunluk(b.talimatlar);;
    tamsayi ti = 0;;
while (ti < t_len) {
    tamsayi d_id = liste_al(b.talimatlar, ti);;
    MirDugum d = liste_al(m_prog.arena, d_id);;
if (d.tip == MIR_MOV) {
    metin h_reg = _mir_reg_isim(d.yazmac_hedef);;
if (metin_uzunlugu(d.anlik_deger) > 0) {
o.cikti_kodu = metin_birlestir(o.cikti_kodu, metin_birlestir(metin_birlestir("    mov ", h_reg), metin_birlestir(", ", metin_birlestir(d.anlik_deger, "\n"))));
} else {
    metin s_reg = _mir_reg_isim(d.yazmac_kaynak1);;
o.cikti_kodu = metin_birlestir(o.cikti_kodu, metin_birlestir(metin_birlestir("    mov ", h_reg), metin_birlestir(", ", metin_birlestir(s_reg, "\n"))));
}
} else if (d.tip == MIR_ADD) {
    metin h_reg = _mir_reg_isim(d.yazmac_hedef);;
    metin s_reg = _mir_reg_isim(d.yazmac_kaynak1);;
o.cikti_kodu = metin_birlestir(o.cikti_kodu, metin_birlestir(metin_birlestir("    add ", h_reg), metin_birlestir(", ", metin_birlestir(s_reg, "\n"))));
} else if (d.tip == MIR_CALL) {
o.cikti_kodu = metin_birlestir(o.cikti_kodu, metin_birlestir("    call ", metin_birlestir(d.etiket, "\n")));
}
ti = ti + 1;
}
bi = bi + 1;
}
o.cikti_kodu = metin_birlestir(o.cikti_kodu, "    mov eax, 0\n");
o.cikti_kodu = metin_birlestir(o.cikti_kodu, "    pop rbp\n");
o.cikti_kodu = metin_birlestir(o.cikti_kodu, "    ret\n\n");
i = i + 1;
}
return o.cikti_kodu;
}
metin kod_uret(KodUretici o) {
if (metin_esitmi(o.hedef_dil, "cpp") == 1) {
o.cikti_kodu = _cpp_kodu_uret(o);
} else {
o.cikti_kodu = _asm_kodu_uret(o);
}
return o.cikti_kodu;
}
// --- END codegen.ykn ---
// --- START preprocessor.ykn ---
metin onislemci_ic(metin dosya_yolu, Liste<metin>& ziyaret_edilenler) {
    tamsayi z_len = liste_uzunluk(ziyaret_edilenler);;
    tamsayi z_i = 0;;
while (z_i < z_len) {
    metin z_dosya = liste_al(ziyaret_edilenler, z_i);;
if (metin_esitmi(z_dosya, dosya_yolu) == 1) {
return "";
}
z_i = z_i + 1;
}
liste_ekle(ziyaret_edilenler, dosya_yolu);
    metin icerik = dosyadan_oku(dosya_yolu);;
icerik = metin_degistir(icerik, "\r", "");
    metin dizin = dosya_dizini(dosya_yolu);;
    Liste<metin> satirlar = metin_bol(icerik, "\n");;
    tamsayi s_len = liste_uzunluk(satirlar);;
    tamsayi s_i = 0;;
    metin sonuc = "";;
sonuc = metin_birlestir(sonuc, metin_birlestir("# --- BASLANGIC ", metin_birlestir(dosya_yolu, " ---\n")));
while (s_i < s_len) {
    metin satir = liste_al(satirlar, s_i);;
if (metin_basliyor_mu(satir, "içe_aktar") == 1 || metin_basliyor_mu(satir, "ice_aktar") == 1) {
    tamsayi tirnak1 = 0;;
    tamsayi tirnak2 = 0;;
    tamsayi j = 0;;
    tamsayi s_uz = metin_uzunlugu(satir);;
while (j < s_uz) {
    metin h = karakter_al(satir, j);;
if (metin_esitmi(h, "\"") == 1) {
if (tirnak1 == 0) {
tirnak1 = j + 1;
} else {
tirnak2 = j;
}
}
j = j + 1;
}
if (tirnak2 > tirnak1) {
    metin aktarilan = alt_metin(satir, tirnak1, tirnak2);;
    metin yeni_yol = yol_birlestir(dizin, aktarilan);;
    metin ic_kod = onislemci_ic(yeni_yol, ziyaret_edilenler);;
sonuc = metin_birlestir(sonuc, ic_kod);
sonuc = metin_birlestir(sonuc, "\n");
}
} else {
sonuc = metin_birlestir(sonuc, satir);
sonuc = metin_birlestir(sonuc, "\n");
}
s_i = s_i + 1;
}
sonuc = metin_birlestir(sonuc, metin_birlestir("# --- BITIS ", metin_birlestir(dosya_yolu, " ---\n")));
return sonuc;
}
metin onislemci_calistir(metin dosya_yolu) {
    Liste<metin> ziyaret_edilenler;
return onislemci_ic(dosya_yolu, ziyaret_edilenler);
}
// --- END preprocessor.ykn ---
// --- START locale_loader.ykn ---
    Liste<LocaleKelime> o_locale_kelimeler;
    tamsayi o_locale_yuklendi = 0;;
tamsayi locale_kelime_ekle(tamsayi t_id, metin y_isim) {
    LocaleKelime m;
m.token_id = t_id;
m.yerel_isim = y_isim;
liste_ekle(o_locale_kelimeler, m);
return 0;
}
tamsayi temizle_ve_ekle(tamsayi token_id, metin val_part) {
    Liste<metin> parcalar = metin_bol(val_part, "\"");;
    tamsayi i = 0;;
    tamsayi n = liste_uzunluk(parcalar);;
while (i < n) {
    metin p = liste_al(parcalar, i);;
if (metin_uzunlugu(p) > 1) {
if (metin_icerir(p, "[") == 0 && metin_icerir(p, "]") == 0 && metin_icerir(p, ",") == 0 && metin_icerir(p, " ") == 0) {
locale_kelime_ekle(token_id, p);
}
}
i = i + 1;
}
return 0;
}
tamsayi locale_yukle(metin dil_kodu) {
if (o_locale_yuklendi == 1) { return 1; }
o_locale_yuklendi = 1;
    metin dosya_adi = metin_birlestir("locales/", dil_kodu);;
dosya_adi = metin_birlestir(dosya_adi, ".json");
    metin icerik = dosyadan_oku(dosya_adi);;
if (metin_uzunlugu(icerik) == 0) {
yazdir("UYARI: Locale dosyasi bulunamadi, varsayilan dil kullanilacak.");
return 0;
}
    Liste<metin> satirlar = metin_bol(icerik, "\n");;
    tamsayi i = 0;;
    tamsayi n = liste_uzunluk(satirlar);;
while (i < n) {
    metin satir = liste_al(satirlar, i);;
if (metin_icerir(satir, "\"TOKEN_") == 1) {
    Liste<metin> iki_nokta_parcalari = metin_bol(satir, ":");;
if (liste_uzunluk(iki_nokta_parcalari) >= 2) {
    metin key_part = liste_al(iki_nokta_parcalari, 0);;
    metin val_part = liste_al(iki_nokta_parcalari, 1);;
    tamsayi t_id = 99;;
if (metin_icerir(key_part, "TOKEN_FUNC") == 1) { t_id = 1; }
else if (metin_icerir(key_part, "TOKEN_VAR") == 1) { t_id = 2; }
else if (metin_icerir(key_part, "TOKEN_RETURN") == 1) { t_id = 3; }
else if (metin_icerir(key_part, "TOKEN_IF") == 1) { t_id = 4; }
else if (metin_icerir(key_part, "TOKEN_ELSE_IF") == 1) { t_id = 6; }
else if (metin_icerir(key_part, "TOKEN_ELSE") == 1) { t_id = 5; }
else if (metin_icerir(key_part, "TOKEN_WHILE") == 1) { t_id = 7; }
else if (metin_icerir(key_part, "TOKEN_FOR") == 1) { t_id = 8; }
else if (metin_icerir(key_part, "TOKEN_STRUCT") == 1) { t_id = 13; }
else if (metin_icerir(key_part, "TOKEN_ENUM") == 1) { t_id = 14; }
else if (metin_icerir(key_part, "TOKEN_BREAK") == 1) { t_id = 17; }
else if (metin_icerir(key_part, "TOKEN_CONTINUE") == 1) { t_id = 18; }
else if (metin_icerir(key_part, "TOKEN_TRY") == 1) { t_id = 19; }
else if (metin_icerir(key_part, "TOKEN_CATCH") == 1) { t_id = 20; }
else if (metin_icerir(key_part, "TOKEN_FINALLY") == 1) { t_id = 21; }
else if (metin_icerir(key_part, "TOKEN_THROW") == 1) { t_id = 22; }
else if (metin_icerir(key_part, "TOKEN_AND") == 1) { t_id = 60; }
else if (metin_icerir(key_part, "TOKEN_OR") == 1) { t_id = 61; }
if (t_id != 99) {
temizle_ve_ekle(t_id, val_part);
}
}
}
i = i + 1;
}
return 1;
}
tamsayi locale_anahtar_kelime(metin isim) {
if (o_locale_yuklendi == 1) {
    tamsayi n = liste_uzunluk(o_locale_kelimeler);;
    tamsayi i = 0;;
while (i < n) {
    LocaleKelime k = liste_al(o_locale_kelimeler, i);;
if (metin_esitmi(k.yerel_isim, isim) == 1) {
return k.token_id;
}
i = i + 1;
}
}
return 9;
}
    Liste<LocaleKelime> o_hedef_kelimeler;
    tamsayi o_hedef_yuklendi = 0;;
tamsayi locale_hedef_kelime_ekle(tamsayi t_id, metin y_isim) {
    LocaleKelime m;
m.token_id = t_id;
m.yerel_isim = y_isim;
liste_ekle(o_hedef_kelimeler, m);
return 0;
}
tamsayi hedef_temizle_ve_ekle(tamsayi token_id, metin val_part) {
    Liste<metin> parcalar = metin_bol(val_part, "\"");;
    tamsayi i = 0;;
    tamsayi n = liste_uzunluk(parcalar);;
    tamsayi eklendi = 0;;
while (i < n) {
    metin p = liste_al(parcalar, i);;
if (metin_uzunlugu(p) > 1) {
if (metin_icerir(p, "[") == 0 && metin_icerir(p, "]") == 0 && metin_icerir(p, ",") == 0 && metin_icerir(p, " ") == 0) {
if (eklendi == 0) {
locale_hedef_kelime_ekle(token_id, p);
eklendi = 1;
}
}
}
i = i + 1;
}
return 0;
}
tamsayi locale_hedef_yukle(metin dil_kodu) {
o_hedef_yuklendi = 1;
    metin dosya_adi = metin_birlestir("locales/", dil_kodu);;
dosya_adi = metin_birlestir(dosya_adi, ".json");
    metin icerik = dosyadan_oku(dosya_adi);;
if (metin_uzunlugu(icerik) == 0) { return 0; }
    Liste<metin> satirlar = metin_bol(icerik, "\n");;
    tamsayi i = 0;;
    tamsayi n = liste_uzunluk(satirlar);;
while (i < n) {
    metin satir = liste_al(satirlar, i);;
if (metin_icerir(satir, "\"TOKEN_") == 1) {
    Liste<metin> iki_nokta_parcalari = metin_bol(satir, ":");;
if (liste_uzunluk(iki_nokta_parcalari) >= 2) {
    metin key_part = liste_al(iki_nokta_parcalari, 0);;
    metin val_part = liste_al(iki_nokta_parcalari, 1);;
    tamsayi t_id = 99;;
if (metin_icerir(key_part, "TOKEN_FUNC") == 1) { t_id = 1; }
else if (metin_icerir(key_part, "TOKEN_VAR") == 1) { t_id = 2; }
else if (metin_icerir(key_part, "TOKEN_RETURN") == 1) { t_id = 3; }
else if (metin_icerir(key_part, "TOKEN_IF") == 1) { t_id = 4; }
else if (metin_icerir(key_part, "TOKEN_ELSE_IF") == 1) { t_id = 6; }
else if (metin_icerir(key_part, "TOKEN_ELSE") == 1) { t_id = 5; }
else if (metin_icerir(key_part, "TOKEN_WHILE") == 1) { t_id = 7; }
else if (metin_icerir(key_part, "TOKEN_FOR") == 1) { t_id = 8; }
else if (metin_icerir(key_part, "TOKEN_STRUCT") == 1) { t_id = 13; }
else if (metin_icerir(key_part, "TOKEN_ENUM") == 1) { t_id = 14; }
else if (metin_icerir(key_part, "TOKEN_BREAK") == 1) { t_id = 17; }
else if (metin_icerir(key_part, "TOKEN_CONTINUE") == 1) { t_id = 18; }
if (t_id != 99) {
hedef_temizle_ve_ekle(t_id, val_part);
}
}
}
i = i + 1;
}
return 1;
}
metin locale_hedef_getir(tamsayi t_id) {
if (o_hedef_yuklendi == 1) {
    tamsayi n = liste_uzunluk(o_hedef_kelimeler);;
    tamsayi i = 0;;
while (i < n) {
    LocaleKelime k = liste_al(o_hedef_kelimeler, i);;
if (k.token_id == t_id) {
return k.yerel_isim;
}
i = i + 1;
}
}
return "";
}
// --- END locale_loader.ykn ---
// --- START pkg_manager.ykn ---
tamsayi _hedef_dizin_olustur(metin repo_yolu) {
return 1;
}
metin _repo_url_to_raw(metin repo_path) {
    metin raw_url = "https://raw.githubusercontent.com/";;
if (metin_uzunlugu(repo_path) > 11) {
    metin alt_path = alt_metin(repo_path, 11, metin_uzunlugu(repo_path));;
raw_url = metin_birlestir(raw_url, alt_path);
raw_url = metin_birlestir(raw_url, "/main/main.ykn");
return raw_url;
}
return "";
}
tamsayi bagimliliklari_coz(metin kaynak) {
    Liste<metin> satirlar = metin_bol(kaynak, "\n");;
    tamsayi i = 0;;
    tamsayi n = liste_uzunluk(satirlar);;
yazdir("--- YKNPM Paket Cozumleyici (Pre-flight) ---");
while (i < n) {
    metin satir = liste_al(satirlar, i);;
if (metin_icerir(satir, "github.com/") == 1) {
    Liste<metin> parcalar = metin_bol(satir, "\"");;
if (liste_uzunluk(parcalar) > 1) {
    metin repo_path = liste_al(parcalar, 1);;
yazdir(metin_birlestir("[YKNPM] Uzak paket tespit edildi: ", repo_path));
    metin raw_url = _repo_url_to_raw(repo_path);;
    metin hedef_dosya = metin_birlestir(".ykn_cache/", repo_path);;
    tamsayi durum = paket_indir(raw_url, "gecici_indirme.ykn");;
if (durum == 1) {
yazdir("[YKNPM] Paket basariyla indirildi.");
} else {
yazdir("[YKNPM] UYARI: Paket indirilemedi.");
}
}
}
i = i + 1;
}
return 1;
}
// --- END pkg_manager.ykn ---
tamsayi kullanimi_goster() {
yazdir("YukiLLM Compiler (yukic) v1.0");
yazdir("Kullanim:");
yazdir("  yukic build <dosya.ykn>  - Derler ve binary olusturur");
yazdir("  yukic run <dosya.ykn>    - Derler ve hemen calistirir");
return 1;
}
int _ykn_argc; char** _ykn_argv;
int main(int argc, char** argv) {
    _ykn_argc = argc; _ykn_argv = argv;
    tamsayi arg_s = argc_al();;
if (arg_s < 3) {
return kullanimi_goster();
}
    metin komut = argv_al(1);;
    metin girdi_dosyasi = argv_al(2);;
if ((metin_esitmi(komut, "build") == 0) && (metin_esitmi(komut, "run") == 0)) {
yazdir("Hata: Bilinmeyen komut.");
return kullanimi_goster();
}
yazdir(metin_birlestir("YKN Derleniyor: ", girdi_dosyasi));
locale_yukle("tr");
    metin asil_kaynak = dosyadan_oku(girdi_dosyasi);;
    bagimliliklari_coz(asil_kaynak);
    metin kaynak = onislemci_calistir(girdi_dosyasi);;
    dosyaya_yaz("preprocessed.ykn", kaynak);
    Liste<Jeton> jetonlar;
    tokenize_et(kaynak, jetonlar);
    Liste<Dugum> ast_arena;
    tamsayi root_id = parser_cozumle(jetonlar, ast_arena);;
    g_ast_arena = &ast_arena;
    g_root_id = root_id;
    tamsayi tip_durum = tip_kontrol_baslat(ast_arena, root_id);;
    HirOlusturucu h_olusturucu = hir_olusturucu_kur(ast_arena);;
ziyaret_global(h_olusturucu, root_id);
    CfgOlusturucu cfg_o = cfg_olusturucu_kur(h_olusturucu.program);;
    CfgProgram cfg_prog = cfg_olustur(cfg_o);;
    SsaOlusturucu ssa_o = ssa_olusturucu_kur(cfg_prog);;
ssa_uygula(ssa_o);
    Optimizator opt_o = optimizator_kur(cfg_prog);;
optimize_et(opt_o);
    RegAllocOlusturucu reg_o = regalloc_kur(cfg_prog);;
regalloc_calistir(reg_o);
    MirOlusturucu mir_o = mir_olusturucu_kur(cfg_prog);;
mir_olustur(mir_o);
    KodUretici codegen_o = kod_uretici_kur(mir_o.mir_prog, "cpp");;
    metin cpp_kodu = kod_uret(codegen_o);;
    metin out_dosya = "app.cpp";;
dosyaya_yaz(out_dosya, cpp_kodu);
yazdir("Makine Kodu (C++) uretildi: app.cpp");
    tamsayi derleme_sonucu = sistem_komutu("g++ -std=c++17 -Wno-write-strings -static-libgcc -static-libstdc++ app.cpp -o app.exe");;
if (derleme_sonucu != 0) {
yazdir("HATA: Assembly derleme asamasinda hata olustu (GCC/AS bulunamadi)!");
return 1;
}
yazdir("Basariyla native binary uretildi: app.exe");
if (metin_esitmi(komut, "run") == 1) {
yazdir("--- CALISTIRILIYOR ---");
sistem_komutu(".\\app.exe");
}
return 0;
}
// --- END main.ykn ---