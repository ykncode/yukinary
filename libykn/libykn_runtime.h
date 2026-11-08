// =============================================================================
// libykn_runtime.h  -  Yukinary Derleyicisi Ortak Calısma Anı Kütüphanesi
// Bu dosya Yukinary derleyicisi tarafından üretilen tüm programlara eklenir.
// CUDA, OpenCL, ARM ve x86 backend'leri bu başlığı paylaşır.
// =============================================================================
#pragma once

// =============================================================================
// --- HATA YÖNETİMİ MAKROLARI ---
// =============================================================================

// CUDA çağrısı başarısız olduğunda Türkçe hata mesajıyla programı sonlandırır.
#ifdef __CUDACC__
#  include <cuda_runtime.h>
#  define YKN_CUDA_KONTROL(cagri) \
    do { \
        cudaError_t _hata = (cagri); \
        if (_hata != cudaSuccess) { \
            fprintf(stderr, "[YKN HATA] CUDA hatası '%s' satirda %d: %s\n", \
                    __FILE__, __LINE__, cudaGetErrorString(_hata)); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)
#else
#  define YKN_CUDA_KONTROL(cagri) (cagri)
#endif

// OpenCL çağrısı başarısız olduğunda Türkçe hata mesajıyla programı sonlandırır.
#define YKN_OCL_KONTROL(err) \
    do { \
        if ((err) != 0) { \
            fprintf(stderr, "[YKN HATA] OpenCL hatası %d satirda %d (%s)\n", \
                    (int)(err), __LINE__, __FILE__); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)


#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

// =============================================================================
// --- VRAM MEMORY ARENA (Bellek Havuzu) ---
// Faz 5: LLM inference için bellek parçalanmasını önler.
// Program başında dev bir bellek bloğu tahsis edilir, ardından O(1) ile dağıtılır.
// =============================================================================

struct YknArena {
    uint8_t* base_ptr;
    size_t total_size;
    size_t current_offset;
};

// Global arena nesnesi (backend'ler tarafından başlatılır)
extern YknArena g_gpu_arena;

// Arena fonksiyonları (Platforma özgü olarak cuda_runtime.cu vs içinde gerçeklenir)
void ykn_arena_init(size_t size_in_bytes);
void* ykn_arena_alloc(size_t size_in_bytes);
void ykn_arena_reset();
void ykn_arena_free();

// =============================================================================
// --- TEMEL VERİ YAPILARI (Matris, Vektör, Bit1Matris, TernaryMatris) ---
// =============================================================================

template <typename T = float>
struct Matris {
    std::shared_ptr<T> data;
    int genislik;
    int yukseklik;
};

template <typename T = float>
struct Vektor {
    std::shared_ptr<T> data;
    int uzunluk;
};

// 1-Bit sinir ağı ağırlıkları (BitNet tarzı, 32-bit kelime başına 32 ağırlık)
struct Bit1Matris {
    std::shared_ptr<uint32_t> data;  // Bit-packed agirliklar
    int genislik;
    int yukseklik;
    int kelime_sayisi; // genislik / 32
};

// Ternary (-1, 0, +1) sinir ağı ağırlıkları
struct TernaryMatris {
    std::shared_ptr<uint64_t> data;
    int genislik;
    int yukseklik;
};

// =============================================================================
// --- METIN (STRING) İŞLEMLERİ ---
// =============================================================================

inline int metin_uzunlugu(const std::string& m) {
    return static_cast<int>(m.length());
}

inline std::string karakter_al(const std::string& m, int indeks) {
    if (indeks >= 0 && indeks < m.length()) {
        return std::string(1, m[indeks]);
    }
    return "";
}

inline std::string alt_metin(const std::string& m, int basla, int bitir) {
    if (basla < 0) basla = 0;
    if (bitir > m.length()) bitir = m.length();
    if (basla >= bitir) return "";
    return m.substr(basla, bitir - basla);
}

inline int metin_esitmi(const std::string& m1, const std::string& m2) {
    return m1 == m2 ? 1 : 0;
}

// =============================================================================
// --- DOSYA G/Ç (File I/O) ---
// =============================================================================

inline std::string dosya_oku(const std::string& yol) {
    std::ifstream t(yol);
    if (!t.is_open()) return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

inline void dosya_yaz(const std::string& yol, const std::string& veri) {
    std::ofstream t(yol);
    t << veri;
}

inline void dosya_sil(const std::string& yol) {
    std::remove(yol.c_str());
}

// =============================================================================
// --- TCP / HTTP SOKET KATMANI ---
// =============================================================================

struct Istek {
    std::string govde;
    int client_socket;

    void yanit_ver(int kod, const std::string& mesaj) {
        std::string response = "HTTP/1.1 " + std::to_string(kod) + " OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(mesaj.length()) + "\r\n\r\n";
        response += mesaj;
#ifdef _WIN32
        send(client_socket, response.c_str(), (int)response.length(), 0);
        closesocket(client_socket);
#else
        send(client_socket, response.c_str(), response.length(), 0);
        close(client_socket);
#endif
    }
};

struct Sunucu {
    int port;
    int server_socket;

    bool aktif_mi() { return true; }

    Istek istek_al() {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int client_len = sizeof(client_addr);
        int client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
#else
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
#endif
        char buffer[4096] = {0};
        recv(client_sock, buffer, 4096, 0);
        return Istek{std::string(buffer), client_sock};
    }
};

inline Sunucu soket_dinle(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 8);
    std::cout << "[Soket] " << port << " portu HTTP TCP icin dinleniyor..." << std::endl;
    return Sunucu{port, server_fd};
}


// =============================================================================
// --- SELF-HOSTING STD KATMANI (Derleyici + Genel Amac icin) ---
// Asagidaki fonksiyonlar src/lexer.ykn, src/parser.ykn ve src/main.ykn
// tarafindan kullanilir. Platforma ozgu override gerektirmez.
// =============================================================================

// --- Genisletilmis Metin (String) Islevleri ---

// Tek karakterin Unicode/ASCII kod noktasini dondurur.
// karakter_kodu("A") -> 65
inline int karakter_kodu(const std::string& m) {
    if (m.empty()) return 0;
    return static_cast<unsigned char>(m[0]);
}

// Kod noktasindan tek karakterli string uretir.
// karakter(65) -> "A"
inline std::string karakter(int kod) {
    if (kod <= 0 || kod > 0x10FFFF) return "";
    return std::string(1, static_cast<char>(kod));
}

// Iki metni birlestir.
inline std::string metin_birlestir(const std::string& a, const std::string& b) {
    return a + b;
}

// Dosyaya metin yazar. Basarili olursa 1, olmazsa 0 dondurur.
inline int dosyaya_yaz(const std::string& dosya_adi, const std::string& icerik) {
    std::ofstream out(dosya_adi);
    if (out.is_open()) {
        out << icerik;
        out.close();
        return 1;
    }
    return 0;
}

// Dosyadan metin okur.
inline std::string dosyadan_oku(const std::string& dosya_adi) {
    std::ifstream in(dosya_adi);
    if (!in.is_open()) return "";
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}


// NOT: Liste template'leri 332. satirdan itibaren tanimlidir.
//      (Tekrarlayan tanimlari onlemek icin bu blok kaldirildi.)



// Bir metinde diger metni ara; ilk bulundugu konumu don (-1 = yok).
inline int metin_bul(const std::string& kaynak, const std::string& aranan) {
    auto pos = kaynak.find(aranan);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
}

// --- Liste (YknListe) Islevleri ---
// Yukinary Liste<T> tipleri YknListe<T> olarak uretilir.

template <typename T>
struct YknListe {
    T* data;
    int boyut;
    int kapasite;
};

template<typename T>
inline YknListe<T> liste_olustur(int kapasite);

template<typename T>
inline void liste_ekle(YknListe<T>& liste, const T& eleman);

// Metnin icerisinde aranan metin var mi? (1/0)
inline int metin_icerir(const std::string& kaynak, const std::string& aranan) {
    return kaynak.find(aranan) != std::string::npos ? 1 : 0;
}

// Bir metni belirtilen ayraca gore boler ve liste dondurur.
inline YknListe<std::string> metin_bol(const std::string& metin, const std::string& ayirici) {
    YknListe<std::string> sonuclar = liste_olustur<std::string>(4);
    size_t baslangic = 0;
    size_t bitis = metin.find(ayirici);

    while (bitis != std::string::npos) {
        liste_ekle(sonuclar, metin.substr(baslangic, bitis - baslangic));
        baslangic = bitis + ayirici.length();
        bitis = metin.find(ayirici, baslangic);
    }
    liste_ekle(sonuclar, metin.substr(baslangic));

    return sonuclar;
}

template<typename T>
inline YknListe<T> liste_olustur(int kapasite) {
    if (kapasite <= 0) kapasite = 4; // default kapasite
    YknListe<T> l;
    l.boyut = 0;
    l.kapasite = kapasite;
    // Arena yoksa veya primitive tipse normal tahsis edebiliriz ama
    // Görev gereği ykn_arena_alloc veya new T[] kullanıyoruz.
    l.data = new T[kapasite]();
    return l;
}

template<typename T>
inline void liste_ekle(YknListe<T>& liste, const T& eleman) {
    if (liste.boyut >= liste.kapasite) {
        int yeni_kapasite = liste.kapasite == 0 ? 4 : liste.kapasite * 2;
        T* yeni_data = new T[yeni_kapasite]();
        for (int i = 0; i < liste.boyut; ++i) {
            yeni_data[i] = liste.data[i];
        }
        if (liste.data) {
            delete[] liste.data;
        }
        liste.data = yeni_data;
        liste.kapasite = yeni_kapasite;
    }
    liste.data[liste.boyut++] = eleman;
}

template<typename T>
inline int liste_uzunluk(const YknListe<T>& liste) {
    return liste.boyut;
}

template<typename T>
inline T liste_al(const YknListe<T>& liste, int indeks) {
    if (indeks < 0 || indeks >= liste.boyut) {
        hata_firlatla("Liste indeksi disinda");
    }
    return liste.data[indeks];
}

template<typename T>
inline void liste_sil(YknListe<T>& liste, int indeks) {
    if (indeks >= 0 && indeks < liste.boyut) {
        for (int i = indeks; i < liste.boyut - 1; ++i) {
            liste.data[i] = liste.data[i + 1];
        }
        liste.boyut--;
    } else {
        hata_firlatla("Liste indeksi disinda");
    }
}

template<typename T>
inline void liste_guncelle(YknListe<T>& liste, int indeks, const T& eleman) {
    if (indeks < 0 || indeks >= liste.boyut) {
        hata_firlatla("Liste indeksi disinda");
    }
    liste.data[indeks] = eleman;
}

template<typename T>
inline void liste_temizle(YknListe<T>& liste) {
    liste.boyut = 0;
}

template<typename T>
inline T liste_sondan_cikar(YknListe<T>& liste) {
    if (liste.boyut <= 0) hata_firlatla("Bos listeden eleman cikarilamaz");
    T sonEleman = liste.data[liste.boyut - 1];
    liste.boyut--;
    return sonEleman;
}

// --- Sozluk (YknSozluk) Islevleri ---

template <typename K, typename V>
using YknSozluk = std::unordered_map<K, V>;

template<typename K, typename V>
inline YknSozluk<K, V> sozluk_olustur() {
    return YknSozluk<K, V>();
}

template<typename K, typename V>
inline void sozluk_ekle(YknSozluk<K,V>& sozluk, const K& anahtar, const V& deger) {
    sozluk[anahtar] = deger;
}

template<typename K, typename V>
inline V sozluk_al(const YknSozluk<K,V>& sozluk, const K& anahtar) {
    auto it = sozluk.find(anahtar);
    if (it == sozluk.end()) {
        hata_firlatla("Anahtar bulunamadi");
    }
    return it->second;
}

template<typename K, typename V>
inline int sozluk_var_mi(const YknSozluk<K,V>& sozluk, const K& anahtar) {
    return sozluk.count(anahtar) ? 1 : 0;
}

template<typename K, typename V>
inline void sozluk_sil(YknSozluk<K,V>& sozluk, const K& anahtar) {
    auto it = sozluk.find(anahtar);
    if (it != sozluk.end()) {
        sozluk.erase(it);
    } else {
        hata_firlatla("Anahtar bulunamadi");
    }
}

// --- Sistem / CLI Islevleri ---
// Not: __argc / __argv global main() uzerinden beslenir.
// Derleyici driver main icine enjekte edilen ykn_argv_init() gerekli.

static int   _ykn_argc = 0;
static char** _ykn_argv = nullptr;

inline void ykn_argv_init(int argc, char** argv) {
    _ykn_argc = argc;
    _ykn_argv = argv;
}

inline std::string argv_al(int indeks) {
    if (indeks < 0 || indeks >= _ykn_argc) return "";
    return std::string(_ykn_argv[indeks]);
}

inline int argc_al() {
    return _ykn_argc;
}

inline void cikis(int kod) {
    exit(kod);
}

inline int sistem_komutu(const std::string& komut) {
    return system(komut.c_str());
}

// --- Tip Donusumleri ---

inline std::string tamsayi_metne(int sayi) {
    return std::to_string(sayi);
}

inline std::string gercek_metne(float sayi) {
    return std::to_string(sayi);
}

// metni_tamsayiya -- bkz. dosyadan_oku blogu uzerinde

inline float metni_gercege(const std::string& m) {
    try { return std::stof(m); } catch(...) { return 0.0f; }
}

// --- Hata / Panic ---

[[noreturn]] inline void hata_firlatla(const std::string& mesaj) {
    fprintf(stderr, "[YKN HATA] %s\n", mesaj.c_str());
    exit(EXIT_FAILURE);
}

// =============================================================================
// --- CPU Tarafı Fallback (Hızlı Test & ARM / x86 Uyumu İçin) ---
// CUDA/OpenCL derlenirken bu kısımlar ilgili backend .cu/.cpp tarafından
// override edilir (operator overloading aracılığıyla).
// =============================================================================

#ifndef YKN_CUDA_BACKEND
#ifndef YKN_OPENCL_BACKEND
// create_matris_cpu: CPU/ARM fallback. CUDA ve OpenCL hostunda override edilir.

template <typename T = float>
inline Matris<T> create_matris_cpu(int w, int h) {
    Matris<T> m; m.genislik = w; m.yukseklik = h;
    m.data = std::shared_ptr<T>(new T[w * h](), [](T* p){ delete[] p; });
    return m;
}

template <typename T = float>
inline Vektor<T> create_vektor_cpu(int len) {
    Vektor<T> v; v.uzunluk = len;
    v.data = std::shared_ptr<T>(new T[len](), [](T* p){ delete[] p; });
    return v;
}

#ifndef __CUDACC__
template <typename T>
inline Matris<T> operator*(const Matris<T>& A, const Matris<T>& B) {
    Matris<T> C = create_matris_cpu<T>(B.genislik, A.yukseklik);
    for (int i = 0; i < A.yukseklik; ++i)
        for (int j = 0; j < B.genislik; ++j) {
            T sum = 0;
            for (int k = 0; k < A.genislik; ++k)
                sum += A.data.get()[i * A.genislik + k] * B.data.get()[k * B.genislik + j];
            C.data.get()[i * C.genislik + j] = sum;
        }
    return C;
}

template <typename T>
inline Matris<T> operator+(const Matris<T>& A, const Matris<T>& B) {
    Matris<T> C = create_matris_cpu<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    for (int i = 0; i < total; ++i)
        C.data.get()[i] = A.data.get()[i] + B.data.get()[i];
    return C;
}
#endif // __CUDACC__
#endif // YKN_OPENCL_BACKEND
#endif // YKN_CUDA_BACKEND
#include <filesystem>
#include <vector>

inline YknListe<std::string> dizin_oku(const std::string& yol) {
    YknListe<std::string> sonuclar = liste_olustur<std::string>(10);
    try {
        for (const auto& entry : std::filesystem::directory_iterator(yol)) {
            if (entry.is_regular_file()) {
                liste_ekle(sonuclar, entry.path().string());
            }
        }
    } catch (...) {
        // Hata durumunda bos liste don
    }
    return sonuclar;
}
#include <cstdlib>
#include <algorithm>

// --- PAKET YONETICISI (GUVENLI) ---
inline int paket_indir(const std::string& url, const std::string& hedef_dosya) {
    // Guvenlik (Command Injection Korumasi): URL icinde zararli karakterleri filtrele
    if (url.find(';') != std::string::npos || url.find('&') != std::string::npos || 
        url.find('|') != std::string::npos || url.find('`') != std::string::npos || 
        url.find('$') != std::string::npos) {
        return 0; // Guvenlik ihlali
    }
    std::string cmd = "curl -sL \"" + url + "\" -o \"" + hedef_dosya + "\"";
    int sonuc = std::system(cmd.c_str());
    return sonuc == 0 ? 1 : 0;
}

// --- YUKI AI: ZERO-COPY MEMORY MAPPING ---
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

// A simple structure to hold mapping info
struct MmapInfo {
    void* addr;
    size_t length;
#ifdef _WIN32
    HANDLE hFile;
    HANDLE hMapping;
#else
    int fd;
#endif
};

// Store active mappings using the pointer address as key (cast to long long)
#include <unordered_map>
std::unordered_map<long long, MmapInfo> g_mappings;

inline long long mmap_dosya_ac(const std::string& dosya_adi) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(dosya_adi.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    size_t length = fileSize.QuadPart;
    
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapping == NULL) {
        CloseHandle(hFile);
        return 0;
    }
    
    void* addr = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (addr == NULL) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return 0;
    }
    
    long long key = (long long)addr;
    MmapInfo info;
    info.addr = addr;
    info.length = length;
    info.hFile = hFile;
    info.hMapping = hMapping;
    g_mappings[key] = info;
    
    return key;
#else
    int fd = open(dosya_adi.c_str(), O_RDONLY);
    if (fd == -1) return 0;
    
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return 0;
    }
    
    void* addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return 0;
    }
    
    long long key = (long long)addr;
    MmapInfo info;
    info.addr = addr;
    info.length = sb.st_size;
    info.fd = fd;
    g_mappings[key] = info;
    
    return key;
#endif
}

inline int mmap_kapat(long long isaretci) {
    if (g_mappings.find(isaretci) == g_mappings.end()) return 0;
    MmapInfo info = g_mappings[isaretci];
#ifdef _WIN32
    UnmapViewOfFile(info.addr);
    CloseHandle(info.hMapping);
    CloseHandle(info.hFile);
#else
    munmap(info.addr, info.length);
    close(info.fd);
#endif
    g_mappings.erase(isaretci);
    return 1;
}

// Memory reader functions
inline long long isaretciden_uzun_tamsayi_oku(long long isaretci, long long offset) {
    long long* ptr = (long long*)((char*)isaretci + offset);
    return *ptr;
}

inline std::string isaretciden_metin_oku(long long isaretci, long long offset, long long length) {
    char* ptr = (char*)isaretci + offset;
    return std::string(ptr, length);
}

// --- YKN WINDOWING (GLFW STUBS) ---
// Note: Real implementation would link against glfw3.lib or libglfw3.a
inline int ykn_glfw_init() {
    return 1;
}
inline int ykn_glfw_create_window(int w, int h, const std::string& title) {
    return 1; // Return window handle ID
}
inline int ykn_glfw_poll_events() {
    return 1;
}
inline int ykn_glfw_window_should_close(int id) {
    return 0; // Fake: don't close immediately
}
inline int ykn_glfw_terminate() {
    return 1;
}

// --- YKN OS (WAYLAND & DAEMON STUBS) ---
inline std::string sistem_metrik_getir(const std::string& type) {
    if (type == "meminfo") return "256MB / 16384MB (YknOS Minimal Allocation)";
    if (type == "cpu_temp") return "42.5 C";
    return "UNKNOWN";
}
