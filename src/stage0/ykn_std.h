#ifndef YKN_STD_H
#define YKN_STD_H

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "ykn_ai_core.h"

using metin = std::string;
using tamsayi = long long;

template<typename T>
using Liste = std::vector<T>;

inline tamsayi yazdir(const metin& m) {
    std::cout << m << std::endl;
    return 0;
}

inline metin tamsayi_metne(tamsayi n) {
    return std::to_string(n);
}

inline tamsayi metin_uzunluk(const metin& m) {
    return m.length();
}

inline metin metin_birlestir(const metin& a, const metin& b) {
    return a + b;
}

inline metin parca_al(const metin& m, tamsayi baslangic, tamsayi bitis) {
    if (baslangic < 0) baslangic = 0;
    if (bitis > m.length()) bitis = m.length();
    if (baslangic >= bitis) return "";
    return m.substr(baslangic, bitis - baslangic);
}

inline metin alt_metin(const metin& m, tamsayi basla, tamsayi bitis) { return parca_al(m, basla, bitis); }
inline tamsayi metin_uzunlugu(const metin& m) { return m.length(); }
inline tamsayi karakter_kodu(metin m) { return m.empty() ? 0 : (unsigned char)m[0]; }
inline metin karakter_al(const metin& m, tamsayi poz) { return (poz >= 0 && poz < m.length()) ? metin(1, m[poz]) : ""; }
inline metin karakter(tamsayi kod) { return metin(1, (char)kod); }
inline tamsayi yazdir(tamsayi n) { std::cout << n << std::endl; return 0; }

inline tamsayi metin_esitmi(const metin& a, const metin& b) {
    return (a == b) ? 1 : 0;
}

template<typename T>
inline tamsayi liste_uzunluk(const Liste<T>& liste) {
    return liste.size();
}

template<typename T>
inline tamsayi liste_ekle(Liste<T>& liste, const T& eleman) {
    liste.push_back(eleman);
    return 0;
}

// For empty string literal passed to liste_ekle
template<typename T>
inline tamsayi liste_ekle(Liste<T>& liste, const char* eleman) {
    liste.push_back(T(eleman));
    return 0;
}

template<typename T>
inline T liste_al(const Liste<T>& liste, tamsayi index) {
    if (index >= 0 && index < liste.size()) {
        return liste[index];
    }
    return T(); 
}

extern int _ykn_argc;
extern char** _ykn_argv;

inline tamsayi argc_al() { return _ykn_argc; }
inline metin argv_al(tamsayi i) { if (i < _ykn_argc) return _ykn_argv[i]; return ""; }

inline metin dosyadan_oku(const metin& yol) {
    std::ifstream t(yol);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

inline tamsayi dosyaya_yaz(const metin& yol, const metin& icerik) {
    std::ofstream out(yol);
    out << icerik;
    return 0;
}

#include <filesystem>

inline Liste<metin> dizin_oku(const metin& yol) {
    Liste<metin> dosyalar;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(yol)) {
            dosyalar.push_back(entry.path().string());
        }
    } catch (...) {}
    return dosyalar;
}

inline tamsayi paket_indir(const metin& url, const metin& yol) {
    if (url.empty() || yol.empty()) return -1;
    // Yerel dosya/mock URL destegi (file:// veya yerel dosya yolu)
    if (url.rfind("file://", 0) == 0 || url.find(":") != std::string::npos || url[0] == '/' || url[0] == '.') {
        std::string src_path = (url.rfind("file://", 0) == 0) ? url.substr(7) : url;
        std::ifstream src(src_path, std::ios::binary);
        if (src.is_open()) {
            std::ofstream dst(yol, std::ios::binary);
            if (dst.is_open()) {
                dst << src.rdbuf();
                return 0; // Basarili kopyalandi
            }
        }
    }
    // Gecerli baglanti veya dosya yoksa fail-closed hata dondur
    return -1;
}

// AI functions moved to ykn_ai_core.h
inline tamsayi metin_icerir(const metin& met, const metin& aranan) {
    return (met.find(aranan) != std::string::npos) ? 1 : 0;
}

inline tamsayi metin_basliyor_mu(const metin& met, const metin& aranan) {
    return (met.rfind(aranan, 0) == 0) ? 1 : 0;
}

inline metin metin_degistir(const metin& met, const metin& aranan, const metin& yeni) {
    std::string s = met;
    size_t pos = 0;
    while((pos = s.find(aranan, pos)) != std::string::npos) {
        s.replace(pos, aranan.length(), yeni);
        pos += yeni.length();
    }
    return s;
}

inline metin dosya_dizini(const metin& yol) {
    size_t pos = yol.find_last_of("/\\");
    if(pos == std::string::npos) return "";
    return yol.substr(0, pos);
}

inline metin yol_birlestir(const metin& dizin, const metin& dosya) {
    if(dizin.empty()) return dosya;
    if(dizin.back() == '/' || dizin.back() == '\\') return dizin + dosya;
    return dizin + "/" + dosya;
}

inline Liste<metin> metin_bol(const metin& s, const metin& delimiter) {
    Liste<metin> tokens;
    size_t pos = 0;
    std::string token;
    std::string s_copy = s;
    while ((pos = s_copy.find(delimiter)) != std::string::npos) {
        token = s_copy.substr(0, pos);
        tokens.push_back(token);
        s_copy.erase(0, pos + delimiter.length());
    }
    tokens.push_back(s_copy);
    return tokens;
}

#define liste_olustur_DokumantasyonOgesi(n) Liste<DokumantasyonOgesi>()
#define liste_olustur_Jeton(n) Liste<Jeton>()
#define liste_olustur_Cikti(n) Liste<metin>()

template<typename T>
inline tamsayi liste_guncelle(Liste<T>& liste, tamsayi poz, const T& eleman) {
    if (poz >= 0 && poz < liste.size()) {
        liste[poz] = eleman;
    }
    return 0;
}

template<typename T>
inline tamsayi liste_sondan_cikar(Liste<T>& liste) {
    if (!liste.empty()) liste.pop_back();
    return 0;
}

template<typename T>
inline tamsayi liste_sil(Liste<T>& liste, tamsayi basla) {
    if (basla >= 0 && basla < liste.size()) {
        liste.erase(liste.begin() + basla, liste.end());
    }
    return 0;
}

template<typename T>
inline tamsayi liste_eleman_sil(Liste<T>& liste, tamsayi poz) {
    if (poz >= 0 && poz < liste.size()) {
        liste.erase(liste.begin() + poz);
    }
    return 0;
}

inline tamsayi metni_tamsayiya(const metin& str) {
    if (str.empty()) return 0;
    return std::strtoll(str.c_str(), nullptr, 10);
}

inline tamsayi hata_firlatla(const metin& msg) {
    std::cerr << "HATA: " << msg << std::endl;
    exit(1);
    return 1;
}

inline tamsayi sistem_komutu(const metin& cmd) {
    return system(cmd.c_str());
}

#endif // YKN_STD_H
