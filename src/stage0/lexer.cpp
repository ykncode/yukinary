#include "ykn_std.h"
// Yukinary Dilinde Yazilmis Lexer (v3 - Self-Hosting)

// Bu dosya Python bootstrap ile derlenebilir ve kendi kaynagini tokenize edebilir.

//

// Degisiklikler v3'te:

//   - ise devam / ise kir yerine devam_et / durdur kullanilir

//   - karakter_kodu(), karakter() builtinleri eklendi

//   - metin_birlestir() ile metin birlestirme

//   - TIP_* sabitleri => numaralandirma ile duzenli


// ============================================================

// Token Tip Tanimlari (tamsayi sabitleri)

// ============================================================

    auto TIP_ISLEV = 1;
    auto TIP_DEGISKEN = 2;
    auto TIP_DONDUR = 3;
    auto TIP_EGER = 4;
    auto TIP_ISE = 5;
    auto TIP_YOKSA = 6;
    auto TIP_DONGU = 7;
    auto TIP_HER = 8;
    auto TIP_KIMLIK = 9;
    auto TIP_TAMSAYI = 10;
    auto TIP_GERCEK_SAYI = 11;
    auto TIP_METIN = 12;
    auto TIP_YAPI = 13;
    auto TIP_NUMARALANDIRMA = 14;
    auto TIP_ICE_AKTAR = 15;
    auto TIP_BORU_HATTI = 16;
    auto TIP_DURDUR = 17;
    auto TIP_DEVAM_ET = 18;
    auto TIP_DENEME = 19;
    auto TIP_YAKALA = 20;
    auto TIP_NIHAYET = 21;
    auto TIP_FIRLATMA = 22;
    auto TIP_ARTI = 30;
    auto TIP_EKSI = 31;
    auto TIP_CARPI = 32;
    auto TIP_BOLME = 33;
    auto TIP_ATAMA = 34;
    auto TIP_ESIT_MI = 35;
    auto TIP_ESIT_DEGIL = 36;
    auto TIP_KUCUKTUR = 37;
    auto TIP_BUYUKTUR = 38;
    auto TIP_KUCUK_ESIT = 39;
    auto TIP_BUYUK_ESIT = 40;
    auto TIP_SOL_PARANTEZ = 50;
    auto TIP_SAG_PARANTEZ = 51;
    auto TIP_SOL_KUSLU = 52;
    auto TIP_SAG_KUSLU = 53;
    auto TIP_SOL_KOSELI = 54;
    auto TIP_SAG_KOSELI = 55;
    auto TIP_IKI_NOKTA = 56;
    auto TIP_NOKTALI_VIRGUL = 57;
    auto TIP_VIRGUL = 58;
    auto TIP_OK = 59;
    auto TIP_VE_MANTIK    = 60;
    auto TIP_VEYA_MANTIK  = 61;
    auto TIP_NOKTA        = 62;
    auto TIP_AMPERSAND    = 63;
    auto TIP_MUT          = 64;
    auto TIP_DAVRANIS     = 65;
    auto TIP_UYGULA       = 66;
    auto TIP_ICIN         = 67;
    auto TIP_MAKRO_KUR    = 68;
    auto TIP_EOF          = 99;

// ============================================================

// Yapi: Jeton (Token)

// ============================================================

struct Jeton {
    tamsayi tip;
    metin deger;
    tamsayi satir;
    tamsayi sutun;
};

// ============================================================

// Karakter Sinif Belirleme Yardimci Fonksiyonlari

// ============================================================

tamsayi harf_mi(tamsayi k) {
    if (k > 64) {
        if (k < 91) { return 1; }
    }
    if (k > 96) {
        if (k < 123) { return 1; }
    }
    // Turkce harfler (kaba aralik)

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

// ============================================================

// Anahtar Kelime Eslestirici

// ============================================================

tamsayi anahtar_kelime_tanimla(metin isim) {
    if (metin_esitmi(isim, "islev") == 1) { return TIP_ISLEV; }
    if (metin_esitmi(isim, "isv") == 1) { return TIP_ISLEV; }
    if (metin_esitmi(isim, "degisken") == 1) { return TIP_DEGISKEN; }
    if (metin_esitmi(isim, "dondu") == 1) { return TIP_DONDUR; }
    if (metin_esitmi(isim, "eger") == 1) { return TIP_EGER; }
    if (metin_esitmi(isim, "ise") == 1) { return TIP_ISE; }
    if (metin_esitmi(isim, "yoksa") == 1) { return TIP_YOKSA; }
    if (metin_esitmi(isim, "dongu") == 1) { return TIP_DONGU; }
    if (metin_esitmi(isim, "her") == 1) { return TIP_HER; }
    if (metin_esitmi(isim, "yapi") == 1) { return TIP_YAPI; }
    if (metin_esitmi(isim, "numaralandirma") == 1) { return TIP_NUMARALANDIRMA; }
    if (metin_esitmi(isim, "durdur") == 1) { return TIP_DURDUR; }
    if (metin_esitmi(isim, "break") == 1) { return TIP_DURDUR; }
    if (metin_esitmi(isim, "devam_et") == 1) { return TIP_DEVAM_ET; }
    if (metin_esitmi(isim, "continue") == 1) { return TIP_DEVAM_ET; }
    if (metin_esitmi(isim, "deneme") == 1) { return TIP_DENEME; }
    if (metin_esitmi(isim, "yakala") == 1) { return TIP_YAKALA; }
    if (metin_esitmi(isim, "nihayet") == 1) { return TIP_NIHAYET; }
    if (metin_esitmi(isim, "firlatma") == 1) { return TIP_FIRLATMA; }
    if (metin_esitmi(isim, "ve") == 1) { return TIP_VE_MANTIK; }
    if (metin_esitmi(isim, "veya") == 1) { return TIP_VEYA_MANTIK; }
    if (metin_esitmi(isim, "mut") == 1) { return TIP_MUT; }
    if (metin_esitmi(isim, "davranis") == 1) { return TIP_DAVRANIS; }
    if (metin_esitmi(isim, "uygula") == 1) { return TIP_UYGULA; }
    if (metin_esitmi(isim, "icin") == 1) { return TIP_ICIN; }
    if (metin_esitmi(isim, "makro_kur") == 1) { return TIP_MAKRO_KUR; }
    return TIP_KIMLIK;
}

// ============================================================

// Jeton Ekleyici Yardimci

// ============================================================

tamsayi jeton_ekle(Liste<Jeton>& jetonlar, tamsayi tip, metin deger, tamsayi sat, tamsayi sut) {
    Jeton j;
    j.tip   = tip;
    j.deger = deger;
    j.satir = sat;
    j.sutun = sut;
    liste_ekle(jetonlar, j);
    return 0;
}

// ============================================================

// Ana Tokenize Fonksiyonu

// ============================================================

tamsayi tokenize_et(metin kaynak, Liste<Jeton>& jetonlar) {
    tamsayi poz = 0;
    tamsayi sat = 1;
    tamsayi sut = 1;
    tamsayi uzunluk;
    uzunluk = metin_uzunlugu(kaynak);

    while (poz < uzunluk) {
    tamsayi c;
        c = karakter_kodu(karakter_al(kaynak, poz));

        // Bosluk / yeni satir atlama

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

        // Yorum satiri: # veya //

        if (c == 35) {
            while (poz < uzunluk && karakter_kodu(karakter_al(kaynak, poz)) != 10) {
                poz = poz + 1;
            }
            continue;
        }
    tamsayi c2 = 0;
        if (poz + 1 < uzunluk) {
            c2 = karakter_kodu(karakter_al(kaynak, poz + 1));
        }
        if (c == 47 && c2 == 47) {
            while (poz < uzunluk && karakter_kodu(karakter_al(kaynak, poz)) != 10) {
                poz = poz + 1;
            }
            continue;
        }

        // Metin sabiti: "..."

        if (c == 34) {
            poz = poz + 1;
            sut = sut + 1;
    metin metin_ic;
            metin_ic = "";
    tamsayi basti_sut = sut;
            while (poz < uzunluk) {
    tamsayi cc;
                cc = karakter_kodu(karakter_al(kaynak, poz));
                if (cc == 34) {
                    poz = poz + 1;
                    sut = sut + 1;
                    break;
                }
                if (cc == 92 && poz + 1 < uzunluk) {
                    // kacis karakteri

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

        // Kimlik / Anahtar kelime

        if (harf_mi(c) == 1 || alt_cizgi_mi(c) == 1) {
    tamsayi basla = poz;
    tamsayi basla_sut = sut;
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

        // Sayi: tamsayi veya gercek

        if (rakam_mi(c) == 1) {
    tamsayi basla = poz;
    tamsayi basla_sut = sut;
    tamsayi nokta_goruldu = 0;
            while (poz < uzunluk) {
    tamsayi kk;
                kk = karakter_kodu(karakter_al(kaynak, poz));
                if (rakam_mi(kk) == 1) {
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

        // Cift karakterli operatorler

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

        // Tek karakterli operatorler

        if (c == 43) { jeton_ekle(jetonlar, TIP_ARTI, "+", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
        if (c == 45) { jeton_ekle(jetonlar, TIP_EKSI, "-", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
        if (c == 42) { jeton_ekle(jetonlar, TIP_CARPI, "*", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
        if (c == 47) { jeton_ekle(jetonlar, TIP_BOLME, "/", sat, sut); poz = poz + 1; sut = sut + 1; continue; }
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
        if (c == 38) { jeton_ekle(jetonlar, TIP_AMPERSAND, "&", sat, sut); poz = poz + 1; sut = sut + 1; continue; }

        // Bilinmeyen karakter — atla

        poz = poz + 1;
        sut = sut + 1;
    }

    // EOF tokeni

    jeton_ekle(jetonlar, TIP_EOF, "", sat, sut);
    return liste_uzunluk(jetonlar);
}

// ============================================================

// Self-Test: lexer.ykn kendi kendini tokenize edebilmeli

// BEKLENEN_CIKIS: 11

// ============================================================

int lexer_main() {
    metin kaynak;
    kaynak = "islev topla(a: tamsayi) -> tamsayi { dondu a + 7; }";
    Liste<Jeton> jetonlar;
    tamsayi n;
    n = tokenize_et(kaynak, jetonlar);
    yazdir(n);
    return 0;
}