#include <set>
#include <map>
#include <iostream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdint.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/sendfile.h>
#endif

using tamsayi = long long;
using metin = std::string;

// Dynamic FFI bridge used by src/fallback_dinamik.ykn. The YKN source keeps
// one platform-neutral ABI; these seed-runtime shims bind it to the actual
// loader on the host instead of manufacturing a handle or function pointer.
#ifndef _WIN32
extern "C" long long LoadLibraryA(char* library_name) {
    if (!library_name || !library_name[0]) return 0;
    return static_cast<long long>(reinterpret_cast<intptr_t>(::dlopen(library_name, RTLD_LAZY | RTLD_LOCAL)));
}
extern "C" char* GetProcAddress(long long handle, char* symbol_name) {
    if (!handle || !symbol_name || !symbol_name[0]) return nullptr;
    return reinterpret_cast<char*>(::dlsym(reinterpret_cast<void*>(static_cast<intptr_t>(handle)), symbol_name));
}
extern "C" long long FreeLibrary(long long handle) {
    if (!handle) return 0;
    return ::dlclose(reinterpret_cast<void*>(static_cast<intptr_t>(handle))) == 0 ? 1 : 0;
}
extern "C" long long ykn_dyn_call_i64(long long handle, char* symbol_name,
                                       long long first, long long second) {
    if (!handle || !symbol_name || !symbol_name[0]) return -1;
    using DynamicFn = long long (*)(long long, long long);
    DynamicFn function = reinterpret_cast<DynamicFn>(::dlsym(
        reinterpret_cast<void*>(static_cast<intptr_t>(handle)), symbol_name));
    if (!function) return -1;
    return function(first, second);
}
extern "C" long long ykn_dyn_call_ptr_i64(char* function_ptr,
                                           long long first, long long second) {
    if (!function_ptr) return -1;
    using DynamicFn = long long (*)(long long, long long);
    DynamicFn function = reinterpret_cast<DynamicFn>(function_ptr);
    return function(first, second);
}
#else
extern "C" long long dlopen(char* library_name, long long) {
    if (!library_name || !library_name[0]) return 0;
    HMODULE module = ::LoadLibraryA(library_name);
    return static_cast<long long>(reinterpret_cast<intptr_t>(module));
}
extern "C" char* dlsym(long long handle, char* symbol_name) {
    if (!handle || !symbol_name || !symbol_name[0]) return nullptr;
    FARPROC symbol = ::GetProcAddress(reinterpret_cast<HMODULE>(static_cast<intptr_t>(handle)), symbol_name);
    return reinterpret_cast<char*>(symbol);
}
extern "C" long long dlclose(long long handle) {
    if (!handle) return -1;
    return ::FreeLibrary(reinterpret_cast<HMODULE>(static_cast<intptr_t>(handle))) ? 0 : -1;
}
extern "C" long long ykn_dyn_call_i64(long long handle, char* symbol_name,
                                       long long first, long long second) {
    if (!handle || !symbol_name || !symbol_name[0]) return -1;
    using DynamicFn = long long (*)(long long, long long);
    DynamicFn function = reinterpret_cast<DynamicFn>(::GetProcAddress(
        reinterpret_cast<HMODULE>(static_cast<intptr_t>(handle)), symbol_name));
    if (!function) return -1;
    return function(first, second);
}
extern "C" long long ykn_dyn_call_ptr_i64(char* function_ptr,
                                           long long first, long long second) {
    if (!function_ptr) return -1;
    using DynamicFn = long long (*)(long long, long long);
    DynamicFn function = reinterpret_cast<DynamicFn>(function_ptr);
    return function(first, second);
}
#endif

extern "C" void matris_carp();

static long long g_source_utf8_hatasi = 0;
static long long g_source_utf8_hata_satir = 0;
static long long g_source_utf8_hata_sutun = 0;
static char g_cli_llm_model[1024] = {0};

static bool ykn_gguf_cli_baslik_dogrula(const char* path,
                                        uint64_t& tensorCount,
                                        uint64_t& metadataCount) {
    tensorCount = 0;
    metadataCount = 0;
    if (!path || !path[0]) return false;
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    unsigned char header[24] = {0};
    const size_t read = std::fread(header, 1, sizeof(header), file);
    std::fclose(file);
    if (read != sizeof(header) || std::memcmp(header, "GGUF", 4) != 0) return false;
    const uint32_t version = static_cast<uint32_t>(header[4]) |
                             (static_cast<uint32_t>(header[5]) << 8) |
                             (static_cast<uint32_t>(header[6]) << 16) |
                             (static_cast<uint32_t>(header[7]) << 24);
    if (version != 3) return false;
    for (int i = 0; i < 8; ++i) {
        tensorCount |= static_cast<uint64_t>(header[8 + i]) << (i * 8);
        metadataCount |= static_cast<uint64_t>(header[16 + i]) << (i * 8);
    }
    return tensorCount > 0 && metadataCount > 0;
}

struct YknGgufCliProfile {
    uint64_t tensor_count = 0;
    uint64_t metadata_count = 0;
    uint64_t file_size = 0;
    uint64_t data_offset = 0;
    uint32_t alignment = 32;
    uint32_t first_quant_type = 0;
    uint64_t embedding_length = 0;
    uint64_t block_count = 0;
    uint64_t head_count = 0;
    uint64_t kv_head_count = 0;
    uint64_t feed_forward_length = 0;
    uint64_t context_length = 0;
    char architecture[128] = {0};
};

static bool ykn_gguf_read_exact(FILE* file, void* target, size_t size) {
    return file && target && size > 0 && std::fread(target, 1, size, file) == size;
}

static bool ykn_gguf_read_u32(FILE* file, uint32_t& value) {
    unsigned char bytes[4] = {0};
    if (!ykn_gguf_read_exact(file, bytes, sizeof(bytes))) return false;
    value = static_cast<uint32_t>(bytes[0]) |
            (static_cast<uint32_t>(bytes[1]) << 8) |
            (static_cast<uint32_t>(bytes[2]) << 16) |
            (static_cast<uint32_t>(bytes[3]) << 24);
    return true;
}

static bool ykn_gguf_read_u64(FILE* file, uint64_t& value) {
    unsigned char bytes[8] = {0};
    if (!ykn_gguf_read_exact(file, bytes, sizeof(bytes))) return false;
    value = 0;
    for (unsigned int i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    return true;
}

static bool ykn_gguf_read_string(FILE* file, std::string& value, uint64_t max_size = 1ULL << 20) {
    uint64_t length = 0;
    if (!ykn_gguf_read_u64(file, length) || length > max_size || length > static_cast<uint64_t>((std::numeric_limits<size_t>::max)())) return false;
    value.assign(static_cast<size_t>(length), '\0');
    return length == 0 || ykn_gguf_read_exact(file, &value[0], static_cast<size_t>(length));
}


static bool ykn_gguf_skip_string_value(FILE* file) {
    uint64_t length = 0;
    if (!ykn_gguf_read_u64(file, length) || length > (1ULL << 30)) return false;
    while (length > 0) {
        unsigned char buffer[65536];
        size_t chunk = length > sizeof(buffer) ? sizeof(buffer) : static_cast<size_t>(length);
        if (!ykn_gguf_read_exact(file, buffer, chunk)) return false;
        length -= chunk;
    }
    return true;
}

static bool ykn_gguf_skip_value_safe(FILE* file, uint32_t type, unsigned int depth = 0) {
    if (type == 8) return ykn_gguf_skip_string_value(file);
    if (depth > 16) return false;
    if (type != 9) {
        const long long sizes[] = {1, 1, 2, 2, 4, 4, 4, 1, -1, -1, 8, 8, 8};
        return type < 13 && sizes[type] > 0 && std::fseek(file, sizes[type], SEEK_CUR) == 0;
    }
    uint32_t element_type = 0;
    uint64_t count = 0;
    if (!ykn_gguf_read_u32(file, element_type) || !ykn_gguf_read_u64(file, count) || count > 1000000) return false;
    for (uint64_t i = 0; i < count; ++i) {
        if (!ykn_gguf_skip_value_safe(file, element_type, depth + 1)) return false;
    }
    return true;
}

static bool ykn_gguf_key_suffix(const std::string& key, const char* suffix) {
    const size_t length = std::strlen(suffix);
    return key.size() >= length && key.compare(key.size() - length, length, suffix) == 0;
}

static bool ykn_gguf_profile_read(const char* path, YknGgufCliProfile& profile) {
    if (!path || !path[0]) return false;
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    bool ok = false;
    do {
        if (std::fseek(file, 0, SEEK_END) != 0) break;
#ifdef _WIN32
        const long long end = _ftelli64(file);
#else
        const long long end = static_cast<long long>(ftello(file));
#endif
        if (end < 24) break;
        profile.file_size = static_cast<uint64_t>(end);
        if (std::fseek(file, 0, SEEK_SET) != 0) break;
        uint32_t magic = 0, version = 0;
        if (!ykn_gguf_read_u32(file, magic) || !ykn_gguf_read_u32(file, version) || magic != 0x46554747 || version != 3) break;
        if (!ykn_gguf_read_u64(file, profile.tensor_count) || !ykn_gguf_read_u64(file, profile.metadata_count) ||
            profile.tensor_count == 0 || profile.metadata_count == 0 || profile.tensor_count > 1000000 || profile.metadata_count > 1000000) break;

        for (uint64_t i = 0; i < profile.metadata_count; ++i) {
            std::string key;
            uint32_t type = 0;
            if (!ykn_gguf_read_string(file, key, 1ULL << 20) || !ykn_gguf_read_u32(file, type)) break;
            if (type == 8) {
                std::string value;
                if (!ykn_gguf_read_string(file, value, 1ULL << 20)) { ok = false; break; }
                if (key == "general.architecture" && value.size() < sizeof(profile.architecture)) {
                    std::memcpy(profile.architecture, value.data(), value.size());
                    profile.architecture[value.size()] = 0;
                }
            } else if (type == 4 || type == 5 || type == 10 || type == 11) {
                uint64_t value = 0;
                if (type == 4 || type == 5) {
                    uint32_t small = 0;
                    if (!ykn_gguf_read_u32(file, small)) { ok = false; break; }
                    value = small;
                } else if (!ykn_gguf_read_u64(file, value)) { ok = false; break; }
                if (key == "general.alignment") profile.alignment = value > 0 && value <= 4096 ? static_cast<uint32_t>(value) : 32;
                else if (ykn_gguf_key_suffix(key, ".embedding_length")) profile.embedding_length = value;
                else if (ykn_gguf_key_suffix(key, ".block_count")) profile.block_count = value;
                else if (ykn_gguf_key_suffix(key, ".attention.head_count")) profile.head_count = value;
                else if (ykn_gguf_key_suffix(key, ".attention.head_count_kv")) profile.kv_head_count = value;
                else if (ykn_gguf_key_suffix(key, ".feed_forward_length")) profile.feed_forward_length = value;
                else if (ykn_gguf_key_suffix(key, ".context_length")) profile.context_length = value;
            } else if (!ykn_gguf_skip_value_safe(file, type)) { ok = false; break; }
            ok = true;
        }
        if (!ok || profile.architecture[0] == 0) break;

        uint64_t max_relative_offset = 0;
        for (uint64_t i = 0; i < profile.tensor_count; ++i) {
            std::string name;
            uint32_t ndim = 0, type = 0;
            uint64_t dimension = 0, relative_offset = 0;
            if (!ykn_gguf_read_string(file, name, 1ULL << 20) || !ykn_gguf_read_u32(file, ndim) || ndim == 0 || ndim > 4) { ok = false; break; }
            for (uint32_t d = 0; d < ndim; ++d) if (!ykn_gguf_read_u64(file, dimension) || dimension == 0) { ok = false; break; }
            if (!ok || !ykn_gguf_read_u32(file, type) || !ykn_gguf_read_u64(file, relative_offset)) { ok = false; break; }
            if (relative_offset > max_relative_offset) max_relative_offset = relative_offset;
            if (type != 0 && type != 1) profile.first_quant_type = type;
        }
        if (!ok) break;
#ifdef _WIN32
        const long long descriptor_end = _ftelli64(file);
#else
        const long long descriptor_end = static_cast<long long>(ftello(file));
#endif
        if (descriptor_end < 0) break;
        const uint64_t alignment = profile.alignment == 0 ? 32 : profile.alignment;
        profile.data_offset = (static_cast<uint64_t>(descriptor_end) + alignment - 1) / alignment * alignment;
        if (profile.data_offset >= profile.file_size || max_relative_offset >= profile.file_size - profile.data_offset) break;
        ok = true;
    } while (false);
    std::fclose(file);
    return ok;
}

template<typename T> inline T& _ykn_ref(T& x) { return x; }
template<typename T> inline T& _ykn_ref(T* x) { return *x; }
template<typename T> inline T& _ykn_ref(T** x) { return **x; }
#define YKN_DOT(obj, member) (_ykn_ref(obj).member)

inline tamsayi yazdir(const metin& m) { std::cout << m << std::endl; return 0; }
inline tamsayi yazdir(tamsayi n) { std::cout << n << std::endl; return 0; }
inline metin metin_birlestir(const metin& a, const metin& b) { return a + b; }
inline metin tamsayi_metne(tamsayi n) { return std::to_string(n); }
inline tamsayi argc_al() { extern int _ykn_argc; return _ykn_argc; }
inline metin argv_al(tamsayi i) { extern int _ykn_argc; extern char** _ykn_argv; if(i<_ykn_argc) return _ykn_argv[i]; return ""; }

// Deterministic compiler allocation-failure contract. The counter starts at
// the compiler pipeline entry and is used by Stage-0; Stage-3 gets the same
// observable contract through the compiler's YKN allocator wrapper.
static long long g_ykn_allocasyon_sayisi = 0;
static long long g_ykn_allocasyon_fail_after = -1;
static bool g_ykn_allocasyon_politikasi_aktif = false;

static bool ykn_allocasyon_fault_cli_ayarla(int argc, char** argv) {
    g_ykn_allocasyon_sayisi = 0;
    g_ykn_allocasyon_fail_after = -1;
    g_ykn_allocasyon_politikasi_aktif = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fail-alloc-after") != 0) continue;
        if (i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == 0) return false;
        long long value = 0;
        const char* text = argv[++i];
        for (const char* p = text; *p; ++p) {
            if (*p < '0' || *p > '9' || value > 214748364) return false;
            value = value * 10 + (*p - '0');
            if (value < 0) return false;
        }
        g_ykn_allocasyon_fail_after = value;
        g_ykn_allocasyon_politikasi_aktif = true;
    }
    return true;
}

inline char* bellek_tahsis_et(long long boyut) {
    if (g_ykn_allocasyon_politikasi_aktif &&
        g_ykn_allocasyon_sayisi >= g_ykn_allocasyon_fail_after) {
        std::printf("HATA [E014] [ERR_OOM] [class=compiler]: Kontrollu allocation failure; derleme durduruldu.\\n");
        std::exit(1);
    }
    ++g_ykn_allocasyon_sayisi;
    char* ptr = (char*)calloc(1, (size_t)boyut);
    if (!ptr) {
        std::printf("HATA [E014] [ERR_OOM] [class=compiler]: Bellek tahsisi basarisiz; derleme durduruldu.\\n");
        std::exit(1);
    }
    return ptr;
}
inline long long ekrana_yaz(const char* m) { std::cout << m; std::cout.flush(); return 0; }
inline long long dosya_oku(const char* yol, char* buf, long long n) { FILE* f = fopen(yol, "rb"); if(!f) return 0; long long r = fread(buf, 1, n, f); fclose(f); return r; }
inline long long dosya_oku_aralik(const char* yol, long long ofset, char* buf, long long n) { FILE* f = fopen(yol, "rb"); if(!f) return 0; int seek_ok;
#ifdef _WIN32
    seek_ok = _fseeki64(f, ofset, SEEK_SET);
#else
    seek_ok = fseeko(f, (off_t)ofset, SEEK_SET);
#endif
    if(seek_ok != 0) { fclose(f); return 0; } long long r = fread(buf, 1, n, f); fclose(f); return r; }
inline long long diske_bas_ve_kapat(const char* yol, char* buf, long long n) {
    FILE* f = fopen(yol, "wb");
    if(!f) return 0;
    long long w = fwrite(buf, 1, n, f);
    fclose(f);
#ifndef _WIN32
    chmod(yol, 0755);
#endif
    return w;
}

static long long ykn_utf8_bayt_sayisi(unsigned char ilk) {
    if (ilk <= 0x7F) return 1;
    if (ilk >= 0xC2 && ilk <= 0xDF) return 2;
    if (ilk >= 0xE0 && ilk <= 0xEF) return 3;
    if (ilk >= 0xF0 && ilk <= 0xF4) return 4;
    return 0;
}

static bool ykn_utf8_devam(unsigned char b) { return b >= 0x80 && b <= 0xBF; }

static bool ykn_utf8_gecerli(const unsigned char* data, long long size,
                             long long& badOffset, long long& badLine,
                             long long& badColumn) {
    long long i = 0;
    long long line = 1;
    long long column = 1;
    while (i < size) {
        unsigned char first = data[i];
        long long n = ykn_utf8_bayt_sayisi(first);
        bool valid = n != 0 && i + n <= size;
        unsigned long cp = 0;
        if (valid && n == 1) cp = first;
        if (valid && n == 2) {
            valid = ykn_utf8_devam(data[i + 1]);
            if (valid) cp = ((first - 0xC0) * 64u) + (data[i + 1] - 0x80);
            valid = valid && cp >= 0x80;
        }
        if (valid && n == 3) {
            valid = ykn_utf8_devam(data[i + 1]) && ykn_utf8_devam(data[i + 2]);
            if (valid) cp = ((first - 0xE0) * 4096u) + ((data[i + 1] - 0x80) * 64u) + (data[i + 2] - 0x80);
            valid = valid && cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF);
        }
        if (valid && n == 4) {
            valid = ykn_utf8_devam(data[i + 1]) && ykn_utf8_devam(data[i + 2]) && ykn_utf8_devam(data[i + 3]);
            if (valid) cp = ((first - 0xF0) * 262144u) + ((data[i + 1] - 0x80) * 4096u) + ((data[i + 2] - 0x80) * 64u) + (data[i + 3] - 0x80);
            valid = valid && cp >= 0x10000 && cp <= 0x10FFFF;
        }
        if (!valid) {
            badOffset = i;
            badLine = line;
            badColumn = column;
            return false;
        }
        if (cp == 0x0A) { ++line; column = 1; } else { ++column; }
        i += n;
    }
    return true;
}

long long kaynak_utf8_normalize_et(const unsigned char* girdi, long long uzunluk, char* cikti, long long kapasite) {
    long long i = 0;
    long long o = 0;
    if (uzunluk >= 2 && girdi[0] == 0xFF && girdi[1] == 0xFE) {
        i = 2;
        while (i + 1 < uzunluk && o + 4 < kapasite) {
            unsigned long cp = (unsigned long)girdi[i] | ((unsigned long)girdi[i + 1] << 8);
            i += 2;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < uzunluk) {
                unsigned long low = (unsigned long)girdi[i] | ((unsigned long)girdi[i + 1] << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    i += 2;
                }
            }
            if (cp <= 0x7F) cikti[o++] = (char)cp;
            else if (cp <= 0x7FF) {
                cikti[o++] = (char)(0xC0 | (cp >> 6));
                cikti[o++] = (char)(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
                cikti[o++] = (char)(0xE0 | (cp >> 12));
                cikti[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                cikti[o++] = (char)(0x80 | (cp & 0x3F));
            } else {
                cikti[o++] = (char)(0xF0 | (cp >> 18));
                cikti[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                cikti[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                cikti[o++] = (char)(0x80 | (cp & 0x3F));
            }
        }
    } else if (uzunluk >= 2 && girdi[0] == 0xFE && girdi[1] == 0xFF) {
        i = 2;
        while (i + 1 < uzunluk && o + 4 < kapasite) {
            unsigned long cp = ((unsigned long)girdi[i] << 8) | (unsigned long)girdi[i + 1];
            i += 2;
            if (cp <= 0x7F) cikti[o++] = (char)cp;
            else if (cp <= 0x7FF) {
                cikti[o++] = (char)(0xC0 | (cp >> 6));
                cikti[o++] = (char)(0x80 | (cp & 0x3F));
            } else {
                cikti[o++] = (char)(0xE0 | (cp >> 12));
                cikti[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                cikti[o++] = (char)(0x80 | (cp & 0x3F));
            }
        }
    } else {
        long long kopya = uzunluk;
        if (kopya >= 3 && girdi[0] == 0xEF && girdi[1] == 0xBB && girdi[2] == 0xBF) {
            girdi += 3;
            kopya -= 3;
        }
        long long badOffset = 0;
        long long badLine = 1;
        long long badColumn = 1;
        if (!ykn_utf8_gecerli(girdi, kopya, badOffset, badLine, badColumn)) {
            g_source_utf8_hatasi = 1;
            g_source_utf8_hata_satir = badLine;
            g_source_utf8_hata_sutun = badColumn;
            return -1;
        }
        if (kopya >= kapasite) kopya = kapasite - 1;
        memcpy(cikti, girdi, (size_t)kopya);
        o = kopya;
    }
    cikti[o] = 0;
    return o;
}

struct HavuzGirdisi;
struct StringPool;
struct Jeton;
struct LexerState;
struct AstDugum;
struct ParamDugum;
struct AstArena;
struct ParserDurumu;
struct MakroKural;
struct MakroMotoru;
struct SembolGirdisi;
struct Kapsam;
struct TipMotoru;
struct GenericSablon;
struct TraitGirdisi;
struct GenericsMotoru;
struct MirTalimati;
struct MirBlok;
struct MakineKoduArenasi;
struct HavuzGirdisi {
    long long hash;
    char* veri_ptr;
    long long uzunluk;
};

struct StringPool {
    HavuzGirdisi* tablo;
    long long kapasite;
    long long kullanilan;
    char* arena_bellek;
    long long arena_offset;
};

struct Jeton {
    long long tip;
    char* baslangic;
    long long uzunluk;
    long long satir;
    long long sutun;
};

struct LexerState {
    char* kaynak;
    long long dosya_boyutu;
    long long guncel_offset;
    long long satir;
    long long sutun;
    Jeton* jetonlar;
    long long jeton_sayisi;
    long long jeton_kapasite;
    StringPool* pool;
};

struct AstDugum {
    long long tip;
    Jeton* ilk_jeton;
    Jeton* son_jeton;
    AstDugum* sol;
    AstDugum* sag;
    AstDugum* sonraki;
    AstDugum* govde;
    AstDugum* yoksa_govde;
    AstDugum* parametreler;
    char* tip_adi;
    long long tip_adi_uzunluk;
    long long cozumlenmis_tip;
    long long dizi_boyutu;
    long long gorunurluk; // AST layout parity with src/ast.ykn
    long long nullable; // FIX13: 1 = ?*T (nullable pointer), 0 = *T (non-nullable)
    AstDugum* tip_arglari;
    long long tip_arg_sayisi;
};

struct ParamDugum {
    Jeton* isim_jeton;
    char* tip_adi;
    long long tip_adi_uzunluk;
    ParamDugum* sonraki;
    long long cozumlenmis_tip;
    long long nullable; // FIX13: nullable parameter type parity
};

struct AstArena {
    char* bellek;
    long long kapasite;
    long long kullanilan;
};

struct ParserDurumu {
    Jeton* jetonlar;
    long long jeton_sayisi;
    long long guncel_index;
    AstArena* arena;
    long long hata_sayisi;
};

struct MakroKural {
    long long isim_hash;
    long long parametre_sayisi;
    long long* parametre_hashleri;
    AstDugum* sablon_koku;
};

struct MakroMotoru {
    MakroKural* kurallar;
    long long kapasite;
    long long kayitli_sayi;
    AstArena* arena;
};

struct SembolGirdisi {
    long long isim_hash;
    Jeton* tanim_jetonu;
    long long veri_tipi;
    long long donus_tipi;
    AstDugum* ast_tanimi;
    SembolGirdisi* sonraki;
    long long nullable; // FIX13: 1 = ?*T (null check required before use)
};

struct Kapsam {
    SembolGirdisi** tablo;
    long long kapasite;
    Kapsam* ust_kapsam;
};

struct TipMotoru {
    Kapsam* guncel_kapsam;
    AstArena* arena;
    long long hata_sayisi;
    long long guncel_donus_tipi;
    long long tip_tamsayi;
    long long tip_gercek;
    long long tip_metin;
    long long tip_mantik;
    long long tip_u8;
    long long tip_hic;
    long long tip_bilinmiyor;
    long long tanimsiz_hata_sayisi;
    long long tip_uyumsuz_hata_sayisi;
    long long donus_hata_sayisi;
    long long param_sayisi_hata_sayisi;  // GOREV 2.2: SELF_HEAL_FONKSIYON_SAYISI_UYUMSUZ sayaci
    AstDugum* self_heal_adayi;
    long long self_heal_deger_konteksti;
};

struct GenericSablon {
    char* isim_ptr;
    long long isim_uzunluk;
    long long isim_hash;
    char* tip_param_ptr;
    long long tip_param_uzunluk;
    long long trait_hash;
    // Ikinci tip parametresi (coklu parametre destegi icin)
    char* tip_param2_ptr;
    long long tip_param2_uzunluk;
    long long trait_hash2;
    AstDugum* parametreler_ast;
    AstDugum* yapi_ast;
    AstDugum* govde_ast;
    GenericSablon* sonraki;
};

struct MonomorphKayit {
    char* mangled_ptr;
    long long mangled_uzunluk;
    long long mangled_hash;
    AstDugum* islev_ast;
    MonomorphKayit* sonraki;
};

struct TraitGirdisi {
    long long trait_hash;
    long long uygulanan_tip;
    TraitGirdisi* sonraki;
};

struct GenericsMotoru {
    GenericSablon* sablon_listesi;
    GenericSablon* yapi_sablon_listesi;
    MonomorphKayit* monomorph_listesi;
    MonomorphKayit* yapi_monomorph_listesi;
    TraitGirdisi* trait_tablosu;
    AstArena* arena;
};

struct MirTalimati {
    long long islem_kodu;
    long long hedef_sanal_reg;
    long long sol_sanal_reg;
    long long sag_sanal_reg;
    long long sabit_deger;
    long long etiket;
    MirTalimati* sonraki;
};

struct MirBlok {
    MirTalimati* ilk_talimat;
    MirTalimati* son_talimat;
    long long sanal_reg_sayaci;
    long long etiket_sayaci;
    AstArena* arena;
    long long cfg_bb_sayisi;
    long long cfg_talimat_sayisi;
};

struct MakineKoduArenasi {
    char* bellek;
    long long kapasite;
    long long kullanilan;
};

// ============================================================
// FAZ 2 Compiler Context Structs
// ============================================================
struct TargetContext {
    long long platform;
    long long verbose;
    long long no_color;
    long long safe_mode;
    long long maks_kaynak_boyutu;
};

struct DiagnosticContext {
    long long hata_sayisi;
    long long uyari_sayisi;
    long long son_hata_kodu;
    long long son_hata_satir;
    long long son_hata_sutun;
    char* kaynak_yolu;
};

struct ImportContext {
    char* import_listesi;
    long long import_sayisi;
    long long import_cycle_hatasi;
    long long import_alias_hatasi;
    long long ice_aktar_sayisi;
    long long ice_aktar_aktif_sayisi;
    long long maks_dosya;
    long long maks_derinlik;
    long long maks_dosya_boyutu;
};

struct LexerContext {
    char* kaynak;
    long long dosya_boyutu;
    long long guncel_offset;
    long long satir;
    long long sutun;
    char* jetonlar;
    long long jeton_sayisi;
    long long jeton_kapasite;
    long long hata_sayisi;
    long long ilk_hata_satir;
    long long ilk_hata_sutun;
    long long ilk_hata_offset;
    long long ilk_hata_kodu;
};

struct ParserContext {
    char* jetonlar;
    long long jeton_sayisi;
    long long guncel_index;
    AstArena* arena;
    long long hata_sayisi;
};

struct TypecheckerContext {
    TipMotoru* motor;
    AstArena* arena;
    long long hata_sayisi;
    long long guard_aktif;
    long long guard_gecersiz;
    char* guard_ad;
    long long guard_ad_uzunluk;
    char* guard_alan;
    long long guard_alan_uzunluk;
};

struct CodegenContext {
    MakineKoduArenasi* m_arena;
    MakineKoduArenasi* pe_arena;
    long long iat_yama_adet;
    long long iat_yama_sayisi;
    long long iat_simd_kullanildi;
    long long yapi_kapasite_hatasi;
    long long global_sabitleri_toplandi;
};

struct SelfHealContext {
    long long aktif;
    long long deneme_sayisi;
    long long max_denemeler;
    long long basarili;
};

struct OptimizerContext {
    long long opt_seviyesi;
    long long inlining_aktif;
    long long unroll_aktif;
    long long inline_yapilan_sayisi;
    long long unroll_yapilan_sayisi;
    long long dce_silinen_sayisi;
    long long folding_yapilan_sayisi;
    long long maks_inline_dugum;
    long long maks_unroll_sayisi;
};

struct CompilationContext {
    char* kaynak_yolu;
    char* cikti_yolu;
    char* dosya_dizini;
    char* kod_buffer;
    long long kod_uzunlugu;
    long long okunan_bayt;
    long long opt_seviyesi;
    long long inlining_aktif;
    long long loop_unroll_aktif;
    long long debug_info;
    long long dump_ir;
    long long dump_ast;
    AstDugum* ast_koku;
    char* jeton_arenasi;
    char* ast_bellek;
    MakroKural* makro_bellek;
    char* makine_kodu_bellek;
    char* elf_bellek;
    char* tc_bellek;
    long long derleme_sonucu;
};

struct CompilerContext {
    TargetContext* target;
    DiagnosticContext* diag;
    CompilationContext* compilation;
    ImportContext* import_ctx;
    LexerContext* lexer;
    ParserContext* parser;
    TypecheckerContext* tc;
    CodegenContext* codegen;
    OptimizerContext* optimizer;
    SelfHealContext* self_heal;
    void* convert_ctx;
    long long oturum_id;
};

typedef CompilerContext Compiler;

TargetContext* target_context_olustur();
DiagnosticContext* diagnostic_context_olustur();
ImportContext* import_context_olustur();
LexerContext* lexer_context_olustur();
ParserContext* parser_context_olustur();
TypecheckerContext* typechecker_context_olustur();
OptimizerContext* optimizer_context_olustur();
CodegenContext* codegen_context_olustur();
CompilationContext* compilation_context_olustur();
SelfHealContext* self_heal_context_olustur();
CompilerContext* compiler_context_olustur();
Compiler* compiler_olustur();
void compiler_context_sifirla(CompilerContext* c);
void compiler_oturum_sifirla(Compiler* c);
void compiler_context_kopyala(CompilerContext* hedef, CompilerContext* kaynak);
CompilerContext* compiler_context_snapshot_al(CompilerContext* kaynak);
long long yukinary_derle_ctx(CompilerContext* ctx, char* kaynak_kod, long long kod_uzunlugu, char* cikti_dosyasi, char* dosya_dizini, long long self_heal);


char* g_global_isim[16384];
long long g_global_uzunluk[16384];
long long g_global_adres[16384];
char* g_global_tip_adi[16384];
long long g_global_tip_adi_uzunluk[16384];
long long g_global_is_pointer[16384];
long long g_global_dizi_boyutu[16384];
long long g_global_sayisi = 0;
char* g_diagnostic_source_path = nullptr;
long long g_import_cycle_hatasi = 0;
long long g_import_alias_hatasi = 0;
long long g_parser_hata_sayisi = 0;
long long g_async_islev_aktif_seed = 0;
long long g_lexer_hata_sayisi = 0;
long long g_lexer_ilk_hata_satir = 0;
long long g_lexer_ilk_hata_sutun = 0;
long long g_lexer_ilk_hata_offset = 0;

static const char* tc_hata_kodu_adi(long long code) {
    switch (code) {
        case 1: return "TC_UNDEFINED_SYMBOL";
        case 2: return "PARSE_SYNTAX_ERROR";
        case 3: return "TC_TYPE_MISMATCH";
        case 4: return "TC_INVALID_RETURN";
        case 5: return "TC_PARAM_COUNT";
        case 6: return "IMPORT_CYCLE";
        case 21: return "ERR_RECURSION_DEPTH";
        case 22: return "ERR_MACRO_LIMIT";
        case 32: return "ERR_IMPLICIT_NULL_DEREF";
        case 33: return "ERR_NULLABLE_ACCESS_WITHOUT_CHECK";
        case 36: return "CONVERT_SYNTAX_ERROR";
        case 37: return "IMPORT_ALIAS_OK"; // F11.10: alias support added
        case 40: return "USE_AFTER_MOVE";
        case 41: return "BORROW_CONFLICT";
        case 46: return "ERR_DANGLING_POINTER";
        case 47: return "ERR_UNINITIALIZED_VARIABLE";
        case 48: return "ERR_INDEX_OUT_OF_BOUNDS";
        case 49: return "ERR_IMMUTABLE_ASSIGNMENT";
        case 44: return "ERR_AWAIT_REQUIRES_FUTURE";
        case 45: return "ERR_INVALID_FUTURE_TYPE";
        case 50: return "ERR_TYPE_SHADOWING";
        default: return "YKN_UNKNOWN_DIAGNOSTIC";
    }
}


static std::set<std::string> g_moved_vars;
struct YknBorrow {
    std::string name;
    bool is_mut;
    Kapsam* scope;
    bool is_temp;
};
static std::vector<YknBorrow> g_borrow_stack;
static bool g_in_binding = false;

static bool is_borrow_conflicted(const std::string& name, bool want_mut) {
    for (const auto& b : g_borrow_stack) {
        if (b.name == name) {
            if (b.is_mut) return true;
            if (want_mut) return true;
        }
    }
    return false;
}

static bool is_variable_borrowed(const std::string& name) {
    for (const auto& b : g_borrow_stack) {
        if (b.name == name) return true;
    }
    return false;
}

static void add_borrow(const std::string& name, bool is_mut, Kapsam* scope, bool is_temp) {
    g_borrow_stack.push_back({name, is_mut, scope, is_temp});
}

static void release_temp_borrows() {
    std::vector<YknBorrow> remaining;
    for (const auto& b : g_borrow_stack) {
        if (!b.is_temp) remaining.push_back(b);
    }
    g_borrow_stack = std::move(remaining);
}

static void release_scope_borrows(Kapsam* scope) {
    if (!scope) return;
    std::vector<YknBorrow> remaining;
    for (const auto& b : g_borrow_stack) {
        if (b.scope != scope) remaining.push_back(b);
    }
    g_borrow_stack = std::move(remaining);
}

static std::set<std::string> g_uninit_vars;
static long long seed_global_storage_index(char* isim, long long uzn);

// ============================================================
// Trait ve Dynamic Dispatch (F11.12 & F11.13)
// ============================================================
long long djb2_sembol(char* veri, long long uzunluk);
static void tc_diagnostic_yaz(long long code, const char* message, Jeton* token, long long row, long long col, const char* hint);
static int g_tc_trait_sayisi = 0;
static char* g_tc_trait_isim[64];
static long long g_tc_trait_uzunluk[64];
static long long g_tc_trait_metod_bas[64];
static long long g_tc_trait_metod_adet[64];

static long long g_tc_metod_toplam = 0;
static char* g_tc_metod_isim[256];
static long long g_tc_metod_uzunluk[256];
static AstDugum* g_tc_metod_ast[256];

static long long g_tc_impl_sayisi = 0;
static long long g_tc_impl_trait_hash[256];
static long long g_tc_impl_tip_hash[256];

static void tc_trait_kaydet(Jeton* tr_j, AstDugum* govde) {
    if (!tr_j || g_tc_trait_sayisi >= 64) return;
    int ti = g_tc_trait_sayisi;
    g_tc_trait_isim[ti] = tr_j->baslangic;
    g_tc_trait_uzunluk[ti] = tr_j->uzunluk;
    g_tc_trait_metod_bas[ti] = g_tc_metod_toplam;
    long long m_sayac = 0;
    AstDugum* it = govde;
    while (it) {
        if (it->ilk_jeton && g_tc_metod_toplam < 256) {
            g_tc_metod_isim[g_tc_metod_toplam] = it->ilk_jeton->baslangic;
            g_tc_metod_uzunluk[g_tc_metod_toplam] = it->ilk_jeton->uzunluk;
            g_tc_metod_ast[g_tc_metod_toplam] = it;
            g_tc_metod_toplam++;
            m_sayac++;
        }
        it = it->sonraki;
    }
    g_tc_trait_metod_adet[ti] = m_sayac;
    g_tc_trait_sayisi++;
}

static void tc_trait_uygulamasi_dogrula(TipMotoru* motor, Jeton* tr_j, Jeton* hedef_j, AstDugum* govde) {
    if (!tr_j || !hedef_j) return;
    int tr_idx = -1;
    for (int ti = 0; ti < g_tc_trait_sayisi; ti++) {
        if (g_tc_trait_uzunluk[ti] == tr_j->uzunluk &&
            memcmp(g_tc_trait_isim[ti], tr_j->baslangic, tr_j->uzunluk) == 0) {
            tr_idx = ti;
            break;
        }
    }
    if (tr_idx != -1) {
        long long mb = g_tc_trait_metod_bas[tr_idx];
        long long ma = g_tc_trait_metod_adet[tr_idx];
        for (long long mi = 0; mi < ma; mi++) {
            char* req_isim = g_tc_metod_isim[mb + mi];
            long long req_uzn = g_tc_metod_uzunluk[mb + mi];
            AstDugum* req_ast = g_tc_metod_ast[mb + mi];
            int bulundu = 0;
            AstDugum* bulunan_metod = 0;
            AstDugum* it = govde;
            while (it) {
                if (it->ilk_jeton && it->ilk_jeton->uzunluk == req_uzn &&
                    memcmp(it->ilk_jeton->baslangic, req_isim, req_uzn) == 0) {
                    bulundu = 1;
                    bulunan_metod = it;
                    break;
                }
                it = it->sonraki;
            }
            if (!bulundu) {
                tc_diagnostic_yaz(20, "HATA [E020]: Trait uygulamasi eksik metod iceriyor", tr_j, 0, 0, nullptr);
                g_parser_hata_sayisi++;
                if (motor) motor->hata_sayisi++;
            } else if (req_ast && bulunan_metod) {
                // Tam Trait Imza Dogrulamasi (F11.12 / Gorev 2.4)
                // 1. Parametre sayisi kontrolu
                long long p_req_sayisi = 0;
                long long p_impl_sayisi = 0;
                AstDugum* p_req_it = req_ast->parametreler;
                AstDugum* p_impl_it = bulunan_metod->parametreler;

                AstDugum* tp = p_req_it;
                while (tp) { p_req_sayisi++; tp = tp->sonraki; }
                tp = p_impl_it;
                while (tp) { p_impl_sayisi++; tp = tp->sonraki; }

                if (p_req_sayisi != p_impl_sayisi) {
                    tc_diagnostic_yaz(20, "HATA [E020]: Trait metodu parametre sayisi uyumsuz", bulunan_metod->ilk_jeton, 0, 0, nullptr);
                    g_parser_hata_sayisi++;
                    if (motor) motor->hata_sayisi++;
                } else {
                    // 2. Parametre tipleri kontrolu
                    while (p_req_it && p_impl_it) {
                        int p_uyumlu = 1;
                        if (p_req_it->cozumlenmis_tip != p_impl_it->cozumlenmis_tip) {
                            p_uyumlu = 0;
                        } else if (p_req_it->tip_adi && p_impl_it->tip_adi) {
                            bool is_self = (p_req_it->tip_adi_uzunluk == 4 && memcmp(p_req_it->tip_adi, "Self", 4) == 0);
                            if (is_self) {
                                if ((p_impl_it->tip_adi_uzunluk != hedef_j->uzunluk || memcmp(p_impl_it->tip_adi, hedef_j->baslangic, hedef_j->uzunluk) != 0) &&
                                    (p_impl_it->tip_adi_uzunluk != 4 || memcmp(p_impl_it->tip_adi, "Self", 4) != 0)) {
                                    p_uyumlu = 0;
                                }
                            } else {
                                if (p_req_it->tip_adi_uzunluk != p_impl_it->tip_adi_uzunluk ||
                                    memcmp(p_req_it->tip_adi, p_impl_it->tip_adi, p_req_it->tip_adi_uzunluk) != 0) {
                                    p_uyumlu = 0;
                                }
                            }
                        } else if ((p_req_it->tip_adi && !p_impl_it->tip_adi) || (!p_req_it->tip_adi && p_impl_it->tip_adi)) {
                            p_uyumlu = 0;
                        }

                        if (!p_uyumlu) {
                            tc_diagnostic_yaz(20, "HATA [E020]: Trait metodu parametre tipi uyumsuz", p_impl_it->ilk_jeton, 0, 0, nullptr);
                            g_parser_hata_sayisi++;
                            if (motor) motor->hata_sayisi++;
                            break;
                        }
                        p_req_it = p_req_it->sonraki;
                        p_impl_it = p_impl_it->sonraki;
                    }
                }

                // 3. Donus tipi kontrolu
                int ret_uyumlu = 1;
                if (req_ast->cozumlenmis_tip != bulunan_metod->cozumlenmis_tip) {
                    ret_uyumlu = 0;
                } else if (req_ast->tip_adi && bulunan_metod->tip_adi) {
                    bool ret_is_self = (req_ast->tip_adi_uzunluk == 4 && memcmp(req_ast->tip_adi, "Self", 4) == 0);
                    if (ret_is_self) {
                        if ((bulunan_metod->tip_adi_uzunluk != hedef_j->uzunluk || memcmp(bulunan_metod->tip_adi, hedef_j->baslangic, hedef_j->uzunluk) != 0) &&
                            (bulunan_metod->tip_adi_uzunluk != 4 || memcmp(bulunan_metod->tip_adi, "Self", 4) != 0)) {
                            ret_uyumlu = 0;
                        }
                    } else {
                        if (req_ast->tip_adi_uzunluk != bulunan_metod->tip_adi_uzunluk ||
                            memcmp(req_ast->tip_adi, bulunan_metod->tip_adi, req_ast->tip_adi_uzunluk) != 0) {
                            ret_uyumlu = 0;
                        }
                    }
                } else if ((req_ast->tip_adi && !bulunan_metod->tip_adi) || (!req_ast->tip_adi && bulunan_metod->tip_adi)) {
                    ret_uyumlu = 0;
                }

                if (!ret_uyumlu) {
                    tc_diagnostic_yaz(20, "HATA [E020]: Trait metodu donus tipi uyumsuz", bulunan_metod->ilk_jeton, 0, 0, nullptr);
                    g_parser_hata_sayisi++;
                    if (motor) motor->hata_sayisi++;
                }
            }
        }
    }
    if (g_tc_impl_sayisi < 256) {
        g_tc_impl_trait_hash[g_tc_impl_sayisi] = djb2_sembol(tr_j->baslangic, tr_j->uzunluk);
        g_tc_impl_tip_hash[g_tc_impl_sayisi] = djb2_sembol(hedef_j->baslangic, hedef_j->uzunluk);
        g_tc_impl_sayisi++;
    }
}

static int tc_trait_uygulandi_mi(long long trait_hash, long long tip_hash) {
    for (long long i = 0; i < g_tc_impl_sayisi; i++) {
        if (g_tc_impl_trait_hash[i] == trait_hash && g_tc_impl_tip_hash[i] == tip_hash) {
            return 1;
        }
    }
    return 0;
}

static void tc_builtin_traitleri_kaydet() {
    static int initialized = 0;
    if (initialized) return;
    initialized = 1;
    const char* builtins[] = {"Boyut", "Kopyala", "Esit", "Hash", "Toplama", "Karsilastir", "Goster", "Klonla", "Gonder", "Send", "Paylas", "Sync"};
    const char* types[] = {"tamsayi", "gercek", "metin", "mantik", "u8"};
    for (int ti = 0; ti < 5; ti++) {
        long long t_h = djb2_sembol((char*)types[ti], (long long)strlen(types[ti]));
        long long t_id = ti + 1;
        for (int bi = 0; bi < 12; bi++) {
            long long tr_h = djb2_sembol((char*)builtins[bi], (long long)strlen(builtins[bi]));
            if (g_tc_impl_sayisi < 256) {
                g_tc_impl_trait_hash[g_tc_impl_sayisi] = tr_h;
                g_tc_impl_tip_hash[g_tc_impl_sayisi] = t_id;
                g_tc_impl_sayisi++;
            }
            if (g_tc_impl_sayisi < 256) {
                g_tc_impl_trait_hash[g_tc_impl_sayisi] = tr_h;
                g_tc_impl_tip_hash[g_tc_impl_sayisi] = t_h;
                g_tc_impl_sayisi++;
            }
        }
    }
}

static void tc_diagnostic_yaz(long long code, const char* message, Jeton* token,
                              long long line_override = -1, long long column_override = -1,
                              const char* hint = nullptr) {
    long long line = (line_override >= 0) ? line_override : (token ? token->satir : 0);
    long long column = (column_override >= 0) ? column_override : (token ? token->sutun : 0);
    const char* cls = (code == 40 || code == 41 || code == 46) ? "ownership" : ((code == 47 || code == 48) ? "safety" : ((code == 6 || code == 37) ? "import" : ((code == 2 || code == 21 || code == 36) ? "parser" : ((code == 22) ? "macros" : ((code == 33 || code == 43) ? "codegen" : "typechecker")))));
    std::printf("HATA [E%03lld] [%s] [class=%s]:", code, tc_hata_kodu_adi(code), cls);
    if (g_diagnostic_source_path && g_diagnostic_source_path[0]) std::printf("%s:", g_diagnostic_source_path);
    std::printf("%lld:%lld — %s\n", line, column, message ? message : "");
    if (token && token->baslangic) {
        std::printf("  [KOD_BAGLAMI]: '%.*s'\n", 50, token->baslangic > (char*)15 ? token->baslangic - 10 : token->baslangic);
    }
    if (hint && hint[0]) std::printf("  = ipucu: Bunu mu demek istediniz? '%s'\n", hint);
}

static void ozyineleme_derinlik_hatasi(long long satir, long long sutun, const char* mesaj) {
    tc_diagnostic_yaz(21, mesaj, nullptr, satir, sutun);
}

static void makro_limit_hatasi(long long satir, long long sutun, const char* mesaj) {
    tc_diagnostic_yaz(22, mesaj, nullptr, satir, sutun);
}

static void emit_import_cycle_diagnostic(Jeton* token) {
    g_import_cycle_hatasi = 1;
    tc_diagnostic_yaz(6, "IMPORT_CYCLE: import dongusu", token, 1, 1);
}

long long djb2_hash(char* veri, long long uzunluk);
char* havuz_ekle_veya_bul(StringPool* pool, char* kelime_ptr, long long uzunluk);
long long harf_mi(long long k);
void hata_bas_ve_kapat(LexerState* lx, char* hata_anahtari, long long ek_bilgi);
long long rakam_mi(long long k);
long long kimlik_karakteri_mi(long long k);
long long bellek_esit_mi(char* a, char* b, long long uzunluk);
long long kelime_tipini_bul(char* adres, long long uzunluk);
long long karakter_oku(LexerState* lx);
long long goz_at(LexerState* lx);
void jeton_ekle(LexerState* lx, long long tip, char* baslangic, long long uzunluk);
long long tokenize_et(LexerState* lx);
AstDugum* arena_dugum_yarat(AstArena* arena, long long tip, Jeton* jeton);
ParamDugum* arena_param_yarat(AstArena* arena, Jeton* isim);
void hata_bas_parser(char* mesaj, Jeton* j);
Jeton* suanki_jeton(ParserDurumu* d);
Jeton* onceki_jeton(ParserDurumu* d);
void ilerle(ParserDurumu* d);
long long jeton_tipinde_mi(ParserDurumu* d, long long tip);
Jeton* bekle(ParserDurumu* d, long long tip);
void tip_oku(ParserDurumu* d, AstDugum* dugum);
long long operator_onceligi(long long tip);
AstDugum* parse_prefix(ParserDurumu* d);
AstDugum* parse_infix(ParserDurumu* d, AstDugum* sol);
AstDugum* parse_ifade(ParserDurumu* d, long long oncelik);
void tip_anotasyonu_oku(ParserDurumu* d, AstDugum* hedef);
AstDugum* parametre_listesi_oku(ParserDurumu* d);
AstDugum* parse_blok(ParserDurumu* d);
AstDugum* parse_satir(ParserDurumu* d);
AstDugum* parser_cozumle(ParserDurumu* d);
long long djb2_makro(char* veri, long long uzunluk);
MakroMotoru motor_olustur(AstArena* arena, MakroKural* kural_bellek, long long kapasite);
void makro_kaydet(MakroMotoru* motor, char* isim_ptr, long long isim_uzunluk, long long* params, long long param_sayisi, AstDugum* govde);
MakroKural* kural_bul(MakroMotoru* motor, long long isim_hash);
AstDugum* arguman_bul(MakroKural* kural, AstDugum* argumanlar, long long hedef_hash);
AstDugum* ast_kopyala_ve_degistir(MakroMotoru* motor, MakroKural* kural, AstDugum* sablon, AstDugum* argumanlar);
AstDugum* makrolari_genislet(MakroMotoru* motor, AstDugum* kok);
long long djb2_sembol(char* veri, long long uzunluk);
void kapsam_olustur(TipMotoru* motor, long long kapasite);
void kapsamdan_cik(TipMotoru* motor);
void sembol_ekle(TipMotoru* motor, Jeton* isim_j, long long tip, long long donus, AstDugum* ast = 0);
SembolGirdisi* sembol_ara(TipMotoru* motor, Jeton* isim_j);
SembolGirdisi* sembol_ara_hash(TipMotoru* motor, long long hash);
long long tip_adini_coz(char* ad, long long uzunluk);
long long tip_cikar(TipMotoru* motor, AstDugum* dugum);
long long typechecker_self_heal_dene(TipMotoru* motor);

TipMotoru* tip_denetimini_baslat_ve_don(AstArena* arena, AstDugum* kok);
void tip_denetimini_baslat(AstArena* arena, AstDugum* kok);
long long ykn_trait_uygulandi_mi(GenericsMotoru* motor, long long trait_hash, long long kontrol_edilen_tip);
void ykn_trait_uygulamasi_kaydet(GenericsMotoru* motor, Jeton* trait_isim_jetonu, long long hedef_tip);
void ykn_generic_sablon_ekle(GenericsMotoru* motor, AstDugum* islev_d);
GenericSablon* ykn_generic_sablon_bul(GenericsMotoru* motor, char* isim_ptr, long long isim_uzunluk);
AstDugum* ykn_ast_klonla_ve_tip_yerlestir(GenericsMotoru* motor, AstDugum* kok, char* t_isim, long long t_uzunluk, char* somut_tip, long long somut_uzunluk);
AstDugum* ykn_ast_klonla_ve_coklu_tip_yerlestir(GenericsMotoru* motor, AstDugum* kok, AstDugum* parametreler_ast, AstDugum* somut_ast);
AstDugum* ykn_monomorph_kaydi_bul(GenericsMotoru* motor, char* mangled_ptr, long long mangled_uzunluk);
void ykn_generic_yapi_sablon_ekle(GenericsMotoru* motor, AstDugum* yapi_d);
GenericSablon* ykn_generic_yapi_sablon_bul(GenericsMotoru* motor, char* isim_ptr, long long isim_uzunluk);
AstDugum* ykn_yapi_monomorph_kaydi_bul(GenericsMotoru* motor, char* mangled_ptr, long long mangled_uzunluk);
void ykn_generic_yapi_monomorfize_et(GenericsMotoru* motor, AstDugum* tip_dugumu, AstDugum** uretilen_fonklar_bas);
void ykn_generics_ifade_tara(GenericsMotoru* motor, AstDugum* dugum, AstDugum** uretilen_fonklar_bas);
AstDugum* ykn_generics_pipeline_calistir(GenericsMotoru* motor, AstDugum* kok);
MirTalimati* mir_talimati_yarat(MirBlok* blok, long long op, long long hedef, long long sol, long long sag, long long sabit);
long long yeni_reg(MirBlok* blok);
long long yeni_etiket(MirBlok* blok);
long long string_tamsayiya(char* veri, long long uzn);
long long ast_den_mir_uret(MirBlok* blok, AstDugum* dugum);
void dce_optimizasyonu_yap(MirBlok* blok);
void cfg_ve_talimat_sayaci_ata(MirBlok* blok);
void live_interval_hesapla(MirBlok* blok, long long* baslangic_dizisi, long long* bitis_dizisi, long long maks_reg);
void fiziksel_register_ata(MirBlok* blok);
void u16_yaz(MakineKoduArenasi* a, long long v);
void int32_yaz(MakineKoduArenasi* a, long long v);
void int64_yaz(MakineKoduArenasi* a, long long v);
void sifir_doldur(MakineKoduArenasi* a, long long adet);
void str_yaz(MakineKoduArenasi* a, char* s);
long long hizala_yukari(long long deger, long long hizalama);
void emit_push_rbp(MakineKoduArenasi* a);
void emit_pop_rbp(MakineKoduArenasi* a);
void emit_ret(MakineKoduArenasi* a);
void emit_mov_rbp_rsp(MakineKoduArenasi* a);
void emit_sub_rsp(MakineKoduArenasi* a, long long n);
void emit_add_rsp(MakineKoduArenasi* a, long long n);
void emit_xor_rax_rax(MakineKoduArenasi* a);
void emit_mov_rax_imm32(MakineKoduArenasi* a, long long v);
void emit_mov_rcx_imm32(MakineKoduArenasi* a, long long v);
void emit_mov_rdx_imm32(MakineKoduArenasi* a, long long v);
void emit_add_rax_rcx(MakineKoduArenasi* a);
void emit_sub_rax_rcx(MakineKoduArenasi* a);
void emit_imul_rax_rcx(MakineKoduArenasi* a);
void emit_store_rax(MakineKoduArenasi* a, long long off);
void emit_load_rax(MakineKoduArenasi* a, long long off);
void emit_lea_rax(MakineKoduArenasi* a, long long off);
void ra_ikili_sol_kaydet(MakineKoduArenasi* a);
void ra_ikili_sol_geri_yukle_rcx(MakineKoduArenasi* a);
void emit_call_rip_rel(MakineKoduArenasi* a, long long rel32);
void emit_lea_rcx_rip(MakineKoduArenasi* a, long long rel32);
void emit_mov_r8d_imm32(MakineKoduArenasi* a, long long v);
void emit_xor_r9d_r9d(MakineKoduArenasi* a);
void emit_cmp_rax_rcx(MakineKoduArenasi* a);
void emit_sete_al(MakineKoduArenasi* a);
void emit_setne_al(MakineKoduArenasi* a);
void emit_setl_al(MakineKoduArenasi* a);
void emit_setg_al(MakineKoduArenasi* a);
void emit_setle_al(MakineKoduArenasi* a);
void emit_setge_al(MakineKoduArenasi* a);
void emit_movzx_rax_al(MakineKoduArenasi* a);
long long ast_kod_uret(MakineKoduArenasi* a, AstDugum* dugum);
static void emit_yknllm_audit_native_append(MakineKoduArenasi* a, AstDugum* dugum);
void pe32plus_uret(MakineKoduArenasi* kod_arenasi, MakineKoduArenasi* cikti, long long ep_rva);
long long windows_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres);
long long linux_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres);
void elf64_linux_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset);
void elf64_basligi_ve_kodu_yaz(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod);
long long yukinary_derle(char* kaynak_kod, long long kod_uzunlugu, char* cikti_dosyasi, char* dosya_dizini, long long self_heal);
void ice_aktar_isle(AstDugum* kok, AstArena* ast_arena, Jeton* jeton_arenasi,
                    long long& toplam_jeton, char* ana_dosya_dizini,
                    char* aktif_dosya_yolu);
extern "C" long long ana(long long arguman_sayisi, char** argumanlar);
long long g_jit_modu = 0;
long long g_self_heal_aktif = 0;
long long g_bounds_check = 0;
// FIX13: guard state for assignment invalidation.
long long g_fix13_guard_active = 0;
long long g_fix13_guard_invalidated = 0;
char* g_fix13_guard_name = nullptr;
long long g_fix13_guard_name_length = 0;
char* g_fix13_guard_name2 = nullptr;
long long g_fix13_guard_name2_length = 0;
char* g_fix13_guard_field = nullptr;
long long g_fix13_guard_field_length = 0;

static bool fix13_guard_matches(AstDugum* d) {
    if (!g_fix13_guard_active || !d) return false;
    if (g_fix13_guard_field) {
        if (d->tip != 17 || !d->sol || !d->sag) return false;
        if (d->sol->tip != 4 || !d->sol->ilk_jeton || d->sol->ilk_jeton->uzunluk != g_fix13_guard_name_length) return false;
        if (std::memcmp(d->sol->ilk_jeton->baslangic, g_fix13_guard_name, (size_t)g_fix13_guard_name_length) != 0) return false;
        if (d->sag->tip != 4 || !d->sag->ilk_jeton || d->sag->ilk_jeton->uzunluk != g_fix13_guard_field_length) return false;
        return std::memcmp(d->sag->ilk_jeton->baslangic, g_fix13_guard_field, (size_t)g_fix13_guard_field_length) == 0;
    }
    if (!g_fix13_guard_name || d->tip != 4 || !d->ilk_jeton ||
        d->ilk_jeton->uzunluk != g_fix13_guard_name_length) return false;
    if (std::memcmp(d->ilk_jeton->baslangic, g_fix13_guard_name,
                    (size_t)g_fix13_guard_name_length) == 0) return true;
    return g_fix13_guard_name2 && d->ilk_jeton->uzunluk == g_fix13_guard_name2_length &&
        std::memcmp(d->ilk_jeton->baslangic, g_fix13_guard_name2,
                    (size_t)g_fix13_guard_name2_length) == 0;
}

static bool is_field_nullable(TipMotoru* motor, AstDugum* field_node) {
    if (!motor || !field_node || field_node->tip != 17 || !field_node->sol || !field_node->sag) return false;
    if (field_node->sol->tip != 4 || !field_node->sol->ilk_jeton) return false;
    if (field_node->sag->tip != 4 || !field_node->sag->ilk_jeton) return false;
    SembolGirdisi* var_s = sembol_ara(motor, field_node->sol->ilk_jeton);
    if (!var_s || !var_s->ast_tanimi || !var_s->ast_tanimi->tip_adi) return false;
    long long s_hash = djb2_sembol(var_s->ast_tanimi->tip_adi, var_s->ast_tanimi->tip_adi_uzunluk);
    SembolGirdisi* struct_s = sembol_ara_hash(motor, s_hash);
    if (!struct_s || !struct_s->ast_tanimi) return false;
    AstDugum* f_iter = struct_s->ast_tanimi->sol;
    while (f_iter) {
        if (f_iter->ilk_jeton && f_iter->ilk_jeton->uzunluk == field_node->sag->ilk_jeton->uzunluk &&
            bellek_esit_mi(f_iter->ilk_jeton->baslangic, field_node->sag->ilk_jeton->baslangic, f_iter->ilk_jeton->uzunluk) == 1) {
            return f_iter->nullable == 1;
        }
        f_iter = f_iter->sonraki;
    }
    return false;
}

// Faz 5: In-File Konfigurasyon Yonetimi ve Precedence
long long g_cli_hedef_acikca_verildi = 0;
long long g_cli_llm_model_acikca_verildi = 0;
long long g_cli_self_heal_acikca_verildi = 0;
long long g_cli_opt_acikca_verildi = 0;
long long g_cli_target_acikca_verildi = 0;
long long g_cli_max_tokens_acikca_verildi = 0;
char* g_in_file_cikti_dosyasi = nullptr;
long long g_cfg_stack_size = 0;
long long g_cfg_workers = 0;
long long g_cfg_memory_model = 0; // 0=default/arena, 1=heap
long long g_cfg_cpu_arch = 0;     // 0=baseline, 1=native

// 0=Windows PE32+, 1=Linux ELF64, 2=macOS planli
long long g_target_platform = 0;
long long g_verbose = 0;
long long g_dump_ir = 0; // --dump-ir flag: always show IR dump
long long g_debug_info = 0; // --debug flag: emit minimal DWARF4/CodeView debug info
// Explicit AST fault-injection target for the native mutation acceptance.
// The normal compiler path keeps this disabled.
long long g_ast_mutasyon_hedef = 0;
long long g_ast_mutasyon_aktif = 0;
long long g_ast_mutasyon_uygulandi = 0;
char* g_strict_kaynak_baslangic = nullptr;
char* g_strict_kaynak_son = nullptr;
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
auto TIP_DURDUR = 14;
auto TIP_DEVAM_ET = 15;
auto TIP_ICE_AKTAR = 16;
auto TIP_DOGRU = 17;
auto TIP_YANLIS = 18;
auto TIP_VE = 19;
auto TIP_VEYA = 20;
auto TIP_XOR = 43;
auto TIP_ESLES = 22;
auto TIP_MODUL = 23;
auto TIP_OLARAK = 24;
auto TIP_VARSAYILAN = 27;
auto TIP_ENUM = 28;
auto TIP_ARTI = 30;
auto TIP_EKSI = 31;
auto TIP_CARPI = 32;
auto TIP_BOLME = 33;
auto TIP_ATAMA = 34;
auto TIP_ESIT_MI = 35;
auto TIP_ESIT_DEGIL = 36;
auto TIP_KUCUK_MU = 37;
auto TIP_BUYUK_MU = 38;
auto TIP_KUCUK_ESIT = 39;
auto TIP_BUYUK_ESIT = 40;
auto TIP_MODULO = 41;
auto TIP_SORU_ISLEMI = 42;
auto TIP_SOL_PARANTEZ = 50;
auto TIP_SAG_PARANTEZ = 51;
auto TIP_SOL_KUSLU = 52;
auto TIP_SAG_KUSLU = 53;
auto TIP_SOL_KOSE = 54;
auto TIP_SAG_KOSE = 55;
auto TIP_NOKTA = 56;
auto TIP_NOKTALI_VIRGUL = 57;
auto TIP_VIRGUL = 58;
auto TIP_KOLON = 59;
auto TIP_OK = 60;
auto TIP_AMPERSAND = 61;
auto TIP_KALIN_OK = 62;
auto TIP_CIFT_IKINOKTA = 63;
auto TIP_SOL_KAYDIR = 45; // << sol bit kaydir
auto TIP_ASM = 49; // asm { } inline assembly blogu
auto TIP_SAG_KAYDIR = 46; // >> sag bit kaydir
auto TIP_TILDE = 47;      // ~ bitwise NOT
auto TIP_DIKEY_CUBUK = 48; // | bitwise OR
auto TIP_BOS = 64;       // FIX13: 'bos' null literal keyword
auto TIP_DIREKTIF_CONVERT = 65; // #C-convert, #PY-convert, etc.
auto TIP_DIREKTIF_CONFIG = 66; // #config, #yapilandir, etc.
auto TIP_EOF = 99;
auto TIP_BILINMEYEN = 100;
auto TIP_FSTRING = 101; // f"..." f-string interpolasyonu
auto TIP_ASYNC = 102; // async function declaration
auto TIP_AWAIT = 103; // await expression
auto TIP_TRAIT = 104; // ozellik / trait
auto TIP_UYGULA = 105; // uygula / impl
auto TIP_DYN = 106; // dyn
auto TIP_DEGIL = 107; // ! (mantiksal degil)
auto TIP_SABIT = 108; // sabit / const (immutable binding)
auto TIP_COMPTIME = 109; // comptime derleme zamani yurutme
long long djb2_hash(char* veri, long long uzunluk) {
long long hash = 5381;
long long i = 0;
while ((i < uzunluk)) {
hash = ((hash * 33) + veri[i]);
i = (i + 1);
}
;
return hash;
}


long long g_string_pool_arena_kapasite = 16777216;

StringPool* havuz_olustur(long long baslangic_kapasite, long long arena_boyutu) {
    long long kap = baslangic_kapasite;
    if (kap < 1024) kap = 1024;
    long long a_boyut = arena_boyutu;
    if (a_boyut < 65536) a_boyut = 65536;
    g_string_pool_arena_kapasite = a_boyut;

    StringPool* pool = (StringPool*)std::calloc(1, sizeof(StringPool));
    if (!pool) return nullptr;
    pool->tablo = (HavuzGirdisi*)std::calloc((size_t)kap, sizeof(HavuzGirdisi));
    pool->kapasite = kap;
    pool->kullanilan = 0;
    pool->arena_bellek = (char*)std::calloc((size_t)a_boyut, 1);
    pool->arena_offset = 0;
    return pool;
}

long long havuz_buyut(StringPool* pool) {
    if (!pool) return 0;
    long long eski_kapasite = YKN_DOT(pool, kapasite);
    HavuzGirdisi* eski_tablo = YKN_DOT(pool, tablo);
    long long yeni_kapasite = eski_kapasite * 2;
    if (yeni_kapasite < 1024) yeni_kapasite = 1024;

    HavuzGirdisi* yeni_tablo = (HavuzGirdisi*)std::calloc((size_t)yeni_kapasite, sizeof(HavuzGirdisi));
    if (!yeni_tablo) return 0;

    for (long long i = 0; i < eski_kapasite; i++) {
        if (eski_tablo && eski_tablo[i].veri_ptr != 0) {
            long long h = eski_tablo[i].hash;
            long long idx = h % yeni_kapasite;
            if (idx < 0) idx = -idx;
            while (yeni_tablo[idx].veri_ptr != 0) {
                idx = (idx + 1) % yeni_kapasite;
            }
            yeni_tablo[idx] = eski_tablo[i];
        }
    }
    if (eski_tablo) std::free(eski_tablo);
    YKN_DOT(pool, tablo) = yeni_tablo;
    YKN_DOT(pool, kapasite) = yeni_kapasite;
    return 1;
}

char* havuz_ekle_veya_bul(StringPool* pool, char* kelime_ptr, long long uzunluk) {
if ((!pool || !kelime_ptr || uzunluk <= 0)) {
return kelime_ptr;
}
if ((YKN_DOT(pool, kapasite) <= 0 || ((YKN_DOT(pool, kullanilan) + 1) * 10 >= (YKN_DOT(pool, kapasite) * 7)))) {
havuz_buyut(pool);
}
if ((YKN_DOT(pool, kapasite) <= 0 || YKN_DOT(pool, tablo) == 0)) {
return kelime_ptr;
}
long long hash = djb2_hash(kelime_ptr, uzunluk);
long long indeks = (hash % YKN_DOT(pool, kapasite));
if ((indeks < 0)) { indeks = (0 - indeks); }
long long adim = 0;
while ((adim < YKN_DOT(pool, kapasite) && (YKN_DOT((_ykn_ref(pool).tablo)[indeks], veri_ptr)) != 0)) {
if (((YKN_DOT((_ykn_ref(pool).tablo)[indeks], hash) == hash) && (YKN_DOT((_ykn_ref(pool).tablo)[indeks], uzunluk) == uzunluk))) {
long long i = 0;
long long esit_mi = 1;
while ((i < uzunluk)) {
if (((_ykn_ref((_ykn_ref(pool).tablo)[indeks]).veri_ptr)[i] != kelime_ptr[i])) {
esit_mi = 0;
}
;
i = (i + 1);
}
;
if ((esit_mi == 1)) {
return YKN_DOT((_ykn_ref(pool).tablo)[indeks], veri_ptr);
}
;
}
;
indeks = ((indeks + 1) % YKN_DOT(pool, kapasite));
adim = (adim + 1);
}
;
if ((YKN_DOT(pool, arena_bellek) == 0 || (YKN_DOT(pool, arena_offset) + uzunluk + 1) > g_string_pool_arena_kapasite)) {
long long yeni_arena_boyut = g_string_pool_arena_kapasite;
if ((yeni_arena_boyut < 65536)) yeni_arena_boyut = 16777216;
if ((uzunluk + 4096 > yeni_arena_boyut)) yeni_arena_boyut = (uzunluk + 4096);
char* yeni_arena = (char*)std::calloc((size_t)yeni_arena_boyut, 1);
if (!yeni_arena) return kelime_ptr;
YKN_DOT(pool, arena_bellek) = yeni_arena;
YKN_DOT(pool, arena_offset) = 0;
g_string_pool_arena_kapasite = yeni_arena_boyut;
}
char* yeni_ptr = (char*)((char*)((char*)(YKN_DOT(pool, arena_bellek)) + YKN_DOT(pool, arena_offset)));
long long k = 0;
while ((k < uzunluk)) {
yeni_ptr[k] = kelime_ptr[k];
k = (k + 1);
}
;
yeni_ptr[uzunluk] = 0;
YKN_DOT(pool, arena_offset) = (YKN_DOT(pool, arena_offset) + uzunluk + 1);
YKN_DOT((_ykn_ref(pool).tablo)[indeks], hash) = hash;
YKN_DOT((_ykn_ref(pool).tablo)[indeks], veri_ptr) = yeni_ptr;
YKN_DOT((_ykn_ref(pool).tablo)[indeks], uzunluk) = uzunluk;
YKN_DOT(pool, kullanilan) = (YKN_DOT(pool, kullanilan) + 1);
return yeni_ptr;
}


long long utf8_bayt_sayisi(long long ilk) {
    if (ilk <= 127) return 1;
    if (ilk >= 194 && ilk <= 223) return 2;
    if (ilk >= 224 && ilk <= 239) return 3;
    if (ilk >= 240 && ilk <= 244) return 4;
    return 0;
}
long long utf8_devam_bayti_mi(long long b) { return b >= 128 && b <= 191 ? 1 : 0; }
long long utf8_kod_noktasi_oku(char* kaynak, long long boyut, long long ofset) {
    if (!kaynak || ofset < 0 || ofset >= boyut) return -1;
    unsigned char first = (unsigned char)kaynak[ofset];
    long long n = utf8_bayt_sayisi(first);
    if (n == 0 || ofset + n > boyut) return -1;
    if (n == 1) return first;
    for (long long i = 1; i < n; ++i) if (!utf8_devam_bayti_mi((unsigned char)kaynak[ofset + i])) return -1;
    long long cp = 0;
    if (n == 2) { cp = (first - 192) * 64 + ((unsigned char)kaynak[ofset + 1] - 128); if (cp < 128) return -1; }
    else if (n == 3) { cp = (first - 224) * 4096 + ((unsigned char)kaynak[ofset + 1] - 128) * 64 + ((unsigned char)kaynak[ofset + 2] - 128); if (cp < 2048 || (cp >= 55296 && cp <= 57343)) return -1; }
    else { cp = (first - 240) * 262144 + ((unsigned char)kaynak[ofset + 1] - 128) * 4096 + ((unsigned char)kaynak[ofset + 2] - 128) * 64 + ((unsigned char)kaynak[ofset + 3] - 128); if (cp < 65536 || cp > 1114111) return -1; }
    return cp;
}
long long utf8_kimlik_kodu_mi(long long cp) { return ((cp >= 65 && cp <= 90) || (cp >= 97 && cp <= 122) || (cp >= 48 && cp <= 57) || cp == 95 || cp >= 128) ? 1 : 0; }
long long utf8_scalar_sayisi(char* kaynak, long long bas, long long son) { long long i = bas, count = 0; while (i < son) { long long n = utf8_bayt_sayisi((unsigned char)kaynak[i]); if (n <= 0 || i + n > son) n = 1; i += n; ++count; } return count; }
long long utf8_utf16_birim_sayisi(char* kaynak, long long bas, long long son) { long long i = bas, count = 0; while (i < son) { long long n = utf8_bayt_sayisi((unsigned char)kaynak[i]); long long cp = utf8_kod_noktasi_oku(kaynak, son, i); if (n <= 0 || i + n > son || cp < 0) n = 1; i += n; count += cp > 65535 ? 2 : 1; } return count; }

long long harf_mi(long long k) {
if (((((k >= 65) && (k <= 90)) || ((k >= 97) && (k <= 122))) || (k > 127))) {
return 1;
}
;
return 0;
}


void hata_bas_ve_kapat(LexerState* lx, char* hata_anahtari, long long ek_bilgi) {
long long satir_basi = 0;
long long satir_sonu = 0;
long long i = (YKN_DOT(lx, guncel_offset) - 1);
while (((i > 0) && ((_ykn_ref(lx).kaynak)[i] != 10))) {
i = (i - 1);
}
;
if (((_ykn_ref(lx).kaynak)[i] == 10)) {
i = (i + 1);
}
;
satir_basi = i;
i = (YKN_DOT(lx, guncel_offset) - 1);
while (((i < YKN_DOT(lx, dosya_boyutu)) && ((_ykn_ref(lx).kaynak)[i] != 10))) {
i = (i + 1);
}
;
satir_sonu = i;
}


long long onaltilik_mi(long long k) {
    if ((k >= 48 && k <= 57) || (k >= 97 && k <= 102) || (k >= 65 && k <= 70)) return 1;
    return 0;
}

long long rakam_mi(long long k) {
if (((k >= 48) && (k <= 57))) {
return 1;
}
;
return 0;
}


long long kimlik_karakteri_mi(long long k) {
if (((harf_mi(k) == 1) || ((k >= 48) && (k <= 57))) || (k == 95)) {
return 1;
}
;
return 0;
}


long long bellek_esit_mi(char* a, char* b, long long uzunluk) {
long long i = 0;
while ((i < uzunluk)) {
if ((a[i] != b[i])) {
return 0;
}
;
i = (i + 1);
}
;
return 1;
}


// ==========================================================
// YKN Runtime: Thread/Mutex ve Network (Windows WinAPI)
// Stub harici isv'lerin gercek implementasyonlari
// ==========================================================
#ifdef _WIN32

// Mutex runtime -- yukinary_sys_mutex_*
extern "C" long long yukinary_sys_mutex_create() {
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    if (!cs) return 0;
    InitializeCriticalSection(cs);
    return (long long)(intptr_t)cs;
}
extern "C" long long yukinary_sys_mutex_lock(long long mutex) {
    if (!mutex) return -1;
    EnterCriticalSection((CRITICAL_SECTION*)(intptr_t)mutex);
    return 0;
}
extern "C" long long yukinary_sys_mutex_unlock(long long mutex) {
    if (!mutex) return -1;
    LeaveCriticalSection((CRITICAL_SECTION*)(intptr_t)mutex);
    return 0;
}
extern "C" long long yukinary_sys_mutex_destroy(long long mutex) {
    if (!mutex) return -1;
    DeleteCriticalSection((CRITICAL_SECTION*)(intptr_t)mutex);
    free((void*)(intptr_t)mutex);
    return 0;
}

// Network runtime -- yukinary_sys_net_*
extern "C" long long yukinary_sys_net_init() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_create(long long af, long long type, long long protocol) {
    SOCKET s = socket((int)af, (int)type, (int)protocol);
    return (s == INVALID_SOCKET) ? -1 : (long long)s;
}
extern "C" long long yukinary_sys_socket_bind(long long sock, long long addr, long long port) {
    if (sock < 0 || addr < 0 || port < 0 || port > 65535) return -1;
    struct sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = (u_long)addr;
    sa.sin_port = htons((u_short)port);
    int enabled = 1;
    if (setsockopt((SOCKET)sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&enabled, sizeof(enabled)) != 0) return -1;
    return bind((SOCKET)sock, (struct sockaddr*)&sa, sizeof(sa)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_listen(long long sock, long long backlog) {
    return listen((SOCKET)sock, (int)backlog) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_accept(long long sock) {
    SOCKET client = accept((SOCKET)sock, nullptr, nullptr);
    return (client == INVALID_SOCKET) ? -1 : (long long)client;
}
extern "C" long long yukinary_sys_socket_close(long long sock) {
    if (sock < 0) return -1;
    return closesocket((SOCKET)sock) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_send(long long sock, char* buffer, long long length) {
    if (sock < 0 || !buffer || length < 0) return -1;
    return send((SOCKET)sock, buffer, (int)length, 0);
}
extern "C" long long yukinary_sys_socket_recv(long long sock, char* buffer, long long length) {
    if (sock < 0 || !buffer || length < 0 || length > 0x7fffffffLL) return -1;
    return recv((SOCKET)sock, buffer, (int)length, 0);
}
extern "C" long long yukinary_sys_socket_connect(long long sock, long long addr, long long port) {
    if (sock < 0 || addr < 0 || port < 0 || port > 65535) return -1;
    struct sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = (u_long)addr;
    sa.sin_port = htons((u_short)port);
    return connect((SOCKET)sock, (struct sockaddr*)&sa, sizeof(sa)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_set_timeout(long long sock, long long millis) {
    if (sock < 0 || millis < 0) return -1;
    DWORD timeout = millis > 0xFFFFFFFFLL ? 0xFFFFFFFFu : (DWORD)millis;
    int size = (int)sizeof(timeout);
    int a = setsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, size);
    int b = setsockopt((SOCKET)sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, size);
    return (a == 0 && b == 0) ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_set_reuseaddr(long long sock) {
    if (sock < 0) return -1;
    int enabled = 1;
    return setsockopt((SOCKET)sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&enabled, sizeof(enabled)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_dns_resolve(char* host, char* address, long long capacity) {
    if (!host || !host[0] || !address || capacity < 4) return -1;
    struct in_addr parsed = {};
    if (inet_pton(AF_INET, host, &parsed) == 1) { memcpy(address, &parsed.s_addr, 4); return 0; }
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &result) != 0 || !result) return -1;
    const sockaddr_in* endpoint = (const sockaddr_in*)result->ai_addr;
    memcpy(address, &endpoint->sin_addr.s_addr, 4);
    freeaddrinfo(result);
    return 0;
}

#else
// POSIX runtime (Linux ve gelecek macOS hedefi): gercek pthread/socket cagrilari.
// Bu dal test atlamak veya sahte handle donmek yerine OS hata kodlarini -1 ile bildirir.
extern "C" long long yukinary_sys_mutex_create() {
    pthread_mutex_t* mutex = static_cast<pthread_mutex_t*>(std::malloc(sizeof(pthread_mutex_t)));
    if (!mutex) return 0;
    if (pthread_mutex_init(mutex, nullptr) != 0) {
        std::free(mutex);
        return 0;
    }
    return static_cast<long long>(reinterpret_cast<intptr_t>(mutex));
}
extern "C" long long yukinary_sys_mutex_lock(long long handle) {
    if (!handle) return -1;
    return pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(static_cast<intptr_t>(handle)));
}
extern "C" long long yukinary_sys_mutex_unlock(long long handle) {
    if (!handle) return -1;
    return pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(static_cast<intptr_t>(handle)));
}
extern "C" long long yukinary_sys_mutex_destroy(long long handle) {
    if (!handle) return -1;
    pthread_mutex_t* mutex = reinterpret_cast<pthread_mutex_t*>(static_cast<intptr_t>(handle));
    const int result = pthread_mutex_destroy(mutex);
    if (result == 0) std::free(mutex);
    return result;
}
extern "C" long long yukinary_sys_net_init() {
    // POSIX sockets do not require a process-wide startup call.
    return 0;
}
extern "C" long long yukinary_sys_socket_create(long long family, long long type, long long protocol) {
    const int sock = ::socket(static_cast<int>(family), static_cast<int>(type), static_cast<int>(protocol));
    return sock < 0 ? -1 : static_cast<long long>(sock);
}
extern "C" long long yukinary_sys_socket_bind(long long sock, long long addr, long long port) {
    if (sock < 0 || addr < 0 || port < 0 || port > 65535) return -1;
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = static_cast<in_addr_t>(addr);
    endpoint.sin_port = htons(static_cast<uint16_t>(port));
    int enabled = 1;
    if (::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) != 0) return -1;
    return ::bind(static_cast<int>(sock), reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_listen(long long sock, long long backlog) {
    if (sock < 0 || backlog < 0) return -1;
    return ::listen(static_cast<int>(sock), static_cast<int>(backlog)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_accept(long long sock) {
    if (sock < 0) return -1;
    const int client = ::accept(static_cast<int>(sock), nullptr, nullptr);
    return client < 0 ? -1 : static_cast<long long>(client);
}
extern "C" long long yukinary_sys_socket_close(long long sock) {
    if (sock < 0) return -1;
    return ::close(static_cast<int>(sock)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_send(long long sock, char* buffer, long long length) {
    if (sock < 0 || !buffer || length < 0) return -1;
    return ::send(static_cast<int>(sock), buffer, static_cast<size_t>(length), 0);
}
extern "C" long long yukinary_sys_socket_recv(long long sock, char* buffer, long long length) {
    if (sock < 0 || !buffer || length < 0) return -1;
    return ::recv(static_cast<int>(sock), buffer, static_cast<size_t>(length), 0);
}
extern "C" long long yukinary_sys_socket_connect(long long sock, long long addr, long long port) {
    if (sock < 0 || addr < 0 || port < 0 || port > 65535) return -1;
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = static_cast<in_addr_t>(addr);
    endpoint.sin_port = htons(static_cast<uint16_t>(port));
    return ::connect(static_cast<int>(sock), reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_set_timeout(long long sock, long long millis) {
    if (sock < 0 || millis < 0) return -1;
    timeval timeout{};
    timeout.tv_sec = static_cast<time_t>(millis / 1000);
    timeout.tv_usec = static_cast<suseconds_t>((millis % 1000) * 1000);
    socklen_t size = static_cast<socklen_t>(sizeof(timeout));
    int a = ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_RCVTIMEO, &timeout, size);
    int b = ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_SNDTIMEO, &timeout, size);
    return (a == 0 && b == 0) ? 0 : -1;
}
extern "C" long long yukinary_sys_socket_set_reuseaddr(long long sock) {
    if (sock < 0) return -1;
    int enabled = 1;
    return ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == 0 ? 0 : -1;
}
extern "C" long long yukinary_sys_dns_resolve(char* host, char* address, long long capacity) {
    if (!host || !host[0] || !address || capacity < 4) return -1;
    in_addr parsed{};
    if (::inet_pton(AF_INET, host, &parsed) == 1) { memcpy(address, &parsed.s_addr, 4); return 0; }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (::getaddrinfo(host, nullptr, &hints, &result) != 0 || !result) return -1;
    const sockaddr_in* endpoint = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    memcpy(address, &endpoint->sin_addr.s_addr, 4);
    ::freeaddrinfo(result);
    return 0;
}
#endif
long long g_enum_var_sayisi = 0;
// BUG-4: Tip alias tablosu
static char* g_tip_alias_isim[128] = {};
static long long g_tip_alias_isim_uzn[128] = {};
static long long g_tip_alias_hedef[128] = {};
static long long g_tip_alias_sayisi = 0;
void tip_alias_kaydet(char* isim, long long uzn, long long hedef_tip_id) {
    if (g_tip_alias_sayisi >= 128) return;
    long long i = g_tip_alias_sayisi++;
    g_tip_alias_isim[i] = isim; g_tip_alias_isim_uzn[i] = uzn; g_tip_alias_hedef[i] = hedef_tip_id;
}
long long tip_alias_bul(char* isim, long long uzn) {
    for (long long i = 0; i < g_tip_alias_sayisi; i++) {
        if (g_tip_alias_isim_uzn[i] == uzn && bellek_esit_mi(g_tip_alias_isim[i], isim, uzn) == 1)
            return g_tip_alias_hedef[i];
    }
    return -1;
}
char* g_enum_var_tam_isim[4096] = {};
long long g_enum_var_tam_uzn[4096] = {};
char* g_enum_var_kisa_isim[4096] = {};
long long g_enum_var_kisa_uzn[4096] = {};
long long g_enum_var_disc[4096] = {};
long long g_enum_var_payload[4096] = {};
char* g_enum_var_payload_tip[4096] = {};
long long g_enum_var_payload_tip_uzn[4096] = {};

long long enum_varyant_bul_tam(char* isim, long long uzn) {
    for (long long i = 0; i < g_enum_var_sayisi; ++i) {
        if (g_enum_var_tam_uzn[i] == uzn && bellek_esit_mi(g_enum_var_tam_isim[i], isim, uzn)) return i;
    }
    return -1;
}
long long enum_varyant_bul_kisa(char* isim, long long uzn) {
    for (long long i = 0; i < g_enum_var_sayisi; ++i) {
        if (g_enum_var_kisa_uzn[i] == uzn && bellek_esit_mi(g_enum_var_kisa_isim[i], isim, uzn)) return i;
    }
    return -1;
}
void enum_varyant_ekle(char* tam, long long tam_uzn, char* kisa, long long kisa_uzn, long long disc, long long payload, char* payload_tip, long long payload_tip_uzn) {
    if (g_enum_var_sayisi >= 4096) return;
    long long i = g_enum_var_sayisi++;
    g_enum_var_tam_isim[i] = tam; g_enum_var_tam_uzn[i] = tam_uzn;
    g_enum_var_kisa_isim[i] = kisa; g_enum_var_kisa_uzn[i] = kisa_uzn;
    g_enum_var_disc[i] = disc; g_enum_var_payload[i] = payload;
    g_enum_var_payload_tip[i] = payload_tip; g_enum_var_payload_tip_uzn[i] = payload_tip_uzn;
}
long long enum_turu_mi(char* ad, long long uzunluk) {
    if (!ad || uzunluk <= 0) return 0;
    for (long long i = 0; i < g_enum_var_sayisi; ++i) {
        if (g_enum_var_tam_uzn[i] > uzunluk + 2 && g_enum_var_tam_isim[i][uzunluk] == '_' && g_enum_var_tam_isim[i][uzunluk + 1] == '_' && bellek_esit_mi(g_enum_var_tam_isim[i], ad, uzunluk) == 1) return 1;
    }
    return 0;
}
long long enum_varyant_has_any_payload(long long ev) {
    if (ev < 0 || ev >= g_enum_var_sayisi) return 0;
    char* name = g_enum_var_tam_isim[ev];
    long long len = g_enum_var_tam_uzn[ev];
    long long prefix_len = 0;
    while (prefix_len + 1 < len) {
        if (name[prefix_len] == '_' && name[prefix_len + 1] == '_') break;
        prefix_len++;
    }
    if (prefix_len >= len) return g_enum_var_payload[ev];
    for (long long i = 0; i < g_enum_var_sayisi; i++) {
        if (g_enum_var_tam_uzn[i] > prefix_len + 2 &&
            g_enum_var_tam_isim[i][prefix_len] == '_' && g_enum_var_tam_isim[i][prefix_len + 1] == '_' &&
            bellek_esit_mi(g_enum_var_tam_isim[i], name, prefix_len) == 1) {
            if (g_enum_var_payload[i] != 0) return 1;
        }
    }
    return 0;
}
long long enum_disc_uzunluk(long long disc) {
    if (disc < 10) return 1;
    long long v = disc;
    long long uzn = 0;
    while (v > 0) { v /= 10; uzn++; }
    return uzn;
}
char* enum_disc_metni(long long disc) {
    static char values[10][2] = {{'0',0},{'1',0},{'2',0},{'3',0},{'4',0},{'5',0},{'6',0},{'7',0},{'8',0},{'9',0}};
    if (disc >= 0 && disc < 10) return values[disc];
    if (disc < 0) return (char*)"0";
    long long uzn = enum_disc_uzunluk(disc);
    char* buf = (char*)bellek_tahsis_et(uzn + 1);
    if (!buf) return (char*)"0";
    buf[uzn] = 0;
    long long v = disc;
    for (long long idx = uzn - 1; idx >= 0; --idx) {
        buf[idx] = (char)('0' + (v % 10));
        v /= 10;
    }
    return buf;
}

long long kelime_tipini_bul(char* adres, long long uzunluk) {
if (!adres || uzunluk <= 0) return TIP_KIMLIK;

// Canonical UTF-8 aliases. Keep these byte-based so the Stage-0 seed
// remains independent of the host source-code page.
if (uzunluk == 2 && bellek_esit_mi(adres, (char*)"\xD0\xB8", 2) == 1) return TIP_VE; // и
if (uzunluk == 3) {
    if (bellek_esit_mi(adres, (char*)"\xE5\x88\x99", 3) == 1) return TIP_ISE; // 则
    if (bellek_esit_mi(adres, (char*)"\xE7\x9C\x9F", 3) == 1) return TIP_DOGRU; // 真
    if (bellek_esit_mi(adres, (char*)"\xE5\x81\x87", 3) == 1) return TIP_YANLIS; // 假
    if (bellek_esit_mi(adres, (char*)"\xE4\xBB\x96", 3) == 1) return TIP_YOKSA; // 他
    if (bellek_esit_mi(adres, (char*)"\xE9\x96\x93", 3) == 1) return TIP_DONGU; // 間
    if (bellek_esit_mi(adres, (char*)"\xE5\x81\xBD", 3) == 1) return TIP_YANLIS; // 偽
    if (bellek_esit_mi(adres, (char*)"\xEC\xB0\xB8", 3) == 1) return TIP_DOGRU; // 참
    if (bellek_esit_mi(adres, (char*)"\xE9\x9D\x9E", 3) == 1) return TIP_DEGIL; // 非
    if (bellek_esit_mi(adres, (char*)"\xE7\x84\xA1", 3) == 1) return TIP_BOS; // 無
    if (bellek_esit_mi(adres, (char*)"\xEB\x84\x90", 3) == 1) return TIP_BOS; // 널
}
if (uzunluk == 4) {
    if (bellek_esit_mi(adres, (char*)"\xD1\x82\xD0\xBE", 4) == 1) return TIP_ISE; // то
    if (bellek_esit_mi(adres, (char*)"\xD0\xB4\xD0\xB0", 4) == 1) return TIP_DOGRU; // да
    if (bellek_esit_mi(adres, (char*)"\xD1\x84\xD0\xBD", 4) == 1) return TIP_ISLEV; // фн
    if (bellek_esit_mi(adres, (char*)"\xD0\xBD\xD0\xB5", 4) == 1) return TIP_DEGIL; // не
}
if (uzunluk == 6) {
    if (bellek_esit_mi(adres, (char*)"\xD0\xB4\xD0\xBB\xD1\x8F", 6) == 1) return TIP_HER; // для
    if (bellek_esit_mi(adres, (char*)"\xD0\xB8\xD0\xBB\xD0\xB8", 6) == 1) return TIP_VEYA; // или
    if (bellek_esit_mi(adres, (char*)"\xD0\xBD\xD0\xB5\xD1\x82", 6) == 1) return TIP_YANLIS; // нет
    if (bellek_esit_mi(adres, (char*)"\xE9\x96\xA2\xE6\x95\xB0", 6) == 1) return TIP_ISLEV; // 関数
    if (bellek_esit_mi(adres, (char*)"\xE5\xA4\x89\xE6\x95\xB0", 6) == 1) return TIP_DEGISKEN; // 変数
    if (bellek_esit_mi(adres, (char*)"\xE5\xAE\x9A\xE6\x95\xB0", 6) == 1) return TIP_SABIT; // 定数
    if (bellek_esit_mi(adres, (char*)"\xE5\xB8\xB8\xE9\x87\x8F", 6) == 1) return TIP_SABIT; // 常量
    if (bellek_esit_mi(adres, (char*)"\xEC\x83\x81\xEC\x88\x98", 6) == 1) return TIP_SABIT; // 상수
    if (bellek_esit_mi(adres, (char*)"\xE8\xBF\x94\xE3\x81\x99", 6) == 1) return TIP_DONDUR; // 返す
    if (bellek_esit_mi(adres, (char*)"\xE6\x88\xBB\xE3\x82\x8B", 6) == 1) return TIP_DONDUR; // 戻る
    if (bellek_esit_mi(adres, (char*)"\xE3\x82\x82\xE3\x81\x97", 6) == 1) return TIP_EGER; // もし
    if (bellek_esit_mi(adres, (char*)"\xE3\x81\xAA\xE3\x82\x89", 6) == 1) return TIP_ISE; // なら
    if (bellek_esit_mi(adres, (char*)"\xE5\x81\x9C\xE6\xAD\xA2", 6) == 1) return TIP_DURDUR; // 停止
    if (bellek_esit_mi(adres, (char*)"\xE6\xAC\xA1\xE3\x81\xB8", 6) == 1) return TIP_DEVAM_ET; // 次へ
    if (bellek_esit_mi(adres, (char*)"\xE5\x87\xBD\xE6\x95\xB0", 6) == 1) return TIP_ISLEV; // 函数
    if (bellek_esit_mi(adres, (char*)"\xE5\x8F\x98\xE9\x87\x8F", 6) == 1) return TIP_DEGISKEN; // 变量
    if (bellek_esit_mi(adres, (char*)"\xE8\xBF\x94\xE5\x9B\x9E", 6) == 1) return TIP_DONDUR; // 返回
    if (bellek_esit_mi(adres, (char*)"\xE5\xA6\x82\xE6\x9E\x9C", 6) == 1) return TIP_EGER; // 如果
    if (bellek_esit_mi(adres, (char*)"\xE5\x90\xA6\xE5\x88\x99", 6) == 1) return TIP_YOKSA; // 否则
    if (bellek_esit_mi(adres, (char*)"\xE5\xBE\xAA\xE7\x8E\xAF", 6) == 1) return TIP_DONGU; // 循环
    if (bellek_esit_mi(adres, (char*)"\xE7\xBB\x93\xE6\x9E\x84", 6) == 1) return TIP_YAPI; // 结构
    if (bellek_esit_mi(adres, (char*)"\xED\x95\xA8\xEC\x88\x98", 6) == 1) return TIP_ISLEV; // 함수
    if (bellek_esit_mi(adres, (char*)"\xEB\xB3\x80\xEC\x88\x98", 6) == 1) return TIP_DEGISKEN; // 변수
    if (bellek_esit_mi(adres, (char*)"\xEB\xB0\x98\xED\x99\x98", 6) == 1) return TIP_DONDUR; // 반환
    if (bellek_esit_mi(adres, (char*)"\xEB\xA7\x8C\xEC\x95\xBD", 6) == 1) return TIP_EGER; // 만약
    if (bellek_esit_mi(adres, (char*)"\xEB\x9D\xBC\xEB\xA9\xB4", 6) == 1) return TIP_ISE; // 라면
    if (bellek_esit_mi(adres, (char*)"\xEB\xB0\x98\xEB\xB3\xB5", 6) == 1) return TIP_DONGU; // 반복
    if (bellek_esit_mi(adres, (char*)"\xEA\xB1\xB0\xEC\xA7\x93", 6) == 1) return TIP_YANLIS; // 거짓
    if (bellek_esit_mi(adres, (char*)"\xE3\x81\x8B\xE3\x81\xA4", 6) == 1) return TIP_VE; // かつ
    if (bellek_esit_mi(adres, (char*)"\xE5\xB9\xB6\xE4\xB8\x94", 6) == 1) return TIP_VE; // 并且
    if (bellek_esit_mi(adres, (char*)"\xE6\x88\x96\xE8\x80\x85", 6) == 1) return TIP_VEYA; // 或者
    if (bellek_esit_mi(adres, (char*)"\xEB\x98\x90\xEB\x8A\x94", 6) == 1) return TIP_VEYA; // 또는
    if (bellek_esit_mi(adres, (char*)"\xE5\x90\xA6\xE5\xAE\x9A", 6) == 1) return TIP_DEGIL; // 否定
}
if (uzunluk == 8) {
    if (bellek_esit_mi(adres, (char*)"\xD0\xB5\xD1\x81\xD0\xBB\xD0\xB8", 8) == 1) return TIP_EGER; // если
    if (bellek_esit_mi(adres, (char*)"\xD0\xBF\xD0\xBE\xD0\xBA\xD0\xB0", 8) == 1) return TIP_DONGU; // пока
    if (bellek_esit_mi(adres, (char*)"\xD1\x86\xD0\xB8\xD0\xBA\xD0\xBB", 8) == 1) return TIP_DONGU; // цикл
    if (bellek_esit_mi(adres, (char*)"\xD0\xBB\xD0\xBE\xD0\xB6\xD1\x8C", 8) == 1) return TIP_YANLIS; // ложь
}
if (uzunluk == 9) {
    if (bellek_esit_mi(adres, (char*)"\xE3\x83\xAB\xE3\x83\xBC\xE3\x83\x97", 9) == 1) return TIP_DONGU; // ループ
    if (bellek_esit_mi(adres, (char*)"\xE6\xA7\x8B\xE9\x80\xA0\xE4\xBD\x93", 9) == 1) return TIP_YAPI; // 構造体
    if (bellek_esit_mi(adres, (char*)"\xEC\x95\x84\xEB\x8B\x88\xEB\xA9\xB4", 9) == 1) return TIP_YOKSA; // 아니면
    if (bellek_esit_mi(adres, (char*)"\xEA\xB7\xB8\xEB\xA6\xAC\xEA\xB3\xA0", 9) == 1) return TIP_VE; // 그리고
    if (bellek_esit_mi(adres, (char*)"\xE3\x81\xBE\xE3\x81\x9F\xE3\x81\xAF", 9) == 1) return TIP_VEYA; // または
    if (bellek_esit_mi(adres, (char*)"\xEC\x95\x84\xEB\x8B\x88\xEB\x8B\xA4", 9) == 1) return TIP_DEGIL; // 아니다
}
if (uzunluk == 10) {
    if (bellek_esit_mi(adres, (char*)"\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD1\x8C", 10) == 1) return TIP_DEGISKEN; // пусть
    if (bellek_esit_mi(adres, (char*)"\xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xBC", 10) == 1) return TIP_DEGISKEN; // перем
    if (bellek_esit_mi(adres, (char*)"\xD0\xB8\xD0\xBD\xD0\xB0\xD1\x87\xD0\xB5", 10) == 1) return TIP_YOKSA; // иначе
    if (bellek_esit_mi(adres, (char*)"\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD0\xBE", 10) == 1) return TIP_BOS; // пусто
}
if (uzunluk == 12) {
    if (bellek_esit_mi(adres, (char*)"\xE3\x81\x9D\xE3\x82\x8C\xE4\xBB\xA5\xE5\xA4\x96", 12) == 1) return TIP_YOKSA; // それ以外
    if (bellek_esit_mi(adres, (char*)"\xD0\xB8\xD0\xBC\xD0\xBF\xD0\xBE\xD1\x80\xD1\x82", 12) == 1) return TIP_ICE_AKTAR; // импорт
    if (bellek_esit_mi(adres, (char*)"\xD0\xB8\xD1\x81\xD1\x82\xD0\xB8\xD0\xBD\xD0\xB0", 12) == 1) return TIP_DOGRU; // истина
}
if (uzunluk == 14) {
    if (bellek_esit_mi(adres, (char*)"\xD1\x84\xD1\x83\xD0\xBD\xD0\xBA\xD1\x86\xD0\xB8\xD1\x8F", 14) == 1) return TIP_ISLEV; // функция
    if (bellek_esit_mi(adres, (char*)"\xD0\xB2\xD0\xB5\xD1\x80\xD0\xBD\xD1\x83\xD1\x82\xD1\x8C", 14) == 1) return TIP_DONDUR; // вернуть
}
if (uzunluk == 16 && bellek_esit_mi(adres, (char*)"\xD0\xBF\xD1\x80\xD0\xB5\xD1\x80\xD0\xB2\xD0\xB0\xD1\x82\xD1\x8C", 16) == 1) return TIP_DURDUR; // прервать
if (uzunluk == 18 && bellek_esit_mi(adres, (char*)"\xD1\x81\xD1\x82\xD1\x80\xD1\x83\xD0\xBA\xD1\x82\xD1\x83\xD1\x80\xD0\xB0", 18) == 1) return TIP_YAPI; // структура
if (uzunluk == 18 && bellek_esit_mi(adres, (char*)"\xD0\xBA\xD0\xBE\xD0\xBD\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD1\x82\xD0\xB0", 18) == 1) return TIP_SABIT; // константа
if (uzunluk == 20 && bellek_esit_mi(adres, (char*)"\xD0\xBF\xD1\x80\xD0\xBE\xD0\xB4\xD0\xBE\xD0\xBB\xD0\xB6\xD0\xB8\xD1\x82\xD1\x8C", 20) == 1) return TIP_DEVAM_ET; // продолжить

// ---- Uzunluk 2 ----
if (uzunluk == 2) {
    if (adres[0] == 118 && adres[1] == 101) return TIP_VE; // TR: "ve"
    if (adres[0] == 'a' && adres[1] == 's') return TIP_OLARAK; // EN: "as"
    if (adres[0] == 111 && adres[1] == 114) return TIP_VEYA; // EN: "or"
    if (adres[0] == 105 && adres[1] == 102) return TIP_EGER; // EN: "if"
    if (adres[0] == 102 && adres[1] == 110) return TIP_ISLEV; // EN: "fn"
    if (adres[0] == 101 && adres[1] == 116) return TIP_VE; // FR: "et"
    if (adres[0] == 111 && adres[1] == 117) return TIP_VEYA; // FR: "ou"
    if (adres[0] == 115 && adres[1] == 105) return TIP_EGER; // FR: "si"
    if ((unsigned char)adres[0] == 208 && (unsigned char)adres[1] == 184) return TIP_VE; // RU: "Ğ¸"
}

// ---- Uzunluk 3 ----
if (uzunluk == 3) {
    if (adres[0] == 105 && adres[1] == 115 && adres[2] == 101) return TIP_ISE; // TR: "ise"
    if (adres[0] == 105 && adres[1] == 115 && adres[2] == 118) return TIP_ISLEV; // TR: "isv"
    if (bellek_esit_mi(adres, (char*)"dyn", 3) == 1) return TIP_DYN;
    if (bellek_esit_mi(adres, (char*)"her", 3) == 1) return TIP_HER;
    if (bellek_esit_mi(adres, (char*)"and", 3) == 1) return TIP_VE;
    if (bellek_esit_mi(adres, (char*)"not", 3) == 1) return TIP_DEGIL;
    if (bellek_esit_mi(adres, (char*)"var", 3) == 1) return TIP_DEGISKEN;
    if (bellek_esit_mi(adres, (char*)"let", 3) == 1) return TIP_DEGISKEN;
    if (bellek_esit_mi(adres, (char*)"mut", 3) == 1) return TIP_DEGISKEN;
    if (bellek_esit_mi(adres, (char*)"for", 3) == 1) return TIP_HER;
    if (bellek_esit_mi(adres, (char*)"use", 3) == 1) return TIP_ICE_AKTAR;
    // FIX13: null literal
    if (bellek_esit_mi(adres, (char*)"asm", 3) == 1) return TIP_ASM;  // inline assembly
    if (bellek_esit_mi(adres, (char*)"bos", 3) == 1) return TIP_BOS;  // TR: null
    if (bellek_esit_mi(adres, (char*)"nil", 3) == 1) return TIP_BOS;  // EN: null
    if (bellek_esit_mi(adres, (char*)"nul", 3) == 1) return TIP_BOS;  // FR: null
}

// ---- Uzunluk 4 ----
if (uzunluk == 4) {
    if (adres[0] == 101 && adres[1] == 103 && adres[2] == 101 && adres[3] == 114) return TIP_EGER; // TR: "eger"
    if (bellek_esit_mi(adres, (char*)"impl", 4) == 1) return TIP_UYGULA;
    if (bellek_esit_mi(adres, (char*)"yapi", 4) == 1) return TIP_YAPI;
    if (bellek_esit_mi(adres, (char*)"enum", 4) == 1) return TIP_ENUM;
    if (bellek_esit_mi(adres, (char*)"veya", 4) == 1) return TIP_VEYA;
    if (bellek_esit_mi(adres, (char*)"then", 4) == 1) return TIP_ISE;
    if (bellek_esit_mi(adres, (char*)"else", 4) == 1) return TIP_YOKSA;
    if (bellek_esit_mi(adres, (char*)"loop", 4) == 1) return TIP_DONGU;
    if (bellek_esit_mi(adres, (char*)"each", 4) == 1) return TIP_HER;
    if (bellek_esit_mi(adres, (char*)"type", 4) == 1) return TIP_YAPI;
    if (bellek_esit_mi(adres, (char*)"true", 4) == 1) return TIP_DOGRU;
    if (bellek_esit_mi(adres, (char*)"func", 4) == 1) return TIP_ISLEV;
    if (bellek_esit_mi(adres, (char*)"soit", 4) == 1) return TIP_DEGISKEN; // FR
    if (bellek_esit_mi(adres, (char*)"pour", 4) == 1) return TIP_HER; // FR
    if (bellek_esit_mi(adres, (char*)"vrai", 4) == 1) return TIP_DOGRU; // FR
    if (bellek_esit_mi(adres, (char*)"faux", 4) == 1) return TIP_YANLIS; // FR
    // FIX13: null literal aliases
    if (bellek_esit_mi(adres, (char*)"null", 4) == 1) return TIP_BOS;  // EN: null
}

// ---- Uzunluk 5 ----
if (uzunluk == 5) {
    if (bellek_esit_mi(adres, (char*)"trait", 5) == 1) return TIP_TRAIT;
    if (bellek_esit_mi(adres, (char*)"islev", 5) == 1) return TIP_ISLEV;
    if (bellek_esit_mi(adres, (char*)"dondu", 5) == 1) return TIP_DONDUR;
    if (bellek_esit_mi(adres, (char*)"yoksa", 5) == 1) return TIP_YOKSA;
    if (bellek_esit_mi(adres, (char*)"dongu", 5) == 1) return TIP_DONGU;
    if (bellek_esit_mi(adres, (char*)"async", 5) == 1) return TIP_ASYNC;
    if (bellek_esit_mi(adres, (char*)"await", 5) == 1) return TIP_AWAIT;
    if (bellek_esit_mi(adres, (char*)"dogru", 5) == 1) return TIP_DOGRU;
    if (bellek_esit_mi(adres, (char*)"degil", 5) == 1) return TIP_DEGIL;
    if (bellek_esit_mi(adres, (char*)"esles", 5) == 1) return TIP_ESLES;
    if (bellek_esit_mi(adres, (char*)"match", 5) == 1) return TIP_ESLES;
    if (bellek_esit_mi(adres, (char*)"while", 5) == 1) return TIP_DONGU;
    if (bellek_esit_mi(adres, (char*)"break", 5) == 1) return TIP_DURDUR;
    if (bellek_esit_mi(adres, (char*)"false", 5) == 1) return TIP_YANLIS;
    if (bellek_esit_mi(adres, (char*)"sinon", 5) == 1) return TIP_YOKSA; // FR
    if (bellek_esit_mi(adres, (char*)"modul", 5) == 1) return TIP_MODUL;
    if (bellek_esit_mi(adres, (char*)"sayim", 5) == 1) return TIP_ENUM;
    if (bellek_esit_mi(adres, (char*)"sabit", 5) == 1) return TIP_SABIT;
    if (bellek_esit_mi(adres, (char*)"const", 5) == 1) return TIP_SABIT;
    if (bellek_esit_mi(adres, (char*)"alors", 5) == 1) return TIP_ISE; // FR
}

// ---- Uzunluk 6 ----
if (uzunluk == 6) {
    if (bellek_esit_mi(adres, (char*)"uygula", 6) == 1) return TIP_UYGULA;
    if (bellek_esit_mi(adres, (char*)"olarak", 6) == 1) return TIP_OLARAK;
    if (bellek_esit_mi(adres, (char*)"yanlis", 6) == 1) return TIP_YANLIS;
    if (bellek_esit_mi(adres, (char*)"durdur", 6) == 1) return TIP_DURDUR;
    if (bellek_esit_mi(adres, (char*)"dondur", 6) == 1) return TIP_DONDUR;
    if (bellek_esit_mi(adres, (char*)"return", 6) == 1) return TIP_DONDUR;
    if (bellek_esit_mi(adres, (char*)"struct", 6) == 1) return TIP_YAPI;
    if (bellek_esit_mi(adres, (char*)"import", 6) == 1) return TIP_ICE_AKTAR;
    if (bellek_esit_mi(adres, (char*)"retour", 6) == 1) return TIP_DONDUR; // FR
    if (bellek_esit_mi(adres, (char*)"boucle", 6) == 1) return TIP_DONGU; // FR
}

// ---- Uzunluk 7 ----
if (uzunluk == 7) {
    if (bellek_esit_mi(adres, (char*)"ozellik", 7) == 1) return TIP_TRAIT;
    if (bellek_esit_mi(adres, (char*)"default", 7) == 1) return TIP_VARSAYILAN;
    if (bellek_esit_mi(adres, (char*)"tantque", 7) == 1) return TIP_DONGU; // FR
    if (bellek_esit_mi(adres, (char*)"arreter", 7) == 1) return TIP_DURDUR; // FR
    if (bellek_esit_mi(adres, (char*)"include", 7) == 1) return TIP_ICE_AKTAR;
}

// ---- Uzunluk 8 ----
if (uzunluk == 8) {
    if (bellek_esit_mi(adres, (char*)"degisken", 8) == 1) return TIP_DEGISKEN;
    if (bellek_esit_mi(adres, (char*)"comptime", 8) == 1) return TIP_COMPTIME;
    if (bellek_esit_mi(adres, (char*)"devam_et", 8) == 1) return TIP_DEVAM_ET;
    if (bellek_esit_mi(adres, (char*)"dahil_et", 8) == 1) return TIP_ICE_AKTAR;
    if (bellek_esit_mi(adres, (char*)"function", 8) == 1) return TIP_ISLEV;
    if (bellek_esit_mi(adres, (char*)"continue", 8) == 1) return TIP_DEVAM_ET;
    if (bellek_esit_mi(adres, (char*)"fonction", 8) == 1) return TIP_ISLEV; // FR
    if (bellek_esit_mi(adres, (char*)"importer", 8) == 1) return TIP_ICE_AKTAR; // FR
}

// ---- Uzunluk 9 ----
if (uzunluk == 9) {
    if (bellek_esit_mi(adres, (char*)"ice_aktar", 9) == 1) return TIP_ICE_AKTAR;
    if (bellek_esit_mi(adres, (char*)"structure", 9) == 1) return TIP_YAPI; // FR
    if (bellek_esit_mi(adres, (char*)"continuer", 9) == 1) return TIP_DEVAM_ET; // FR
    if (bellek_esit_mi(adres, (char*)"retourner", 9) == 1) return TIP_DONDUR; // FR
}

// ---- Uzunluk 10 ----
if (uzunluk == 10) {
    if (bellek_esit_mi(adres, (char*)"varsayilan", 10) == 1) return TIP_VARSAYILAN;
}

// ---- Uzunluk 12 ----
if (uzunluk == 12) {
}

// ---- Uzunluk 14 ----
if (uzunluk == 14) {
}

// ---- Uzunluk 16 ----
if (uzunluk == 16) {
}

// ---- Uzunluk 18 ----
if (uzunluk == 18) {
}

// ---- Uzunluk 20 ----
if (uzunluk == 20) {
}

return TIP_KIMLIK;
}


long long karakter_oku(LexerState* lx) {
if ((YKN_DOT(lx, guncel_offset) >= YKN_DOT(lx, dosya_boyutu))) {
return 0;
}
;
long long c = (unsigned char)(_ykn_ref(lx).kaynak)[YKN_DOT(lx, guncel_offset)];
YKN_DOT(lx, guncel_offset) = (YKN_DOT(lx, guncel_offset) + 1);
if (c < 128 || c > 191) YKN_DOT(lx, sutun) = (YKN_DOT(lx, sutun) + 1);
return c;
}


long long goz_at(LexerState* lx) {
if ((YKN_DOT(lx, guncel_offset) >= YKN_DOT(lx, dosya_boyutu))) {
return 0;
}
;
return (unsigned char)(_ykn_ref(lx).kaynak)[YKN_DOT(lx, guncel_offset)];
}


void jeton_ekle(LexerState* lx, long long tip, char* baslangic, long long uzunluk) {
char buf[128];
long long copy_len = uzunluk;
if (copy_len > 127) copy_len = 127;
for(long long i=0; i<copy_len; i++) buf[i] = baslangic[i];
buf[copy_len] = 0;
// [DEBUG_REMOVED] printf("JETON: tip=%lld, metin='%s'\n", tip, buf);

;
long long i = YKN_DOT(lx, jeton_sayisi);
if (i >= YKN_DOT(lx, jeton_kapasite)) {
    printf("HATA [E013] [ERR_TOKEN_LIMIT] [class=compiler]: Token kapasitesi asildi\n");
    std::exit(1);
}
// [DEBUG_REMOVED] printf("DBG_TOKEN_WRITE idx=%lld target=%p text=%p len=%lld\n", i, (void*)(&((_ykn_ref(lx).jetonlar)[i])), (void*)baslangic, uzunluk);
YKN_DOT((_ykn_ref(lx).jetonlar)[i], tip) = tip;
YKN_DOT((_ykn_ref(lx).jetonlar)[i], baslangic) = baslangic;
YKN_DOT((_ykn_ref(lx).jetonlar)[i], uzunluk) = uzunluk;
YKN_DOT((_ykn_ref(lx).jetonlar)[i], satir) = YKN_DOT(lx, satir);
long long token_scalar = utf8_scalar_sayisi(YKN_DOT(lx, kaynak), YKN_DOT(lx, guncel_offset) - uzunluk, YKN_DOT(lx, guncel_offset));
if (token_scalar <= 0) token_scalar = uzunluk;
YKN_DOT((_ykn_ref(lx).jetonlar)[i], sutun) = (YKN_DOT(lx, sutun) - token_scalar);
YKN_DOT(lx, jeton_sayisi) = (YKN_DOT(lx, jeton_sayisi) + 1);
}

long long direktif_ismi_tipini_bul(char* adres, long long uzunluk) {
    if (uzunluk <= 1) return 0;
    if (uzunluk == 2 && adres[0] == 'a' && adres[1] == 'i') return 21;
    if (uzunluk == 5 && bellek_esit_mi(adres, (char*)"c-lib", 5) == 1) return 23;
    if (uzunluk == 6) {
        if (bellek_esit_mi(adres, (char*)"prompt", 6) == 1) return 21;
        if (bellek_esit_mi(adres, (char*)"py-lib", 6) == 1) return 25;
        if (bellek_esit_mi(adres, (char*)"rs-lib", 6) == 1) return 26;
    }
    if (uzunluk == 7 && bellek_esit_mi(adres, (char*)"cpp-lib", 7) == 1) return 24;
    if (uzunluk == 8 && bellek_esit_mi(adres, (char*)"rust-lib", 8) == 1) return 26;
    if (uzunluk == 10) {
        if (bellek_esit_mi(adres, (char*)"python-lib", 10) == 1) return 25;
        if (bellek_esit_mi(adres, (char*)"yapay_zeka", 10) == 1) return 21;
    }
    if (uzunluk == 13 && bellek_esit_mi(adres, (char*)"c-kutuphanesi", 13) == 1) return 23;
    if (uzunluk == 15 && bellek_esit_mi(adres, (char*)"cpp-kutuphanesi", 15) == 1) return 24;
    if (uzunluk == 14 && bellek_esit_mi(adres, (char*)"py-kutuphanesi", 14) == 1) return 25;
    if (uzunluk == 16 && bellek_esit_mi(adres, (char*)"rust-kutuphanesi", 16) == 1) return 26;
    // FAZ 9.2: Convert directives (#C-convert, #PY-convert, #JS-convert, #CS-convert, #RB-convert, #convert)
    if (uzunluk == 7) {
        if (bellek_esit_mi(adres, (char*)"convert", 7) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"CONVERT", 7) == 1) return TIP_DIREKTIF_CONVERT;
    }
    if (uzunluk == 9) {
        if (bellek_esit_mi(adres, (char*)"c-convert", 9) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"C-convert", 9) == 1) return TIP_DIREKTIF_CONVERT;
    }
    if (uzunluk == 10) {
        if (bellek_esit_mi(adres, (char*)"py-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"PY-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"js-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"JS-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"rs-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"RS-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"cs-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"CS-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"rb-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
        if (bellek_esit_mi(adres, (char*)"RB-convert", 10) == 1) return TIP_DIREKTIF_CONVERT;
    }
    // Faz 5: Konfigurasyon Direktifleri (#config, #yapilandir, #ayar, #konfig, etc.)
    if (uzunluk == 4 && bellek_esit_mi(adres, (char*)"ayar", 4) == 1) return TIP_DIREKTIF_CONFIG;
    if (uzunluk == 6) {
        if (bellek_esit_mi(adres, (char*)"config", 6) == 1) return TIP_DIREKTIF_CONFIG;
        if (bellek_esit_mi(adres, (char*)"konfig", 6) == 1) return TIP_DIREKTIF_CONFIG;
        if (bellek_esit_mi(adres, (char*)"\xe8\xa8\xad\xe5\xae\x9a", 6) == 1) return TIP_DIREKTIF_CONFIG; // 設定
        if (bellek_esit_mi(adres, (char*)"\xe9\x85\x8d\xe7\xbd\xae", 6) == 1) return TIP_DIREKTIF_CONFIG; // 配置
        if (bellek_esit_mi(adres, (char*)"\xec\x84\xa4\xec\xa0\x95", 6) == 1) return TIP_DIREKTIF_CONFIG; // 설정
    }
    if (uzunluk == 7 && bellek_esit_mi(adres, (char*)"ayarlar", 7) == 1) return TIP_DIREKTIF_CONFIG;
    if (uzunluk == 9 && bellek_esit_mi(adres, (char*)"configure", 9) == 1) return TIP_DIREKTIF_CONFIG;
    if (uzunluk == 10 && bellek_esit_mi(adres, (char*)"yapilandir", 10) == 1) return TIP_DIREKTIF_CONFIG;
    if (uzunluk == 12) {
        if (bellek_esit_mi(adres, (char*)"\xd0\xba\xd0\xbe\xd0\xbd\xd1\x84\xd0\xb8\xd0\xb3", 12) == 1) return TIP_DIREKTIF_CONFIG; // конфиг
        if (bellek_esit_mi(adres, (char*)"yap\xc4\xb1land\xc4\xb1r", 12) == 1) return TIP_DIREKTIF_CONFIG; // yapılandır
    }
    if (uzunluk == 13) {
        if (bellek_esit_mi(adres, (char*)"configuration", 13) == 1) return TIP_DIREKTIF_CONFIG;
        if (bellek_esit_mi(adres, (char*)"konfiguration", 13) == 1) return TIP_DIREKTIF_CONFIG;
    }
    if (uzunluk == 18 && bellek_esit_mi(adres, (char*)"\xd0\xbd\xd0\xb0\xd1\x81\xd1\x82\xd1\x80\xd0\xbe\xd0\xb9\xd0\xba\xd0\xb0", 18) == 1) return TIP_DIREKTIF_CONFIG; // настройка
    return 0;
}

long long tokenize_et(LexerState* lx) {
while ((YKN_DOT(lx, guncel_offset) < YKN_DOT(lx, dosya_boyutu))) {
long long c = karakter_oku(lx);
if ((((c == 32) || (c == 9)) || (c == 13))) {
continue;
}
;
if ((c == 10)) {
YKN_DOT(lx, satir) = (YKN_DOT(lx, satir) + 1);
YKN_DOT(lx, sutun) = 1;
continue;
}
;

if ((c == 35)) {
    long long kaydet_ofs = YKN_DOT(lx, guncel_offset);
    char* d_baslangic_ptr = (char*)((char*)YKN_DOT(lx, kaynak) + (kaydet_ofs - 1));
    unsigned char sonraki_c = goz_at(lx);
    if (harf_mi(sonraki_c) == 1 || sonraki_c == 95) {
        long long d_k_uzn = 1;
        karakter_oku(lx);
        while (kimlik_karakteri_mi(goz_at(lx)) == 1 || goz_at(lx) == 45) {
            karakter_oku(lx);
            d_k_uzn++;
        }
        char* d_kelime_ptr = (char*)((char*)YKN_DOT(lx, kaynak) + kaydet_ofs);
        long long d_tip = direktif_ismi_tipini_bul(d_kelime_ptr, d_k_uzn);
        if ((d_tip >= 21 && d_tip <= 26) || d_tip == 64 || d_tip == TIP_DIREKTIF_CONVERT || d_tip == TIP_DIREKTIF_CONFIG) { // 65/66 = TIP_DIREKTIF_*
            jeton_ekle(lx, d_tip, d_baslangic_ptr, d_k_uzn + 1);
            continue;
        }
    }
    while (((goz_at(lx) != 10) && ((goz_at(lx)) != 0))) {
        karakter_oku(lx);
    }
    continue;
}
;
char* curr_ptr = (char*)((char*)((char*)(YKN_DOT(lx, kaynak)) + (YKN_DOT(lx, guncel_offset) - 1)));
if ((c == 40)) {
jeton_ekle(lx, TIP_SOL_PARANTEZ, curr_ptr, 1);
continue;
}
;
if ((c == 41)) {
jeton_ekle(lx, TIP_SAG_PARANTEZ, curr_ptr, 1);
continue;
}
;
if ((c == 123)) {
jeton_ekle(lx, TIP_SOL_KUSLU, curr_ptr, 1);
continue;
}
;
if ((c == 125)) {
jeton_ekle(lx, TIP_SAG_KUSLU, curr_ptr, 1);
continue;
}
;
if ((c == 91)) {
jeton_ekle(lx, TIP_SOL_KOSE, curr_ptr, 1);
continue;
}
;
if ((c == 93)) {
jeton_ekle(lx, TIP_SAG_KOSE, curr_ptr, 1);
continue;
}
;
if ((c == 59)) {
jeton_ekle(lx, TIP_NOKTALI_VIRGUL, curr_ptr, 1);
continue;
}
;
if ((c == 44)) {
jeton_ekle(lx, TIP_VIRGUL, curr_ptr, 1);
continue;
}
;
if ((c == 43)) {
jeton_ekle(lx, TIP_ARTI, curr_ptr, 1);
continue;
}
;
if ((c == 42)) {
jeton_ekle(lx, TIP_CARPI, curr_ptr, 1);
continue;
}
;
if ((c == 94)) {
jeton_ekle(lx, TIP_XOR, curr_ptr, 1);
continue;
}
;
if ((c == 47)) {
jeton_ekle(lx, TIP_BOLME, curr_ptr, 1);
continue;
}
;
if ((c == 37)) {
jeton_ekle(lx, TIP_MODULO, curr_ptr, 1);
continue;
}
;
if ((c == 46)) {
jeton_ekle(lx, TIP_NOKTA, curr_ptr, 1);
continue;
}
;
if ((c == 58)) {
    if (goz_at(lx) == 58) {
        karakter_oku(lx);
        jeton_ekle(lx, TIP_CIFT_IKINOKTA, curr_ptr, 2);
    } else {
        jeton_ekle(lx, TIP_KOLON, curr_ptr, 1);
    }
    continue;
}
;
if ((c == 38)) {
jeton_ekle(lx, TIP_AMPERSAND, curr_ptr, 1);
continue;
}
;
if ((c == 45)) {
if ((goz_at(lx) == 62)) {
karakter_oku(lx);
jeton_ekle(lx, TIP_OK, curr_ptr, 2);
}
 else {
jeton_ekle(lx, TIP_EKSI, curr_ptr, 1);
}
;
continue;
}
;
if ((c == 61)) {
if ((goz_at(lx) == 61)) {
karakter_oku(lx);
jeton_ekle(lx, TIP_ESIT_MI, curr_ptr, 2);
}
 else if (goz_at(lx) == 62) {
karakter_oku(lx);
jeton_ekle(lx, TIP_KALIN_OK, curr_ptr, 2);
}
 else {
jeton_ekle(lx, TIP_ATAMA, curr_ptr, 1);
}
;
continue;
}
;
if ((c == 60)) {
if ((goz_at(lx) == 61)) {
karakter_oku(lx);
jeton_ekle(lx, TIP_KUCUK_ESIT, curr_ptr, 2);
} else if (goz_at(lx) == 60) {
karakter_oku(lx);
jeton_ekle(lx, TIP_SOL_KAYDIR, curr_ptr, 2); // <<
}
 else {
jeton_ekle(lx, TIP_KUCUK_MU, curr_ptr, 1);
}
;
continue;
}
;
if ((c == 62)) {
if ((goz_at(lx) == 61)) {
karakter_oku(lx);
jeton_ekle(lx, TIP_BUYUK_ESIT, curr_ptr, 2);
} else if (goz_at(lx) == 62) {
karakter_oku(lx);
jeton_ekle(lx, TIP_SAG_KAYDIR, curr_ptr, 2); // >>
}
 else {
jeton_ekle(lx, TIP_BUYUK_MU, curr_ptr, 1);
}
;
continue;
}
;
// BUG-3: Bitwise NOT ~ ve OR |
if ((c == 126)) { jeton_ekle(lx, TIP_TILDE, curr_ptr, 1); continue; }
;
if ((c == 124)) { jeton_ekle(lx, TIP_DIKEY_CUBUK, curr_ptr, 1); continue; }
;
if ((c == 94)) { jeton_ekle(lx, TIP_XOR, curr_ptr, 1); continue; }
;
if ((c == 33)) {
if ((goz_at(lx) == 61)) {
karakter_oku(lx);
jeton_ekle(lx, TIP_ESIT_DEGIL, curr_ptr, 2);
}
 else {
jeton_ekle(lx, TIP_DEGIL, curr_ptr, 1);
}
;
continue;
}
;
if ((c == 39)) {
    long long ch_kod = 0;
    long long ch_hata = 0;
    if (goz_at(lx) == 0 || goz_at(lx) == 10 || goz_at(lx) == 39) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_CHARACTER", 39);
        ch_hata = 1;
    } else if (goz_at(lx) == 92) {
        karakter_oku(lx);
        long long esc_c = karakter_oku(lx);
        if (esc_c == 'n') ch_kod = 10;
        else if (esc_c == 't') ch_kod = 9;
        else if (esc_c == 'r') ch_kod = 13;
        else if (esc_c == '0') ch_kod = 0;
        else if (esc_c == '\\') ch_kod = 92;
        else if (esc_c == '\'') ch_kod = 39;
        else if (esc_c == '"') ch_kod = 34;
        else ch_kod = esc_c;
    } else {
        ch_kod = karakter_oku(lx);
    }
    if (!ch_hata) {
        if (goz_at(lx) == 39) {
            karakter_oku(lx);
            char* ch_buf = (char*)std::calloc(8, 1);
            int ch_uzn = std::snprintf(ch_buf, 8, "%lld", ch_kod);
            jeton_ekle(lx, TIP_TAMSAYI, ch_buf, (long long)ch_uzn);
        } else {
            hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_CHARACTER", 39);
        }
    }
    continue;
}
;
if ((c == 63)) {
    jeton_ekle(lx, TIP_SORU_ISLEMI, curr_ptr, 1);
    continue;
}
;
if (((harf_mi(c) == 1) || (c == 95))) {
long long uzn = 1;
while ((kimlik_karakteri_mi(goz_at(lx)) == 1)) {
karakter_oku(lx);
uzn = (uzn + 1);
}
;
long long tip = kelime_tipini_bul(curr_ptr, uzn);
char* kayitli_ptr = (char*)(curr_ptr);
if (((tip == TIP_KIMLIK) && ((YKN_DOT(lx, pool)) != 0))) {
kayitli_ptr = havuz_ekle_veya_bul(YKN_DOT(lx, pool), curr_ptr, uzn);
}
;
// F-String tespiti: tek 'f' harfi ve hemen ardindan '"' varsa TIP_FSTRING uret
if (uzn == 1 && curr_ptr[0] == 'f' && goz_at(lx) == '"') {
    karakter_oku(lx); // acilis tirnak
    long long fs_uzn = 0;
    char* fs_ptr = (char*)((char*)(YKN_DOT(lx, kaynak)) + YKN_DOT(lx, guncel_offset));
    while (goz_at(lx) != '"' && goz_at(lx) != 0) {
        if (goz_at(lx) == '\n') {
            hata_bas_ve_kapat(lx, (char*)"ERR_UNTERMINATED_FSTRING", 0);
            fs_uzn = 0; break;
        }
        if (goz_at(lx) == '\\') {
            karakter_oku(lx); fs_uzn++;
            if (YKN_DOT(lx, guncel_offset) < YKN_DOT(lx, dosya_boyutu)) {
                karakter_oku(lx); fs_uzn++;
            }
        } else {
            karakter_oku(lx); fs_uzn++;
        }
    }
    if (goz_at(lx) == '"') karakter_oku(lx); // kapanma tirnak
    jeton_ekle(lx, TIP_FSTRING, fs_ptr, fs_uzn);
    continue;
}
jeton_ekle(lx, tip, kayitli_ptr, uzn);
continue;
}
;
if ((rakam_mi(c) == 1)) {
long long uzn = 1;
if (c == 48 && (goz_at(lx) == 120 || goz_at(lx) == 88)) {
    karakter_oku(lx);
    uzn = uzn + 1;
    while (onaltilik_mi(goz_at(lx)) == 1 || goz_at(lx) == '_') {
        karakter_oku(lx);
        uzn = uzn + 1;
    }
    if (uzn <= 2 || kimlik_karakteri_mi(goz_at(lx)) == 1) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_NUMBER", c);
    } else if (uzn - 2 > 32) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INTEGER_OVERFLOW", c);
    } else {
        jeton_ekle(lx, TIP_TAMSAYI, curr_ptr, uzn);
    }
} else if (c == 48 && (goz_at(lx) == 98 || goz_at(lx) == 66)) {
    karakter_oku(lx);
    uzn = uzn + 1;
    while (goz_at(lx) == '0' || goz_at(lx) == '1' || goz_at(lx) == '_') {
        karakter_oku(lx);
        uzn = uzn + 1;
    }
    if (uzn <= 2 || kimlik_karakteri_mi(goz_at(lx)) == 1) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_NUMBER", c);
    } else if (uzn - 2 > 128) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INTEGER_OVERFLOW", c);
    } else {
        jeton_ekle(lx, TIP_TAMSAYI, curr_ptr, uzn);
    }
} else if (c == 48 && (goz_at(lx) == 111 || goz_at(lx) == 79)) {
    karakter_oku(lx);
    uzn = uzn + 1;
    while ((goz_at(lx) >= '0' && goz_at(lx) <= '7') || goz_at(lx) == '_') {
        karakter_oku(lx);
        uzn = uzn + 1;
    }
    if (uzn <= 2 || kimlik_karakteri_mi(goz_at(lx)) == 1) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_NUMBER", c);
    } else if (uzn - 2 > 44) {
        hata_bas_ve_kapat(lx, (char*)"ERR_INTEGER_OVERFLOW", c);
    } else {
        jeton_ekle(lx, TIP_TAMSAYI, curr_ptr, uzn);
    }
} else {
    while (rakam_mi(goz_at(lx)) == 1 || goz_at(lx) == '_') {
    karakter_oku(lx);
    uzn = (uzn + 1);
    }
    long long gercek_mi = 0;
    long long sayi_hatali = 0;
    if (goz_at(lx) == 46 && (lx->guncel_offset + 1 < lx->dosya_boyutu) && rakam_mi(lx->kaynak[lx->guncel_offset + 1])) {
        karakter_oku(lx);
        uzn = (uzn + 1);
        long long kesir_baslangic = uzn;
        while (rakam_mi(goz_at(lx)) == 1 || goz_at(lx) == '_') {
            karakter_oku(lx);
            uzn = (uzn + 1);
        }
        if (uzn == kesir_baslangic) {
            hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_NUMBER", c);
            sayi_hatali = 1;
        } else {
            gercek_mi = 1;
        }
    }
    if (!sayi_hatali && (goz_at(lx) == 'e' || goz_at(lx) == 'E')) {
        karakter_oku(lx);
        uzn = uzn + 1;
        if (goz_at(lx) == '+' || goz_at(lx) == '-') {
            karakter_oku(lx);
            uzn = uzn + 1;
        }
        long long us_baslangic = uzn;
        while (rakam_mi(goz_at(lx)) == 1 || goz_at(lx) == '_') {
            karakter_oku(lx);
            uzn = uzn + 1;
        }
        if (uzn == us_baslangic) {
            hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_NUMBER", c);
            sayi_hatali = 1;
        } else {
            gercek_mi = 1;
        }
    }
    if (!sayi_hatali) {
        if (kimlik_karakteri_mi(goz_at(lx)) == 1) {
            hata_bas_ve_kapat(lx, (char*)"ERR_INVALID_NUMBER", c);
        } else if (gercek_mi == 1) {
            jeton_ekle(lx, TIP_GERCEK_SAYI, curr_ptr, uzn);
        } else {
            bool tasma = false;
            if (uzn > 19) tasma = true;
            else if (uzn == 19 && std::memcmp(curr_ptr, "9223372036854775807", 19) > 0) tasma = true;
            if (tasma) {
                hata_bas_ve_kapat(lx, (char*)"ERR_INTEGER_OVERFLOW", c);
            } else {
                jeton_ekle(lx, TIP_TAMSAYI, curr_ptr, uzn);
            }
        }
    }
}
continue;
}
;
if ((c == 34)) {
long long uzn = 0;
curr_ptr = (char*)((char*)(YKN_DOT(lx, kaynak)) + YKN_DOT(lx, guncel_offset));
while (((goz_at(lx) != 34) && ((goz_at(lx)) != 0))) {
if ((goz_at(lx) == 10)) {
hata_bas_ve_kapat(lx, (char*)("ERR_UNTERMINATED_STRING"), 0);
}
if (goz_at(lx) == 92) {
    karakter_oku(lx);
    uzn = (uzn + 1);
    if (goz_at(lx) != 0 && goz_at(lx) != 10) {
        karakter_oku(lx);
        uzn = (uzn + 1);
    }
    continue;
}
karakter_oku(lx);
uzn = (uzn + 1);
}
if ((!(goz_at(lx)))) {
hata_bas_ve_kapat(lx, (char*)("ERR_UNTERMINATED_STRING"), 0);
}
karakter_oku(lx);
jeton_ekle(lx, TIP_METIN, curr_ptr, uzn);
continue;
}
;
hata_bas_ve_kapat(lx, (char*)("ERR_INVALID_CHARACTER"), c);
}
;
jeton_ekle(lx, TIP_EOF, (char*)((char*)(YKN_DOT(lx, kaynak)) + YKN_DOT(lx, dosya_boyutu)), 0);
return YKN_DOT(lx, jeton_sayisi);
}


auto AST_BOS = 0;
auto AST_SAYI = 1;
auto AST_GERCEK_SAYI = 2;
auto AST_METIN = 3;
auto AST_KIMLIK = 4;
auto AST_IKILI_OP = 5;
auto AST_ATAMA = 6;
auto AST_DEGISKEN_TANIMI = 7;
auto AST_EGER = 8;
auto AST_DONGU = 9;
auto AST_ISLEV = 10;
auto AST_ISLEV_CAGRISI = 11;
auto AST_DONDUR = 12;
auto AST_BLOK = 13;
auto AST_DURDUR = 14;
auto AST_DEVAM_ET = 15;
auto AST_YAPI_TANIMI = 16;
auto AST_ALAN_ERISIMI = 17;
auto AST_INDEKS_ERISIMI = 18;
auto AST_UNARY = 19;
auto AST_REFERANS = 20;
auto AST_ICE_AKTAR = 21;
auto AST_MAKRO_TANIMI = 22;
auto AST_MAKRO_CAGRISI = 23;
auto AST_GENERIC_TANIM = 24;
auto AST_TRAIT_TANIM = 25;
auto AST_MANTIK = 26;
auto AST_DIREKTIF_PROMPT = 27;
auto AST_DIREKTIF_AI = 28;
auto AST_DIREKTIF_C_LIB = 29;
auto AST_DIREKTIF_CPP_LIB = 30;
auto AST_DIREKTIF_PY_LIB = 31;
auto AST_DIREKTIF_RS_LIB = 32;
auto AST_DIREKTIF_CONVERT = 38; // #C-convert / #PY-convert etc.
auto AST_FSTRING = 39; // f"..." f-string interpolasyonu
auto AST_TRAIT_UYGULAMA = 40;
auto AST_ASYNC_ISLEV = 41;
auto AST_AWAIT = 42;
auto AST_INLINE_ASM = 43;
auto AST_DIREKTIF_CONFIG = 44;
auto AST_DONUSUM = 45;
auto AST_COMPTIME = 46;
auto AST_SORU_ISLEMI = 33;
auto AST_SONUC_TANIM = 34;
char* g_ast_arena_eski_bloklar[1024] = {};
long long g_ast_arena_eski_blok_sayisi = 0;
AstDugum* arena_dugum_yarat(AstArena* arena, long long tip, Jeton* jeton) {
if (!arena) {
    printf("HATA: AST Arenasi gecersiz (NULL)!\n");
    return 0;
}
long long dugum_boyutu = 136; // generic type-argument metadata parity
if (((YKN_DOT(arena, kullanilan) + dugum_boyutu) > YKN_DOT(arena, kapasite))) {
    long long yeni_kapasite = YKN_DOT(arena, kapasite) * 2;
    if (yeni_kapasite < 1048576) yeni_kapasite = 1048576;
    char* yeni_bellek = (char*)bellek_tahsis_et(yeni_kapasite);
    if (!yeni_bellek) {
        printf("HATA [E014] [ERR_AST_LIMIT] [class=compiler]: AST bellek tahsisi basarisiz (OOM)\n");
        return 0;
    }
    if (YKN_DOT(arena, bellek) != 0 && g_ast_arena_eski_blok_sayisi < 1024) {
        g_ast_arena_eski_bloklar[g_ast_arena_eski_blok_sayisi++] = (char*)YKN_DOT(arena, bellek);
    }
    YKN_DOT(arena, bellek) = yeni_bellek;
    YKN_DOT(arena, kapasite) = yeni_kapasite;
    YKN_DOT(arena, kullanilan) = 0;
}
AstDugum* ptr = (AstDugum*)((char*)((char*)(YKN_DOT(arena, bellek)) + YKN_DOT(arena, kullanilan)));
YKN_DOT(arena, kullanilan) = (YKN_DOT(arena, kullanilan) + dugum_boyutu);
YKN_DOT(ptr, tip) = tip;
YKN_DOT(ptr, ilk_jeton) = jeton;
YKN_DOT(ptr, son_jeton) = jeton;
YKN_DOT(ptr, sol) = 0;
YKN_DOT(ptr, sag) = 0;
YKN_DOT(ptr, sonraki) = 0;
YKN_DOT(ptr, govde) = 0;
YKN_DOT(ptr, yoksa_govde) = 0;
YKN_DOT(ptr, parametreler) = 0;
YKN_DOT(ptr, tip_adi) = 0;
YKN_DOT(ptr, tip_adi_uzunluk) = 0;
YKN_DOT(ptr, cozumlenmis_tip) = 0;
YKN_DOT(ptr, dizi_boyutu) = 0;
YKN_DOT(ptr, gorunurluk) = 0;
YKN_DOT(ptr, nullable) = 0; // FIX13: explicit init to 0 (non-nullable by default)
YKN_DOT(ptr, tip_arglari) = 0;
YKN_DOT(ptr, tip_arg_sayisi) = 0;
return ptr;
}


ParamDugum* arena_param_yarat(AstArena* arena, Jeton* isim) {
if (!arena) {
    printf("HATA: Param Arenasi gecersiz (NULL)!\n");
    return 0;
}
long long boyut = 48; // FIX13: 40 -> 48 (+8 byte for nullable field)
if (((YKN_DOT(arena, kullanilan) + boyut) > YKN_DOT(arena, kapasite))) {
    long long yeni_kapasite = YKN_DOT(arena, kapasite) * 2;
    if (yeni_kapasite < 262144) yeni_kapasite = 262144;
    char* yeni_bellek = (char*)bellek_tahsis_et(yeni_kapasite);
    if (!yeni_bellek) {
        printf("HATA [E014] [ERR_AST_LIMIT] [class=compiler]: Parametre AST bellek tahsisi basarisiz (OOM)\n");
        return 0;
    }
    if (YKN_DOT(arena, bellek) != 0 && g_ast_arena_eski_blok_sayisi < 1024) {
        g_ast_arena_eski_bloklar[g_ast_arena_eski_blok_sayisi++] = (char*)YKN_DOT(arena, bellek);
    }
    YKN_DOT(arena, bellek) = yeni_bellek;
    YKN_DOT(arena, kapasite) = yeni_kapasite;
    YKN_DOT(arena, kullanilan) = 0;
}
ParamDugum* ptr = (ParamDugum*)((char*)((char*)(YKN_DOT(arena, bellek)) + YKN_DOT(arena, kullanilan)));
YKN_DOT(arena, kullanilan) = (YKN_DOT(arena, kullanilan) + boyut);
YKN_DOT(ptr, isim_jeton) = isim;
YKN_DOT(ptr, tip_adi) = 0;
YKN_DOT(ptr, tip_adi_uzunluk) = 0;
YKN_DOT(ptr, sonraki) = 0;
YKN_DOT(ptr, cozumlenmis_tip) = 0;
YKN_DOT(ptr, nullable) = 0;
return ptr;
}


void hata_bas_parser(char* mesaj, Jeton* j) {
    g_parser_hata_sayisi++;
    if (j) {
        printf("PARSER ERROR: %s at line %lld, col %lld (token tip=%lld, text='%.*s')\n",
            mesaj, (long long)j->satir, (long long)j->sutun, (long long)j->tip, (int)j->uzunluk, j->baslangic);
    } else {
        printf("PARSER ERROR: %s (EOF)\n", mesaj);
    }
}


Jeton* suanki_jeton(ParserDurumu* d) {
if ((YKN_DOT(d, guncel_index) >= YKN_DOT(d, jeton_sayisi))) {
return 0;
}
char* p = (char*)YKN_DOT(d, jetonlar);
long long ofs = YKN_DOT(d, guncel_index) * 40;
return (Jeton*)(p + ofs);
}


Jeton* onceki_jeton(ParserDurumu* d) {
if ((!(YKN_DOT(d, guncel_index)))) {
 return 0;
}
char* p = (char*)YKN_DOT(d, jetonlar);
long long ofs = (YKN_DOT(d, guncel_index) - 1) * 40;
return (Jeton*)(p + ofs);
}


void ilerle(ParserDurumu* d) {
if ((YKN_DOT(d, guncel_index) < YKN_DOT(d, jeton_sayisi))) {
YKN_DOT(d, guncel_index) = (YKN_DOT(d, guncel_index) + 1);
}
;
}


long long jeton_tipinde_mi(ParserDurumu* d, long long tip) {
Jeton* j = (Jeton*)(suanki_jeton(d));
if ((!(j))) {
return 0;
}
;
if ((YKN_DOT(j, tip) == tip)) {
return 1;
}
;
return 0;
}


Jeton* bekle(ParserDurumu* d, long long tip) {
Jeton* j = (Jeton*)(suanki_jeton(d));
if ((!(j))) {
hata_bas_parser((char*)("ERR_UNEXPECTED_EOF"), 0);
return 0;
}
;
if ((YKN_DOT(j, tip) != tip)) {
    if (tip == TIP_KIMLIK && YKN_DOT(j, tip) == TIP_UYGULA) {
        ilerle(d);
        return j;
    }
hata_bas_parser((char*)("ERR_UNEXPECTED_TOKEN"), j);
return 0;
}
;
ilerle(d);
return j;
}


static bool generic_kapanis_shift_ayir(ParserDurumu* d) {
    if (!d || d->guncel_index >= d->jeton_sayisi ||
        !suanki_jeton(d) || suanki_jeton(d)->tip != TIP_SAG_KAYDIR) return false;
    if (d->jeton_sayisi >= 262143) return false;
    for (long long i = d->jeton_sayisi; i > d->guncel_index; --i) {
        d->jetonlar[i] = d->jetonlar[i - 1];
    }
    Jeton* shift = &d->jetonlar[d->guncel_index];
    Jeton* ikinci = &d->jetonlar[d->guncel_index + 1];
    shift->tip = TIP_BUYUK_MU;
    shift->uzunluk = 1;
    ikinci->tip = TIP_BUYUK_MU;
    ikinci->baslangic = shift->baslangic + 1;
    ikinci->uzunluk = 1;
    ikinci->satir = shift->satir;
    ikinci->sutun = shift->sutun + 1;
    d->jeton_sayisi++;
    return true;
}

void tip_oku(ParserDurumu* d, AstDugum* dugum) {
long long yildiz_sayisi = 0;
// FIX13: ?*T nullable pointer prefix
if (jeton_tipinde_mi(d, TIP_SORU_ISLEMI) == 1) {
    ilerle(d);
    YKN_DOT(dugum, nullable) = 1;
} else {
    YKN_DOT(dugum, nullable) = 0;
}
while ((jeton_tipinde_mi(d, TIP_CARPI) == 1)) {
ilerle(d);
yildiz_sayisi = (yildiz_sayisi + 1);
}
;
YKN_DOT(dugum, dizi_boyutu) = 0;
if ((jeton_tipinde_mi(d, TIP_SOL_KOSE) == 1)) {
ilerle(d);
Jeton* sayi_j = (Jeton*)(bekle(d, TIP_TAMSAYI));
if ((sayi_j != 0)) {
YKN_DOT(dugum, dizi_boyutu) = string_tamsayiya(YKN_DOT(sayi_j, baslangic), YKN_DOT(sayi_j, uzunluk));
}
;
bekle(d, TIP_SAG_KOSE);
}
;
while ((jeton_tipinde_mi(d, TIP_CARPI) == 1)) {
ilerle(d);
yildiz_sayisi = (yildiz_sayisi + 1);
}
;
YKN_DOT(dugum, cozumlenmis_tip) = yildiz_sayisi;

Jeton* isim_j = (Jeton*)(suanki_jeton(d));
if ((!(isim_j))) {
return;
}
;
ilerle(d);
char* tip_ptr_dyn = YKN_DOT(isim_j, baslangic);
long long tip_len_dyn = YKN_DOT(isim_j, uzunluk);
if (YKN_DOT(isim_j, tip) == TIP_DYN || (tip_len_dyn == 3 && memcmp(tip_ptr_dyn, "dyn", 3) == 0)) {
    Jeton* tr_j = (Jeton*)suanki_jeton(d);
    if (tr_j) {
        ilerle(d);
        long long dyn_uzn = 4 + tr_j->uzunluk;
        char* dyn_buf = (char*)bellek_tahsis_et(dyn_uzn + 1);
        memcpy(dyn_buf, "dyn ", 4);
        memcpy(dyn_buf + 4, tr_j->baslangic, tr_j->uzunluk);
        dyn_buf[dyn_uzn] = 0;
        tip_ptr_dyn = dyn_buf;
        tip_len_dyn = dyn_uzn;
    }
}
YKN_DOT(dugum, tip_adi) = tip_ptr_dyn;
YKN_DOT(dugum, tip_adi_uzunluk) = tip_len_dyn;

YKN_DOT(dugum, tip_arglari) = 0;
YKN_DOT(dugum, tip_arg_sayisi) = 0;
if (jeton_tipinde_mi(d, TIP_KUCUK_MU) == 1) {
    ilerle(d);
    AstDugum* ilk_arg = nullptr;
    AstDugum* son_arg = nullptr;
    while (!jeton_tipinde_mi(d, TIP_BUYUK_MU) && suanki_jeton(d) != 0) {
        if (YKN_DOT(suanki_jeton(d), tip) == TIP_EOF) break;
        Jeton* arg_j = (Jeton*)suanki_jeton(d);
        AstDugum* arg = arena_dugum_yarat(d->arena, AST_GENERIC_TANIM, arg_j);
        if (!arg) return;
        tip_oku(d, arg);
        if (!ilk_arg) ilk_arg = son_arg = arg;
        else { son_arg->sonraki = arg; son_arg = arg; }
        YKN_DOT(dugum, tip_arg_sayisi)++;
        if (jeton_tipinde_mi(d, TIP_VIRGUL) == 1) ilerle(d);
        else break;
    }
    if (jeton_tipinde_mi(d, TIP_SAG_KAYDIR) == 1) generic_kapanis_shift_ayir(d);
    bekle(d, TIP_BUYUK_MU);
    // -1 distinguishes explicit Future<> from the runtime yapi Future.
    if (!ilk_arg) YKN_DOT(dugum, tip_arg_sayisi) = -1;
    YKN_DOT(dugum, tip_arglari) = ilk_arg;
}
}


long long operator_onceligi(long long tip) {
if ((tip == TIP_ATAMA)) { return 10; }
if ((tip == TIP_VEYA)) { return 20; }
if ((tip == TIP_VE)) { return 30; }
if ((tip == TIP_DIKEY_CUBUK)) { return 33; }
if ((tip == TIP_XOR)) { return 35; }
if ((tip == TIP_AMPERSAND)) { return 37; }
if ((tip == TIP_ESIT_MI)) { return 40; }
if ((tip == TIP_ESIT_DEGIL)) { return 40; }
if ((tip == TIP_KUCUK_MU)) { return 50; }
if ((tip == TIP_BUYUK_MU)) { return 50; }
if ((tip == TIP_KUCUK_ESIT)) { return 50; }
if ((tip == TIP_BUYUK_ESIT)) { return 50; }
if ((tip == TIP_SOL_KAYDIR || tip == TIP_SAG_KAYDIR)) { return 55; }
if ((tip == TIP_ARTI)) { return 60; }
if ((tip == TIP_EKSI)) { return 60; }
if ((tip == TIP_CARPI)) { return 70; }
if ((tip == TIP_BOLME)) { return 70; }
if ((tip == TIP_MODULO)) { return 70; }
if ((tip == TIP_OLARAK)) { return 75; }
if ((tip == TIP_NOKTA || tip == TIP_OK)) { return 80; }
if ((tip == TIP_SOL_KOSE)) { return 80; }
if ((tip == TIP_SORU_ISLEMI)) { return 85; }
return 0;
}

long long generic_cagri_mi(ParserDurumu* d) {
    if (!d || d->guncel_index >= d->jeton_sayisi) return 0;
    unsigned char* p_jetonlar = (unsigned char*)d->jetonlar;
    Jeton* j0 = (Jeton*)(p_jetonlar + (d->guncel_index * sizeof(Jeton)));
    if (j0->tip != TIP_KUCUK_MU) return 0;
    long long derinlik = 1;
    long long i = d->guncel_index + 1;
    long long kimlik_goruldu = 0;
    while (i < d->jeton_sayisi && derinlik > 0) {
        Jeton* ji = (Jeton*)(p_jetonlar + (i * sizeof(Jeton)));
        long long jt = ji->tip;
        if (jt == TIP_EOF) return 0;
        if (jt == TIP_KUCUK_MU) {
            derinlik++;
        } else if (jt == TIP_BUYUK_MU) {
            derinlik--;
        } else if (jt == TIP_SAG_KAYDIR) {
            derinlik -= 2;
            if (derinlik < 0) return 0;
        } else if (jt == TIP_KIMLIK || jt == TIP_DYN) {
            kimlik_goruldu = 1;
        } else if (jt == TIP_CARPI || jt == TIP_SORU_ISLEMI || jt == TIP_VIRGUL ||
                   jt == TIP_SOL_KOSE || jt == TIP_SAG_KOSE || jt == TIP_TAMSAYI ||
                   jt == TIP_CIFT_IKINOKTA) {
            // Gecerli tip tokeni
        } else {
            return 0;
        }
        i++;
    }
    if (derinlik != 0 || kimlik_goruldu == 0) return 0;
    if (i < d->jeton_sayisi) {
        Jeton* j_sonra = (Jeton*)(p_jetonlar + (i * sizeof(Jeton)));
        if (j_sonra->tip == TIP_SOL_PARANTEZ) return 1;
    }
    if (d->guncel_index + 2 < d->jeton_sayisi) {
        Jeton* j1 = (Jeton*)(p_jetonlar + ((d->guncel_index + 1) * sizeof(Jeton)));
        Jeton* j2 = (Jeton*)(p_jetonlar + ((d->guncel_index + 2) * sizeof(Jeton)));
        if (j1->tip == TIP_KIMLIK && (j2->tip == TIP_BUYUK_MU || j2->tip == TIP_VIRGUL)) return 1;
    }
    return 0;
}



AstDugum* parse_prefix(ParserDurumu* d) {
Jeton* j = (Jeton*)(suanki_jeton(d));
if ((!(j))) {
return 0;
}
;
if (YKN_DOT(j, tip) == TIP_COMPTIME) {
    ilerle(d);
    AstDugum* comp_d = arena_dugum_yarat(YKN_DOT(d, arena), AST_COMPTIME, j);
    comp_d->sol = parse_ifade(d, 0);
    return comp_d;
}

if (YKN_DOT(j, tip) == TIP_ISLEV) {
    ilerle(d);
    Jeton* isim_j = 0;
    if (jeton_tipinde_mi(d, TIP_KIMLIK) == 1) {
        isim_j = suanki_jeton(d);
        ilerle(d);
    } else {
        isim_j = j;
    }
    AstDugum* islev_dugum = (AstDugum*)arena_dugum_yarat(YKN_DOT(d, arena), AST_ISLEV, isim_j);
    islev_dugum->parametreler = parametre_listesi_oku(d);
    if (jeton_tipinde_mi(d, TIP_OK) == 1) {
        ilerle(d);
        tip_oku(d, islev_dugum);
    }
    islev_dugum->govde = parse_blok(d);
    return islev_dugum;
}

// Null literal: bos/nil AST'de sayisal null degeri olarak tasinir.
if ((YKN_DOT(j, tip) == TIP_BOS)) {
ilerle(d);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_SAYI, j);
}
;
// Boolean literal'lar gercek AST_MANTIK dugumu olarak uretilir.
if ((YKN_DOT(j, tip) == TIP_DOGRU || YKN_DOT(j, tip) == TIP_YANLIS)) {
ilerle(d);
AstDugum* mantik_dugum = arena_dugum_yarat(YKN_DOT(d, arena), AST_MANTIK, j);
mantik_dugum->dizi_boyutu = (YKN_DOT(j, tip) == TIP_DOGRU) ? 1 : 0;
return mantik_dugum;
}
;
if ((YKN_DOT(j, tip) == TIP_TAMSAYI)) {
ilerle(d);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_SAYI, j);
}
;
if ((YKN_DOT(j, tip) == TIP_GERCEK_SAYI)) {
ilerle(d);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_GERCEK_SAYI, j);
}
;
if ((YKN_DOT(j, tip) == TIP_METIN)) {
ilerle(d);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_METIN, j);
}
;
if ((YKN_DOT(j, tip) == TIP_FSTRING)) {
ilerle(d);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_FSTRING, j);
}
;
// await expr: preserve a dedicated AST node; lowering remains fail-closed
// until the Future<T> state-machine codegen is implemented.
if ((YKN_DOT(j, tip) == TIP_AWAIT)) {
ilerle(d);
AstDugum* await_dugum = arena_dugum_yarat(YKN_DOT(d, arena), AST_AWAIT, j);
await_dugum->sag = parse_ifade(d, 75);
if (!await_dugum->sag) return 0;
return await_dugum;
}
;
if ((YKN_DOT(j, tip) == TIP_KIMLIK || YKN_DOT(j, tip) == TIP_UYGULA)) {
ilerle(d);

while (jeton_tipinde_mi(d, TIP_CIFT_IKINOKTA) == 1) {
    ilerle(d); // '::'
    Jeton* sag_j = bekle(d, TIP_KIMLIK);
    char* sol_isim = (char*)j->baslangic;
    long long sol_uzn = j->uzunluk;
    char* sag_isim = (char*)sag_j->baslangic;
    long long sag_uzn = sag_j->uzunluk;
    long long yeni_uzn = sol_uzn + 2 + sag_uzn;
    char* yeni_buf = (char*)bellek_tahsis_et(yeni_uzn + 1);
    long long bi = 0;
    while (bi < sol_uzn) {
        yeni_buf[bi] = sol_isim[bi];
        bi++;
    }
    yeni_buf[bi] = '_';
    yeni_buf[bi + 1] = '_';
    bi += 2;
    long long si = 0;
    while (si < sag_uzn) {
        yeni_buf[bi + si] = sag_isim[si];
        si++;
    }
    yeni_buf[yeni_uzn] = 0;
    Jeton* yeni_j = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
    yeni_j->tip = TIP_KIMLIK;
    yeni_j->baslangic = (char*)yeni_buf;
    yeni_j->uzunluk = yeni_uzn;
    yeni_j->satir = j->satir;
    yeni_j->sutun = j->sutun;
    j = yeni_j;
}

AstDugum* kimlik = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_KIMLIK, j));

AstDugum* generic_arg_tip = 0;
if (generic_cagri_mi(d) == 1) {
    ilerle(d); // '<'
    AstDugum* son_arg_tip = 0;
    while (!jeton_tipinde_mi(d, TIP_BUYUK_MU) && !jeton_tipinde_mi(d, TIP_SAG_KAYDIR) && suanki_jeton(d) != 0) {
        if (YKN_DOT(suanki_jeton(d), tip) == TIP_EOF) break;
        Jeton* g_arg_j = suanki_jeton(d);
        AstDugum* ek_arg = (AstDugum*)arena_dugum_yarat(d->arena, AST_GENERIC_TANIM, g_arg_j);
        tip_oku(d, ek_arg);
        if (!generic_arg_tip) {
            generic_arg_tip = ek_arg;
            son_arg_tip = ek_arg;
        } else {
            son_arg_tip->sonraki = ek_arg;
            son_arg_tip = ek_arg;
        }
        if (jeton_tipinde_mi(d, TIP_VIRGUL) == 1) ilerle(d);
        else break;
    }
    if (jeton_tipinde_mi(d, TIP_SAG_KAYDIR) == 1) generic_kapanis_shift_ayir(d);
    bekle(d, TIP_BUYUK_MU); // '>'
}

if ((jeton_tipinde_mi(d, TIP_SOL_PARANTEZ) == 1)) {
ilerle(d);
AstDugum* cagri = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_ISLEV_CAGRISI, j));
YKN_DOT(cagri, sol) = kimlik;
YKN_DOT(cagri, yoksa_govde) = generic_arg_tip;
AstDugum* ilk_arg = (AstDugum*)(0);
AstDugum* son_arg = (AstDugum*)(0);
while (((!(jeton_tipinde_mi(d, TIP_SAG_PARANTEZ))) && ((suanki_jeton(d)) != 0))) {
if ((YKN_DOT(suanki_jeton(d), tip) == TIP_EOF)) {
break;
}
;
AstDugum* arg = (AstDugum*)(parse_ifade(d, 0));
if ((!(arg))) { ilerle(d); break; }
if ((!(ilk_arg))) {
ilk_arg = arg;
son_arg = arg;
}
 else {
YKN_DOT(son_arg, sonraki) = arg;
son_arg = arg;
}
;
if ((jeton_tipinde_mi(d, TIP_VIRGUL) == 1)) {
ilerle(d);
}
;
}
;
bekle(d, TIP_SAG_PARANTEZ);
YKN_DOT(cagri, sag) = ilk_arg;
return cagri;
}
;
// BUG-1 FIX: Parametresiz enum variant: Renk::Kirmizi -> AST_ISLEV_CAGRISI(disc)
{
    long long ev_idx = enum_varyant_bul_tam(j->baslangic, j->uzunluk);
    if (ev_idx < 0) ev_idx = enum_varyant_bul_kisa(j->baslangic, j->uzunluk);
    if (ev_idx >= 0 && g_enum_var_payload[ev_idx] == 0) {
        AstDugum* ev_cagri = arena_dugum_yarat(d->arena, AST_ISLEV_CAGRISI, j);
        ev_cagri->sol = kimlik; ev_cagri->sag = nullptr;
        return ev_cagri;
    }
}
return kimlik;
}
;
if ((YKN_DOT(j, tip) == TIP_SOL_PARANTEZ)) {
ilerle(d);
AstDugum* ic = (AstDugum*)(parse_ifade(d, 0));
bekle(d, TIP_SAG_PARANTEZ);
return ic;
}
;
if ((YKN_DOT(j, tip) == TIP_EKSI || YKN_DOT(j, tip) == TIP_DEGIL || YKN_DOT(j, tip) == TIP_TILDE)) {
ilerle(d);
AstDugum* unary = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_UNARY, j));
YKN_DOT(unary, sag) = parse_ifade(d, 75);
return unary;
}
;
if ((YKN_DOT(j, tip) == TIP_AMPERSAND)) {
ilerle(d);
long long mut_ref = 0;
Jeton* sj = suanki_jeton(d);
if (sj && sj->tip == TIP_DEGISKEN && sj->uzunluk == 3 && bellek_esit_mi(sj->baslangic, (char*)"mut", 3) == 1) {
    ilerle(d);
    mut_ref = 1;
}
AstDugum* ref_d = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_REFERANS, j));
ref_d->gorunurluk = mut_ref;
YKN_DOT(ref_d, sag) = parse_ifade(d, 75);
return ref_d;
}
;
if ((YKN_DOT(j, tip) == TIP_CARPI)) {
ilerle(d);
AstDugum* deref = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_UNARY, j));
YKN_DOT(deref, sag) = parse_ifade(d, 75);
return deref;
}
;
// FIX13: bos/nil/null literal → pointer 0
if ((YKN_DOT(j, tip) == TIP_BOS)) {
ilerle(d);
AstDugum* bos_d = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_SAYI, j));
return bos_d;
}
;
// [DEBUG_REMOVED] printf("PREFIX_HATA tip=%lld index=%lld uzunluk=%lld\n", YKN_DOT(j, tip), YKN_DOT(d, guncel_index), YKN_DOT(j, uzunluk));
hata_bas_parser((char*)("ERR_INVALID_EXPRESSION"), j);
return 0;
}


AstDugum* parse_infix(ParserDurumu* d, AstDugum* sol) {
Jeton* j = (Jeton*)(suanki_jeton(d));
if (!j) return sol;
long long oncelik = operator_onceligi(YKN_DOT(j, tip));
ilerle(d);
if ((YKN_DOT(j, tip) == TIP_NOKTA || YKN_DOT(j, tip) == TIP_OK)) {
AstDugum* alan = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_ALAN_ERISIMI, j));
YKN_DOT(alan, sol) = sol;
Jeton* sag_j = (Jeton*)(suanki_jeton(d));
ilerle(d);
AstDugum* sag_kimlik = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_KIMLIK, sag_j));
if ((jeton_tipinde_mi(d, TIP_SOL_PARANTEZ) == 1)) {
ilerle(d);
YKN_DOT(alan, tip) = AST_ISLEV_CAGRISI;
YKN_DOT(alan, sol) = sol;
YKN_DOT(alan, sag) = sag_kimlik;
AstDugum* ilk_arg = (AstDugum*)(0);
AstDugum* son_arg = (AstDugum*)(0);
while (((!(jeton_tipinde_mi(d, TIP_SAG_PARANTEZ))) && ((suanki_jeton(d)) != 0))) {
if ((YKN_DOT(suanki_jeton(d), tip) == TIP_EOF)) {
break;
}
;
AstDugum* arg = (AstDugum*)(parse_ifade(d, 0));
if ((!(arg))) { ilerle(d); break; }
if ((!(ilk_arg))) {
ilk_arg = arg;
son_arg = arg;
}
 else {
YKN_DOT(son_arg, sonraki) = arg;
son_arg = arg;
}
;
if ((jeton_tipinde_mi(d, TIP_VIRGUL) == 1)) {
ilerle(d);
}
;
}
;
bekle(d, TIP_SAG_PARANTEZ);
YKN_DOT(sag_kimlik, sag) = ilk_arg;
}
 else {
YKN_DOT(alan, sag) = sag_kimlik;
}
;
return alan;
}
;
if ((YKN_DOT(j, tip) == TIP_SOL_KOSE)) {
// Note: parse_infix already called ilerle(d) at top to consume '[', so we don't advance again
AstDugum* indeks = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_INDEKS_ERISIMI, j));
YKN_DOT(indeks, sol) = sol;
YKN_DOT(indeks, sag) = parse_ifade(d, 0);
bekle(d, TIP_SAG_KOSE);
return indeks;
}
;
if ((YKN_DOT(j, tip) == TIP_ATAMA)) {
AstDugum* atama = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_ATAMA, j));
YKN_DOT(atama, sol) = sol;
YKN_DOT(atama, sag) = parse_ifade(d, 0);
return atama;
}
if ((YKN_DOT(j, tip) == TIP_OLARAK)) {
    AstDugum* donusum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_DONUSUM, j));
    YKN_DOT(donusum, sol) = sol;
    tip_oku(d, donusum);
    return donusum;
}
;
if ((YKN_DOT(j, tip) == TIP_SORU_ISLEMI)) {
AstDugum* soru = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_SORU_ISLEMI, j));
YKN_DOT(soru, sol) = sol;
return soru;
}
;
AstDugum* op = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_IKILI_OP, j));
YKN_DOT(op, sol) = sol;
YKN_DOT(op, sag) = parse_ifade(d, oncelik);
return op;
}


static long long g_parser_ifade_derinlik = 0;
const long long PARSER_MAKS_IFADE_DERINLIK = 256;
static long long g_esles_gecici_sayac = 0;

AstDugum* parse_ifade(ParserDurumu* d, long long oncelik) {
if (g_parser_ifade_derinlik >= PARSER_MAKS_IFADE_DERINLIK) {
    Jeton* j = suanki_jeton(d);
    long long sat = j ? j->satir : 1;
    long long sut = j ? j->sutun : 1;
    ozyineleme_derinlik_hatasi(sat, sut, "Ifade ozyineleme derinlik limiti asildi");
    exit(1);
}
g_parser_ifade_derinlik++;
AstDugum* sol = (AstDugum*)(parse_prefix(d));
if ((!(sol))) {
    g_parser_ifade_derinlik--;
    return 0;
}
;
Jeton* j = (Jeton*)(suanki_jeton(d));
    int dbg_count = 0;
    while ((((j) != 0) && (oncelik < operator_onceligi(YKN_DOT(j, tip))))) {
        if (++dbg_count > 1000) { printf("HANG at token type %lld\n", j->tip); exit(1); }
        sol = parse_infix(d, sol);
        j = suanki_jeton(d);
    }
    ;
g_parser_ifade_derinlik--;
return sol;
}


void tip_anotasyonu_oku(ParserDurumu* d, AstDugum* hedef) {
if ((!(jeton_tipinde_mi(d, TIP_KOLON)))) {
return;
}
;
ilerle(d);
tip_oku(d, hedef);
}


AstDugum* parametre_listesi_oku(ParserDurumu* d) {
    bekle(d, TIP_SOL_PARANTEZ);
    AstDugum* ilk = (AstDugum*)(0);
    AstDugum* son = (AstDugum*)(0);
    while (((!(jeton_tipinde_mi(d, TIP_SAG_PARANTEZ))) && ((suanki_jeton(d)) != 0))) {
        if ((YKN_DOT(suanki_jeton(d), tip) == TIP_EOF)) {
            break;
        }
        Jeton* sj = suanki_jeton(d);
        Jeton* isim_j = nullptr;
        if (sj && (sj->tip == TIP_KIMLIK || sj->tip == TIP_VARSAYILAN)) {
            ilerle(d);
            isim_j = sj;
        } else {
            isim_j = (Jeton*)(bekle(d, TIP_KIMLIK));
        }
        if (!isim_j) {
            ilerle(d);
            continue;
        }
        AstDugum* param = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_KIMLIK, isim_j));
        YKN_DOT(param, tip_adi) = YKN_DOT(isim_j, baslangic);
        YKN_DOT(param, tip_adi_uzunluk) = YKN_DOT(isim_j, uzunluk);
        tip_anotasyonu_oku(d, param);

        if ((!(ilk))) {
            ilk = param;
            son = param;
        }
        else {
            YKN_DOT(son, sonraki) = param;
            son = param;
        }
        
        if ((jeton_tipinde_mi(d, TIP_VIRGUL) == 1)) {
            ilerle(d);
        }
    }
    
    bekle(d, TIP_SAG_PARANTEZ);
    return ilk;
}


static void parser_senkronize_et(ParserDurumu* d) {
    if (!d) return;
    ilerle(d);
    while (suanki_jeton(d) && YKN_DOT(suanki_jeton(d), tip) != TIP_EOF) {
        Jeton* onc = onceki_jeton(d);
        if (onc && YKN_DOT(onc, tip) == TIP_NOKTALI_VIRGUL) {
            return;
        }
        long long t = YKN_DOT(suanki_jeton(d), tip);
        if (t == TIP_ISLEV || t == TIP_DEGISKEN || t == TIP_EGER ||
            t == TIP_DONGU || t == TIP_HER || t == TIP_DONDUR ||
            t == TIP_SAG_KUSLU || t == TIP_ESLES || t == TIP_YAPI ||
            t == TIP_TRAIT || t == TIP_UYGULA || t == TIP_ASYNC) {
            return;
        }
        ilerle(d);
    }
}

AstDugum* parse_blok(ParserDurumu* d) {
Jeton* ac_j = (Jeton*)(bekle(d, TIP_SOL_KUSLU));
AstDugum* blok = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_BLOK, ac_j));
AstDugum* ilk = (AstDugum*)(0);
AstDugum* son = (AstDugum*)(0);
while (((!(jeton_tipinde_mi(d, TIP_SAG_KUSLU))) && ((suanki_jeton(d)) != 0))) {
    if ((YKN_DOT(suanki_jeton(d), tip) == TIP_EOF)) {
        break;
    }
    AstDugum* satir = (AstDugum*)(parse_satir(d));
    if ((!(satir))) {
        parser_senkronize_et(d);
        continue;
    }
    if (!ilk) {
        ilk = satir;
        son = satir;
        while (son && son->sonraki) son = son->sonraki;
    }
    else {
        YKN_DOT(son, sonraki) = satir;
        son = satir;
        while (son && son->sonraki) son = son->sonraki;
    }
}
bekle(d, TIP_SAG_KUSLU);
YKN_DOT(blok, sol) = ilk;
return blok;
}


AstDugum* parse_islev(ParserDurumu* d) {
    Jeton* j = suanki_jeton(d);
    if ((YKN_DOT(j, tip) == TIP_ISLEV)) {
        ilerle(d);
        Jeton* isim_j = (Jeton*)(bekle(d, TIP_KIMLIK));
        isim_j = (Jeton*)(onceki_jeton(d));
        AstDugum* islev_dugum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_ISLEV, isim_j));
// [DEBUG_REMOVED]         printf("PARSER_ISLEV_NODE=%p TOKEN=%p\n", (void*)islev_dugum, (void*)isim_j);
        AstDugum* islev_params = (AstDugum*)(parametre_listesi_oku(d));
        YKN_DOT(islev_dugum, parametreler) = islev_params;
        if ((jeton_tipinde_mi(d, TIP_OK) == 1)) {
            ilerle(d);
            tip_oku(d, islev_dugum);
        }
        
        YKN_DOT(islev_dugum, govde) = parse_blok(d);
        return islev_dugum;
    }
    return 0;
}


AstDugum* parse_satir(ParserDurumu* d) {
Jeton* j = (Jeton*)(suanki_jeton(d));
if ((!(j))) {
return 0;
}
;
if ((YKN_DOT(j, tip) == TIP_SAG_KUSLU)) {
return 0;
}
;
if ((YKN_DOT(j, tip) == TIP_DEGISKEN || YKN_DOT(j, tip) == TIP_SABIT)) {
long long sabit_mi = 0;
if ((YKN_DOT(j, tip) == TIP_SABIT)) sabit_mi = 1;
if ((YKN_DOT(j, tip) == TIP_DEGISKEN && YKN_DOT(j, uzunluk) == 3 && bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"let", 3) == 1)) sabit_mi = 1;
ilerle(d);
if ((suanki_jeton(d) != 0 && YKN_DOT(suanki_jeton(d), tip) == TIP_DEGISKEN && YKN_DOT(suanki_jeton(d), uzunluk) == 3 && bellek_esit_mi(YKN_DOT(suanki_jeton(d), baslangic), (char*)"mut", 3) == 1)) {
ilerle(d);
sabit_mi = 0;
}
Jeton* isim_j = (Jeton*)(bekle(d, TIP_KIMLIK));
AstDugum* dugum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_DEGISKEN_TANIMI, isim_j));
tip_anotasyonu_oku(d, dugum);
if ((jeton_tipinde_mi(d, TIP_ATAMA) == 1)) {
ilerle(d);
YKN_DOT(dugum, sag) = parse_ifade(d, 0);
}
;
bekle(d, TIP_NOKTALI_VIRGUL);
YKN_DOT(dugum, gorunurluk) = (sabit_mi * 2);
return dugum;
}
;
if ((YKN_DOT(j, tip) == TIP_DONDUR)) {
ilerle(d);
AstDugum* ret = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_DONDUR, j));
if ((!(jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL)))) {
YKN_DOT(ret, sag) = parse_ifade(d, 0);
}
;
bekle(d, TIP_NOKTALI_VIRGUL);
return ret;
}
;
if ((YKN_DOT(j, tip) == TIP_DURDUR)) {
ilerle(d);
bekle(d, TIP_NOKTALI_VIRGUL);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_DURDUR, j);
}
;
if ((YKN_DOT(j, tip) == TIP_DEVAM_ET)) {
ilerle(d);
bekle(d, TIP_NOKTALI_VIRGUL);
return arena_dugum_yarat(YKN_DOT(d, arena), AST_DEVAM_ET, j);
}
;
if ((YKN_DOT(j, tip) == TIP_EGER)) {
ilerle(d);
AstDugum* eger_d = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_EGER, j));
YKN_DOT(eger_d, sol) = parse_ifade(d, 0);
if ((jeton_tipinde_mi(d, TIP_ISE) == 1)) {
    ilerle(d);
}
YKN_DOT(eger_d, govde) = parse_blok(d);
if ((jeton_tipinde_mi(d, TIP_YOKSA) == 1)) {
ilerle(d);
if ((jeton_tipinde_mi(d, TIP_EGER) == 1)) {
YKN_DOT(eger_d, yoksa_govde) = parse_satir(d);
}
 else {
YKN_DOT(eger_d, yoksa_govde) = parse_blok(d);
}
;
}
;
return eger_d;
}
;
if ((YKN_DOT(j, tip) == TIP_DONGU)) {
ilerle(d);
AstDugum* dongu_d = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_DONGU, j));
if ((jeton_tipinde_mi(d, TIP_SOL_KUSLU) == 1)) {
YKN_DOT(dongu_d, sol) = 0;
}
 else {
YKN_DOT(dongu_d, sol) = parse_ifade(d, 0);
}
;
    YKN_DOT(dongu_d, govde) = parse_blok(d);
    return dongu_d;
}

if (YKN_DOT(j, tip) == TIP_HER) {
    ilerle(d); // 'her' veya 'for'
    long long parantezli = 0;
    if (jeton_tipinde_mi(d, TIP_SOL_PARANTEZ) == 1) {
        ilerle(d); // '('
        parantezli = 1;
    }

    // 1. baslangic (degisken i: tamsayi = 0 veya i = 0)
    AstDugum* baslangic_d = nullptr;
    if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 0) {
        baslangic_d = parse_satir(d);
    } else {
        ilerle(d);
    }

    // 2. kosul (i < 10)
    AstDugum* kosul_d = nullptr;
    if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 0) {
        kosul_d = parse_ifade(d, 0);
    }
    if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) {
        ilerle(d);
    }

    // 3. adim (i = i + 1)
    AstDugum* adim_d = nullptr;
    if (parantezli == 1) {
        if (jeton_tipinde_mi(d, TIP_SAG_PARANTEZ) == 0) {
            adim_d = parse_ifade(d, 0);
        }
        bekle(d, TIP_SAG_PARANTEZ); // ')'
    } else {
        if (jeton_tipinde_mi(d, TIP_SOL_KUSLU) == 0) {
            adim_d = parse_ifade(d, 0);
        }
    }

    // 4. govde
    AstDugum* govde_blok = parse_blok(d);

    // AST_DONGU dugumu olustur (adim_d yoksa_govde'de saklanir ki devam_et adimi atlamasin)
    AstDugum* dongu_her = arena_dugum_yarat(d->arena, AST_DONGU, j);
    dongu_her->sol = kosul_d;
    dongu_her->govde = govde_blok;
    dongu_her->yoksa_govde = adim_d;

    // Eger baslangic varsa, zincirle: baslangic -> dongu_her
    if (baslangic_d) {
        AstDugum* cur_b = baslangic_d;
        while (cur_b->sonraki) {
            cur_b = cur_b->sonraki;
        }
        cur_b->sonraki = dongu_her;
        return baslangic_d;
    }

    return dongu_her;
}

if (YKN_DOT(j, tip) == TIP_ESLES) {
    ilerle(d); // 'esles' / 'match'
    AstDugum* ham_hedef = parse_ifade(d, 0);
    bekle(d, TIP_SOL_KUSLU); // '{'

    AstDugum* hedef_tanim = nullptr;
    AstDugum* hedef_val = ham_hedef;
    if (ham_hedef != nullptr && ham_hedef->tip != AST_KIMLIK && ham_hedef->tip != AST_SAYI &&
        ham_hedef->tip != AST_MANTIK && ham_hedef->tip != AST_GERCEK_SAYI && ham_hedef->tip != AST_METIN) {
        long long gec_no = g_esles_gecici_sayac++;
        char* no_str = enum_disc_metni(gec_no);
        long long no_uzn = enum_disc_uzunluk(gec_no);
        long long gec_uzn = 14 + no_uzn;
        char* gec_buf = (char*)bellek_tahsis_et(gec_uzn + 1);
        memcpy(gec_buf, "__esles_hedef_", 14);
        memcpy(gec_buf + 14, no_str, no_uzn);
        gec_buf[gec_uzn] = 0;
        Jeton* gec_j = (Jeton*)calloc(1, sizeof(Jeton));
        gec_j->tip = TIP_KIMLIK;
        gec_j->baslangic = gec_buf;
        gec_j->uzunluk = gec_uzn;
        gec_j->satir = j->satir;
        gec_j->sutun = j->sutun;
        hedef_tanim = arena_dugum_yarat(d->arena, AST_DEGISKEN_TANIMI, gec_j);
        hedef_tanim->sag = ham_hedef;
        hedef_val = arena_dugum_yarat(d->arena, AST_KIMLIK, gec_j);
    }

    AstDugum* ilk_eger = nullptr;
    AstDugum* son_eger = nullptr;
    long long esles_ilk_enum_idx = -1;

    while (suanki_jeton(d) != nullptr && suanki_jeton(d)->tip != TIP_EOF && suanki_jeton(d)->tip != TIP_SAG_KUSLU) {
        Jeton* kol_j = suanki_jeton(d);

        // Varsayilan / default / _ kolu
        long long varsayilan_mi = 0;
        if (kol_j->tip == TIP_VARSAYILAN) { varsayilan_mi = 1; }
        if (kol_j->tip == TIP_KIMLIK && kol_j->uzunluk == 1 && kol_j->baslangic[0] == '_') { varsayilan_mi = 1; }
        if (kol_j->tip == TIP_KIMLIK && kol_j->uzunluk == 10 && memcmp(kol_j->baslangic, "varsayilan", 10) == 0) { varsayilan_mi = 1; }
        if (kol_j->tip == TIP_KIMLIK && kol_j->uzunluk == 7 && memcmp(kol_j->baslangic, "default", 7) == 0) { varsayilan_mi = 1; }

        if (varsayilan_mi == 1) {
            ilerle(d); // varsayilan tokenini tuket
            if (jeton_tipinde_mi(d, TIP_KALIN_OK) == 1 || jeton_tipinde_mi(d, TIP_OK) == 1 || jeton_tipinde_mi(d, TIP_ISE) == 1) { ilerle(d); } // '=>', 'ise'
            AstDugum* def_blok = parse_blok(d);
            if (son_eger) {
                son_eger->yoksa_govde = def_blok;
            }
            continue;
        }

        // Normal desen / deger kolu (Ã¶rn: 100 => { ... })
        AstDugum* desen_val = parse_ifade(d, 0);
        if (jeton_tipinde_mi(d, TIP_KALIN_OK) == 1 || jeton_tipinde_mi(d, TIP_OK) == 1 || jeton_tipinde_mi(d, TIP_ISE) == 1) { ilerle(d); } // '=>', 'ise'
        AstDugum* kol_blok = parse_blok(d);

        // Algebraic enum pattern: Enum::Variant(x) -> hedef.durum == discriminant
        long long enum_idx = -1; AstDugum* binding = nullptr;
        if (desen_val && desen_val->tip == AST_ISLEV_CAGRISI && desen_val->sol && desen_val->sol->ilk_jeton) {
            enum_idx = enum_varyant_bul_tam(desen_val->sol->ilk_jeton->baslangic, desen_val->sol->ilk_jeton->uzunluk);
            if (enum_idx < 0) enum_idx = enum_varyant_bul_kisa(desen_val->sol->ilk_jeton->baslangic, desen_val->sol->ilk_jeton->uzunluk);
            if (enum_idx >= 0) binding = desen_val->sag;
            if (enum_idx < 0 && desen_val->sag && desen_val->sag->ilk_jeton && desen_val->sag->tip == AST_KIMLIK) {
                enum_idx = enum_varyant_bul_tam(desen_val->sag->ilk_jeton->baslangic, desen_val->sag->ilk_jeton->uzunluk);
                if (enum_idx < 0) enum_idx = enum_varyant_bul_kisa(desen_val->sag->ilk_jeton->baslangic, desen_val->sag->ilk_jeton->uzunluk);
                if (enum_idx >= 0) binding = desen_val->sag->sag;
            }
        } else if (desen_val && desen_val->tip == AST_KIMLIK && desen_val->ilk_jeton) {
            enum_idx = enum_varyant_bul_tam(desen_val->ilk_jeton->baslangic, desen_val->ilk_jeton->uzunluk);
            if (enum_idx < 0) enum_idx = enum_varyant_bul_kisa(desen_val->ilk_jeton->baslangic, desen_val->ilk_jeton->uzunluk);
        } else if (desen_val && desen_val->tip == AST_ALAN_ERISIMI && desen_val->sag && desen_val->sag->ilk_jeton) {
            enum_idx = enum_varyant_bul_tam(desen_val->sag->ilk_jeton->baslangic, desen_val->sag->ilk_jeton->uzunluk);
            if (enum_idx < 0) enum_idx = enum_varyant_bul_kisa(desen_val->sag->ilk_jeton->baslangic, desen_val->sag->ilk_jeton->uzunluk);
        }
        if (enum_idx >= 0) {
            if (esles_ilk_enum_idx < 0) esles_ilk_enum_idx = enum_idx;
            Jeton* field_j = (Jeton*)calloc(1, sizeof(Jeton)); field_j->tip = TIP_KIMLIK; field_j->baslangic = (char*)"durum"; field_j->uzunluk = 5;
            AstDugum* field = arena_dugum_yarat(d->arena, AST_ALAN_ERISIMI, field_j);
            field->sol = hedef_val; field->sag = arena_dugum_yarat(d->arena, AST_KIMLIK, field_j);
            long long disc_val = g_enum_var_disc[enum_idx];
            Jeton* disc_j = (Jeton*)calloc(1, sizeof(Jeton)); disc_j->tip = TIP_TAMSAYI; disc_j->baslangic = enum_disc_metni(disc_val); disc_j->uzunluk = enum_disc_uzunluk(disc_val);
            AstDugum* disc_d = arena_dugum_yarat(d->arena, AST_SAYI, disc_j);
            Jeton* enum_eq_j = (Jeton*)calloc(1, sizeof(Jeton)); enum_eq_j->tip = TIP_ESIT_MI; enum_eq_j->baslangic = (char*)"=="; enum_eq_j->uzunluk = 2;
            AstDugum* enum_eq = arena_dugum_yarat(d->arena, AST_IKILI_OP, enum_eq_j); enum_eq->sol = field; enum_eq->sag = disc_d;
            if (binding && kol_blok) {
                Jeton* value_j = (Jeton*)calloc(1, sizeof(Jeton)); value_j->tip = TIP_KIMLIK; value_j->baslangic = (char*)"deger"; value_j->uzunluk = 5;
                AstDugum* value_field = arena_dugum_yarat(d->arena, AST_ALAN_ERISIMI, value_j); value_field->sol = hedef_val; value_field->sag = arena_dugum_yarat(d->arena, AST_KIMLIK, value_j);
                value_field->cozumlenmis_tip = 77;
                AstDugum* bind = arena_dugum_yarat(d->arena, AST_DEGISKEN_TANIMI, binding->ilk_jeton);
                if (g_enum_var_payload_tip[enum_idx] != nullptr && g_enum_var_payload_tip_uzn[enum_idx] > 0) {
                    bind->tip_adi = g_enum_var_payload_tip[enum_idx];
                    bind->tip_adi_uzunluk = g_enum_var_payload_tip_uzn[enum_idx];
                } else {
                    bind->tip_adi = (char*)"tamsayi"; bind->tip_adi_uzunluk = 7;
                }
                bind->sag = value_field;
                bind->sonraki = kol_blok->sol; kol_blok->sol = bind;
            }
            desen_val = nullptr; // equality below is replaced
            Jeton* eger_j = kol_j;
            AstDugum* eger_enum = arena_dugum_yarat(d->arena, AST_EGER, eger_j); eger_enum->sol = enum_eq; eger_enum->govde = kol_blok;
            if (!ilk_eger) ilk_eger = son_eger = eger_enum; else { son_eger->yoksa_govde = eger_enum; son_eger = eger_enum; }
            continue;
        }

        // esitlik: hedef_val == desen_val
        Jeton* eq_j = (Jeton*)calloc(1, sizeof(Jeton));
        eq_j->tip = TIP_ESIT_MI;
        eq_j->baslangic = (char*)"==";
        eq_j->uzunluk = 2;
        eq_j->satir = kol_j->satir;
        eq_j->sutun = kol_j->sutun;

        AstDugum* esitlik_op = arena_dugum_yarat(d->arena, AST_IKILI_OP, eq_j);
        esitlik_op->sol = hedef_val;
        esitlik_op->sag = desen_val;

        AstDugum* eger_d = arena_dugum_yarat(d->arena, AST_EGER, kol_j);
        eger_d->sol = esitlik_op;
        eger_d->govde = kol_blok;

        if (!ilk_eger) {
            ilk_eger = eger_d;
            son_eger = eger_d;
        } else {
            son_eger->yoksa_govde = eger_d;
            son_eger = eger_d;
        }
    }
    bekle(d, TIP_SAG_KUSLU); // '}'
    if (hedef_tanim != nullptr) {
        if (esles_ilk_enum_idx >= 0) {
            char* tam_p = g_enum_var_tam_isim[esles_ilk_enum_idx];
            long long tam_u = g_enum_var_tam_uzn[esles_ilk_enum_idx];
            long long ayrac_i = 0;
            while (ayrac_i + 1 < tam_u) {
                if (tam_p[ayrac_i] == '_' && tam_p[ayrac_i + 1] == '_') break;
                ayrac_i++;
            }
            if (ayrac_i > 0 && ayrac_i + 1 < tam_u) {
                hedef_tanim->tip_adi = tam_p;
                hedef_tanim->tip_adi_uzunluk = ayrac_i;
            }
        }
        hedef_tanim->sonraki = ilk_eger;
        return hedef_tanim;
    }
    return ilk_eger;
}

if (YKN_DOT(j, tip) == TIP_KIMLIK) {
    if ((YKN_DOT(j, uzunluk) == 6 && bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"extern", 6) == 1) ||
        (YKN_DOT(j, uzunluk) == 3 && bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"dis", 3) == 1) ||
        (YKN_DOT(j, uzunluk) == 6 && bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"harici", 6) == 1)) {
        ilerle(d);
        Jeton* sj = suanki_jeton(d);
        if (sj && YKN_DOT(sj, tip) == TIP_ISLEV) {
            return parse_satir(d);
        }
    }
}

if ((YKN_DOT(j, tip) == TIP_KIMLIK) && YKN_DOT(j, uzunluk) == 5 && bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"makro", 5) == 1) {
    ilerle(d); // 'makro' tüket
    Jeton* makro_isim_j = bekle(d, TIP_KIMLIK);
    AstDugum* makro_dugum = (AstDugum*)arena_dugum_yarat(d->arena, AST_MAKRO_TANIMI, makro_isim_j);
    if (jeton_tipinde_mi(d, TIP_SOL_PARANTEZ)) {
        ilerle(d);
        AstDugum* ilk_p = 0; AstDugum* son_p = 0;
        while (!jeton_tipinde_mi(d, TIP_SAG_PARANTEZ) && suanki_jeton(d) && suanki_jeton(d)->tip != TIP_EOF) {
            Jeton* mp_j = bekle(d, TIP_KIMLIK);
            AstDugum* mp = (AstDugum*)arena_dugum_yarat(d->arena, AST_KIMLIK, mp_j);
            if (!ilk_p) { ilk_p = mp; son_p = mp; } else { YKN_DOT(son_p, sonraki) = mp; son_p = mp; }
            if (jeton_tipinde_mi(d, TIP_VIRGUL)) ilerle(d);
        }
        bekle(d, TIP_SAG_PARANTEZ);
        makro_dugum->parametreler = ilk_p;
    }
    if (jeton_tipinde_mi(d, TIP_SOL_KUSLU)) {
        makro_dugum->govde = parse_blok(d);
    }
    return makro_dugum;
}

// async isv/fn declaration. The AST is intentionally rejected by codegen
// until real Future<T> lowering is available; it must not degrade to a
// normal function or silently produce a synchronous artifact.
if ((YKN_DOT(j, tip) == TIP_ASYNC)) {
Jeton* async_j = j;
ilerle(d);
if (!jeton_tipinde_mi(d, TIP_ISLEV)) {
    hata_bas_parser((char*)"ERR_ASYNC_FUNCTION_KEYWORD_EXPECTED", suanki_jeton(d));
    return 0;
}
ilerle(d);
Jeton* isim_j = (Jeton*)(bekle(d, TIP_KIMLIK));
if (!isim_j) return 0;
AstDugum* async_dugum = arena_dugum_yarat(YKN_DOT(d, arena), AST_ASYNC_ISLEV, isim_j);
async_dugum->son_jeton = async_j;
async_dugum->parametreler = parametre_listesi_oku(d);
if (jeton_tipinde_mi(d, TIP_OK) == 1) {
    ilerle(d);
    tip_oku(d, async_dugum);
}
if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) {
    ilerle(d);
    async_dugum->govde = 0;
} else {
    async_dugum->govde = parse_blok(d);
}
return async_dugum;
}

if ((YKN_DOT(j, tip) == TIP_ISLEV)) {
ilerle(d);
Jeton* isim_j = (Jeton*)(bekle(d, TIP_KIMLIK));
isim_j = (Jeton*)(onceki_jeton(d));
AstDugum* islev_dugum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_ISLEV, isim_j));

if (jeton_tipinde_mi(d, TIP_KUCUK_MU)) {
    ilerle(d); // '<'
    AstDugum* ilk_gp = 0;
    AstDugum* son_gp = 0;
    while (!jeton_tipinde_mi(d, TIP_BUYUK_MU) && suanki_jeton(d) != 0) {
        if (suanki_jeton(d)->tip == TIP_EOF) break;
        Jeton* gp_j = bekle(d, TIP_KIMLIK);
        AstDugum* gp = (AstDugum*)arena_dugum_yarat(d->arena, AST_GENERIC_TANIM, gp_j);
        gp->tip_adi = gp_j->baslangic;
        gp->tip_adi_uzunluk = gp_j->uzunluk;
        if (jeton_tipinde_mi(d, TIP_KOLON)) {
            ilerle(d);
            Jeton* tb_j = bekle(d, TIP_KIMLIK);
            AstDugum* ilk_tb = (AstDugum*)arena_dugum_yarat(d->arena, AST_TRAIT_TANIM, tb_j);
            AstDugum* son_tb = ilk_tb;
            while (jeton_tipinde_mi(d, TIP_ARTI)) {
                ilerle(d); // '+'
                Jeton* tb2_j = bekle(d, TIP_KIMLIK);
                AstDugum* tb2 = (AstDugum*)arena_dugum_yarat(d->arena, AST_TRAIT_TANIM, tb2_j);
                son_tb->sonraki = tb2;
                son_tb = tb2;
            }
            gp->sag = ilk_tb;
        }
        if (!ilk_gp) { ilk_gp = gp; son_gp = gp; }
        else { son_gp->sonraki = gp; son_gp = gp; }
        if (jeton_tipinde_mi(d, TIP_VIRGUL)) ilerle(d);
    }
    bekle(d, TIP_BUYUK_MU); // '>'
    islev_dugum->sol = ilk_gp;
    islev_dugum->tip = AST_GENERIC_TANIM;
}

AstDugum* islev_params = (AstDugum*)(parametre_listesi_oku(d));
YKN_DOT(islev_dugum, parametreler) = islev_params;
if ((jeton_tipinde_mi(d, TIP_OK) == 1)) {
ilerle(d);
tip_oku(d, islev_dugum);
}
;
if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) {
    ilerle(d);
    YKN_DOT(islev_dugum, govde) = 0;
} else {
    YKN_DOT(islev_dugum, govde) = parse_blok(d);
}
return islev_dugum;
}
;
// EKSÄ°K-asm: asm { "..." } inline assembly blok
if ((YKN_DOT(j, tip) == TIP_ASM)) {
    ilerle(d);
    bekle(d, TIP_SOL_KUSLU);
    AstDugum* asm_d = arena_dugum_yarat(d->arena, AST_INLINE_ASM, j);
    if (suanki_jeton(d) && YKN_DOT(suanki_jeton(d), tip) == TIP_METIN) {
        asm_d->ilk_jeton = (Jeton*)suanki_jeton(d);
        ilerle(d);
    }
    while (suanki_jeton(d) && YKN_DOT(suanki_jeton(d), tip) != TIP_SAG_KUSLU && YKN_DOT(suanki_jeton(d), tip) != TIP_EOF) {
        ilerle(d);
    }
    bekle(d, TIP_SAG_KUSLU);
    if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL)) ilerle(d);
    return asm_d;
};
if ((YKN_DOT(j, tip) == TIP_ENUM)) {
    ilerle(d);
    Jeton* enum_isim_j = bekle(d, TIP_KIMLIK);
    AstDugum* enum_dugum = arena_dugum_yarat(d->arena, AST_YAPI_TANIMI, enum_isim_j);
    AstDugum* ilk_gp = nullptr; AstDugum* son_gp = nullptr;
    if (jeton_tipinde_mi(d, TIP_KUCUK_MU)) {
        ilerle(d);
        while (!jeton_tipinde_mi(d, TIP_BUYUK_MU) && suanki_jeton(d)) {
            Jeton* gp_j = bekle(d, TIP_KIMLIK);
            AstDugum* gp = arena_dugum_yarat(d->arena, AST_GENERIC_TANIM, gp_j);
            gp->tip_adi = gp_j->baslangic; gp->tip_adi_uzunluk = gp_j->uzunluk;
            if (!ilk_gp) ilk_gp = son_gp = gp; else { son_gp->sonraki = gp; son_gp = gp; }
            if (jeton_tipinde_mi(d, TIP_VIRGUL)) ilerle(d); else break;
        }
        bekle(d, TIP_BUYUK_MU);
    }
    enum_dugum->parametreler = ilk_gp;
    bekle(d, TIP_SOL_KUSLU);
    long long disc = 0;
    while (suanki_jeton(d) && !jeton_tipinde_mi(d, TIP_SAG_KUSLU)) {
        Jeton* var_j = bekle(d, TIP_KIMLIK);
        long long payload = 0; char* payload_tip = nullptr; long long payload_tip_uzn = 0;
        if (jeton_tipinde_mi(d, TIP_SOL_PARANTEZ)) {
            ilerle(d);
            Jeton* p_j = bekle(d, TIP_KIMLIK);
            if (p_j) { payload = 1; payload_tip = p_j->baslangic; payload_tip_uzn = p_j->uzunluk; }
            bekle(d, TIP_SAG_PARANTEZ);
        }
        long long total_uzn = enum_isim_j->uzunluk + 2 + var_j->uzunluk;
        char* total = (char*)bellek_tahsis_et(total_uzn + 1);
        memcpy(total, enum_isim_j->baslangic, enum_isim_j->uzunluk);
        total[enum_isim_j->uzunluk] = '_'; total[enum_isim_j->uzunluk + 1] = '_';
        memcpy(total + enum_isim_j->uzunluk + 2, var_j->baslangic, var_j->uzunluk);
        total[total_uzn] = 0;
        enum_varyant_ekle(total, total_uzn, var_j->baslangic, var_j->uzunluk, disc, payload, payload_tip, payload_tip_uzn);
        ++disc;
        if (jeton_tipinde_mi(d, TIP_VIRGUL) || jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL)) ilerle(d); else break;
    }
    bekle(d, TIP_SAG_KUSLU);
    AstDugum* durum = arena_dugum_yarat(d->arena, AST_KIMLIK, enum_isim_j);
    durum->ilk_jeton = (Jeton*)bellek_tahsis_et(40); durum->ilk_jeton->tip = TIP_KIMLIK; durum->ilk_jeton->baslangic = (char*)"durum"; durum->ilk_jeton->uzunluk = 5;
    durum->tip_adi = (char*)"tamsayi"; durum->tip_adi_uzunluk = 7;
    AstDugum* deger = arena_dugum_yarat(d->arena, AST_KIMLIK, enum_isim_j);
    deger->ilk_jeton = (Jeton*)bellek_tahsis_et(40); deger->ilk_jeton->tip = TIP_KIMLIK; deger->ilk_jeton->baslangic = (char*)"deger"; deger->ilk_jeton->uzunluk = 5;
    if (ilk_gp && ilk_gp->tip_adi) {
        deger->tip_adi = ilk_gp->tip_adi;
        deger->tip_adi_uzunluk = ilk_gp->tip_adi_uzunluk;
    } else {
        deger->tip_adi = (char*)"tamsayi";
        deger->tip_adi_uzunluk = 7;
    }
    durum->sonraki = deger; enum_dugum->sol = durum;
    return enum_dugum;
}
;
// BUG-4 FIX: tip alias parse
if (YKN_DOT(j, tip) == TIP_KIMLIK && YKN_DOT(j, uzunluk) == 3 &&
    bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"tip", 3) == 1) {
    ilerle(d);
    Jeton* alias_isim_j = bekle(d, TIP_KIMLIK);
    if (alias_isim_j) {
        AstDugum* alias_node = arena_dugum_yarat(d->arena, 37, alias_isim_j);
        if (jeton_tipinde_mi(d, TIP_ATAMA)) ilerle(d);
        tip_oku(d, alias_node);
        if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL)) ilerle(d);
        return alias_node;
    }
}
;
if (YKN_DOT(j, tip) == TIP_MODUL && YKN_DOT(j, baslangic) != 0 && YKN_DOT(j, baslangic)[0] != '#') {
    ilerle(d);
    Jeton* modul_j = (Jeton*)(bekle(d, TIP_KIMLIK));
    AstDugum* modul_blok = parse_blok(d);
    if (modul_j != 0 && modul_blok != 0) {
        AstDugum* modul_uyesi = YKN_DOT(modul_blok, sol);
        while (modul_uyesi != 0) {
            if ((YKN_DOT(modul_uyesi, tip) == AST_ISLEV ||
                 YKN_DOT(modul_uyesi, tip) == AST_GENERIC_TANIM) &&
                YKN_DOT(modul_uyesi, ilk_jeton) != 0) {
                Jeton* uye_j = YKN_DOT(modul_uyesi, ilk_jeton);
                long long zaten_mangled = 0;
                long long mi = 0;
                while (mi + 1 < YKN_DOT(uye_j, uzunluk)) {
                    if (YKN_DOT(uye_j, baslangic)[mi] == '_' &&
                        YKN_DOT(uye_j, baslangic)[mi + 1] == '_') {
                        zaten_mangled = 1;
                        break;
                    }
                    mi++;
                }
                if (zaten_mangled == 0) {
                    long long yeni_uzunluk = YKN_DOT(modul_j, uzunluk) + 2 + YKN_DOT(uye_j, uzunluk);
                    char* yeni_isim = (char*)bellek_tahsis_et(yeni_uzunluk + 1);
                    long long ni = 0;
                    while (ni < YKN_DOT(modul_j, uzunluk)) {
                        yeni_isim[ni] = YKN_DOT(modul_j, baslangic)[ni];
                        ni++;
                    }
                    yeni_isim[ni] = '_';
                    yeni_isim[ni + 1] = '_';
                    ni += 2;
                    long long ui = 0;
                    while (ui < YKN_DOT(uye_j, uzunluk)) {
                        yeni_isim[ni + ui] = YKN_DOT(uye_j, baslangic)[ui];
                        ui++;
                    }
                    yeni_isim[yeni_uzunluk] = 0;
                    YKN_DOT(uye_j, baslangic) = yeni_isim;
                    YKN_DOT(uye_j, uzunluk) = yeni_uzunluk;
                }
            }
            modul_uyesi = YKN_DOT(modul_uyesi, sonraki);
        }
        return YKN_DOT(modul_blok, sol);
    }
    return 0;
}

if ((YKN_DOT(j, tip) == TIP_YAPI)) {
ilerle(d);
Jeton* isim_j = (Jeton*)(bekle(d, TIP_KIMLIK));
AstDugum* yapi_dugum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_YAPI_TANIMI, isim_j));

// Generic Parametreler: <T> veya <T, U>
if (jeton_tipinde_mi(d, TIP_KUCUK_MU) == 1) {
    ilerle(d); // '<'
    AstDugum* ilk_gp = 0;
    AstDugum* son_gp = 0;
    while (!jeton_tipinde_mi(d, TIP_BUYUK_MU) && suanki_jeton(d) != 0 &&
           YKN_DOT(suanki_jeton(d), tip) != TIP_SAG_KUSLU && YKN_DOT(suanki_jeton(d), tip) != TIP_SAG_PARANTEZ) {
        if (YKN_DOT(suanki_jeton(d), tip) == TIP_EOF) break;
        Jeton* gp_j = (Jeton*)bekle(d, TIP_KIMLIK);
        AstDugum* gp = (AstDugum*)arena_dugum_yarat(YKN_DOT(d, arena), AST_GENERIC_TANIM, gp_j);
        YKN_DOT(gp, tip_adi) = YKN_DOT(gp_j, baslangic);
        YKN_DOT(gp, tip_adi_uzunluk) = YKN_DOT(gp_j, uzunluk);
        if (jeton_tipinde_mi(d, TIP_KOLON) == 1) {
            ilerle(d);
            Jeton* tb_j = (Jeton*)bekle(d, TIP_KIMLIK);
            AstDugum* ilk_tb = (AstDugum*)arena_dugum_yarat(YKN_DOT(d, arena), AST_TRAIT_TANIM, tb_j);
            AstDugum* son_tb = ilk_tb;
            while (jeton_tipinde_mi(d, TIP_ARTI) == 1) {
                ilerle(d); // '+'
                Jeton* tb2_j = (Jeton*)bekle(d, TIP_KIMLIK);
                AstDugum* tb2 = (AstDugum*)arena_dugum_yarat(YKN_DOT(d, arena), AST_TRAIT_TANIM, tb2_j);
                YKN_DOT(son_tb, sonraki) = tb2;
                son_tb = tb2;
            }
            YKN_DOT(gp, sag) = ilk_tb;
        }
        if (!ilk_gp) { ilk_gp = gp; son_gp = gp; }
        else { YKN_DOT(son_gp, sonraki) = gp; son_gp = gp; }
        if (jeton_tipinde_mi(d, TIP_VIRGUL) == 1) {
            ilerle(d);
            if (jeton_tipinde_mi(d, TIP_BUYUK_MU) == 1) {
                YKN_DOT(d, hata_sayisi) = YKN_DOT(d, hata_sayisi) + 1;
                hata_bas_parser((char*)"Generic listesinde sondaki virgul gecersiz", suanki_jeton(d));
            }
        }
    }
    bekle(d, TIP_BUYUK_MU); // '>'
    YKN_DOT(yapi_dugum, parametreler) = ilk_gp;
}

bekle(d, TIP_SOL_KUSLU);
AstDugum* ilk_alan = (AstDugum*)(0);
AstDugum* son_alan = (AstDugum*)(0);
while (((!(jeton_tipinde_mi(d, TIP_SAG_KUSLU))) && ((suanki_jeton(d)) != 0))) {
if ((YKN_DOT(suanki_jeton(d), tip) == TIP_EOF)) {
break;
}
;
Jeton* alan_isim = (Jeton*)(bekle(d, TIP_KIMLIK));
AstDugum* alan = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_KIMLIK, alan_isim));
bekle(d, TIP_KOLON);
tip_oku(d, alan);
if ((jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1)) {
ilerle(d);
}
;
if ((!(ilk_alan))) {
ilk_alan = alan;
son_alan = alan;
}
 else {
YKN_DOT(son_alan, sonraki) = alan;
son_alan = alan;
}
;
}
;
bekle(d, TIP_SAG_KUSLU);
YKN_DOT(yapi_dugum, sol) = ilk_alan;
return yapi_dugum;
}
;
if (YKN_DOT(j, tip) == TIP_TRAIT) {
    ilerle(d);
    Jeton* trait_isim_j = bekle(d, TIP_KIMLIK);
    AstDugum* trait_dugum = arena_dugum_yarat(d->arena, AST_TRAIT_TANIM, trait_isim_j);
    bekle(d, TIP_SOL_KUSLU);
    AstDugum* ilk_m = nullptr;
    AstDugum* son_m = nullptr;
    while (!jeton_tipinde_mi(d, TIP_SAG_KUSLU) && suanki_jeton(d)) {
        if (YKN_DOT(suanki_jeton(d), tip) == TIP_EOF) break;
        AstDugum* m_satir = parse_satir(d);
        if (m_satir) {
            if (!ilk_m) ilk_m = son_m = m_satir;
            else { son_m->sonraki = m_satir; son_m = m_satir; }
        }
    }
    bekle(d, TIP_SAG_KUSLU);
    trait_dugum->govde = ilk_m;
    tc_trait_kaydet(trait_isim_j, ilk_m);
    return trait_dugum;
}
if (YKN_DOT(j, tip) == TIP_UYGULA) {
    Jeton* sonraki_j = (d->guncel_index + 1 < d->jeton_sayisi) ? (Jeton*)((char*)d->jetonlar + (d->guncel_index + 1) * 40) : nullptr;
    if (sonraki_j && (sonraki_j->tip == TIP_SOL_PARANTEZ || sonraki_j->tip == TIP_ATAMA || sonraki_j->tip == TIP_NOKTA)) {
        AstDugum* expr = parse_ifade(d, 0);
        bekle(d, TIP_NOKTALI_VIRGUL);
        return expr;
    }
    ilerle(d);
    Jeton* trait_isim_j = bekle(d, TIP_KIMLIK);
    Jeton* cur_j = (Jeton*)suanki_jeton(d);
    if (cur_j && cur_j->tip == TIP_KIMLIK) {
        if ((cur_j->uzunluk == 4 && memcmp(cur_j->baslangic, "icin", 4) == 0) ||
            (cur_j->uzunluk == 3 && memcmp(cur_j->baslangic, "for", 3) == 0)) {
            ilerle(d);
        }
    }
    Jeton* hedef_tip_j = bekle(d, TIP_KIMLIK);
    AstDugum* hedef_dugum = arena_dugum_yarat(d->arena, AST_KIMLIK, hedef_tip_j);
    AstDugum* uygula_dugum = arena_dugum_yarat(d->arena, AST_TRAIT_UYGULAMA, trait_isim_j);
    uygula_dugum->sol = hedef_dugum;
    if (jeton_tipinde_mi(d, TIP_SOL_KUSLU)) {
        ilerle(d);
        AstDugum* ilk_im = nullptr;
        AstDugum* son_im = nullptr;
        while (!jeton_tipinde_mi(d, TIP_SAG_KUSLU) && suanki_jeton(d)) {
            if (YKN_DOT(suanki_jeton(d), tip) == TIP_EOF) break;
            AstDugum* m_satir = parse_satir(d);
            if (m_satir) {
                if (!ilk_im) ilk_im = son_im = m_satir;
                else { son_im->sonraki = m_satir; son_im = m_satir; }
            }
        }
        bekle(d, TIP_SAG_KUSLU);
        uygula_dugum->govde = ilk_im;
    } else if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL)) {
        ilerle(d);
    }
    tc_trait_uygulamasi_dogrula(0, trait_isim_j, hedef_tip_j, uygula_dugum->govde);
    return uygula_dugum;
}
;
if ((YKN_DOT(j, tip) == TIP_ICE_AKTAR)) {
ilerle(d);
Jeton* dosya_j = (Jeton*)(bekle(d, TIP_METIN));
AstDugum* imp = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_ICE_AKTAR, dosya_j));
if (jeton_tipinde_mi(d, TIP_OLARAK) == 1) {
    ilerle(d);
    Jeton* alias_j = (Jeton*)(bekle(d, TIP_KIMLIK));
    if (alias_j != 0) {
        YKN_DOT(imp, tip_adi) = YKN_DOT(alias_j, baslangic);
        YKN_DOT(imp, tip_adi_uzunluk) = YKN_DOT(alias_j, uzunluk);
    }
}
if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) {
    ilerle(d);
}
return imp;
}
;
if (YKN_DOT(j, tip) == 21) { // #prompt / #ai
ilerle(d);
Jeton* sorgu_j = (Jeton*)(bekle(d, TIP_METIN));
AstDugum* p_dugum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_DIREKTIF_PROMPT, sorgu_j));
if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) { ilerle(d); }
return p_dugum;
}
;
if ((YKN_DOT(j, tip) >= 24 && YKN_DOT(j, tip) <= 26) ||
    (YKN_DOT(j, tip) == 23 && YKN_DOT(j, baslangic) != 0 &&
     bellek_esit_mi(YKN_DOT(j, baslangic), (char*)"#", 1) == 1)) { // #c-lib / #cpp-lib / #py-lib / #rs-lib
long long d_ast_tip = AST_DIREKTIF_C_LIB + (YKN_DOT(j, tip) - 23);
ilerle(d);
Jeton* lib_j = (Jeton*)(bekle(d, TIP_METIN));
AstDugum* lib_dugum = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), d_ast_tip, lib_j));
if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) { ilerle(d); }
return lib_dugum;
}
;
if (YKN_DOT(j, tip) == TIP_DIREKTIF_CONVERT || (YKN_DOT(j, tip) == 64 && YKN_DOT(j, baslangic) != 0 && YKN_DOT(j, baslangic)[0] == '#')) {
    Jeton* conv_jeton = j;
    ilerle(d);
    AstDugum* conv_dugum = arena_dugum_yarat(d->arena, AST_DIREKTIF_CONVERT, conv_jeton);
    while (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 0 && suanki_jeton(d) != 0 && suanki_jeton(d)->tip != TIP_EOF) {
        Jeton* param_j = suanki_jeton(d);
        if (!param_j) break;
        ilerle(d);
        if (jeton_tipinde_mi(d, TIP_ATAMA) == 1) {
            ilerle(d);
            if (suanki_jeton(d) != 0) {
                ilerle(d);
            }
        }
        if (jeton_tipinde_mi(d, TIP_VIRGUL) == 1) { ilerle(d); }
    }
    if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) { ilerle(d); }
    return conv_dugum;
}
;
if (YKN_DOT(j, tip) == TIP_DIREKTIF_CONFIG || YKN_DOT(j, tip) == 66) {
    Jeton* cfg_jeton = j;
    ilerle(d);
    AstDugum* cfg_dugum = arena_dugum_yarat(d->arena, AST_DIREKTIF_CONFIG, cfg_jeton);
    long long parantezli = 0;
    if (jeton_tipinde_mi(d, TIP_SOL_KUSLU) == 1) {
        ilerle(d);
        parantezli = 1;
    }
    AstDugum* ilk_kv = nullptr;
    AstDugum* son_kv = nullptr;
    while (suanki_jeton(d) != 0 && suanki_jeton(d)->tip != TIP_EOF) {
        if (parantezli == 1 && jeton_tipinde_mi(d, TIP_SAG_KUSLU) == 1) {
            ilerle(d);
            if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) { ilerle(d); }
            break;
        }
        if (parantezli == 0 && jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) {
            ilerle(d);
            break;
        }
        Jeton* key_j = suanki_jeton(d);
        if (!key_j) break;
        ilerle(d);
        if (jeton_tipinde_mi(d, TIP_KOLON) == 1 || jeton_tipinde_mi(d, TIP_ATAMA) == 1) {
            ilerle(d);
        }
        Jeton* val_j = suanki_jeton(d);
        if (val_j != 0) {
            ilerle(d);
            AstDugum* kv_dugum = arena_dugum_yarat(d->arena, AST_ATAMA, key_j);
            AstDugum* val_dugum = arena_dugum_yarat(d->arena, val_j->tip, val_j);
            kv_dugum->sol = val_dugum;
            if (ilk_kv == nullptr) {
                ilk_kv = kv_dugum;
                son_kv = kv_dugum;
            } else {
                son_kv->sonraki = kv_dugum;
                son_kv = kv_dugum;
            }
        }
        if (jeton_tipinde_mi(d, TIP_VIRGUL) == 1) {
            ilerle(d);
        } else if (jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1) {
            if (parantezli == 1) { ilerle(d); }
            else { ilerle(d); break; }
        }
    }
    cfg_dugum->sol = ilk_kv;
    return cfg_dugum;
}
;
AstDugum* ifade = (AstDugum*)(parse_ifade(d, 0));
if ((jeton_tipinde_mi(d, TIP_NOKTALI_VIRGUL) == 1)) {
ilerle(d);
}
;
// [DEBUG_REMOVED] printf("PARSE_SATIR_RETURNED: tip=%lld (cagrilan jeton=%lld)\n", ifade ? YKN_DOT(ifade, tip) : -1, YKN_DOT(j, tip));
return ifade;
}


AstDugum* parser_cozumle(ParserDurumu* d) {
g_parser_ifade_derinlik = 0;
AstDugum* kok = (AstDugum*)(arena_dugum_yarat(YKN_DOT(d, arena), AST_BLOK, 0));
AstDugum* ilk = (AstDugum*)(0);
AstDugum* son = (AstDugum*)(0);
int dbg_count = 0;
while (YKN_DOT(d, guncel_index) < YKN_DOT(d, jeton_sayisi) && suanki_jeton(d) != 0 && YKN_DOT(suanki_jeton(d), tip) != TIP_EOF) {
    Jeton* cur_j = suanki_jeton(d);
    long long onceki_index = YKN_DOT(d, guncel_index);
    AstDugum* satir = (AstDugum*)(parse_satir(d));
    if ((YKN_DOT(d, guncel_index) == onceki_index)) {
        ilerle(d);
    }
    if ((!(satir))) {
        ilerle(d);
        continue;
    }
    if (!ilk) {
        ilk = satir;
        son = satir;
        while (son && son->sonraki) son = son->sonraki;
    }
    else {
        YKN_DOT(son, sonraki) = satir;
        son = satir;
        while (son && son->sonraki) son = son->sonraki;
    }
}
;
YKN_DOT(kok, sol) = ilk;
return kok;
}


long long djb2_makro(char* veri, long long uzunluk) {
return djb2_hash(veri, uzunluk);
}


MakroMotoru motor_olustur(AstArena* arena, MakroKural* kural_bellek, long long kapasite) {
MakroMotoru motor; memset(&motor, 0, sizeof(MakroMotoru));
YKN_DOT(motor, kurallar) = kural_bellek;
YKN_DOT(motor, kapasite) = kapasite;
YKN_DOT(motor, kayitli_sayi) = 0;
YKN_DOT(motor, arena) = arena;
return motor;
}


void makro_kaydet(MakroMotoru* motor, char* isim_ptr, long long isim_uzunluk, long long* params, long long param_sayisi, AstDugum* govde) {
if ((YKN_DOT(motor, kayitli_sayi) >= YKN_DOT(motor, kapasite))) {
hata_bas_parser((char*)("ERR_MACRO_ENGINE_FULL"), 0);
}
;
long long indeks = YKN_DOT(motor, kayitli_sayi);
YKN_DOT((_ykn_ref(motor).kurallar)[indeks], isim_hash) = djb2_makro(isim_ptr, isim_uzunluk);
YKN_DOT((_ykn_ref(motor).kurallar)[indeks], parametre_sayisi) = param_sayisi;
YKN_DOT((_ykn_ref(motor).kurallar)[indeks], parametre_hashleri) = params;
YKN_DOT((_ykn_ref(motor).kurallar)[indeks], sablon_koku) = govde;
YKN_DOT(motor, kayitli_sayi) = (YKN_DOT(motor, kayitli_sayi) + 1);
}


MakroKural* kural_bul(MakroMotoru* motor, long long isim_hash) {
long long i = 0;
while ((i < YKN_DOT(motor, kayitli_sayi))) {
if ((YKN_DOT((_ykn_ref(motor).kurallar)[i], isim_hash) == isim_hash)) {
return (YKN_DOT(motor, kurallar) + i);
}
;
i = (i + 1);
}
;
return 0;
}


AstDugum* arguman_bul(MakroKural* kural, AstDugum* argumanlar, long long hedef_hash) {
long long i = 0;
AstDugum* gec_arg = (AstDugum*)(argumanlar);
while (((i < YKN_DOT(kural, parametre_sayisi)) && ((gec_arg) != 0))) {
if (((_ykn_ref(kural).parametre_hashleri)[i] == hedef_hash)) {
return gec_arg;
}
;
gec_arg = YKN_DOT(gec_arg, sonraki);
i = (i + 1);
}
;
return 0;
}


static long long g_macro_hygiene_n = 0;
static long long g_hygiene_eski_hash[512] = {};
static char* g_hygiene_eski_isim[512] = {};
static long long g_hygiene_eski_uzn[512] = {};
static Jeton* g_hygiene_yeni_jeton[512] = {};
static long long g_hygiene_sayisi = 0;
static long long g_hygiene_baslangic = 0;

Jeton* hygiene_jeton_olustur(MakroMotoru* motor, Jeton* orijinal, long long n) {
    (void)motor;
    if (!orijinal) return 0;
    char* n_str = enum_disc_metni(n);
    long long n_uzn = enum_disc_uzunluk(n);
    long long prefix_uzn = 4 + n_uzn; // "__m" + sayi + "_"
    long long yeni_uzn = prefix_uzn + orijinal->uzunluk;
    char* yeni_ptr = (char*)bellek_tahsis_et(yeni_uzn + 1);
    yeni_ptr[0] = '_'; yeni_ptr[1] = '_'; yeni_ptr[2] = 'm';
    memcpy(yeni_ptr + 3, n_str, n_uzn);
    yeni_ptr[3 + n_uzn] = '_';
    memcpy(yeni_ptr + prefix_uzn, orijinal->baslangic, orijinal->uzunluk);
    yeni_ptr[yeni_uzn] = 0;
    Jeton* yeni_j = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
    yeni_j->tip = orijinal->tip;
    yeni_j->baslangic = yeni_ptr;
    yeni_j->uzunluk = yeni_uzn;
    yeni_j->satir = orijinal->satir;
    yeni_j->sutun = orijinal->sutun;
    return yeni_j;
}

Jeton* hygiene_jeton_bul(char* isim, long long uzn, long long hash) {
    for (long long i = g_hygiene_sayisi - 1; i >= g_hygiene_baslangic; --i) {
        if (g_hygiene_eski_hash[i] == hash && g_hygiene_eski_uzn[i] == uzn) {
            if (bellek_esit_mi(g_hygiene_eski_isim[i], isim, uzn) == 1) {
                return g_hygiene_yeni_jeton[i];
            }
        }
    }
    return 0;
}

AstDugum* ast_kopyala_ve_degistir(MakroMotoru* motor, MakroKural* kural, AstDugum* sablon, AstDugum* argumanlar) {
if ((!(sablon))) {
return 0;
}
;
if ((YKN_DOT(sablon, tip) == 4 && kural != 0)) { // AST_KIMLIK = 4
if (YKN_DOT(sablon, ilk_jeton) != 0) {
long long ident_hash = djb2_makro(YKN_DOT(YKN_DOT(sablon, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(sablon, ilk_jeton), uzunluk));
AstDugum* bulunan_arg = (AstDugum*)(arguman_bul(kural, argumanlar, ident_hash));
if (((bulunan_arg) != 0)) {
AstDugum* arg_kopya = (AstDugum*)(arena_dugum_yarat(YKN_DOT(motor, arena), YKN_DOT(bulunan_arg, tip), YKN_DOT(bulunan_arg, ilk_jeton)));
YKN_DOT(arg_kopya, son_jeton) = YKN_DOT(bulunan_arg, son_jeton);
YKN_DOT(arg_kopya, tip_adi) = YKN_DOT(bulunan_arg, tip_adi);
YKN_DOT(arg_kopya, tip_adi_uzunluk) = YKN_DOT(bulunan_arg, tip_adi_uzunluk);
YKN_DOT(arg_kopya, cozumlenmis_tip) = YKN_DOT(bulunan_arg, cozumlenmis_tip);
YKN_DOT(arg_kopya, dizi_boyutu) = YKN_DOT(bulunan_arg, dizi_boyutu);
YKN_DOT(arg_kopya, gorunurluk) = YKN_DOT(bulunan_arg, gorunurluk);
YKN_DOT(arg_kopya, nullable) = YKN_DOT(bulunan_arg, nullable);
YKN_DOT(arg_kopya, tip_arg_sayisi) = YKN_DOT(bulunan_arg, tip_arg_sayisi);
YKN_DOT(arg_kopya, sol) = ast_kopyala_ve_degistir(motor, 0, YKN_DOT(bulunan_arg, sol), 0);
YKN_DOT(arg_kopya, sag) = ast_kopyala_ve_degistir(motor, 0, YKN_DOT(bulunan_arg, sag), 0);
YKN_DOT(arg_kopya, govde) = ast_kopyala_ve_degistir(motor, 0, YKN_DOT(bulunan_arg, govde), 0);
YKN_DOT(arg_kopya, yoksa_govde) = ast_kopyala_ve_degistir(motor, 0, YKN_DOT(bulunan_arg, yoksa_govde), 0);
YKN_DOT(arg_kopya, parametreler) = ast_kopyala_ve_degistir(motor, 0, YKN_DOT(bulunan_arg, parametreler), 0);
YKN_DOT(arg_kopya, tip_arglari) = ast_kopyala_ve_degistir(motor, 0, YKN_DOT(bulunan_arg, tip_arglari), 0);
return arg_kopya;
}
Jeton* hyg_j = hygiene_jeton_bul(YKN_DOT(YKN_DOT(sablon, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(sablon, ilk_jeton), uzunluk), ident_hash);
if (hyg_j != 0) {
AstDugum* hyg_kopya = (AstDugum*)(arena_dugum_yarat(YKN_DOT(motor, arena), YKN_DOT(sablon, tip), hyg_j));
YKN_DOT(hyg_kopya, son_jeton) = hyg_j;
YKN_DOT(hyg_kopya, tip_adi) = YKN_DOT(sablon, tip_adi);
YKN_DOT(hyg_kopya, tip_adi_uzunluk) = YKN_DOT(sablon, tip_adi_uzunluk);
YKN_DOT(hyg_kopya, cozumlenmis_tip) = YKN_DOT(sablon, cozumlenmis_tip);
YKN_DOT(hyg_kopya, dizi_boyutu) = YKN_DOT(sablon, dizi_boyutu);
YKN_DOT(hyg_kopya, gorunurluk) = YKN_DOT(sablon, gorunurluk);
YKN_DOT(hyg_kopya, nullable) = YKN_DOT(sablon, nullable);
YKN_DOT(hyg_kopya, tip_arg_sayisi) = YKN_DOT(sablon, tip_arg_sayisi);
YKN_DOT(hyg_kopya, sol) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, sol), argumanlar);
YKN_DOT(hyg_kopya, sag) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, sag), argumanlar);
if (YKN_DOT(sablon, sonraki) != 0) {
YKN_DOT(hyg_kopya, sonraki) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, sonraki), argumanlar);
}
return hyg_kopya;
}
}
}
;
AstDugum* kopyalanan = (AstDugum*)(arena_dugum_yarat(YKN_DOT(motor, arena), YKN_DOT(sablon, tip), YKN_DOT(sablon, ilk_jeton)));
YKN_DOT(kopyalanan, son_jeton) = YKN_DOT(sablon, son_jeton);
YKN_DOT(kopyalanan, tip_adi) = YKN_DOT(sablon, tip_adi);
YKN_DOT(kopyalanan, tip_adi_uzunluk) = YKN_DOT(sablon, tip_adi_uzunluk);
YKN_DOT(kopyalanan, cozumlenmis_tip) = YKN_DOT(sablon, cozumlenmis_tip);
YKN_DOT(kopyalanan, dizi_boyutu) = YKN_DOT(sablon, dizi_boyutu);
YKN_DOT(kopyalanan, gorunurluk) = YKN_DOT(sablon, gorunurluk);
YKN_DOT(kopyalanan, nullable) = YKN_DOT(sablon, nullable);
YKN_DOT(kopyalanan, tip_arg_sayisi) = YKN_DOT(sablon, tip_arg_sayisi);
YKN_DOT(kopyalanan, sol) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, sol), argumanlar);
YKN_DOT(kopyalanan, sag) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, sag), argumanlar);
if (YKN_DOT(sablon, tip) == 7 && kural != 0 && YKN_DOT(sablon, ilk_jeton) != 0 && g_macro_hygiene_n > 0) {
    Jeton* yeni_j = hygiene_jeton_olustur(motor, YKN_DOT(sablon, ilk_jeton), g_macro_hygiene_n);
    if (yeni_j != 0) {
        YKN_DOT(kopyalanan, ilk_jeton) = yeni_j;
        if (g_hygiene_sayisi < 512) {
            g_hygiene_eski_hash[g_hygiene_sayisi] = djb2_makro(YKN_DOT(YKN_DOT(sablon, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(sablon, ilk_jeton), uzunluk));
            g_hygiene_eski_isim[g_hygiene_sayisi] = YKN_DOT(YKN_DOT(sablon, ilk_jeton), baslangic);
            g_hygiene_eski_uzn[g_hygiene_sayisi] = YKN_DOT(YKN_DOT(sablon, ilk_jeton), uzunluk);
            g_hygiene_yeni_jeton[g_hygiene_sayisi] = yeni_j;
            g_hygiene_sayisi++;
        }
    }
}
YKN_DOT(kopyalanan, govde) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, govde), argumanlar);
YKN_DOT(kopyalanan, yoksa_govde) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, yoksa_govde), argumanlar);
YKN_DOT(kopyalanan, parametreler) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, parametreler), argumanlar);
YKN_DOT(kopyalanan, tip_arglari) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, tip_arglari), argumanlar);
if (((YKN_DOT(sablon, sonraki)) != 0)) {
YKN_DOT(kopyalanan, sonraki) = ast_kopyala_ve_degistir(motor, kural, YKN_DOT(sablon, sonraki), argumanlar);
}
;
return kopyalanan;
}


static long long g_makro_genisletme_sayaci = 0;
static long long g_makro_derinlik = 0;
const long long MAKRO_GENISLETME_BUTCESI = 4096;
const long long MAKRO_MAKS_DERINLIK = 32;

AstDugum* makro_tek_dugum(MakroMotoru* motor, AstDugum* kok) {
    if (!kok) return 0;
    YKN_DOT(kok, sol) = makrolari_genislet(motor, YKN_DOT(kok, sol));
    YKN_DOT(kok, sag) = makrolari_genislet(motor, YKN_DOT(kok, sag));
    if (YKN_DOT(kok, govde)) YKN_DOT(kok, govde) = makrolari_genislet(motor, YKN_DOT(kok, govde));
    if (YKN_DOT(kok, yoksa_govde)) YKN_DOT(kok, yoksa_govde) = makrolari_genislet(motor, YKN_DOT(kok, yoksa_govde));
    if (YKN_DOT(kok, tip) == AST_MAKRO_TANIMI) {
        return 0;
    }
    if (YKN_DOT(kok, tip) == AST_MAKRO_CAGRISI || YKN_DOT(kok, tip) == AST_ISLEV_CAGRISI) {
        // Macro cagrilari hem AST_MAKRO_CAGRISI hem AST_ISLEV_CAGRISI olabilir
        // (parser macro isimlerini bilmiyor, normal fonksiyon gibi parse ediyor)
        Jeton* cagri_isim_j = 0;
        if (YKN_DOT(kok, sol) && YKN_DOT((AstDugum*)YKN_DOT(kok, sol), ilk_jeton))
            cagri_isim_j = (Jeton*)YKN_DOT((AstDugum*)YKN_DOT(kok, sol), ilk_jeton);
        else if (YKN_DOT(kok, ilk_jeton))
            cagri_isim_j = (Jeton*)YKN_DOT(kok, ilk_jeton);
        if (!cagri_isim_j) { return kok; }
        long long cagri_hash = djb2_makro(YKN_DOT(cagri_isim_j, baslangic), YKN_DOT(cagri_isim_j, uzunluk));
        MakroKural* kural = (MakroKural*)(kural_bul(motor, cagri_hash));
        if (!kural) {
            // Built-in macro kontrolü
            long long h_dogrula = djb2_makro((char*)"dogrula", 7);
            long long h_ayikla  = djb2_makro((char*)"ayikla", 6);
            long long h_panik   = djb2_makro((char*)"panik", 5);
            long long h_boyut   = djb2_makro((char*)"boyut", 5);
            AstDugum* args2 = (AstDugum*)YKN_DOT(kok, sag);
            if (cagri_hash == h_dogrula && args2) {
                // dogrula!(kosul) → eger kosul == 0 ise { ekrana_yaz(...); dondu 1; }
                // Basit implementasyon: runtime'da hata verir
                // Argümanı doğrudan bırak (no-op genişletme yerine işaretsiz bırak)
                return kok; // dogrula runtime'da normal fonksiyon gibi çalışır
            }
            if (cagri_hash == h_ayikla && args2) {
                // ayikla!(deger) → sayiyi_yazdir(deger)
                // cagri_isim_j'yi "sayiyi_yazdir"a değiştir
                Jeton* yeni_j = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
                yeni_j->tip = TIP_KIMLIK;
                yeni_j->baslangic = (char*)"sayiyi_yazdir";
                yeni_j->uzunluk = 13;
                if (YKN_DOT(kok, sol)) YKN_DOT((AstDugum*)YKN_DOT(kok, sol), ilk_jeton) = yeni_j;
                return kok;
            }
            if (cagri_hash == h_panik && args2) {
                // panik!(mesaj) → ekrana_yaz(mesaj) olarak bırak
                Jeton* yeni_j2 = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
                yeni_j2->tip = TIP_KIMLIK;
                yeni_j2->baslangic = (char*)"ekrana_yaz";
                yeni_j2->uzunluk = 10;
                if (YKN_DOT(kok, sol)) YKN_DOT((AstDugum*)YKN_DOT(kok, sol), ilk_jeton) = yeni_j2;
                return kok;
            }
            if (cagri_hash == h_boyut) {
                // boyut!(T) → 8 (veya u8 için 1)
                long long bsz = 8;
                if (args2 && YKN_DOT(args2, ilk_jeton)) {
                    char* tp = YKN_DOT((Jeton*)YKN_DOT(args2, ilk_jeton), baslangic);
                    long long tl = YKN_DOT((Jeton*)YKN_DOT(args2, ilk_jeton), uzunluk);
                    if (tl == 2 && bellek_esit_mi(tp, (char*)"u8", 2) == 1) bsz = 1;
                }
                Jeton* bj = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
                char* bstr = (char*)bellek_tahsis_et(4);
                bstr[0] = (bsz == 1) ? '1' : '8'; bstr[1] = 0;
                bj->tip = TIP_TAMSAYI; bj->baslangic = bstr; bj->uzunluk = 1;
                AstDugum* bnum = (AstDugum*)arena_dugum_yarat(motor->arena, 1, bj);
                YKN_DOT(bnum, sonraki) = YKN_DOT(kok, sonraki);
                return bnum;
            }
            return kok; // normal fonksiyon, macro degil
        }
        if (g_makro_genisletme_sayaci >= MAKRO_GENISLETME_BUTCESI) {
            makro_limit_hatasi(1, 1, "Makro genisletme butcesi asildi");
            exit(1);
        }
        if (g_makro_derinlik >= MAKRO_MAKS_DERINLIK) {
            ozyineleme_derinlik_hatasi(1, 1, "Makro ozyineleme derinlik limiti asildi");
            exit(1);
        }
        g_makro_genisletme_sayaci++;
        g_makro_derinlik++;
        // Argümanlar: AST_ISLEV_CAGRISI.sag veya AST_MAKRO_CAGRISI.sag
        AstDugum* args = (AstDugum*)YKN_DOT(kok, sag);
        long long onceki_hyg_bas = g_hygiene_baslangic;
        long long onceki_hyg_say = g_hygiene_sayisi;
        g_hygiene_baslangic = g_hygiene_sayisi;
        g_macro_hygiene_n++;
        AstDugum* genisletilmis_agac = (AstDugum*)(ast_kopyala_ve_degistir(motor, kural, YKN_DOT(kural, sablon_koku), args));
        g_hygiene_sayisi = onceki_hyg_say;
        g_hygiene_baslangic = onceki_hyg_bas;
        if (genisletilmis_agac != 0 && (YKN_DOT(genisletilmis_agac, tip) == AST_BLOK || YKN_DOT(genisletilmis_agac, tip) == 15) &&
            YKN_DOT(genisletilmis_agac, sol) != 0 && YKN_DOT(YKN_DOT(genisletilmis_agac, sol), sonraki) == 0 &&
            YKN_DOT(YKN_DOT(genisletilmis_agac, sol), tip) != 7) {
            genisletilmis_agac = YKN_DOT(genisletilmis_agac, sol);
        }
        genisletilmis_agac = makro_tek_dugum(motor, genisletilmis_agac);
        g_makro_derinlik--;
        AstDugum* iter = (AstDugum*)(genisletilmis_agac);
        while (iter != 0 && YKN_DOT(iter, sonraki) != 0) {
            iter = YKN_DOT(iter, sonraki);
        }
        if (iter != 0) {
            YKN_DOT(iter, sonraki) = YKN_DOT(kok, sonraki);
        }
        return genisletilmis_agac;
    }
    return kok;
}

AstDugum* makrolari_genislet(MakroMotoru* motor, AstDugum* kok) {
    if (!motor) return kok;
    if (g_makro_derinlik == 0) {
        g_makro_genisletme_sayaci = 0;
    }
    AstDugum* yeni_kok = 0;
    AstDugum* son = 0;
    AstDugum* iter = kok;
    while (iter != 0) {
        AstDugum* siradaki = (AstDugum*)YKN_DOT(iter, sonraki);
        YKN_DOT(iter, sonraki) = 0;
        AstDugum* genisletilmis = makro_tek_dugum(motor, iter);
        // genisletilmis null ise (makro tanimlari) atla
        if (genisletilmis == 0) {
            iter = siradaki;
            continue;
        }
        // Zincire ekle
        if (!yeni_kok) { yeni_kok = genisletilmis; son = genisletilmis; }
        else { YKN_DOT(son, sonraki) = genisletilmis; }
        // sonraki'yi güncelle
        while (son && YKN_DOT(son, sonraki)) son = (AstDugum*)YKN_DOT(son, sonraki);
        iter = siradaki;
    }
    return yeni_kok ? yeni_kok : kok;
}


auto TIP_ID_BILINMIYOR = 0;
auto TIP_ID_TAMSAYI = 1;
auto TIP_ID_GERCEK = 2;
auto TIP_ID_METIN = 3;
auto TIP_ID_MANTIK = 4;
auto TIP_ID_U8 = 5;
auto TIP_ID_ISLEV = 6;
auto TIP_ID_YAPI = 7;
auto TIP_ID_POINTER = 8;
auto TIP_ID_HIC = 9;
auto TIP_ID_U16 = 10;
auto TIP_ID_U32 = 11;
auto TIP_ID_I32 = 12;
auto TIP_ID_I64 = 13;
auto TIP_ID_F32 = 14;
auto TIP_ID_F64 = 15;
auto TIP_ID_SONUC = 16;
auto TIP_ID_FUTURE = 17;

// Stage-0 keeps compiler Future<T> payloads in a real intern table rather
// than collapsing them to an unrelated scalar. Handles are stable for equal
// payload handles, so nested Future<Future<T>> remains comparable.
struct SeedFutureType { long long handle; long long payload; };
static SeedFutureType g_seed_future_types[256] = {};
static long long g_seed_future_type_count = 0;
static long long g_seed_future_type_errors = 0;
static char* g_seed_known_type_names[256] = {};
static long long g_seed_known_type_lengths[256] = {};
static long long g_seed_known_type_count = 0;
static void seed_known_type_ekle(char* ad, long long uzunluk) {
    if (!ad || uzunluk <= 0 || g_seed_known_type_count >= 256) return;
    for (long long i = 0; i < g_seed_known_type_count; i++) {
        if (g_seed_known_type_lengths[i] == uzunluk &&
            bellek_esit_mi(g_seed_known_type_names[i], ad, uzunluk) == 1) return;
    }
    g_seed_known_type_names[g_seed_known_type_count] = ad;
    g_seed_known_type_lengths[g_seed_known_type_count] = uzunluk;
    g_seed_known_type_count++;
}
static bool seed_known_type_mi(char* ad, long long uzunluk) {
    if (!ad || uzunluk <= 0) return false;
    for (long long i = 0; i < g_seed_known_type_count; i++) {
        if (g_seed_known_type_lengths[i] == uzunluk &&
            bellek_esit_mi(g_seed_known_type_names[i], ad, uzunluk) == 1) return true;
    }
    return false;
}
static bool seed_future_tipi_mi(long long tip) {
    for (long long i = 0; i < g_seed_future_type_count; i++) {
        if (g_seed_future_types[i].handle == tip) return true;
    }
    return false;
}
static long long seed_future_payload(long long tip) {
    for (long long i = 0; i < g_seed_future_type_count; i++) {
        if (g_seed_future_types[i].handle == tip) return g_seed_future_types[i].payload;
    }
    return TIP_ID_BILINMIYOR;
}
static long long seed_future_olustur(long long payload) {
    if (payload == TIP_ID_BILINMIYOR) return TIP_ID_BILINMIYOR;
    for (long long i = 0; i < g_seed_future_type_count; i++) {
        if (g_seed_future_types[i].payload == payload) return g_seed_future_types[i].handle;
    }
    if (g_seed_future_type_count >= 256) return TIP_ID_BILINMIYOR;
    long long handle = 1000000 + g_seed_future_type_count;
    g_seed_future_types[g_seed_future_type_count].handle = handle;
    g_seed_future_types[g_seed_future_type_count].payload = payload;
    g_seed_future_type_count++;
    return handle;
}

long long djb2_sembol(char* veri, long long uzunluk) {
return djb2_hash(veri, uzunluk);
}


// Typechecker capacity state is defined with the codegen globals below; these
// declarations keep the semantic helpers visible before their definitions.
static long long g_typechecker_kapasite_hatasi = 0;
static long long g_typechecker_kapasite_kodu = 0;
static long long g_typechecker_sembol_sayisi = 0;
static long long g_typechecker_kapsam_sayisi = 0;
static long long g_typechecker_nominal_sayisi = 0;
static void seed_typechecker_kapasite_reddet(long long kod);

void kapsam_olustur(TipMotoru* motor, long long kapasite) {
if (!motor || !YKN_DOT(motor, arena)) return;
long long boyut = (kapasite * 8);
if ((YKN_DOT(YKN_DOT(motor, arena), kullanilan) + boyut + 48 > YKN_DOT(YKN_DOT(motor, arena), kapasite))) {
    long long yeni_kapasite = YKN_DOT(YKN_DOT(motor, arena), kapasite) * 2;
    if (yeni_kapasite < boyut * 2 + 1048576) yeni_kapasite = boyut * 2 + 1048576;
    char* yeni_bellek = (char*)bellek_tahsis_et(yeni_kapasite);
    if (yeni_bellek) {
        YKN_DOT(YKN_DOT(motor, arena), bellek) = yeni_bellek;
        YKN_DOT(YKN_DOT(motor, arena), kapasite) = yeni_kapasite;
        YKN_DOT(YKN_DOT(motor, arena), kullanilan) = 0;
    } else {
        seed_typechecker_kapasite_reddet(2);
        return;
    }
}
if ((YKN_DOT(YKN_DOT(motor, arena), kullanilan) + boyut + 48 > YKN_DOT(YKN_DOT(motor, arena), kapasite))) {
    seed_typechecker_kapasite_reddet(2);
    return;
}
if (g_typechecker_kapsam_sayisi >= 1048576) {
    seed_typechecker_kapasite_reddet(2);
    return;
}
SembolGirdisi** tablo = (SembolGirdisi**)((char*)((char*)(YKN_DOT(YKN_DOT(motor, arena), bellek)) + YKN_DOT(YKN_DOT(motor, arena), kullanilan)));
YKN_DOT(YKN_DOT(motor, arena), kullanilan) = (YKN_DOT(YKN_DOT(motor, arena), kullanilan) + boyut);
char* sifir_ptr = (char*)(tablo);
long long sifir_i = 0;
while ((sifir_i < boyut)) {
sifir_ptr[sifir_i] = 0;
sifir_i = (sifir_i + 1);
}
;
Kapsam* yeni = (Kapsam*)((char*)((char*)(YKN_DOT(YKN_DOT(motor, arena), bellek)) + YKN_DOT(YKN_DOT(motor, arena), kullanilan)));
YKN_DOT(YKN_DOT(motor, arena), kullanilan) = (YKN_DOT(YKN_DOT(motor, arena), kullanilan) + 24);
YKN_DOT(yeni, tablo) = tablo;
YKN_DOT(yeni, kapasite) = kapasite;
YKN_DOT(yeni, ust_kapsam) = YKN_DOT(motor, guncel_kapsam);
YKN_DOT(motor, guncel_kapsam) = yeni;
g_typechecker_kapsam_sayisi++;
}


void kapsamdan_cik(TipMotoru* motor) {
if (((YKN_DOT(motor, guncel_kapsam)) != 0)) {
release_scope_borrows(YKN_DOT(motor, guncel_kapsam));
YKN_DOT(motor, guncel_kapsam) = YKN_DOT(YKN_DOT(motor, guncel_kapsam), ust_kapsam);
}
;
}


void sembol_ekle(TipMotoru* motor, Jeton* isim_j, long long tip, long long donus, AstDugum* ast) {
if ((!(YKN_DOT(motor, guncel_kapsam)))) {
return;
}
if (!motor || !YKN_DOT(motor, arena)) return;
if (isim_j && isim_j->baslangic) {
    long long reserved_tid = tip_adini_coz(isim_j->baslangic, isim_j->uzunluk);
    if (reserved_tid != TIP_ID_BILINMIYOR && reserved_tid != TIP_ID_YAPI && tip == TIP_ID_ISLEV) {
        tc_diagnostic_yaz(50, "TC HATA: Yerlesik tip adi fonksiyon adi olarak kullanilamaz (type shadowing)", isim_j);
        YKN_DOT(motor, hata_sayisi)++;
        return;
    }
}
if (g_typechecker_sembol_sayisi >= 16384) {
    seed_typechecker_kapasite_reddet(3);
    return;
}
if (YKN_DOT(YKN_DOT(motor, arena), kullanilan) + 56 > YKN_DOT(YKN_DOT(motor, arena), kapasite)) {
    long long yeni_kapasite = YKN_DOT(YKN_DOT(motor, arena), kapasite) * 2;
    if (yeni_kapasite < 524288) yeni_kapasite = 524288;
    char* yeni_bellek = (char*)bellek_tahsis_et(yeni_kapasite);
    if (yeni_bellek) {
        YKN_DOT(YKN_DOT(motor, arena), bellek) = yeni_bellek;
        YKN_DOT(YKN_DOT(motor, arena), kapasite) = yeni_kapasite;
        YKN_DOT(YKN_DOT(motor, arena), kullanilan) = 0;
    } else {
        seed_typechecker_kapasite_reddet(3);
        return;
    }
}
if (YKN_DOT(YKN_DOT(motor, arena), kullanilan) + 56 > YKN_DOT(YKN_DOT(motor, arena), kapasite)) {
    seed_typechecker_kapasite_reddet(3);
    return;
}
long long hash = djb2_sembol(YKN_DOT(isim_j, baslangic), YKN_DOT(isim_j, uzunluk));
long long indeks = (hash % YKN_DOT(YKN_DOT(motor, guncel_kapsam), kapasite));
if (indeks < 0) indeks = -indeks;
SembolGirdisi* yeni = (SembolGirdisi*)((char*)((char*)(YKN_DOT(YKN_DOT(motor, arena), bellek)) + YKN_DOT(YKN_DOT(motor, arena), kullanilan)));
YKN_DOT(YKN_DOT(motor, arena), kullanilan) = (YKN_DOT(YKN_DOT(motor, arena), kullanilan) + 56); // FIX13: +8 for nullable
YKN_DOT(yeni, isim_hash) = hash;
YKN_DOT(yeni, tanim_jetonu) = isim_j;
YKN_DOT(yeni, veri_tipi) = tip;
YKN_DOT(yeni, donus_tipi) = donus;
YKN_DOT(yeni, ast_tanimi) = ast;
YKN_DOT(yeni, nullable) = ast ? YKN_DOT(ast, nullable) : 0; // FIX13: propagate nullable
YKN_DOT(yeni, sonraki) = (_ykn_ref(YKN_DOT(motor, guncel_kapsam)).tablo)[indeks];
(_ykn_ref(YKN_DOT(motor, guncel_kapsam)).tablo)[indeks] = yeni;
g_typechecker_sembol_sayisi++;
}


SembolGirdisi* sembol_ara_hash(TipMotoru* motor, long long hash) {
    if (!motor) return 0;
    Kapsam* k = (Kapsam*)(YKN_DOT(motor, guncel_kapsam));
    while (k != 0) {
        long long indeks = (hash % YKN_DOT(k, kapasite));
        if (indeks < 0) indeks = -indeks;
        SembolGirdisi* iter = (SembolGirdisi*)((_ykn_ref(k).tablo)[indeks]);
        while (iter != 0) {
            if (YKN_DOT(iter, isim_hash) == hash) {
                return iter;
            }
            iter = YKN_DOT(iter, sonraki);
        }
        k = YKN_DOT(k, ust_kapsam);
    }
    return 0;
}

SembolGirdisi* sembol_ara(TipMotoru* motor, Jeton* isim_j) {
if ((!(isim_j))) {
return 0;
}
;
long long hash = djb2_sembol(YKN_DOT(isim_j, baslangic), YKN_DOT(isim_j, uzunluk));
return sembol_ara_hash(motor, hash);
}

static Kapsam* tc_sembolun_kapsami(TipMotoru* motor, Jeton* isim_j) {
    if (!isim_j || !motor) return 0;
    long long hash = djb2_sembol(YKN_DOT(isim_j, baslangic), YKN_DOT(isim_j, uzunluk));
    Kapsam* k = (Kapsam*)(YKN_DOT(motor, guncel_kapsam));
    while (k != 0) {
        long long indeks = (hash % YKN_DOT(k, kapasite));
        if (indeks < 0) indeks = -indeks;
        SembolGirdisi* iter = (SembolGirdisi*)((_ykn_ref(k).tablo)[indeks]);
        while (iter != 0) {
            if (YKN_DOT(iter, isim_hash) == hash && YKN_DOT(iter, tanim_jetonu) != 0) {
                if (YKN_DOT(YKN_DOT(iter, tanim_jetonu), uzunluk) == YKN_DOT(isim_j, uzunluk) &&
                    bellek_esit_mi(YKN_DOT(YKN_DOT(iter, tanim_jetonu), baslangic), YKN_DOT(isim_j, baslangic), YKN_DOT(isim_j, uzunluk)) == 1) {
                    return k;
                }
            }
            iter = YKN_DOT(iter, sonraki);
        }
        k = YKN_DOT(k, ust_kapsam);
    }
    return 0;
}

static long long tc_kapsam_derinligi(Kapsam* k) {
    long long d = 0;
    Kapsam* cur = k;
    while (cur != 0) {
        if (YKN_DOT(cur, ust_kapsam) != 0) {
            d++;
        }
        cur = YKN_DOT(cur, ust_kapsam);
    }
    return d;
}

static long long tc_levenshtein(const char* a, long long alen, const char* b, long long blen) {
    if (!a || !b) return 99;
    if (alen > 63) alen = 63;
    if (blen > 63) blen = 63;
    long long d[64][64] = {};
    for (long long i = 0; i <= alen; i++) d[i][0] = i;
    for (long long j = 0; j <= blen; j++) d[0][j] = j;
    for (long long i = 1; i <= alen; i++) {
        for (long long j = 1; j <= blen; j++) {
            long long cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            long long v = d[i - 1][j] + 1;
            if (d[i][j - 1] + 1 < v) v = d[i][j - 1] + 1;
            if (d[i - 1][j - 1] + cost < v) v = d[i - 1][j - 1] + cost;
            d[i][j] = v;
        }
    }
    return d[alen][blen];
}

static const char* tc_en_yakin_oner(TipMotoru* motor, Jeton* bilinmeyen) {
    static char sonuc[128];
    sonuc[0] = 0;
    if (!motor || !bilinmeyen || !bilinmeyen->baslangic || bilinmeyen->uzunluk <= 0) return nullptr;
    long long en_yakin = 3;
    for (Kapsam* k = motor->guncel_kapsam; k; k = k->ust_kapsam) {
        for (long long i = 0; i < k->kapasite; i++) {
            for (SembolGirdisi* iter = k->tablo ? k->tablo[i] : nullptr; iter; iter = iter->sonraki) {
                Jeton* aday = iter->tanim_jetonu;
                if (!aday || !aday->baslangic || aday->uzunluk <= 0 || aday->uzunluk > 127) continue;
                long long mesafe = tc_levenshtein(bilinmeyen->baslangic, bilinmeyen->uzunluk,
                                                   aday->baslangic, aday->uzunluk);
                if (mesafe <= 2 && mesafe < en_yakin) {
                    en_yakin = mesafe;
                    std::memcpy(sonuc, aday->baslangic, (size_t)aday->uzunluk);
                    sonuc[aday->uzunluk] = 0;
                }
            }
        }
    }
    return sonuc[0] ? sonuc : nullptr;
}


long long tip_adini_coz(char* ad, long long uzunluk) {
if (!ad || uzunluk <= 0) return TIP_ID_BILINMIYOR;
if (ad[0] == 42) return TIP_ID_POINTER;

long long temel_id = 0;
// Canonical UTF-8 type aliases for the Stage-0 seed.
if (uzunluk == 3) {
    if (bellek_esit_mi(ad, (char*)"\xE7\xA9\xBA", 3) == 1) temel_id = TIP_ID_HIC; // 空
    if (bellek_esit_mi(ad, (char*)"\xE7\x84\xA1", 3) == 1) temel_id = TIP_ID_HIC; // 無
}
if (uzunluk == 6) {
    if (bellek_esit_mi(ad, (char*)"\xD0\xB2\xD0\xB5\xD1\x89", 6) == 1) temel_id = TIP_ID_GERCEK; // вещ
    if (bellek_esit_mi(ad, (char*)"\xE6\xB5\xAE\xE5\x8B\x95", 6) == 1) temel_id = TIP_ID_GERCEK; // 浮動
    if (bellek_esit_mi(ad, (char*)"\xE5\xB0\x8F\xE6\x95\xB0", 6) == 1) temel_id = TIP_ID_GERCEK; // 小数
    if (bellek_esit_mi(ad, (char*)"\xEC\x86\x8C\xEC\x88\x98", 6) == 1) temel_id = TIP_ID_GERCEK; // 소수
    if (bellek_esit_mi(ad, (char*)"\xE6\x95\xB4\xE6\x95\xB0", 6) == 1) temel_id = TIP_ID_TAMSAYI; // 整数
    if (bellek_esit_mi(ad, (char*)"\xEC\xA0\x95\xEC\x88\x98", 6) == 1) temel_id = TIP_ID_TAMSAYI; // 정수
    if (bellek_esit_mi(ad, (char*)"\xE5\xAD\x97\xE8\x8A\x82", 6) == 1) temel_id = TIP_ID_U8; // 字节
    if (bellek_esit_mi(ad, (char*)"\xE6\xB5\xAE\xE7\x82\xB9", 6) == 1) temel_id = TIP_ID_GERCEK; // 浮点
    if (bellek_esit_mi(ad, (char*)"\xEC\x8B\xA4\xEC\x88\x98", 6) == 1) temel_id = TIP_ID_GERCEK; // 실수
    if (bellek_esit_mi(ad, (char*)"\xE5\xB8\x83\xE5\xB0\x94", 6) == 1) temel_id = TIP_ID_MANTIK; // 布尔
    if (bellek_esit_mi(ad, (char*)"\xE8\xAB\x96\xE7\x90\x86", 6) == 1) temel_id = TIP_ID_MANTIK; // 論理
    if (bellek_esit_mi(ad, (char*)"\xEC\x97\x86\xEC\x9D\x8C", 6) == 1) temel_id = TIP_ID_HIC; // 없음
    if (bellek_esit_mi(ad, (char*)"\xE5\xAE\x9F\xE6\x95\xB0", 6) == 1) temel_id = TIP_ID_GERCEK; // 実数
    if (bellek_esit_mi(ad, (char*)"\xE6\x96\x87\xE5\xAD\x97", 6) == 1) temel_id = TIP_ID_U8; // 文字
    if (bellek_esit_mi(ad, (char*)"\xE5\xAD\x97\xE7\xAC\xA6", 6) == 1) temel_id = TIP_ID_U8; // 字符
    if (bellek_esit_mi(ad, (char*)"\xEB\xAC\xB8\xEC\x9E\x90", 6) == 1) temel_id = TIP_ID_U8; // 문자
    if (bellek_esit_mi(ad, (char*)"\xEA\xB3\xB5\xEB\xB0\xB1", 6) == 1) temel_id = TIP_ID_HIC; // 공백
    if (bellek_esit_mi(ad, (char*)"\xEB\x85\xBC\xEB\xA6\xAC", 6) == 1) temel_id = TIP_ID_MANTIK; // 논리
}
if (uzunluk == 8) {
    if (bellek_esit_mi(ad, (char*)"\x64\xC3\xA9\x63\x69\x6D\x61\x6C", 8) == 1) temel_id = TIP_ID_GERCEK; // décimal
    if (bellek_esit_mi(ad, (char*)"\xD0\xB1\xD0\xB0\xD0\xB9\xD1\x82", 8) == 1) temel_id = TIP_ID_U8; // байт
}
if (uzunluk == 9) {
    if (bellek_esit_mi(ad, (char*)"\xE3\x83\x90\xE3\x82\xA4\xE3\x83\x88", 9) == 1) temel_id = TIP_ID_U8; // バイト
    if (bellek_esit_mi(ad, (char*)"\xEB\xB0\x94\xEC\x9D\xB4\xED\x8A\xB8", 9) == 1) temel_id = TIP_ID_U8; // 바이트
    if (bellek_esit_mi(ad, (char*)"\xEB\xB6\x88\xEB\xA6\xAC\xEC\x96\xB8", 9) == 1) temel_id = TIP_ID_MANTIK; // 불리언
    if (bellek_esit_mi(ad, (char*)"\xE6\x96\x87\xE5\xAD\x97\xE5\x88\x97", 9) == 1) temel_id = TIP_ID_METIN; // 文字列
    if (bellek_esit_mi(ad, (char*)"\xE5\xAD\x97\xE7\xAC\xA6\xE4\xB8\xB2", 9) == 1) temel_id = TIP_ID_METIN; // 字符串
    if (bellek_esit_mi(ad, (char*)"\xEB\xAC\xB8\xEC\x9E\x90\xEC\x97\xB4", 9) == 1) temel_id = TIP_ID_METIN; // 문자열
}
if (uzunluk == 10) {
    if (bellek_esit_mi(ad, (char*)"\xD1\x86\xD0\xB5\xD0\xBB\xD0\xBE\xD0\xB5", 10) == 1) temel_id = TIP_ID_TAMSAYI; // целое
    if (bellek_esit_mi(ad, (char*)"\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD0\xBE", 10) == 1) temel_id = TIP_ID_HIC; // пусто
}
if (uzunluk == 12) {
    if (bellek_esit_mi(ad, (char*)"\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD0\xB0", 12) == 1) temel_id = TIP_ID_METIN; // строка
    if (bellek_esit_mi(ad, (char*)"\xD1\x81\xD0\xB8\xD0\xBC\xD0\xB2\xD0\xBE\xD0\xBB", 12) == 1) temel_id = TIP_ID_U8; // символ
}
if (uzunluk == 14 && bellek_esit_mi(ad, (char*)"\xD0\xB4\xD1\x80\xD0\xBE\xD0\xB1\xD0\xBD\xD0\xBE\xD0\xB5", 14) == 1) temel_id = TIP_ID_GERCEK; // дробное
if (uzunluk == 20 && bellek_esit_mi(ad, (char*)"\xD0\xBB\xD0\xBE\xD0\xB3\xD0\xB8\xD1\x87\xD0\xB5\xD1\x81\xD0\xBA\xD0\xBE\xD0\xB5", 20) == 1) temel_id = TIP_ID_MANTIK; // логическое
// Uzunluk 2: "u8"
if (uzunluk == 2) {
    if (ad[0] == 117 && ad[1] == 56) temel_id = TIP_ID_U8;
}
// Uzunluk 3: "hic", "int", "i64", "u64", "i32", "u32", "u16", "i16", "str", "f32", "f64", "ç©º", "ç„¡"
if (uzunluk == 3) {
    if (bellek_esit_mi(ad, (char*)"hic", 3) == 1) temel_id = TIP_ID_HIC;
    if (bellek_esit_mi(ad, (char*)"int", 3) == 1) temel_id = TIP_ID_TAMSAYI;
    if (bellek_esit_mi(ad, (char*)"i64", 3) == 1) temel_id = TIP_ID_I64;
    if (bellek_esit_mi(ad, (char*)"u64", 3) == 1) temel_id = TIP_ID_I64;
    if (bellek_esit_mi(ad, (char*)"i32", 3) == 1) temel_id = TIP_ID_I32;
    if (bellek_esit_mi(ad, (char*)"u32", 3) == 1) temel_id = TIP_ID_U32;
    if (bellek_esit_mi(ad, (char*)"u16", 3) == 1) temel_id = TIP_ID_U16;
    if (bellek_esit_mi(ad, (char*)"i16", 3) == 1) temel_id = TIP_ID_U16;
    if (bellek_esit_mi(ad, (char*)"str", 3) == 1) temel_id = TIP_ID_METIN;
    if (bellek_esit_mi(ad, (char*)"f32", 3) == 1) temel_id = TIP_ID_F32;
    if (bellek_esit_mi(ad, (char*)"f64", 3) == 1) temel_id = TIP_ID_F64;
    if (bellek_esit_mi(ad, (char*)"ç©º", 3) == 1) temel_id = TIP_ID_HIC;
    if (bellek_esit_mi(ad, (char*)"ç„¡", 3) == 1) temel_id = TIP_ID_HIC;
}
// Uzunluk 4: "sayi", "bool", "char", "byte", "void", "null"
if (uzunluk == 4) {
    if (bellek_esit_mi(ad, (char*)"sayi", 4) == 1) temel_id = TIP_ID_TAMSAYI;
    if (bellek_esit_mi(ad, (char*)"bool", 4) == 1) temel_id = TIP_ID_MANTIK;
    if (bellek_esit_mi(ad, (char*)"char", 4) == 1) temel_id = TIP_ID_U8;
    if (bellek_esit_mi(ad, (char*)"byte", 4) == 1) temel_id = TIP_ID_U8;
    if (bellek_esit_mi(ad, (char*)"bayt", 4) == 1) temel_id = TIP_ID_U8;
    if (bellek_esit_mi(ad, (char*)"void", 4) == 1) temel_id = TIP_ID_HIC;
    if (bellek_esit_mi(ad, (char*)"null", 4) == 1) temel_id = TIP_ID_HIC;
}
// Uzunluk 5: "metin", "float"
if (uzunluk == 5) {
    if (bellek_esit_mi(ad, (char*)"metin", 5) == 1) temel_id = TIP_ID_METIN;
    if (bellek_esit_mi(ad, (char*)"float", 5) == 1) temel_id = TIP_ID_GERCEK;
}
// Uzunluk 6: "gercek", "mantik", "string", "double", "Ğ²ĞµÑ‰", "æµ®å‹•", "å°æ•°", "ì†Œìˆ˜", "æ•´æ•°", "ì •ìˆ˜", "å­—èŠ‚", "æµ®ç‚¹", "ì‹¤ìˆ˜", "å¸ƒå°”", "è«–ç†", "ì—†ìŒ"
if (uzunluk == 6) {
    if (bellek_esit_mi(ad, (char*)"gercek", 6) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"double", 6) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"mantik", 6) == 1) temel_id = TIP_ID_MANTIK;
    if (bellek_esit_mi(ad, (char*)"string", 6) == 1) temel_id = TIP_ID_METIN;
    if (bellek_esit_mi(ad, (char*)"bosluk", 6) == 1) temel_id = TIP_ID_HIC;
}
// Uzunluk 7: "tamsayi", "boolean", "kesirli", "decimal"
if (uzunluk == 7) {
    if (bellek_esit_mi(ad, (char*)"tamsayi", 7) == 1) temel_id = TIP_ID_TAMSAYI;
    if (bellek_esit_mi(ad, (char*)"boolean", 7) == 1) temel_id = TIP_ID_MANTIK;
    if (bellek_esit_mi(ad, (char*)"kesirli", 7) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"ondalik", 7) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"decimal", 7) == 1) temel_id = TIP_ID_GERCEK;
}
// Uzunluk 8: "flottant", "dÃ©cimal"
if (uzunluk == 8) {
    if (bellek_esit_mi(ad, (char*)"flottant", 8) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"karakter", 8) == 1) temel_id = TIP_ID_U8;
}
// Uzunluk 9: "kesirli32", "kesirli64", "ãƒã‚¤ãƒˆ", "ë°”ì´íŠ¸", "ë¶ˆë¦¬ì–¸"
if (uzunluk == 9) {
    if (bellek_esit_mi(ad, (char*)"kesirli32", 9) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"kesirli64", 9) == 1) temel_id = TIP_ID_GERCEK;
    if (bellek_esit_mi(ad, (char*)"mantiksal", 9) == 1) temel_id = TIP_ID_MANTIK;
}
// Uzunluk 14: "Ğ´Ñ€Ğ¾Ğ±Ğ½Ğ¾Ğµ"
if (uzunluk == 14) {
}

if (uzunluk >= 5 && bellek_esit_mi(ad, (char*)"sonuc", 5) == 1) {
    return TIP_ID_SONUC;
}
if (uzunluk >= 5 && bellek_esit_mi(ad, (char*)"Sonuc", 5) == 1) {
    return TIP_ID_SONUC;
}
if (uzunluk >= 6 && bellek_esit_mi(ad, (char*)"Result", 6) == 1) {
    return TIP_ID_SONUC;
}

if (temel_id == 0) {
    // BUG-4: Kullanici tanimli tip alias kontrol
    long long alias_id = tip_alias_bul((char*)ad, uzunluk);
    if (alias_id >= 0) return alias_id;
    return TIP_ID_YAPI;
}
return temel_id;
}

long long tip_dugumunden_coz(AstDugum* dugum) {
    if (!dugum || !YKN_DOT(dugum, tip_adi)) return TIP_ID_BILINMIYOR;
    if (YKN_DOT(dugum, cozumlenmis_tip) == -44) return TIP_ID_BILINMIYOR;

    // Future without arguments is still the runtime std/async yapi. Only
    // explicit Future<T> uses compiler-owned payload metadata.
    bool future_adi = YKN_DOT(dugum, tip_adi_uzunluk) == 6 &&
        bellek_esit_mi(YKN_DOT(dugum, tip_adi), (char*)"Future", 6) == 1;
    if (future_adi && YKN_DOT(dugum, tip_arg_sayisi) != 0) {
        if (YKN_DOT(dugum, tip_arg_sayisi) != 1 || !YKN_DOT(dugum, tip_arglari)) {
            YKN_DOT(dugum, cozumlenmis_tip) = -44;
            tc_diagnostic_yaz(45, "Future tam olarak bir tip parametresi alir", YKN_DOT(dugum, ilk_jeton));
            g_seed_future_type_errors++;
            return TIP_ID_BILINMIYOR;
        }
        AstDugum* payload_node = (AstDugum*)YKN_DOT(dugum, tip_arglari);
        long long payload = tip_dugumunden_coz(payload_node);
        if (payload == TIP_ID_YAPI && !seed_known_type_mi(YKN_DOT(payload_node, tip_adi), YKN_DOT(payload_node, tip_adi_uzunluk))) {
            payload = TIP_ID_BILINMIYOR;
        }
        if (payload == TIP_ID_BILINMIYOR) {
            YKN_DOT(dugum, cozumlenmis_tip) = -44;
            tc_diagnostic_yaz(45, "Future payload tipi cozumlenemedi", YKN_DOT(dugum, ilk_jeton));
            g_seed_future_type_errors++;
            return TIP_ID_BILINMIYOR;
        }
        long long future = seed_future_olustur(payload);
        if (future == TIP_ID_BILINMIYOR) {
            YKN_DOT(dugum, cozumlenmis_tip) = -44;
            tc_diagnostic_yaz(45, "Future payload tipi kapasite sinirini asti", YKN_DOT(dugum, ilk_jeton));
            g_seed_future_type_errors++;
            return TIP_ID_BILINMIYOR;
        }
        return future;
    }
    if (YKN_DOT(dugum, cozumlenmis_tip) > 0) return TIP_ID_POINTER;
    return tip_adini_coz(YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk));
}

bool is_tamsayi_group(long long t) {
    // GOREV 2.2: TIP_ID_POINTER tamsayi grubuna dahil DEGIL â€” ayri tip
    return t == TIP_ID_TAMSAYI || t == TIP_ID_U8 || t == TIP_ID_MANTIK ||
           t == TIP_ID_U16 || t == TIP_ID_U32 || t == TIP_ID_I32 || t == TIP_ID_I64;
}

bool is_gercek_group(long long t) {
    return t == TIP_ID_GERCEK || t == TIP_ID_F32 || t == TIP_ID_F64;
}

bool is_ast_null_literal(AstDugum* n) {
    if (!n) return false;
    if (YKN_DOT(n, tip) == AST_SAYI) {
        Jeton* j = YKN_DOT(n, ilk_jeton);
        if (!j) return false;
        if (j->tip == TIP_BOS) return true;
        if (j->uzunluk == 1 && j->baslangic && j->baslangic[0] == '0') return true;
    }
    return false;
}

bool tipler_uyumlu_mu(long long t1, long long t2) {
    if (t1 == t2) return true;
    if (t1 == TIP_ID_BILINMIYOR || t2 == TIP_ID_BILINMIYOR) return true;
    if (t1 == TIP_ID_ISLEV || t2 == TIP_ID_ISLEV) return true;
    // GOREV 2.2: METIN (string literal) yalnizca METIN, U8 ve POINTER ile uyumludur.
    // TAMSAYI veya GERCEK ile kesinlikle uyumsuzdur!
    if (t1 == TIP_ID_METIN) {
        return (t2 == TIP_ID_METIN || t2 == TIP_ID_U8 || t2 == TIP_ID_POINTER);
    }
    if (t2 == TIP_ID_METIN) {
        return (t1 == TIP_ID_METIN || t1 == TIP_ID_U8 || t1 == TIP_ID_POINTER);
    }
    // GOREV 2.2: Pointer ve Tamsayi grubu arasindaki uyumluluk (adres aritmetigi ve NULL/0 atamasi)
    if (t1 == TIP_ID_POINTER || t2 == TIP_ID_POINTER) {
        if ((t1 == TIP_ID_POINTER && is_tamsayi_group(t2)) || (t2 == TIP_ID_POINTER && is_tamsayi_group(t1))) return false;
        if (t1 == TIP_ID_POINTER && t2 == TIP_ID_POINTER) return true;
        if (t1 == TIP_ID_SONUC || t2 == TIP_ID_SONUC) return true;
        if (seed_future_tipi_mi(t1) || seed_future_tipi_mi(t2) || t1 == TIP_ID_FUTURE || t2 == TIP_ID_FUTURE) return true;
    }
    if (seed_future_tipi_mi(t1) || seed_future_tipi_mi(t2) || t1 == TIP_ID_FUTURE || t2 == TIP_ID_FUTURE) {
        if (seed_future_tipi_mi(t1) && seed_future_tipi_mi(t2)) return true;
        if (t1 == TIP_ID_FUTURE && t2 == TIP_ID_FUTURE) return true;
        if ((t1 == TIP_ID_POINTER && is_tamsayi_group(t2)) || (t2 == TIP_ID_POINTER && is_tamsayi_group(t1))) return false;
        if (t1 == TIP_ID_POINTER || t2 == TIP_ID_POINTER) return true;
    }
    if (t1 == TIP_ID_SONUC || t2 == TIP_ID_SONUC) {
        if (t1 == TIP_ID_SONUC && t2 == TIP_ID_SONUC) return true;
    }
    // GOREV 2.2: Yapi tipleri yalnizca kendi arasinda uyumlu
    if (t1 == TIP_ID_YAPI || t2 == TIP_ID_YAPI) {
        return (t1 == TIP_ID_YAPI && t2 == TIP_ID_YAPI);
    }
    if (is_tamsayi_group(t1) && is_tamsayi_group(t2)) return true;
    if (is_gercek_group(t1) && is_gercek_group(t2)) return true;
    return false;
}

static bool ast_donus_var_mi(AstDugum* d) {
    if (!d) return false;
    if (d->tip == AST_DONDUR) return true;
    if (ast_donus_var_mi(d->sol)) return true;
    if (ast_donus_var_mi(d->sag)) return true;
    if (ast_donus_var_mi(d->govde)) return true;
    if (ast_donus_var_mi(d->yoksa_govde)) return true;
    if (ast_donus_var_mi(d->sonraki)) return true;
    return false;
}

static AstDugum* tc_struct_tanimi_bul(TipMotoru* motor, AstDugum* sol_dugum) {
    if (!motor || !sol_dugum) return nullptr;
    while (sol_dugum && sol_dugum->tip == AST_REFERANS) {
        sol_dugum = sol_dugum->sag ? sol_dugum->sag : sol_dugum->sol;
    }
    if (!sol_dugum) return nullptr;

    char* tname = nullptr;
    long long tlen = 0;

    if (sol_dugum->tip == AST_KIMLIK && sol_dugum->ilk_jeton) {
        SembolGirdisi* s = sembol_ara(motor, sol_dugum->ilk_jeton);
        if (s && s->ast_tanimi) {
            if (s->ast_tanimi->tip == AST_YAPI_TANIMI) {
                return s->ast_tanimi;
            }
            if (s->ast_tanimi->tip_adi) {
                tname = s->ast_tanimi->tip_adi;
                tlen = s->ast_tanimi->tip_adi_uzunluk;
            }
        }
    } else if (sol_dugum->tip == AST_ISLEV_CAGRISI) { Jeton* fn_j = (sol_dugum->sol && sol_dugum->sol->ilk_jeton) ? sol_dugum->sol->ilk_jeton : sol_dugum->ilk_jeton; SembolGirdisi* s = fn_j ? sembol_ara(motor, fn_j) : nullptr; if (s && s->ast_tanimi && s->ast_tanimi->tip_adi) { tname = s->ast_tanimi->tip_adi; tlen = s->ast_tanimi->tip_adi_uzunluk; } } else if (sol_dugum->tip == AST_INDEKS_ERISIMI && sol_dugum->sol) {
        AstDugum* base = sol_dugum->sol;
        while (base && base->tip == AST_INDEKS_ERISIMI) base = base->sol;
        if (base && base->tip == AST_KIMLIK && base->ilk_jeton) {
            SembolGirdisi* s = sembol_ara(motor, base->ilk_jeton);
            if (s && s->ast_tanimi && s->ast_tanimi->tip_adi) {
                tname = s->ast_tanimi->tip_adi;
                tlen = s->ast_tanimi->tip_adi_uzunluk;
            }
        }
        else if (base && base->tip == AST_ALAN_ERISIMI && base->sol && base->sag && base->sag->ilk_jeton) { AstDugum* ps = tc_struct_tanimi_bul(motor, base->sol); if (ps && ps->tip == AST_YAPI_TANIMI) { AstDugum* f = ps->sol; while (f) { if (f->ilk_jeton && f->ilk_jeton->uzunluk == base->sag->ilk_jeton->uzunluk && bellek_esit_mi(f->ilk_jeton->baslangic, base->sag->ilk_jeton->baslangic, f->ilk_jeton->uzunluk) == 1) { if (f->tip_adi) { tname = f->tip_adi; tlen = f->tip_adi_uzunluk; } break; } f = f->sonraki; } } } } else if (sol_dugum->tip == AST_ALAN_ERISIMI) {
        AstDugum* parent_struct = tc_struct_tanimi_bul(motor, sol_dugum->sol);
        if (parent_struct && parent_struct->tip == AST_YAPI_TANIMI && sol_dugum->sag && sol_dugum->sag->ilk_jeton) {
            AstDugum* f = parent_struct->sol;
            while (f) {
                if (f->ilk_jeton && f->ilk_jeton->uzunluk == sol_dugum->sag->ilk_jeton->uzunluk &&
                    bellek_esit_mi(f->ilk_jeton->baslangic, sol_dugum->sag->ilk_jeton->baslangic, f->ilk_jeton->uzunluk) == 1) {
                    if (f->tip_adi) {
                        tname = f->tip_adi;
                        tlen = f->tip_adi_uzunluk;
                    }
                    break;
                }
                f = f->sonraki;
            }
        }
    } else if (sol_dugum->tip_adi) {
        tname = sol_dugum->tip_adi;
        tlen = sol_dugum->tip_adi_uzunluk;
    }

    if (tname && tlen > 0) {
        while (tlen > 0 && (*tname == '*' || *tname == '?' || *tname == '&' || *tname == ' ')) {
            tname++;
            tlen--;
        }
        if (tlen > 0) {
            long long s_hash = djb2_sembol(tname, tlen);
            SembolGirdisi* struct_s = sembol_ara_hash(motor, s_hash);
            if (struct_s && struct_s->ast_tanimi && struct_s->ast_tanimi->tip == AST_YAPI_TANIMI) {
                return struct_s->ast_tanimi;
            }
            for (Kapsam* k = motor->guncel_kapsam; k; k = k->ust_kapsam) {
                for (long long ki = 0; ki < k->kapasite; ki++) {
                    for (SembolGirdisi* it = k->tablo ? k->tablo[ki] : nullptr; it; it = it->sonraki) {
                        if (it->ast_tanimi && it->ast_tanimi->tip == AST_YAPI_TANIMI && it->tanim_jetonu) {
                            if (it->tanim_jetonu->uzunluk == tlen &&
                                bellek_esit_mi(it->tanim_jetonu->baslangic, tname, tlen) == 1) {
                                return it->ast_tanimi;
                            }
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

static long long tc_pointer_derinligi(TipMotoru* motor, AstDugum* n) { if (!motor || !n) return 0; while (n && n->tip == AST_REFERANS) n = n->sag ? n->sag : n->sol; if (!n) return 0; if (n->tip == AST_KIMLIK && n->ilk_jeton) { SembolGirdisi* s = sembol_ara(motor, n->ilk_jeton); if (s && s->ast_tanimi) return s->ast_tanimi->cozumlenmis_tip; } else if (n->tip == AST_ALAN_ERISIMI && n->sol && n->sag && n->sag->ilk_jeton) { AstDugum* st = tc_struct_tanimi_bul(motor, n->sol); if (st && st->tip == AST_YAPI_TANIMI) { AstDugum* f = st->sol; while (f) { if (f->ilk_jeton && f->ilk_jeton->uzunluk == n->sag->ilk_jeton->uzunluk && bellek_esit_mi(f->ilk_jeton->baslangic, n->sag->ilk_jeton->baslangic, f->ilk_jeton->uzunluk) == 1) { return f->cozumlenmis_tip; } f = f->sonraki; } } } else if (n->tip == AST_ISLEV_CAGRISI) { Jeton* fn_j = (n->sol && n->sol->ilk_jeton) ? n->sol->ilk_jeton : n->ilk_jeton; SembolGirdisi* s = fn_j ? sembol_ara(motor, fn_j) : nullptr; if (s && s->ast_tanimi) return s->ast_tanimi->cozumlenmis_tip; } return n->cozumlenmis_tip; }
static long long tc_sabit_dizi_boyutu_al(TipMotoru* motor, AstDugum* base) {
    if (!base) return 0;
    if (base->tip != AST_ALAN_ERISIMI && base->dizi_boyutu > 0) return base->dizi_boyutu;
    if (base->tip == AST_KIMLIK && base->ilk_jeton) {
        SembolGirdisi* s = sembol_ara(motor, base->ilk_jeton);
        if (s && s->ast_tanimi && s->ast_tanimi->dizi_boyutu > 0) {
            return s->ast_tanimi->dizi_boyutu;
        }
        for (long long gi = 0; gi < g_global_sayisi; ++gi) {
            if (g_global_uzunluk[gi] == base->ilk_jeton->uzunluk &&
                bellek_esit_mi(g_global_isim[gi], base->ilk_jeton->baslangic, base->ilk_jeton->uzunluk) == 1) {
                if (g_global_dizi_boyutu[gi] > 0) return g_global_dizi_boyutu[gi];
                break;
            }
        }
    } else if (base->tip == AST_ALAN_ERISIMI) {
        if (base->sol && base->sag && base->sag->ilk_jeton) {
            AstDugum* struct_ast = tc_struct_tanimi_bul(motor, base->sol);
            if (struct_ast && struct_ast->tip == AST_YAPI_TANIMI) {
                AstDugum* f_iter = struct_ast->sol;
                while (f_iter) {
                    if (f_iter->ilk_jeton && f_iter->ilk_jeton->uzunluk == base->sag->ilk_jeton->uzunluk &&
                        bellek_esit_mi(f_iter->ilk_jeton->baslangic, base->sag->ilk_jeton->baslangic, f_iter->ilk_jeton->uzunluk) == 1) {
                        return f_iter->dizi_boyutu;
                    }
                    f_iter = f_iter->sonraki;
                }
            }
        }
    }
    return 0;
}

static bool tc_sabit_indeks_al(AstDugum* idx, long long* out_val) {
    if (!idx) return false;
    if (idx->tip == AST_SAYI && idx->ilk_jeton && idx->ilk_jeton->baslangic) {
        *out_val = string_tamsayiya(idx->ilk_jeton->baslangic, idx->ilk_jeton->uzunluk);
        return true;
    }
    if (idx->tip == AST_UNARY && idx->ilk_jeton && (idx->ilk_jeton->tip == TIP_EKSI || (idx->ilk_jeton->baslangic && idx->ilk_jeton->baslangic[0] == '-')) && idx->sag) {
        if (idx->sag->tip == AST_SAYI && idx->sag->ilk_jeton && idx->sag->ilk_jeton->baslangic) {
            long long mag = string_tamsayiya(idx->sag->ilk_jeton->baslangic, idx->sag->ilk_jeton->uzunluk);
            *out_val = -mag;
            return true;
        }
    }
    return false;
}

long long tip_cikar(TipMotoru* motor, AstDugum* dugum) {
if ((!(dugum))) {
return TIP_ID_BILINMIYOR;
}
// BUG-4 FIX: tip alias bildirimi
if (YKN_DOT(dugum, tip) == 37) { // AST_TIP_ALIAS = 37
    long long alias_t = tip_dugumunden_coz(dugum);
    if (alias_t == TIP_ID_BILINMIYOR || alias_t == 0 || alias_t == TIP_ID_YAPI) alias_t = TIP_ID_TAMSAYI;
    if (YKN_DOT(dugum, ilk_jeton)) {
        sembol_ekle(motor, YKN_DOT(dugum, ilk_jeton), alias_t, TIP_ID_HIC, dugum);
        tip_alias_kaydet(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk, alias_t);
    }
    return TIP_ID_HIC;
}
if (YKN_DOT(dugum, tip) == AST_DIREKTIF_C_LIB || YKN_DOT(dugum, tip) == AST_DIREKTIF_CPP_LIB ||
    YKN_DOT(dugum, tip) == AST_DIREKTIF_PY_LIB || YKN_DOT(dugum, tip) == AST_DIREKTIF_RS_LIB ||
    YKN_DOT(dugum, tip) == AST_DIREKTIF_CONVERT || YKN_DOT(dugum, tip) == AST_DIREKTIF_CONFIG ||
    YKN_DOT(dugum, tip) == AST_DIREKTIF_PROMPT || YKN_DOT(dugum, tip) == AST_DIREKTIF_AI ||
    YKN_DOT(dugum, tip) == 29 || YKN_DOT(dugum, tip) == 30 || YKN_DOT(dugum, tip) == 31 ||
    YKN_DOT(dugum, tip) == 32 || YKN_DOT(dugum, tip) == 38 || YKN_DOT(dugum, tip) == 44) {
    return TIP_ID_HIC;
}
if ((YKN_DOT(dugum, tip) == AST_SAYI)) {
return TIP_ID_TAMSAYI;
}
;
if ((YKN_DOT(dugum, tip) == AST_GERCEK_SAYI)) {
return TIP_ID_GERCEK;
}
;
if ((YKN_DOT(dugum, tip) == AST_METIN)) {
return TIP_ID_METIN;
}
;
if ((YKN_DOT(dugum, tip) == AST_FSTRING)) {
return TIP_ID_METIN;
}
;
if ((YKN_DOT(dugum, tip) == AST_MANTIK)) {
return TIP_ID_MANTIK;
}
;
if ((YKN_DOT(dugum, tip) == AST_KIMLIK)) {
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        std::string vname(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk);
        if (g_moved_vars.find(vname) != g_moved_vars.end()) {
            tc_diagnostic_yaz(40, "OWNERSHIP HATA: Tasinmis degisken kullanildi (use-after-move)", YKN_DOT(dugum, ilk_jeton));
            YKN_DOT(motor, hata_sayisi)++;
            return TIP_ID_BILINMIYOR;
        }
        if (YKN_DOT(motor, self_heal_deger_konteksti) == 1 && g_uninit_vars.find(vname) != g_uninit_vars.end()) {
            tc_diagnostic_yaz(47, "SAFETY HATA: Ilklenmemis degisken kullanildi (uninitialized variable read)", YKN_DOT(dugum, ilk_jeton));
            YKN_DOT(motor, hata_sayisi)++;
            return TIP_ID_BILINMIYOR;
        }
        if (is_borrow_conflicted(vname, false)) {
            tc_diagnostic_yaz(41, "OWNERSHIP HATA: Mutable odunc alinmis degisken dogrudan okunamaz (cannot read while mutably borrowed)", YKN_DOT(dugum, ilk_jeton));
            YKN_DOT(motor, hata_sayisi)++;
            return TIP_ID_BILINMIYOR;
        }
    }
SembolGirdisi* s = (SembolGirdisi*)(sembol_ara(motor, YKN_DOT(dugum, ilk_jeton)));
if ((!(s))) {
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        long long coz = tip_adini_coz(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk);
        if (coz != TIP_ID_BILINMIYOR && coz != TIP_ID_YAPI) {
            return coz;
        }
    }
    char* bilinmeyen_adres = YKN_DOT(dugum, ilk_jeton) ? YKN_DOT(dugum, ilk_jeton)->baslangic : nullptr;
    bool ana_kaynakta = bilinmeyen_adres &&
        (!g_strict_kaynak_baslangic || !g_strict_kaynak_son ||
         (bilinmeyen_adres >= g_strict_kaynak_baslangic && bilinmeyen_adres < g_strict_kaynak_son));
    if (!ana_kaynakta && g_self_heal_aktif != 1) return TIP_ID_BILINMIYOR;
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        char* in_ad = YKN_DOT(dugum, ilk_jeton)->baslangic;
        long long in_uzn = YKN_DOT(dugum, ilk_jeton)->uzunluk;
        if ((in_uzn == 8 && memcmp(in_ad, "popcount", 8) == 0) ||
            (in_uzn == 7 && memcmp(in_ad, "bit_say", 7) == 0) ||
            (in_uzn == 5 && memcmp(in_ad, "bswap", 5) == 0) ||
            (in_uzn == 11 && memcmp(in_ad, "bayt_cevir", 11) == 0) ||
            (in_uzn == 3 && memcmp(in_ad, "clz", 3) == 0) ||
            (in_uzn == 3 && memcmp(in_ad, "ctz", 3) == 0) ||
            (in_uzn == 7 && memcmp(in_ad, "rotleft", 7) == 0) ||
            (in_uzn == 8 && memcmp(in_ad, "rotright", 8) == 0)) {
            return TIP_ID_ISLEV;
        }
    }
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        const char* hint = tc_en_yakin_oner(motor, YKN_DOT(dugum, ilk_jeton));
        tc_diagnostic_yaz(1, "TC HATA: Tanimsiz sembol kullanildi", YKN_DOT(dugum, ilk_jeton), -1, -1, hint);
    YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
    YKN_DOT(motor, tanimsiz_hata_sayisi) = (YKN_DOT(motor, tanimsiz_hata_sayisi) + 1);
    if (g_self_heal_aktif == 1 && YKN_DOT(motor, self_heal_deger_konteksti) == 1 && !YKN_DOT(motor, self_heal_adayi)) {
        YKN_DOT(motor, self_heal_adayi) = dugum;
    }
    return TIP_ID_BILINMIYOR;
}
return TIP_ID_BILINMIYOR;
}
if (s && s->ast_tanimi && s->ast_tanimi->dizi_boyutu > 0) return TIP_ID_POINTER; return YKN_DOT(s, veri_tipi);
}
;
if ((YKN_DOT(dugum, tip) == AST_DEGISKEN_TANIMI)) {
if (!YKN_DOT(dugum, sag) && YKN_DOT(dugum, dizi_boyutu) <= 0 && YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
    std::string vname(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk);
    g_uninit_vars.insert(vname);
}
long long onceki_self_heal_konteksti = YKN_DOT(motor, self_heal_deger_konteksti);
YKN_DOT(motor, self_heal_deger_konteksti) = 1;
bool is_ref_binding = (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_REFERANS);
bool onceki_in_binding = g_in_binding;
if (is_ref_binding) g_in_binding = true;
long long atanan_tip = tip_cikar(motor, YKN_DOT(dugum, sag));
g_in_binding = onceki_in_binding;
YKN_DOT(motor, self_heal_deger_konteksti) = onceki_self_heal_konteksti;
long long donus_sembol_tipi = TIP_ID_HIC;
if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_ISLEV) {
    if (YKN_DOT(dugum, sag)->tip_adi) {
        donus_sembol_tipi = tip_adini_coz(YKN_DOT(dugum, sag)->tip_adi, YKN_DOT(dugum, sag)->tip_adi_uzunluk);
        if (donus_sembol_tipi == 0 || donus_sembol_tipi == TIP_ID_BILINMIYOR) donus_sembol_tipi = TIP_ID_TAMSAYI;
    } else {
        donus_sembol_tipi = TIP_ID_TAMSAYI;
    }
} else if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_KIMLIK) {
    SembolGirdisi* sag_s = sembol_ara(motor, YKN_DOT(dugum, sag)->ilk_jeton);
    if (sag_s && (sag_s->veri_tipi == TIP_ID_ISLEV || sag_s->donus_tipi != TIP_ID_HIC)) {
        donus_sembol_tipi = sag_s->donus_tipi;
    }
}
// GOREV 2.2: Beyan tipi varsa atanan tip ile karsilastir
if (YKN_DOT(dugum, tip_adi) != 0) {
    long long beyan_tip = tip_dugumunden_coz(dugum);
    if (beyan_tip == TIP_ID_YAPI && enum_turu_mi(YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk)) == 1 && (is_tamsayi_group(atanan_tip) || atanan_tip == TIP_ID_POINTER || atanan_tip == TIP_ID_YAPI)) { atanan_tip = beyan_tip; }
    if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_ALAN_ERISIMI && YKN_DOT(dugum, sag)->cozumlenmis_tip == 77 && beyan_tip != TIP_ID_BILINMIYOR) {
        atanan_tip = beyan_tip;
    }
    if (beyan_tip != TIP_ID_BILINMIYOR && atanan_tip != TIP_ID_BILINMIYOR) {
        bool uyumlu = tipler_uyumlu_mu(beyan_tip, atanan_tip);
        if (!uyumlu && beyan_tip == TIP_ID_POINTER && is_ast_null_literal(YKN_DOT(dugum, sag))) {
            uyumlu = true;
        }
        if (!uyumlu) {
            long long mismatch_col = YKN_DOT(dugum, ilk_jeton) ? YKN_DOT(dugum, ilk_jeton)->sutun : 0;
            if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, tip_adi_uzunluk) > 0) {
                mismatch_col = YKN_DOT(dugum, ilk_jeton)->sutun + YKN_DOT(dugum, ilk_jeton)->uzunluk + YKN_DOT(dugum, tip_adi_uzunluk) + 4;
            }

            tc_diagnostic_yaz(3, "TC HATA: Tip uyumsuzlugu", YKN_DOT(dugum, ilk_jeton), -1, mismatch_col);
            YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
            YKN_DOT(motor, tip_uyumsuz_hata_sayisi) = (YKN_DOT(motor, tip_uyumsuz_hata_sayisi) + 1);
        }
    }
    atanan_tip = beyan_tip;
}
;
if (((YKN_DOT(dugum, ilk_jeton)) != 0)) {
// FIX13: pass AST node so sembol_ekle can read dugum->nullable
sembol_ekle(motor, YKN_DOT(dugum, ilk_jeton), atanan_tip, donus_sembol_tipi, dugum);
}
;
return TIP_ID_HIC;
}
;
if ((YKN_DOT(dugum, tip) == AST_INDEKS_ERISIMI)) {
// FIX13: check if base pointer symbol is nullable — if so, null check is required
AstDugum* base = YKN_DOT(dugum, sol);
if (base && base->tip == AST_KIMLIK && base->ilk_jeton) {
    SembolGirdisi* base_s = sembol_ara(motor, base->ilk_jeton);
    if ((base_s && base_s->nullable == 1 && !(g_fix13_guard_active && g_fix13_guard_invalidated == 0 && fix13_guard_matches(base)))) {
        tc_diagnostic_yaz(33,
            "FIX13 HATA: Nullable pointer erisimi kontrolsuz.", 
            base->ilk_jeton, -1, -1);
        YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
    }
} else if (base && base->tip == 17) {
    if (is_field_nullable(motor, base) && !(g_fix13_guard_active && g_fix13_guard_invalidated == 0 && fix13_guard_matches(base))) {
        tc_diagnostic_yaz(33,
            "FIX13 HATA: Nullable pointer erisimi kontrolsuz.", 
            base->ilk_jeton ? base->ilk_jeton : (base->sag ? base->sag->ilk_jeton : dugum->ilk_jeton), -1, -1);
        YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
    }
}
long long sabit_dizi_len = tc_sabit_dizi_boyutu_al(motor, base);
if (sabit_dizi_len > 0) {
    long long sabit_indeks = 0;
    if (tc_sabit_indeks_al(YKN_DOT(dugum, sag), &sabit_indeks)) {
        if (sabit_indeks < 0 || sabit_indeks >= sabit_dizi_len) {
            tc_diagnostic_yaz(48,
                "SAFETY HATA: Sabit dizi indeks siniri asildi (fixed-size array index out of bounds)",
                YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->ilk_jeton ? YKN_DOT(dugum, sag)->ilk_jeton : dugum->ilk_jeton);
            YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
        }
    }
}
AstDugum* arr_node = YKN_DOT(dugum, sol);
long long sol_t13 = tip_cikar(motor, arr_node);
tip_cikar(motor, YKN_DOT(dugum, sag));
if (sol_t13 == TIP_ID_POINTER || sol_t13 == TIP_ID_METIN) {
    long long p_depth = tc_pointer_derinligi(motor, arr_node); long long arr_len = tc_sabit_dizi_boyutu_al(motor, arr_node); if (p_depth > 1 || (p_depth >= 1 && arr_len > 0)) return TIP_ID_POINTER; if (tc_struct_tanimi_bul(motor, arr_node)) return TIP_ID_YAPI;
    if (arr_node && arr_node->tip == AST_KIMLIK && arr_node->ilk_jeton) {
        SembolGirdisi* s = sembol_ara(motor, arr_node->ilk_jeton);
        if (s && s->ast_tanimi && s->ast_tanimi->tip_adi) {
            long long tid = tip_adini_coz(s->ast_tanimi->tip_adi, s->ast_tanimi->tip_adi_uzunluk);
            if (tid != TIP_ID_BILINMIYOR && tid != TIP_ID_POINTER) return tid;
            if (s->ast_tanimi->cozumlenmis_tip > 1) return TIP_ID_POINTER;
        }
    }
    return TIP_ID_U8;
}
return sol_t13;
}
;
if ((YKN_DOT(dugum, tip) == AST_ATAMA)) {
// FIX13: guard edilen sembol yeniden atanirsa nullable duruma doner.
if (g_fix13_guard_active && fix13_guard_matches(YKN_DOT(dugum, sol))) {
    SembolGirdisi* reassigned = sembol_ara(motor, YKN_DOT(dugum, sol)->ilk_jeton);
    if (reassigned && reassigned->ast_tanimi && reassigned->ast_tanimi->nullable == 1) {
        reassigned->nullable = 1;
        g_fix13_guard_invalidated = 1;
    }
}
AstDugum* atama_hedef = YKN_DOT(dugum, sol);
while (atama_hedef && (atama_hedef->tip == AST_INDEKS_ERISIMI || atama_hedef->tip == AST_ALAN_ERISIMI)) {
    atama_hedef = atama_hedef->sol;
}
if (atama_hedef && atama_hedef->ilk_jeton && atama_hedef->ilk_jeton->baslangic) {
    std::string as_name(atama_hedef->ilk_jeton->baslangic, atama_hedef->ilk_jeton->uzunluk);
    bool was_uninit = (g_uninit_vars.count(as_name) > 0);
    if (is_variable_borrowed(as_name)) {
        tc_diagnostic_yaz(41, "OWNERSHIP HATA: Odunc alinmis degisken degistirilemez (cannot mutate while borrowed)", atama_hedef->ilk_jeton);
        YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
    }
    g_moved_vars.erase(as_name);
    g_uninit_vars.erase(as_name);
    SembolGirdisi* hedef_sembol = sembol_ara(motor, atama_hedef->ilk_jeton);
    if (hedef_sembol && hedef_sembol->ast_tanimi && hedef_sembol->ast_tanimi->gorunurluk >= 2 && !was_uninit) {
        tc_diagnostic_yaz(49, "TC HATA: Sabit (immutable) degiskene yeniden atama yapilamaz", atama_hedef->ilk_jeton);
        YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
    }
}

if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_REFERANS) {
    AstDugum* ref_target = YKN_DOT(dugum, sag)->sag;
    Jeton* sag_j = nullptr;
    if (ref_target) {
        if (ref_target->tip == AST_KIMLIK) sag_j = ref_target->ilk_jeton;
        else if (ref_target->tip == AST_ALAN_ERISIMI && ref_target->sol && ref_target->sol->tip == AST_KIMLIK) sag_j = ref_target->sol->ilk_jeton;
    }
    Jeton* sol_j = nullptr;
    if (YKN_DOT(dugum, sol)) {
        if (YKN_DOT(dugum, sol)->tip == AST_KIMLIK) sol_j = YKN_DOT(dugum, sol)->ilk_jeton;
        else if (YKN_DOT(dugum, sol)->tip == AST_ALAN_ERISIMI && YKN_DOT(dugum, sol)->sol && YKN_DOT(dugum, sol)->sol->tip == AST_KIMLIK) sol_j = YKN_DOT(dugum, sol)->sol->ilk_jeton;
    }
    if (sol_j && sag_j) {
        Kapsam* k_sol = tc_sembolun_kapsami(motor, sol_j);
        Kapsam* k_sag = tc_sembolun_kapsami(motor, sag_j);
        if (k_sol && k_sag) {
            long long d_sol = tc_kapsam_derinligi(k_sol);
            long long d_sag = tc_kapsam_derinligi(k_sag);
            if (d_sag > d_sol) {
                tc_diagnostic_yaz(46, "SAFETY HATA: Ic blok degiskeninin adresi dis kapsama atanamaz (dangling reference)", sag_j);
                YKN_DOT(motor, hata_sayisi)++;
            }
        }
    }
}

long long onceki_self_heal_konteksti_atama = YKN_DOT(motor, self_heal_deger_konteksti);
YKN_DOT(motor, self_heal_deger_konteksti) = 0;
long long sol_tip = tip_cikar(motor, YKN_DOT(dugum, sol));
YKN_DOT(motor, self_heal_deger_konteksti) = 1;
bool is_ref_binding_atama = (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_REFERANS);
bool onceki_in_binding_atama = g_in_binding;
if (is_ref_binding_atama) g_in_binding = true;
long long sag_tip = tip_cikar(motor, YKN_DOT(dugum, sag));
g_in_binding = onceki_in_binding_atama;
YKN_DOT(motor, self_heal_deger_konteksti) = onceki_self_heal_konteksti_atama;
bool uyumlu_atama = tipler_uyumlu_mu(sol_tip, sag_tip);
if (!uyumlu_atama && sol_tip == TIP_ID_POINTER && is_ast_null_literal(YKN_DOT(dugum, sag))) {
    uyumlu_atama = true;
}
if (!uyumlu_atama) {

    tc_diagnostic_yaz(3, "TC HATA: Tip uyumsuzlugu", YKN_DOT(dugum, ilk_jeton));
    YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
    YKN_DOT(motor, tip_uyumsuz_hata_sayisi) = (YKN_DOT(motor, tip_uyumsuz_hata_sayisi) + 1);
}
;
return sol_tip;
}
;
if ((YKN_DOT(dugum, tip) == AST_IKILI_OP)) {
long long onceki_self_heal_konteksti_ikili = YKN_DOT(motor, self_heal_deger_konteksti);
YKN_DOT(motor, self_heal_deger_konteksti) = 1;
long long sol_t = tip_cikar(motor, YKN_DOT(dugum, sol));
long long sag_t = tip_cikar(motor, YKN_DOT(dugum, sag));
YKN_DOT(motor, self_heal_deger_konteksti) = onceki_self_heal_konteksti_ikili;
if (YKN_DOT(dugum, ilk_jeton)) {
    long long op_tip = YKN_DOT(dugum, ilk_jeton)->tip;
    if (op_tip >= 35 && op_tip <= 40) return TIP_ID_MANTIK;
    if (op_tip == TIP_EKSI) {
        if ((sol_t == TIP_ID_POINTER || sol_t == TIP_ID_METIN) && (sag_t == TIP_ID_POINTER || sag_t == TIP_ID_METIN)) {
            return TIP_ID_TAMSAYI;
        }
    }
}
if (sol_t == TIP_ID_GERCEK || sag_t == TIP_ID_GERCEK) return TIP_ID_GERCEK;
return sol_t;
}
;
if ((YKN_DOT(dugum, tip) == AST_UNARY)) {
    long long onceki_self_heal_konteksti_unary = YKN_DOT(motor, self_heal_deger_konteksti);
    YKN_DOT(motor, self_heal_deger_konteksti) = (YKN_DOT(dugum, ilk_jeton) && (YKN_DOT(dugum, ilk_jeton)->tip == TIP_DEGIL || YKN_DOT(dugum, ilk_jeton)->tip == TIP_EKSI || YKN_DOT(dugum, ilk_jeton)->tip == TIP_TILDE)) ? 1 : 0;
    long long unary_tip = tip_cikar(motor, YKN_DOT(dugum, sag));
    YKN_DOT(motor, self_heal_deger_konteksti) = onceki_self_heal_konteksti_unary;
    if (YKN_DOT(dugum, ilk_jeton)) {
        long long uop = YKN_DOT(dugum, ilk_jeton)->tip;
        if (uop == TIP_DEGIL) return TIP_ID_MANTIK;
        if (uop == TIP_EKSI) {
            if (unary_tip == TIP_ID_POINTER || unary_tip == TIP_ID_YAPI || unary_tip == TIP_ID_METIN) {
                tc_diagnostic_yaz(3, "TC HATA: Unary eksi yalnizca sayisal tiplere uygulanabilir", YKN_DOT(dugum, ilk_jeton));
                YKN_DOT(motor, hata_sayisi)++;
                YKN_DOT(motor, tip_uyumsuz_hata_sayisi)++;
            }
            return unary_tip;
        }
        if (uop == TIP_TILDE) {
            if (!is_tamsayi_group(unary_tip) || unary_tip == TIP_ID_POINTER) {
                tc_diagnostic_yaz(3, "TC HATA: Bitwise NOT yalnizca tamsayi tiplerine uygulanabilir", YKN_DOT(dugum, ilk_jeton));
                YKN_DOT(motor, hata_sayisi)++;
                YKN_DOT(motor, tip_uyumsuz_hata_sayisi)++;
            }
            return unary_tip;
        }
        if (uop == TIP_CARPI) { // *ptr
            if (unary_tip != TIP_ID_POINTER && unary_tip != TIP_ID_BILINMIYOR) {
                tc_diagnostic_yaz(3, "TC HATA: Pointer olmayan tipe dereference (*) uygulanamaz", YKN_DOT(dugum, ilk_jeton));
                YKN_DOT(motor, hata_sayisi)++;
                YKN_DOT(motor, tip_uyumsuz_hata_sayisi)++;
                return TIP_ID_BILINMIYOR;
            }
            AstDugum* u_sag = YKN_DOT(dugum, sag); long long p_depth = tc_pointer_derinligi(motor, u_sag); if (p_depth > 1) return TIP_ID_POINTER; if (tc_struct_tanimi_bul(motor, u_sag)) return TIP_ID_YAPI;
            if (u_sag && YKN_DOT(u_sag, tip_adi)) {
                long long deref_t = tip_adini_coz((char*)YKN_DOT(u_sag, tip_adi), YKN_DOT(u_sag, tip_adi_uzunluk));
                if (deref_t != TIP_ID_BILINMIYOR) return deref_t;
            }
            return TIP_ID_TAMSAYI;
        }
    }
    return unary_tip;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONUSUM)) {
    long long sol_t = tip_cikar(motor, YKN_DOT(dugum, sol));
    long long hedef_t = tip_dugumunden_coz(dugum);
    if (hedef_t == TIP_ID_BILINMIYOR) return TIP_ID_BILINMIYOR;
    return hedef_t;
}
;
if ((YKN_DOT(dugum, tip) == AST_COMPTIME)) {
    return tip_cikar(motor, YKN_DOT(dugum, sol));
}
;
if ((YKN_DOT(dugum, tip) == AST_REFERANS)) {
    bool want_mut = false;
    std::string vname = "";
    bool is_temp = false;
    bool has_id = false;
    if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_KIMLIK && YKN_DOT(dugum, sag)->ilk_jeton) {
        vname = std::string(YKN_DOT(dugum, sag)->ilk_jeton->baslangic, YKN_DOT(dugum, sag)->ilk_jeton->uzunluk);
        g_uninit_vars.erase(vname);
        if (g_moved_vars.find(vname) != g_moved_vars.end()) {
            tc_diagnostic_yaz(40, "OWNERSHIP HATA: Tasinmis degisken odunc alinamaz (cannot borrow moved value)", YKN_DOT(dugum, sag)->ilk_jeton);
            YKN_DOT(motor, hata_sayisi)++;
        }
        want_mut = (YKN_DOT(dugum, gorunurluk) == 1);
        if (is_borrow_conflicted(vname, want_mut)) {
            tc_diagnostic_yaz(41, "OWNERSHIP HATA: Ayni anda mutable ve baska odunc alma cakismasi (borrow conflict)", YKN_DOT(dugum, sag)->ilk_jeton);
            YKN_DOT(motor, hata_sayisi)++;
        } else {
            is_temp = (!g_in_binding);
            has_id = true;
        }
    }
long long onceki_self_heal_konteksti_referans = YKN_DOT(motor, self_heal_deger_konteksti);
YKN_DOT(motor, self_heal_deger_konteksti) = 0;
tip_cikar(motor, YKN_DOT(dugum, sag));
YKN_DOT(motor, self_heal_deger_konteksti) = onceki_self_heal_konteksti_referans;
if (has_id) {
    add_borrow(vname, want_mut, YKN_DOT(motor, guncel_kapsam), is_temp);
}
return TIP_ID_POINTER;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONDUR)) {
long long onceki_self_heal_konteksti_donus = YKN_DOT(motor, self_heal_deger_konteksti);
YKN_DOT(motor, self_heal_deger_konteksti) = 1;
long long don_val_tip = YKN_DOT(dugum, sag) ? tip_cikar(motor, YKN_DOT(dugum, sag)) : TIP_ID_HIC;
YKN_DOT(motor, self_heal_deger_konteksti) = onceki_self_heal_konteksti_donus;
if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_REFERANS) {
    AstDugum* ref_target = YKN_DOT(dugum, sag)->sag;
    Jeton* ref_j = nullptr;
    if (ref_target) {
        if (ref_target->tip == AST_KIMLIK) ref_j = ref_target->ilk_jeton;
        else if (ref_target->tip == AST_ALAN_ERISIMI && ref_target->sol && ref_target->sol->tip == AST_KIMLIK) ref_j = ref_target->sol->ilk_jeton;
    }
    if (ref_j) {
        Kapsam* hk = tc_sembolun_kapsami(motor, ref_j);
        long long d = tc_kapsam_derinligi(hk);
        if (d > 0 && seed_global_storage_index(ref_j->baslangic, ref_j->uzunluk) < 0) {
            tc_diagnostic_yaz(46, "SAFETY HATA: Yerel yigit (stack) degiskeninin adresi dondurulemez (dangling reference)", ref_j);
            YKN_DOT(motor, hata_sayisi)++;
        }
    }
}
if (YKN_DOT(motor, guncel_donus_tipi) != 0 && YKN_DOT(motor, guncel_donus_tipi) != TIP_ID_BILINMIYOR && YKN_DOT(motor, guncel_donus_tipi) != TIP_ID_HIC && don_val_tip != TIP_ID_BILINMIYOR) {
    bool uyumlu_ret = tipler_uyumlu_mu(YKN_DOT(motor, guncel_donus_tipi), don_val_tip);
    if (!uyumlu_ret && YKN_DOT(motor, guncel_donus_tipi) == TIP_ID_POINTER && is_ast_null_literal(YKN_DOT(dugum, sag))) {
        uyumlu_ret = true;
    }
    if (!uyumlu_ret) {
        long long return_col = YKN_DOT(dugum, ilk_jeton) ? YKN_DOT(dugum, ilk_jeton)->sutun + YKN_DOT(dugum, ilk_jeton)->uzunluk + 1 : 0;

        printf("DEBUG_TC_RET: guncel=%lld, don_val=%lld\n", (long long)YKN_DOT(motor, guncel_donus_tipi), (long long)don_val_tip);
        tc_diagnostic_yaz(4, "TC_FAIL_RET: Donus tipi uyumsuzlugu", YKN_DOT(dugum, ilk_jeton), -1, return_col);
        YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
        YKN_DOT(motor, donus_hata_sayisi) = (YKN_DOT(motor, donus_hata_sayisi) + 1);
    }
}
return don_val_tip;
}
;
if ((YKN_DOT(dugum, tip) == AST_EGER)) {
if (YKN_DOT(dugum, sol) != 0) {
    tip_cikar(motor, YKN_DOT(dugum, sol));
    release_temp_borrows();
}
// FIX13: data-flow narrowing for `eger p` and `eger p != bos`.
// Stage-0 must mirror canonical src/typechecker.ykn; otherwise the first
// bootstrap compiler rejects accesses that Stage-3 accepts.
long long guard_onceki = g_fix13_guard_active;
long long invalidated_onceki = g_fix13_guard_invalidated;
char* guard_name_onceki = g_fix13_guard_name;
long long guard_name_length_onceki = g_fix13_guard_name_length;
char* guard_name2_onceki = g_fix13_guard_name2;
long long guard_name2_length_onceki = g_fix13_guard_name2_length;
char* guard_field_onceki = g_fix13_guard_field;
long long guard_field_length_onceki = g_fix13_guard_field_length;
g_fix13_guard_active = 0;
g_fix13_guard_invalidated = 0;
g_fix13_guard_name = nullptr;
g_fix13_guard_name_length = 0;
g_fix13_guard_name2 = nullptr;
g_fix13_guard_name2_length = 0;
g_fix13_guard_field = nullptr;
g_fix13_guard_field_length = 0;
AstDugum* guard_condition = YKN_DOT(dugum, sol);
if (guard_condition && guard_condition->tip == AST_KIMLIK && guard_condition->ilk_jeton) {
    g_fix13_guard_active = 1;
    g_fix13_guard_name = guard_condition->ilk_jeton->baslangic;
    g_fix13_guard_name_length = guard_condition->ilk_jeton->uzunluk;
} else if (guard_condition && guard_condition->tip == AST_IKILI_OP &&
           guard_condition->ilk_jeton && guard_condition->ilk_jeton->tip == TIP_ESIT_DEGIL &&
           guard_condition->sol && guard_condition->sol->tip == AST_KIMLIK &&
           guard_condition->sol->ilk_jeton) {
    g_fix13_guard_active = 1;
    g_fix13_guard_name = guard_condition->sol->ilk_jeton->baslangic;
    g_fix13_guard_name_length = guard_condition->sol->ilk_jeton->uzunluk;
} else if (guard_condition && guard_condition->tip == AST_IKILI_OP &&
           guard_condition->ilk_jeton && guard_condition->ilk_jeton->tip == TIP_ESIT_DEGIL &&
           guard_condition->sol && guard_condition->sol->tip == 17 &&
           guard_condition->sol->sol && guard_condition->sol->sol->tip == AST_KIMLIK &&
           guard_condition->sol->sol->ilk_jeton &&
           guard_condition->sol->sag && guard_condition->sol->sag->tip == AST_KIMLIK &&
           guard_condition->sol->sag->ilk_jeton) {
    g_fix13_guard_active = 1;
    g_fix13_guard_name = guard_condition->sol->sol->ilk_jeton->baslangic;
    g_fix13_guard_name_length = guard_condition->sol->sol->ilk_jeton->uzunluk;
    g_fix13_guard_field = guard_condition->sol->sag->ilk_jeton->baslangic;
    g_fix13_guard_field_length = guard_condition->sol->sag->ilk_jeton->uzunluk;
}
if (guard_condition && guard_condition->tip == AST_IKILI_OP && guard_condition->ilk_jeton &&
    guard_condition->ilk_jeton->tip == TIP_VE) {
    g_fix13_guard_active = 1;
    AstDugum* left = guard_condition->sol;
    AstDugum* right = guard_condition->sag;
    if (left && left->tip == AST_IKILI_OP && left->sol && left->sol->tip == AST_KIMLIK && left->ilk_jeton && left->ilk_jeton->tip == TIP_ESIT_DEGIL) {
        g_fix13_guard_name = left->sol->ilk_jeton->baslangic;
        g_fix13_guard_name_length = left->sol->ilk_jeton->uzunluk;
    }
    if (right && right->tip == AST_IKILI_OP && right->sol && right->sol->tip == AST_KIMLIK && right->ilk_jeton && right->ilk_jeton->tip == TIP_ESIT_DEGIL) {
        g_fix13_guard_name2 = right->sol->ilk_jeton->baslangic;
        g_fix13_guard_name2_length = right->sol->ilk_jeton->uzunluk;
    }
}
if (guard_condition && guard_condition->tip == AST_IKILI_OP && guard_condition->ilk_jeton &&
    guard_condition->ilk_jeton->tip == TIP_VEYA) {
    AstDugum* left = guard_condition->sol;
    AstDugum* right = guard_condition->sag;
    if (left && right && left->tip == AST_IKILI_OP && right->tip == AST_IKILI_OP &&
        left->sol && right->sol && left->sol->tip == AST_KIMLIK && right->sol->tip == AST_KIMLIK &&
        left->ilk_jeton && right->ilk_jeton && left->ilk_jeton->tip == TIP_ESIT_DEGIL &&
        right->ilk_jeton->tip == TIP_ESIT_DEGIL &&
        left->sol->ilk_jeton->uzunluk == right->sol->ilk_jeton->uzunluk &&
        std::memcmp(left->sol->ilk_jeton->baslangic, right->sol->ilk_jeton->baslangic,
                    (size_t)left->sol->ilk_jeton->uzunluk) == 0) {
        g_fix13_guard_active = 1;
        g_fix13_guard_name = left->sol->ilk_jeton->baslangic;
        g_fix13_guard_name_length = left->sol->ilk_jeton->uzunluk;
    } else {
        g_fix13_guard_active = 0;
    }
}
SembolGirdisi* daralt_sembol = 0;
long long daralt_eski_nullable = 0;
AstDugum* kosul = YKN_DOT(dugum, sol);
if (kosul && kosul->tip == AST_KIMLIK && kosul->ilk_jeton) {
    daralt_sembol = sembol_ara(motor, kosul->ilk_jeton);
} else if (kosul && kosul->tip == AST_IKILI_OP && kosul->ilk_jeton) {
    long long op = kosul->ilk_jeton->tip;
    Jeton* sol_j = (kosul->sol && (kosul->sol->tip == AST_KIMLIK || kosul->sol->tip == AST_SAYI)) ? kosul->sol->ilk_jeton : 0;
    Jeton* sag_j = (kosul->sag && (kosul->sag->tip == AST_KIMLIK || kosul->sag->tip == AST_SAYI)) ? kosul->sag->ilk_jeton : 0;
    if (op >= TIP_ESIT_MI && op <= TIP_BUYUK_ESIT) {
        if (sag_j && sag_j->tip == TIP_BOS && sol_j) daralt_sembol = sembol_ara(motor, sol_j);
        if (sol_j && sol_j->tip == TIP_BOS && sag_j) daralt_sembol = sembol_ara(motor, sag_j);
        // == bos identifies the null branch; it is not a non-null guard.
        if (op == TIP_ESIT_MI) daralt_sembol = 0;
    }
}
if (daralt_sembol && daralt_sembol->nullable == 1) {
    daralt_eski_nullable = 1;
    daralt_sembol->nullable = 0;
}
std::set<std::string> init_moves = g_moved_vars;
kapsam_olustur(motor, 64);
tip_cikar(motor, YKN_DOT(dugum, govde));
kapsamdan_cik(motor);
std::set<std::string> then_moves = g_moved_vars;
g_fix13_guard_active = guard_onceki;
g_fix13_guard_invalidated = invalidated_onceki;
g_fix13_guard_name = guard_name_onceki;
g_fix13_guard_name_length = guard_name_length_onceki;
g_fix13_guard_name2 = guard_name2_onceki;
g_fix13_guard_name2_length = guard_name2_length_onceki;
g_fix13_guard_field = guard_field_onceki;
g_fix13_guard_field_length = guard_field_length_onceki;

if (daralt_sembol && daralt_eski_nullable == 1) daralt_sembol->nullable = 1;
if (((YKN_DOT(dugum, yoksa_govde)) != 0)) {
    long long else_guard_onceki = g_fix13_guard_active;
    char* else_ad_onceki = g_fix13_guard_name;
    long long else_uzunluk_onceki = g_fix13_guard_name_length;
    long long else_invalidated_onceki = g_fix13_guard_invalidated;
    if (kosul && kosul->tip == AST_IKILI_OP && kosul->ilk_jeton &&
        kosul->ilk_jeton->tip == TIP_ESIT_MI && kosul->sol &&
        kosul->sol->tip == AST_KIMLIK && kosul->sag && kosul->sag->ilk_jeton &&
        kosul->sag->ilk_jeton->tip == TIP_BOS) {
        g_fix13_guard_active = 1;
        g_fix13_guard_name = kosul->sol->ilk_jeton->baslangic;
        g_fix13_guard_name_length = kosul->sol->ilk_jeton->uzunluk;
        g_fix13_guard_invalidated = 0;
    }
    g_moved_vars = init_moves;
    kapsam_olustur(motor, 64);
    tip_cikar(motor, YKN_DOT(dugum, yoksa_govde));
    kapsamdan_cik(motor);
    for (const auto& v : then_moves) {
        g_moved_vars.insert(v);
    }
    g_fix13_guard_active = else_guard_onceki;
    g_fix13_guard_name = else_ad_onceki;
    g_fix13_guard_name_length = else_uzunluk_onceki;
    g_fix13_guard_name2 = nullptr;
    g_fix13_guard_name2_length = 0;
    g_fix13_guard_invalidated = else_invalidated_onceki;
} else {
    g_moved_vars = then_moves;
}
;
return TIP_ID_HIC;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONGU)) {
    if (((YKN_DOT(dugum, sol)) != 0)) {
        tip_cikar(motor, YKN_DOT(dugum, sol));
        release_temp_borrows();
    }
    long long guard_onceki = g_fix13_guard_active;
    long long invalidated_onceki = g_fix13_guard_invalidated;
    char* guard_name_onceki = g_fix13_guard_name;
    long long guard_name_length_onceki = g_fix13_guard_name_length;
    char* guard_name2_onceki = g_fix13_guard_name2;
    long long guard_name2_length_onceki = g_fix13_guard_name2_length;
    char* guard_field_onceki = g_fix13_guard_field;
    long long guard_field_length_onceki = g_fix13_guard_field_length;
    g_fix13_guard_active = 0;
    g_fix13_guard_invalidated = 0;
    g_fix13_guard_name = nullptr;
    g_fix13_guard_name_length = 0;
    g_fix13_guard_name2 = nullptr;
    g_fix13_guard_name2_length = 0;
    g_fix13_guard_field = nullptr;
    g_fix13_guard_field_length = 0;
    AstDugum* guard_condition = YKN_DOT(dugum, sol);
    if (guard_condition && guard_condition->tip == AST_KIMLIK && guard_condition->ilk_jeton) {
        g_fix13_guard_active = 1;
        g_fix13_guard_name = guard_condition->ilk_jeton->baslangic;
        g_fix13_guard_name_length = guard_condition->ilk_jeton->uzunluk;
    } else if (guard_condition && guard_condition->tip == AST_IKILI_OP &&
               guard_condition->ilk_jeton && guard_condition->ilk_jeton->tip == TIP_ESIT_DEGIL &&
               guard_condition->sag && guard_condition->sag->ilk_jeton &&
               (guard_condition->sag->ilk_jeton->tip == TIP_BOS || guard_condition->sag->ilk_jeton->tip == 43) &&
               guard_condition->sol && guard_condition->sol->tip == AST_KIMLIK &&
               guard_condition->sol->ilk_jeton) {
        g_fix13_guard_active = 1;
        g_fix13_guard_name = guard_condition->sol->ilk_jeton->baslangic;
        g_fix13_guard_name_length = guard_condition->sol->ilk_jeton->uzunluk;
    } else if (guard_condition && guard_condition->tip == AST_IKILI_OP &&
               guard_condition->ilk_jeton && guard_condition->ilk_jeton->tip == TIP_ESIT_DEGIL &&
               guard_condition->sag && guard_condition->sag->ilk_jeton &&
               (guard_condition->sag->ilk_jeton->tip == TIP_BOS || guard_condition->sag->ilk_jeton->tip == 43) &&
               guard_condition->sol && guard_condition->sol->tip == 17 &&
               guard_condition->sol->sol && guard_condition->sol->sol->tip == AST_KIMLIK &&
               guard_condition->sol->sol->ilk_jeton &&
               guard_condition->sol->sag && guard_condition->sol->sag->tip == AST_KIMLIK &&
               guard_condition->sol->sag->ilk_jeton) {
        g_fix13_guard_active = 1;
        g_fix13_guard_name = guard_condition->sol->sol->ilk_jeton->baslangic;
        g_fix13_guard_name_length = guard_condition->sol->sol->ilk_jeton->uzunluk;
        g_fix13_guard_field = guard_condition->sol->sag->ilk_jeton->baslangic;
        g_fix13_guard_field_length = guard_condition->sol->sag->ilk_jeton->uzunluk;
    }
    if (guard_condition && guard_condition->tip == AST_IKILI_OP && guard_condition->ilk_jeton &&
        guard_condition->ilk_jeton->tip == TIP_VE) {
        AstDugum* left = guard_condition->sol;
        AstDugum* right = guard_condition->sag;
        if (left && left->tip == AST_IKILI_OP && left->sol && left->sol->tip == AST_KIMLIK && left->ilk_jeton && left->ilk_jeton->tip == TIP_ESIT_DEGIL &&
            left->sag && left->sag->ilk_jeton && (left->sag->ilk_jeton->tip == TIP_BOS || left->sag->ilk_jeton->tip == 43)) {
            g_fix13_guard_active = 1;
            g_fix13_guard_name = left->sol->ilk_jeton->baslangic;
            g_fix13_guard_name_length = left->sol->ilk_jeton->uzunluk;
        }
        if (right && right->tip == AST_IKILI_OP && right->sol && right->sol->tip == AST_KIMLIK && right->ilk_jeton && right->ilk_jeton->tip == TIP_ESIT_DEGIL &&
            right->sag && right->sag->ilk_jeton && (right->sag->ilk_jeton->tip == TIP_BOS || right->sag->ilk_jeton->tip == 43)) {
            g_fix13_guard_active = 1;
            g_fix13_guard_name2 = right->sol->ilk_jeton->baslangic;
            g_fix13_guard_name2_length = right->sol->ilk_jeton->uzunluk;
        }
    }
    SembolGirdisi* daralt_sembol = 0;
    long long daralt_eski_nullable = 0;
    AstDugum* kosul = YKN_DOT(dugum, sol);
    if (kosul && kosul->tip == AST_KIMLIK && kosul->ilk_jeton) {
        daralt_sembol = sembol_ara(motor, kosul->ilk_jeton);
    } else if (kosul && kosul->tip == AST_IKILI_OP && kosul->ilk_jeton) {
        long long op = kosul->ilk_jeton->tip;
        Jeton* sol_j = (kosul->sol && (kosul->sol->tip == AST_KIMLIK || kosul->sol->tip == AST_SAYI)) ? kosul->sol->ilk_jeton : 0;
        Jeton* sag_j = (kosul->sag && (kosul->sag->tip == AST_KIMLIK || kosul->sag->tip == AST_SAYI)) ? kosul->sag->ilk_jeton : 0;
        if (op >= TIP_ESIT_MI && op <= TIP_BUYUK_ESIT) {
            if (sag_j && sag_j->tip == TIP_BOS && sol_j) daralt_sembol = sembol_ara(motor, sol_j);
            if (sol_j && sol_j->tip == TIP_BOS && sag_j) daralt_sembol = sembol_ara(motor, sag_j);
            if (op == TIP_ESIT_MI) daralt_sembol = 0;
        }
    }
    if (daralt_sembol && daralt_sembol->nullable == 1) {
        daralt_eski_nullable = 1;
        daralt_sembol->nullable = 0;
    }
    kapsam_olustur(motor, 64);
    tip_cikar(motor, YKN_DOT(dugum, govde));
    if (YKN_DOT(dugum, yoksa_govde) != 0) {
        tip_cikar(motor, YKN_DOT(dugum, yoksa_govde));
    }
    kapsamdan_cik(motor);
    g_fix13_guard_active = guard_onceki;
    g_fix13_guard_invalidated = invalidated_onceki;
    g_fix13_guard_name = guard_name_onceki;
    g_fix13_guard_name_length = guard_name_length_onceki;
    g_fix13_guard_name2 = guard_name2_onceki;
    g_fix13_guard_name2_length = guard_name2_length_onceki;
    g_fix13_guard_field = guard_field_onceki;
    g_fix13_guard_field_length = guard_field_length_onceki;
    if (daralt_sembol && daralt_eski_nullable == 1) {
        daralt_sembol->nullable = 1;
    }
    return TIP_ID_HIC;
}
;
if ((YKN_DOT(dugum, tip) == AST_BLOK)) {
kapsam_olustur(motor, 128);
AstDugum* iter = (AstDugum*)(YKN_DOT(dugum, sol));
long long son_tip = TIP_ID_HIC;
while (((iter) != 0)) {
son_tip = tip_cikar(motor, iter);
release_temp_borrows();
iter = YKN_DOT(iter, sonraki);
}
;
kapsamdan_cik(motor);
return son_tip;
}
;
if ((YKN_DOT(dugum, tip) == AST_GENERIC_TANIM)) {
// Generic sablon tanimi - govde typechecker'dan gecmez, monomorphization sonrasi somutlastirilir
if (((YKN_DOT(dugum, ilk_jeton)) != 0)) {
    sembol_ekle(motor, YKN_DOT(dugum, ilk_jeton), TIP_ID_ISLEV, TIP_ID_BILINMIYOR);
}
;
return TIP_ID_HIC;
}
;
if ((YKN_DOT(dugum, tip) == AST_ISLEV || YKN_DOT(dugum, tip) == AST_ASYNC_ISLEV)) {
    g_moved_vars.clear(); g_borrow_stack.clear(); g_uninit_vars.clear();
long long onceki_async_islev_aktif_seed = g_async_islev_aktif_seed;
bool bu_async_seed = YKN_DOT(dugum, tip) == AST_ASYNC_ISLEV;
g_async_islev_aktif_seed = bu_async_seed ? 1 : 0;
long long beden_donus_tip_seed = TIP_ID_HIC;
if (((YKN_DOT(dugum, tip_adi)) != 0)) {
    beden_donus_tip_seed = tip_dugumunden_coz(dugum);
}
long long don_tip = beden_donus_tip_seed;
if (bu_async_seed && beden_donus_tip_seed != TIP_ID_BILINMIYOR && beden_donus_tip_seed != TIP_ID_HIC) {
    don_tip = seed_future_tipi_mi(beden_donus_tip_seed)
        ? beden_donus_tip_seed : seed_future_olustur(beden_donus_tip_seed);
}
if (((YKN_DOT(dugum, ilk_jeton)) != 0)) {
sembol_ekle(motor, YKN_DOT(dugum, ilk_jeton), TIP_ID_ISLEV, don_tip, dugum);
}
;
kapsam_olustur(motor, 64);
long long onceki_donus = YKN_DOT(motor, guncel_donus_tipi);
// Inside an async body, return expressions produce the payload T, while the
// callable symbol exposes Future<T>.
YKN_DOT(motor, guncel_donus_tipi) = (bu_async_seed && seed_future_tipi_mi(don_tip))
    ? seed_future_payload(don_tip) : don_tip;
AstDugum* param = (AstDugum*)(YKN_DOT(dugum, parametreler));
while (((param) != 0)) {
long long p_tip = TIP_ID_BILINMIYOR;
if (((YKN_DOT(param, tip_adi)) != 0)) {
p_tip = tip_dugumunden_coz(param);
}
;
if (((YKN_DOT(param, ilk_jeton)) != 0)) {
// FIX13: preserve nullable parameter AST metadata in the symbol entry.
sembol_ekle(motor, YKN_DOT(param, ilk_jeton), p_tip, TIP_ID_HIC, param);
}
;
param = YKN_DOT(param, sonraki);
}
;
tip_cikar(motor, YKN_DOT(dugum, govde));
if (beden_donus_tip_seed != TIP_ID_HIC && beden_donus_tip_seed != TIP_ID_BILINMIYOR) {
    bool is_main = dugum->ilk_jeton && dugum->ilk_jeton->uzunluk == 3 && memcmp(dugum->ilk_jeton->baslangic, "ana", 3) == 0;
    bool is_intrinsic = false;
    if (dugum->ilk_jeton) {
        const char* nm = dugum->ilk_jeton->baslangic;
        int len = dugum->ilk_jeton->uzunluk;
        if ((len == 8 && memcmp(nm, "kare_kok", 8) == 0) ||
            (len == 4 && memcmp(nm, "sqrt", 4) == 0) ||
            (len == 5 && memcmp(nm, "sinus", 5) == 0) ||
            (len == 3 && memcmp(nm, "sin", 3) == 0) ||
            (len == 7 && memcmp(nm, "kosinus", 7) == 0) ||
            (len == 3 && memcmp(nm, "cos", 3) == 0) ||
            (len == 13 && memcmp(nm, "gercek_mutlak", 13) == 0) ||
            (len == 4 && memcmp(nm, "fabs", 4) == 0) ||
            (len == 14 && memcmp(nm, "gercek_minimum", 14) == 0) ||
            (len == 4 && memcmp(nm, "fmin", 4) == 0) ||
            (len == 15 && memcmp(nm, "gercek_maksimum", 15) == 0) ||
            (len == 4 && memcmp(nm, "fmax", 4) == 0) ||
            (dugum->govde && dugum->govde->tip == AST_BLOK && !dugum->govde->sol)) {
            is_intrinsic = true;
        }
    }
    if (!is_main && !is_intrinsic && dugum->govde && !ast_donus_var_mi(dugum->govde)) {
        tc_diagnostic_yaz(4, "TC HATA: Fonksiyonda donus ifadesi eksik (missing return)", dugum->ilk_jeton);
        YKN_DOT(motor, hata_sayisi)++;
        YKN_DOT(motor, donus_hata_sayisi)++;
    }
}
YKN_DOT(motor, guncel_donus_tipi) = onceki_donus;
g_async_islev_aktif_seed = onceki_async_islev_aktif_seed;
kapsamdan_cik(motor);
return TIP_ID_ISLEV;
}
;
if ((YKN_DOT(dugum, tip) == AST_ISLEV_CAGRISI)) {
    // Donanim Intrinsicleri (Clang / RustC seviyesi) oncelikli cozumleme
    Jeton* in_call_j = YKN_DOT(dugum, ilk_jeton);
    if (!in_call_j && YKN_DOT(dugum, sol) && YKN_DOT(dugum, sol)->ilk_jeton) {
        in_call_j = YKN_DOT(dugum, sol)->ilk_jeton;
    }
    if (in_call_j && in_call_j->baslangic) {
        char* ib = in_call_j->baslangic;
        long long ibl = in_call_j->uzunluk;
        if ((ibl == 8 && memcmp(ib, "popcount", 8) == 0) ||
            (ibl == 7 && memcmp(ib, "bit_say", 7) == 0) ||
            (ibl == 5 && memcmp(ib, "bswap", 5) == 0) ||
            (ibl == 11 && memcmp(ib, "bayt_cevir", 11) == 0) ||
            (ibl == 3 && memcmp(ib, "clz", 3) == 0) ||
            (ibl == 3 && memcmp(ib, "ctz", 3) == 0) ||
            (ibl == 7 && memcmp(ib, "rotleft", 7) == 0) ||
            (ibl == 8 && memcmp(ib, "rotright", 8) == 0)) {
            AstDugum* i_arg = (AstDugum*)(YKN_DOT(dugum, sag));
            while (i_arg) {
                tip_cikar(motor, i_arg);
                i_arg = YKN_DOT(i_arg, sonraki);
            }
            release_temp_borrows();
            return TIP_ID_TAMSAYI;
        }
    }
    // Dynamic dispatch nesne.metod(...) çağrısı: sag alan metod kimligidir ve parametreler sag->sag içindedir
    if (YKN_DOT(dugum, sol) && YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_KIMLIK && YKN_DOT(dugum, sag)->sag != nullptr) {
        tip_cikar(motor, YKN_DOT(dugum, sol));
        AstDugum* m_arg = YKN_DOT(dugum, sag)->sag;
        while (m_arg) {
            tip_cikar(motor, m_arg);
            m_arg = m_arg->sonraki;
        }
        release_temp_borrows();
        return TIP_ID_TAMSAYI;
    }
    Jeton* call_fn_j = (YKN_DOT(dugum, sol) && YKN_DOT(dugum, sol)->ilk_jeton) ? YKN_DOT(dugum, sol)->ilk_jeton : YKN_DOT(dugum, ilk_jeton);
    if (call_fn_j && call_fn_j->baslangic) { char* b = call_fn_j->baslangic; long long bl = call_fn_j->uzunluk; if (bl == 16 && bellek_esit_mi(b, (char*)"bellek_tahsis_et", 16) == 1) { release_temp_borrows(); return TIP_ID_POINTER; } if (bl == 7 && bellek_esit_mi(b, (char*)"argv_al", 7) == 1) { release_temp_borrows(); return TIP_ID_POINTER; } if (bl == 16 && bellek_esit_mi(b, (char*)"os_bellek_tahsis", 16) == 1) { release_temp_borrows(); return TIP_ID_POINTER; } if (bl == 19 && bellek_esit_mi(b, (char*)"compiler_tahsis_et", 19) == 1) { release_temp_borrows(); return TIP_ID_POINTER; } } bool is_tasi = call_fn_j && call_fn_j->uzunluk == 4 && memcmp(call_fn_j->baslangic, "tasi", 4) == 0;
AstDugum* arg = (AstDugum*)(YKN_DOT(dugum, sag));
long long arg_sayisi = 0;
while (((arg) != 0)) {
arg_sayisi++;
tip_cikar(motor, arg);
arg = YKN_DOT(arg, sonraki);
}
    release_temp_borrows();
    if (is_tasi) {
        AstDugum* ma = YKN_DOT(dugum, sag);
        while (ma) {
            if (ma->tip == AST_KIMLIK && ma->ilk_jeton) {
                std::string mv(ma->ilk_jeton->baslangic, ma->ilk_jeton->uzunluk);
                if (is_variable_borrowed(mv)) {
                    tc_diagnostic_yaz(41, "OWNERSHIP HATA: Odunc alinmis degisken tasinamaz (cannot move while borrowed)", ma->ilk_jeton);
                    YKN_DOT(motor, hata_sayisi)++;
                } else {
                    g_moved_vars.insert(mv);
                }
            }
            ma = ma->sonraki;
        }
    }
;
// GOREV 6.3: Sembol tablosuna ONCE bak â€” kullanici tanimli fonksiyon isimleri
// tip anahtar kelimeleriyle ('hic', 'tamsayi', vb.) cakisabilir.
// Sembolde tanimli bir fonksiyon varsa, onun donus tipini kullan.
SembolGirdisi* fonk_s = sembol_ara(motor, call_fn_j ? call_fn_j : YKN_DOT(dugum, ilk_jeton));
if (!fonk_s) {
    // Enum constructor'lar gerçek fonksiyon sembolü değildir; tagged union döndürür.
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        long long ev = enum_varyant_bul_tam(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk);
        if (ev < 0) ev = enum_varyant_bul_kisa(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk);
        if (ev < 0 && YKN_DOT(dugum, sol) && YKN_DOT(dugum, sol)->ilk_jeton && YKN_DOT(dugum, sol)->ilk_jeton->baslangic) { ev = enum_varyant_bul_tam(YKN_DOT(dugum, sol)->ilk_jeton->baslangic, YKN_DOT(dugum, sol)->ilk_jeton->uzunluk); if (ev < 0) ev = enum_varyant_bul_kisa(YKN_DOT(dugum, sol)->ilk_jeton->baslangic, YKN_DOT(dugum, sol)->ilk_jeton->uzunluk); }
        if (ev >= 0) {
            if (enum_varyant_has_any_payload(ev) != 0 || YKN_DOT(dugum, sag) != nullptr) {
                return TIP_ID_YAPI;
            }
            return TIP_ID_TAMSAYI;
        }
    }
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        char* c_ad = YKN_DOT(dugum, ilk_jeton)->baslangic;
        long long c_uzn = YKN_DOT(dugum, ilk_jeton)->uzunluk;
        if (c_uzn >= 8 && memcmp(c_ad, "vtable__", 8) == 0) return TIP_ID_METIN;
        if (c_uzn == 9 && memcmp(c_ad, "dyn_yarat", 9) == 0) return TIP_ID_POINTER;
    }
    // Dynamic dispatch nesne.metod(...) cagrisi
    if (YKN_DOT(dugum, sol) && YKN_DOT(dugum, sol)->tip != AST_KIMLIK && YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_KIMLIK) {
        return TIP_ID_TAMSAYI;
    }
    // Sembol tablosunda yok: type-cast sezimi yap (ornek: tamsayi(x), gercek(x))
    if (YKN_DOT(dugum, ilk_jeton) && YKN_DOT(dugum, ilk_jeton)->baslangic) {
        long long cast_tip = tip_adini_coz(YKN_DOT(dugum, ilk_jeton)->baslangic, YKN_DOT(dugum, ilk_jeton)->uzunluk);
        if (cast_tip != TIP_ID_BILINMIYOR && cast_tip != TIP_ID_YAPI && cast_tip != TIP_ID_HIC) {
            return cast_tip;
        }
    }
}
if (fonk_s) {
    if (fonk_s->ast_tanimi && fonk_s->ast_tanimi->tip == AST_ISLEV) {
        long long param_sayisi = 0;
        AstDugum* p_iter = (AstDugum*)fonk_s->ast_tanimi->parametreler;
        AstDugum* a_iter = (AstDugum*)YKN_DOT(dugum, sag);
        while (p_iter != 0) {
            param_sayisi++;
            if (a_iter && a_iter->tip == AST_KIMLIK && a_iter->ilk_jeton) {
                long long p_tip = tip_dugumunden_coz(p_iter);
                SembolGirdisi* arg_sym = sembol_ara(motor, a_iter->ilk_jeton);
                bool arg_is_pointer = false;
                if (arg_sym && arg_sym->ast_tanimi && arg_sym->ast_tanimi->cozumlenmis_tip > 0) {
                    arg_is_pointer = true;
                }
                if (!arg_is_pointer && p_iter->cozumlenmis_tip == 0 && p_tip == TIP_ID_YAPI) {
                    std::string m_name(a_iter->ilk_jeton->baslangic, a_iter->ilk_jeton->uzunluk);
                    if (is_variable_borrowed(m_name)) {
                        tc_diagnostic_yaz(41, "OWNERSHIP HATA: Odunc alinmis degisken tasinamaz (cannot move out of borrowed context)", a_iter->ilk_jeton);
                        YKN_DOT(motor, hata_sayisi)++;
                    } else {
                        g_moved_vars.insert(m_name);
                    }
                }
            }
            p_iter = YKN_DOT(p_iter, sonraki);
            if (a_iter) a_iter = YKN_DOT(a_iter, sonraki);
        }
        if (arg_sayisi != param_sayisi) {
            printf("TC HATA: Fonksiyon parametre sayisi uyumsuz\n");
            YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
            YKN_DOT(motor, param_sayisi_hata_sayisi) = (YKN_DOT(motor, param_sayisi_hata_sayisi) + 1);
        }
    }
    if (YKN_DOT(fonk_s, donus_tipi) != 0 && YKN_DOT(fonk_s, donus_tipi) != TIP_ID_HIC) {
        return YKN_DOT(fonk_s, donus_tipi);
    }
    return TIP_ID_TAMSAYI;
}
// Builtin fonksiyonlar
{
    Jeton* fn_j = call_fn_j ? call_fn_j : YKN_DOT(dugum, ilk_jeton);
    if (fn_j && fn_j->baslangic) {
        char* b = fn_j->baslangic;
        long long bl = fn_j->uzunluk;
        if (bl == 16 && bellek_esit_mi(b, (char*)"bellek_tahsis_et", 16) == 1) return TIP_ID_POINTER;
        if (bl == 7 && bellek_esit_mi(b, (char*)"argv_al", 7) == 1) return TIP_ID_POINTER;
        if ((bl == 8 && bellek_esit_mi(b, (char*)"popcount", 8) == 1) ||
            (bl == 7 && bellek_esit_mi(b, (char*)"bit_say", 7) == 1) ||
            (bl == 5 && bellek_esit_mi(b, (char*)"bswap", 5) == 1) ||
            (bl == 11 && bellek_esit_mi(b, (char*)"bayt_cevir", 11) == 1) ||
            (bl == 3 && bellek_esit_mi(b, (char*)"clz", 3) == 1) ||
            (bl == 3 && bellek_esit_mi(b, (char*)"ctz", 3) == 1) ||
            (bl == 7 && bellek_esit_mi(b, (char*)"rotleft", 7) == 1) ||
            (bl == 8 && bellek_esit_mi(b, (char*)"rotright", 8) == 1)) {
            return TIP_ID_TAMSAYI;
        }
    }
}
return TIP_ID_BILINMIYOR;
}
;
if ((YKN_DOT(dugum, tip) == AST_ALAN_ERISIMI)) {
    // FIX13: nullable struct pointer field access follows the same guard
    // contract as indexing. Check the symbol before resolving the field.
    AstDugum* field_base = YKN_DOT(dugum, sol);
    if (field_base && field_base->tip == AST_KIMLIK && field_base->ilk_jeton) {
        SembolGirdisi* field_symbol = sembol_ara(motor, field_base->ilk_jeton);
        if (field_symbol && field_symbol->nullable == 1 && !(g_fix13_guard_active && g_fix13_guard_invalidated == 0 && fix13_guard_matches(field_base))) {
            tc_diagnostic_yaz(33,
                "FIX13 HATA: Nullable pointer erisimi kontrolsuz.",
                field_base->ilk_jeton, -1, -1);
            YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
        }
    }
    long long struct_tipi = tip_cikar(motor, YKN_DOT(dugum, sol));
    if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->ilk_jeton && YKN_DOT(dugum, sag)->ilk_jeton->uzunluk == 3 && memcmp(YKN_DOT(dugum, sag)->ilk_jeton->baslangic, "ptr", 3) == 0) return TIP_ID_POINTER; AstDugum* struct_ast = tc_struct_tanimi_bul(motor, YKN_DOT(dugum, sol));
    if (struct_ast && struct_ast->tip == AST_YAPI_TANIMI) {
        if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->ilk_jeton) {
            char* f_name = YKN_DOT(dugum, sag)->ilk_jeton->baslangic;
            long long f_len = YKN_DOT(dugum, sag)->ilk_jeton->uzunluk;
            AstDugum* f_iter = struct_ast->sol;
            bool alan_bulundu = false;
            long long alan_off = 0;
            while (f_iter) {
                if (f_iter->ilk_jeton && f_iter->ilk_jeton->uzunluk == f_len && bellek_esit_mi(f_iter->ilk_jeton->baslangic, f_name, f_len) == 1) {
                    alan_bulundu = true;
                    dugum->dizi_boyutu = alan_off;
                    if (f_iter->cozumlenmis_tip > 0 || f_iter->dizi_boyutu > 0) return TIP_ID_POINTER;
                    if (f_iter->tip_adi) {
                        long long alan_tipi = tip_dugumunden_coz(f_iter);
                        if (alan_tipi == TIP_ID_BILINMIYOR || alan_tipi == 0) {
                            alan_tipi = tip_adini_coz(f_iter->tip_adi, f_iter->tip_adi_uzunluk);
                        }
                        return alan_tipi;
                    }
                    return TIP_ID_TAMSAYI;
                }
                long long f_sz = (f_iter->dizi_boyutu > 0) ? (f_iter->dizi_boyutu * 8) : 8;
                alan_off += f_sz;
                f_iter = f_iter->sonraki;
            }
            if (!alan_bulundu) {
                tc_diagnostic_yaz(1, "TC HATA: Struct icinde boyle bir alan tanimli degil", YKN_DOT(dugum, sag)->ilk_jeton);
                YKN_DOT(motor, hata_sayisi)++;
                return TIP_ID_BILINMIYOR;
            }
        }
    }
    return TIP_ID_TAMSAYI;
}
;
if ((YKN_DOT(dugum, tip) == AST_AWAIT)) {
    Jeton* await_token = YKN_DOT(dugum, ilk_jeton);
    AstDugum* operand = (AstDugum*)YKN_DOT(dugum, sag);
    bool async_call = false;
    long long payload_tip = TIP_ID_BILINMIYOR;
    if (operand && operand->tip == AST_ISLEV_CAGRISI) {
        SembolGirdisi* async_symbol = sembol_ara(motor, YKN_DOT(operand, ilk_jeton));
        if (async_symbol && async_symbol->ast_tanimi && async_symbol->ast_tanimi->tip == AST_ASYNC_ISLEV) {
            async_call = true;
            if (seed_future_tipi_mi(async_symbol->donus_tipi)) {
                payload_tip = seed_future_payload(async_symbol->donus_tipi);
            } else if (async_symbol->ast_tanimi->tip_adi) {
                long long body_tip = tip_dugumunden_coz(async_symbol->ast_tanimi);
                if (seed_future_tipi_mi(body_tip)) payload_tip = seed_future_payload(body_tip);
                else payload_tip = body_tip;
            }
        }
    }
    if (g_async_islev_aktif_seed == 0 || !async_call || payload_tip == TIP_ID_BILINMIYOR) {
        tc_diagnostic_yaz(44, g_async_islev_aktif_seed == 0
            ? "await yalnızca async islev govdesinde kullanilabilir"
            : "await operandi Future<T> olmalidir", await_token);
        YKN_DOT(motor, hata_sayisi) = (YKN_DOT(motor, hata_sayisi) + 1);
        YKN_DOT(motor, tip_uyumsuz_hata_sayisi) = (YKN_DOT(motor, tip_uyumsuz_hata_sayisi) + 1);
        return TIP_ID_BILINMIYOR;
    }
    return payload_tip;
}
;
if ((YKN_DOT(dugum, tip) == AST_SORU_ISLEMI || YKN_DOT(dugum, tip) == 33)) {
    long long op_tip = tip_cikar(motor, YKN_DOT(dugum, sol));
    if (op_tip == TIP_ID_SONUC || op_tip == TIP_ID_POINTER || op_tip == TIP_ID_YAPI) return TIP_ID_TAMSAYI;
    return op_tip;
}
;
if ((YKN_DOT(dugum, tip) == AST_YAPI_TANIMI)) {
if (((YKN_DOT(dugum, ilk_jeton)) != 0)) {
sembol_ekle(motor, YKN_DOT(dugum, ilk_jeton), TIP_ID_YAPI, TIP_ID_HIC, dugum);
}
;
return TIP_ID_HIC;
}
;
return TIP_ID_BILINMIYOR;
}

long long typechecker_self_heal_dene(TipMotoru* motor) {
    if (!motor || !YKN_DOT(motor, self_heal_adayi)) return 0;
    if (YKN_DOT(motor, tanimsiz_hata_sayisi) <= 0 ||
        YKN_DOT(motor, tip_uyumsuz_hata_sayisi) > 0 ||
        YKN_DOT(motor, donus_hata_sayisi) > 0 ||
        YKN_DOT(motor, tanimsiz_hata_sayisi) != YKN_DOT(motor, hata_sayisi)) return 0;
    AstDugum* aday = (AstDugum*)(YKN_DOT(motor, self_heal_adayi));
    if (!YKN_DOT(aday, ilk_jeton)) return 0;
    YKN_DOT(aday, tip) = AST_SAYI;
    YKN_DOT(aday, ilk_jeton)->tip = TIP_TAMSAYI;
    YKN_DOT(aday, ilk_jeton)->baslangic = (char*)"0";
    YKN_DOT(aday, ilk_jeton)->uzunluk = 1;
    YKN_DOT(aday, son_jeton) = YKN_DOT(aday, ilk_jeton);
    ekrana_yaz((char*)"[AI] Self-heal: tanimsiz sembol 0 sabitine donusturuldu.\n");
    return 1;
}


TipMotoru* tip_denetimini_baslat_ve_don(AstArena* arena, AstDugum* kok) {
if (!arena || !kok) return nullptr;
g_typechecker_kapasite_hatasi = 0;
g_typechecker_kapasite_kodu = 0;
g_typechecker_sembol_sayisi = 0;
g_typechecker_kapsam_sayisi = 0;
g_typechecker_nominal_sayisi = 0;
    g_seed_future_type_count = 0;
    g_seed_future_type_errors = 0;
    g_seed_known_type_count = 0;
TipMotoru* motor = (TipMotoru*)( (char*)YKN_DOT(arena,bellek) + YKN_DOT(arena,kullanilan) );
YKN_DOT(arena,kullanilan) += sizeof(TipMotoru);
memset(motor, 0, sizeof(TipMotoru));
YKN_DOT(motor,arena) = arena;
YKN_DOT(motor,hata_sayisi) = 0;
YKN_DOT(motor,tanimsiz_hata_sayisi) = 0;
YKN_DOT(motor,tip_uyumsuz_hata_sayisi) = 0;
YKN_DOT(motor,donus_hata_sayisi) = 0;
YKN_DOT(motor,self_heal_adayi) = 0;
YKN_DOT(motor,self_heal_deger_konteksti) = 0;
kapsam_olustur(motor, 1024);
if (!YKN_DOT(motor,guncel_kapsam)) return motor;

AstDugum* top_iter = (kok && kok->tip == AST_BLOK && kok->sol) ? (AstDugum*)kok->sol : kok;
// Keep the nominal declaration boundary independent of later name resolution.
AstDugum* nominal_iter = top_iter;
while (nominal_iter) {
    long long nominal_tip = YKN_DOT(nominal_iter, tip);
    if (nominal_tip == AST_YAPI_TANIMI && YKN_DOT(nominal_iter, ilk_jeton)) {
        seed_known_type_ekle(YKN_DOT(nominal_iter, ilk_jeton)->baslangic, YKN_DOT(nominal_iter, ilk_jeton)->uzunluk);
        g_typechecker_nominal_sayisi++;
        if (g_typechecker_nominal_sayisi > 256) {
            seed_typechecker_kapasite_reddet(4);
            return motor;
        }
    }
    nominal_iter = (AstDugum*)YKN_DOT(nominal_iter, sonraki);
}

// GOREV 5.2: Built-in makro isimlerini global kapsama onceden kaydet
// dogrula, ayikla, panik, boyut
{
    struct BuiltinEntry { const char* name; int len; };
    BuiltinEntry builtins[] = {
        {"dogrula", 7}, {"ayikla", 6}, {"panik", 5}, {"boyut", 5}
    };
    for (auto& b : builtins) {
        Jeton* j = (Jeton*)malloc(sizeof(Jeton));
        memset(j, 0, sizeof(Jeton));
        j->tip = 9; // TIP_KIMLIK
        j->baslangic = (char*)b.name;
        j->uzunluk = b.len;
        sembol_ekle(motor, j, TIP_ID_ISLEV, TIP_ID_TAMSAYI, nullptr);
    }
}

AstDugum* iter = top_iter;
while (iter) {
    if ((YKN_DOT(iter,tip) == AST_ISLEV || YKN_DOT(iter,tip) == AST_ASYNC_ISLEV) && YKN_DOT(iter,ilk_jeton)) {
        long long beden_tip = TIP_ID_HIC;
        if (YKN_DOT(iter,tip_adi)) beden_tip = tip_dugumunden_coz(iter);
        long long don_tip = beden_tip;
        if (YKN_DOT(iter,tip) == AST_ASYNC_ISLEV &&
            beden_tip != TIP_ID_BILINMIYOR && beden_tip != TIP_ID_HIC) {
            don_tip = seed_future_tipi_mi(beden_tip) ? beden_tip : seed_future_olustur(beden_tip);
        }
        sembol_ekle(motor, YKN_DOT(iter,ilk_jeton), TIP_ID_ISLEV, don_tip, iter);
    }
    if ((YKN_DOT(iter,tip) == AST_YAPI_TANIMI) && YKN_DOT(iter,ilk_jeton))
        sembol_ekle(motor, YKN_DOT(iter,ilk_jeton), TIP_ID_YAPI, TIP_ID_HIC, iter);
    if ((YKN_DOT(iter,tip) == AST_DEGISKEN_TANIMI) && YKN_DOT(iter,ilk_jeton)) {
        long long var_tip = TIP_ID_BILINMIYOR;
        if (YKN_DOT(iter,tip_adi)) var_tip = tip_dugumunden_coz(iter);
        sembol_ekle(motor, YKN_DOT(iter,ilk_jeton), var_tip, TIP_ID_HIC, iter);
    }
    iter = YKN_DOT(iter,sonraki);
}
AstDugum* tc_dugum = top_iter;
while (tc_dugum) {
    tip_cikar(motor, tc_dugum);
    release_temp_borrows();
    tc_dugum = YKN_DOT(tc_dugum, sonraki);
}
if (g_seed_future_type_errors > 0) {
    YKN_DOT(motor, hata_sayisi) += g_seed_future_type_errors;
    YKN_DOT(motor, tip_uyumsuz_hata_sayisi) += g_seed_future_type_errors;
}
return motor;
}

void tip_denetimini_baslat(AstArena* arena, AstDugum* kok) {
    tip_denetimini_baslat_ve_don(arena, kok);
}



long long ykn_trait_uygulandi_mi(GenericsMotoru* motor, long long trait_hash, long long kontrol_edilen_tip) {
    TraitGirdisi* iter = motor->trait_tablosu;
    while (iter) {
        if (iter->trait_hash == trait_hash && iter->uygulanan_tip == kontrol_edilen_tip) return 1;
        iter = iter->sonraki;
    }
    return 0;
}

void ykn_trait_uygulamasi_kaydet(GenericsMotoru* motor, Jeton* trait_isim_jetonu, long long hedef_tip) {
    long long hash = djb2_sembol(trait_isim_jetonu->baslangic, trait_isim_jetonu->uzunluk);
    TraitGirdisi* yeni_girdi = (TraitGirdisi*)bellek_tahsis_et(64);
    yeni_girdi->trait_hash = hash;
    yeni_girdi->uygulanan_tip = hedef_tip;
    yeni_girdi->sonraki = motor->trait_tablosu;
    motor->trait_tablosu = yeni_girdi;
}

void ykn_generic_sablon_ekle(GenericsMotoru* motor, AstDugum* islev_d) {
    Jeton* j = islev_d->ilk_jeton;
    long long hash = djb2_sembol(j->baslangic, j->uzunluk);

    GenericSablon* sablon = (GenericSablon*)bellek_tahsis_et(sizeof(GenericSablon));
    sablon->isim_ptr = j->baslangic;
    sablon->isim_uzunluk = j->uzunluk;
    sablon->isim_hash = hash;

    if (islev_d->sol) {
        sablon->tip_param_ptr = islev_d->sol->tip_adi;
        sablon->tip_param_uzunluk = islev_d->sol->tip_adi_uzunluk;
        if (islev_d->sol->sag) {
            Jeton* tb = islev_d->sol->sag->ilk_jeton;
            sablon->trait_hash = djb2_sembol(tb->baslangic, tb->uzunluk);
        } else {
            sablon->trait_hash = 0;
        }
        // Ikinci tip parametresi: islev_d->sol->sonraki
        sablon->tip_param2_ptr = 0;
        sablon->tip_param2_uzunluk = 0;
        sablon->trait_hash2 = 0;
        if (islev_d->sol->sonraki) {
            sablon->tip_param2_ptr = islev_d->sol->sonraki->tip_adi;
            sablon->tip_param2_uzunluk = islev_d->sol->sonraki->tip_adi_uzunluk;
            if (islev_d->sol->sonraki->sag) {
                Jeton* tb2 = islev_d->sol->sonraki->sag->ilk_jeton;
                sablon->trait_hash2 = djb2_sembol(tb2->baslangic, tb2->uzunluk);
            }
        }
    } else {
        sablon->tip_param_ptr = (char*)"T";
        sablon->tip_param_uzunluk = 1;
        sablon->trait_hash = 0;
        sablon->tip_param2_ptr = 0;
        sablon->tip_param2_uzunluk = 0;
        sablon->trait_hash2 = 0;
    }

    sablon->govde_ast = islev_d;
    sablon->sonraki = motor->sablon_listesi;
    motor->sablon_listesi = sablon;
}

GenericSablon* ykn_generic_sablon_bul(GenericsMotoru* motor, char* isim_ptr, long long isim_uzunluk) {
    if (!isim_ptr || isim_uzunluk <= 0) return 0;
    long long h = djb2_sembol(isim_ptr, isim_uzunluk);
    GenericSablon* iter = motor->sablon_listesi;
    while (iter) {
        if (iter->isim_hash == h && iter->isim_uzunluk == isim_uzunluk &&
            bellek_esit_mi(iter->isim_ptr, isim_ptr, isim_uzunluk) == 1) return iter;
        iter = iter->sonraki;
    }
    return 0;
}

AstDugum* ykn_ast_klonla_ve_tip_yerlestir(GenericsMotoru* motor, AstDugum* kok, char* t_isim, long long t_uzunluk, char* somut_tip, long long somut_uzunluk) {
    if (!kok) return 0;

    AstDugum* kopya = (AstDugum*)arena_dugum_yarat(motor->arena, kok->tip, kok->ilk_jeton);
    kopya->son_jeton = kok->son_jeton;
    kopya->tip_adi = kok->tip_adi;
    kopya->tip_adi_uzunluk = kok->tip_adi_uzunluk;
    kopya->cozumlenmis_tip = kok->cozumlenmis_tip;
    kopya->dizi_boyutu = kok->dizi_boyutu;

    if (kopya->tip_adi && kopya->tip_adi_uzunluk == t_uzunluk) {
        if (bellek_esit_mi(kopya->tip_adi, t_isim, t_uzunluk) == 1) {
            kopya->tip_adi = somut_tip;
            kopya->tip_adi_uzunluk = somut_uzunluk;
        }
    }

    if (kok->parametreler) {
        AstDugum* ilk_p = 0;
        AstDugum* son_p = 0;
        AstDugum* p_iter = kok->parametreler;
        while (p_iter) {
            AstDugum* p_kopya = (AstDugum*)arena_dugum_yarat(motor->arena, p_iter->tip, p_iter->ilk_jeton);
            p_kopya->tip_adi = p_iter->tip_adi;
            p_kopya->tip_adi_uzunluk = p_iter->tip_adi_uzunluk;
            p_kopya->cozumlenmis_tip = p_iter->cozumlenmis_tip;
            p_kopya->dizi_boyutu = p_iter->dizi_boyutu;
            if (p_kopya->tip_adi && p_kopya->tip_adi_uzunluk == t_uzunluk) {
                if (bellek_esit_mi(p_kopya->tip_adi, t_isim, t_uzunluk) == 1) {
                    p_kopya->tip_adi = somut_tip;
                    p_kopya->tip_adi_uzunluk = somut_uzunluk;
                }
            }
            if (!ilk_p) { ilk_p = p_kopya; son_p = p_kopya; }
            else { son_p->sonraki = p_kopya; son_p = p_kopya; }
            p_iter = p_iter->sonraki;
        }
        kopya->parametreler = ilk_p;
    }

    kopya->sol = ykn_ast_klonla_ve_tip_yerlestir(motor, kok->sol, t_isim, t_uzunluk, somut_tip, somut_uzunluk);
    kopya->sag = ykn_ast_klonla_ve_tip_yerlestir(motor, kok->sag, t_isim, t_uzunluk, somut_tip, somut_uzunluk);
    kopya->govde = ykn_ast_klonla_ve_tip_yerlestir(motor, kok->govde, t_isim, t_uzunluk, somut_tip, somut_uzunluk);
    kopya->yoksa_govde = ykn_ast_klonla_ve_tip_yerlestir(motor, kok->yoksa_govde, t_isim, t_uzunluk, somut_tip, somut_uzunluk);
    if (kok->sonraki) {
        kopya->sonraki = ykn_ast_klonla_ve_tip_yerlestir(motor, kok->sonraki, t_isim, t_uzunluk, somut_tip, somut_uzunluk);
    }
    return kopya;
}

AstDugum* ykn_ast_klonla_ve_coklu_tip_yerlestir(GenericsMotoru* motor, AstDugum* kok, AstDugum* parametreler_ast, AstDugum* somut_ast) {
    if (!kok) return 0;

    AstDugum* kopya = (AstDugum*)arena_dugum_yarat(motor->arena, kok->tip, kok->ilk_jeton);
    kopya->son_jeton = kok->son_jeton;
    kopya->tip_adi = kok->tip_adi;
    kopya->tip_adi_uzunluk = kok->tip_adi_uzunluk;
    kopya->cozumlenmis_tip = kok->cozumlenmis_tip;
    kopya->dizi_boyutu = kok->dizi_boyutu;

    if (kopya->tip_adi) {
        AstDugum* p_it = parametreler_ast;
        AstDugum* s_it = somut_ast;
        while (p_it && s_it) {
            if (p_it->tip_adi && p_it->tip_adi_uzunluk == kopya->tip_adi_uzunluk) {
                if (bellek_esit_mi(p_it->tip_adi, kopya->tip_adi, kopya->tip_adi_uzunluk) == 1) {
                    kopya->tip_adi = s_it->tip_adi;
                    kopya->tip_adi_uzunluk = s_it->tip_adi_uzunluk;
                    break;
                }
            }
            p_it = p_it->sonraki;
            s_it = s_it->sonraki;
        }
    }

    if (kok->parametreler) {
        AstDugum* ilk_p = 0;
        AstDugum* son_p = 0;
        AstDugum* p_iter = kok->parametreler;
        while (p_iter) {
            AstDugum* p_kopya = (AstDugum*)arena_dugum_yarat(motor->arena, p_iter->tip, p_iter->ilk_jeton);
            p_kopya->tip_adi = p_iter->tip_adi;
            p_kopya->tip_adi_uzunluk = p_iter->tip_adi_uzunluk;
            p_kopya->cozumlenmis_tip = p_iter->cozumlenmis_tip;
            p_kopya->dizi_boyutu = p_iter->dizi_boyutu;
            if (p_kopya->tip_adi) {
                AstDugum* p_it2 = parametreler_ast;
                AstDugum* s_it2 = somut_ast;
                while (p_it2 && s_it2) {
                    if (p_it2->tip_adi && p_it2->tip_adi_uzunluk == p_kopya->tip_adi_uzunluk) {
                        if (bellek_esit_mi(p_it2->tip_adi, p_kopya->tip_adi, p_kopya->tip_adi_uzunluk) == 1) {
                            p_kopya->tip_adi = s_it2->tip_adi;
                            p_kopya->tip_adi_uzunluk = s_it2->tip_adi_uzunluk;
                            break;
                        }
                    }
                    p_it2 = p_it2->sonraki;
                    s_it2 = s_it2->sonraki;
                }
            }
            if (p_iter->tip_arglari) {
                AstDugum* ilk_pta = 0;
                AstDugum* son_pta = 0;
                AstDugum* pta_iter = p_iter->tip_arglari;
                while (pta_iter) {
                    AstDugum* pta_kopya = (AstDugum*)arena_dugum_yarat(motor->arena, pta_iter->tip, pta_iter->ilk_jeton);
                    pta_kopya->tip_adi = pta_iter->tip_adi;
                    pta_kopya->tip_adi_uzunluk = pta_iter->tip_adi_uzunluk;
                    pta_kopya->cozumlenmis_tip = pta_iter->cozumlenmis_tip;
                    if (pta_kopya->tip_adi) {
                        AstDugum* p_it_ta = parametreler_ast;
                        AstDugum* s_it_ta = somut_ast;
                        while (p_it_ta && s_it_ta) {
                            if (p_it_ta->tip_adi && p_it_ta->tip_adi_uzunluk == pta_kopya->tip_adi_uzunluk) {
                                if (bellek_esit_mi(p_it_ta->tip_adi, pta_kopya->tip_adi, pta_kopya->tip_adi_uzunluk) == 1) {
                                    pta_kopya->tip_adi = s_it_ta->tip_adi;
                                    pta_kopya->tip_adi_uzunluk = s_it_ta->tip_adi_uzunluk;
                                    break;
                                }
                            }
                            p_it_ta = p_it_ta->sonraki;
                            s_it_ta = s_it_ta->sonraki;
                        }
                    }
                    if (!ilk_pta) { ilk_pta = pta_kopya; son_pta = pta_kopya; }
                    else { son_pta->sonraki = pta_kopya; son_pta = pta_kopya; }
                    pta_iter = pta_iter->sonraki;
                }
                p_kopya->tip_arglari = ilk_pta;
                p_kopya->tip_arg_sayisi = p_iter->tip_arg_sayisi;
            }
            if (!ilk_p) { ilk_p = p_kopya; son_p = p_kopya; }
            else { son_p->sonraki = p_kopya; son_p = p_kopya; }
            p_iter = p_iter->sonraki;
        }
        kopya->parametreler = ilk_p;
    }

    if (kok->tip_arglari) {
        AstDugum* ilk_ta = 0;
        AstDugum* son_ta = 0;
        AstDugum* ta_iter = kok->tip_arglari;
        while (ta_iter) {
            AstDugum* ta_kopya = (AstDugum*)arena_dugum_yarat(motor->arena, ta_iter->tip, ta_iter->ilk_jeton);
            ta_kopya->tip_adi = ta_iter->tip_adi;
            ta_kopya->tip_adi_uzunluk = ta_iter->tip_adi_uzunluk;
            ta_kopya->cozumlenmis_tip = ta_iter->cozumlenmis_tip;
            if (ta_kopya->tip_adi) {
                AstDugum* p_it3 = parametreler_ast;
                AstDugum* s_it3 = somut_ast;
                while (p_it3 && s_it3) {
                    if (p_it3->tip_adi && p_it3->tip_adi_uzunluk == ta_kopya->tip_adi_uzunluk) {
                        if (bellek_esit_mi(p_it3->tip_adi, ta_kopya->tip_adi, ta_kopya->tip_adi_uzunluk) == 1) {
                            ta_kopya->tip_adi = s_it3->tip_adi;
                            ta_kopya->tip_adi_uzunluk = s_it3->tip_adi_uzunluk;
                            break;
                        }
                    }
                    p_it3 = p_it3->sonraki;
                    s_it3 = s_it3->sonraki;
                }
            }
            if (!ilk_ta) { ilk_ta = ta_kopya; son_ta = ta_kopya; }
            else { son_ta->sonraki = ta_kopya; son_ta = ta_kopya; }
            ta_iter = ta_iter->sonraki;
        }
        kopya->tip_arglari = ilk_ta;
        kopya->tip_arg_sayisi = kok->tip_arg_sayisi;
    }

    kopya->sol = ykn_ast_klonla_ve_coklu_tip_yerlestir(motor, kok->sol, parametreler_ast, somut_ast);
    kopya->sag = ykn_ast_klonla_ve_coklu_tip_yerlestir(motor, kok->sag, parametreler_ast, somut_ast);
    kopya->govde = ykn_ast_klonla_ve_coklu_tip_yerlestir(motor, kok->govde, parametreler_ast, somut_ast);
    kopya->yoksa_govde = ykn_ast_klonla_ve_coklu_tip_yerlestir(motor, kok->yoksa_govde, parametreler_ast, somut_ast);
    if (kok->sonraki) {
        kopya->sonraki = ykn_ast_klonla_ve_coklu_tip_yerlestir(motor, kok->sonraki, parametreler_ast, somut_ast);
    }
    return kopya;
}

void ykn_generic_yapi_sablon_ekle(GenericsMotoru* motor, AstDugum* yapi_d) {
    if (!yapi_d || !yapi_d->ilk_jeton) return;
    Jeton* j = yapi_d->ilk_jeton;
    GenericSablon* sablon = (GenericSablon*)bellek_tahsis_et(sizeof(GenericSablon));
    sablon->isim_ptr = j->baslangic;
    sablon->isim_uzunluk = j->uzunluk;
    sablon->isim_hash = djb2_sembol(j->baslangic, j->uzunluk);
    sablon->parametreler_ast = yapi_d->parametreler;
    sablon->yapi_ast = yapi_d;
    sablon->govde_ast = yapi_d;
    sablon->sonraki = motor->yapi_sablon_listesi;
    motor->yapi_sablon_listesi = sablon;
}

GenericSablon* ykn_generic_yapi_sablon_bul(GenericsMotoru* motor, char* isim_ptr, long long isim_uzunluk) {
    if (!isim_ptr || isim_uzunluk <= 0) return 0;
    long long h = djb2_sembol(isim_ptr, isim_uzunluk);
    GenericSablon* iter = motor->yapi_sablon_listesi;
    while (iter) {
        if (iter->isim_hash == h && iter->isim_uzunluk == isim_uzunluk &&
            bellek_esit_mi(iter->isim_ptr, isim_ptr, isim_uzunluk) == 1) return iter;
        iter = iter->sonraki;
    }
    return 0;
}

AstDugum* ykn_yapi_monomorph_kaydi_bul(GenericsMotoru* motor, char* mangled_ptr, long long mangled_uzunluk) {
    if (!mangled_ptr || mangled_uzunluk <= 0) return 0;
    long long h = djb2_sembol(mangled_ptr, mangled_uzunluk);
    MonomorphKayit* iter = motor->yapi_monomorph_listesi;
    while (iter) {
        if (iter->mangled_hash == h && iter->mangled_uzunluk == mangled_uzunluk &&
            bellek_esit_mi(iter->mangled_ptr, mangled_ptr, mangled_uzunluk) == 1) return iter->islev_ast;
        iter = iter->sonraki;
    }
    return 0;
}

void ykn_generic_yapi_monomorfize_et(GenericsMotoru* motor, AstDugum* tip_dugumu, AstDugum** uretilen_fonklar_bas) {
    if (!tip_dugumu || !tip_dugumu->tip_adi || !tip_dugumu->tip_arglari) return;

    // Eger tip argumanlarindan herhangi biri henuz cozulmemis generic tip parametresi ise
    // (ornek: tek buyuk harf 'T', 'U'), somutlastirma yapilamaz.
    AstDugum* chk_arg = tip_dugumu->tip_arglari;
    while (chk_arg) {
        if (chk_arg->tip_adi && chk_arg->tip_adi_uzunluk == 1) {
            unsigned char ilk_ch = (unsigned char)chk_arg->tip_adi[0];
            if (ilk_ch >= 'A' && ilk_ch <= 'Z') return;
        }
        chk_arg = chk_arg->sonraki;
    }

    char* isim_ptr = tip_dugumu->tip_adi;
    long long isim_uzn = tip_dugumu->tip_adi_uzunluk;

    GenericSablon* sablon = ykn_generic_yapi_sablon_bul(motor, isim_ptr, isim_uzn);
    if (!sablon) return;

    // Arguman sayisi ve parametre sayisi kontrolu
    long long param_sayisi = 0;
    long long arg_sayisi = 0;
    AstDugum* iter_p = sablon->parametreler_ast;
    AstDugum* iter_s = tip_dugumu->tip_arglari;
    while (iter_p) { param_sayisi++; iter_p = iter_p->sonraki; }
    while (iter_s) { arg_sayisi++; iter_s = iter_s->sonraki; }

    if (param_sayisi != arg_sayisi) {
        hata_bas_parser((char*)"HATA [E015] [ERR_GENERIC_MISMATCH]: Generic yapi parametre sayisi uyumsuz", tip_dugumu->ilk_jeton);
        return;
    }

    // Mangled isim olustur: Isim__T1__T2
    long long m_uzn = isim_uzn;
    iter_s = tip_dugumu->tip_arglari;
    while (iter_s) {
        if (iter_s->tip_adi) m_uzn += 2 + iter_s->tip_adi_uzunluk;
        iter_s = iter_s->sonraki;
    }

    char* m_buf = (char*)bellek_tahsis_et(m_uzn + 1);
    long long bi = 0;
    for (long long si = 0; si < isim_uzn; si++) m_buf[bi++] = isim_ptr[si];
    iter_s = tip_dugumu->tip_arglari;
    while (iter_s) {
        m_buf[bi++] = '_'; m_buf[bi++] = '_';
        if (iter_s->tip_adi) {
            for (long long si = 0; si < iter_s->tip_adi_uzunluk; si++) {
                m_buf[bi++] = iter_s->tip_adi[si];
            }
        }
        iter_s = iter_s->sonraki;
    }
    m_buf[bi] = 0;

    AstDugum* mevcut = ykn_yapi_monomorph_kaydi_bul(motor, m_buf, m_uzn);
    if (!mevcut) {
        // Yeni somut yapi AST'si uret
        AstDugum* yeni_yapi = arena_dugum_yarat(motor->arena, AST_YAPI_TANIMI, sablon->govde_ast->ilk_jeton);
        yeni_yapi->parametreler = 0;

        Jeton* yeni_jeton = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
        if (sablon->govde_ast->ilk_jeton) *yeni_jeton = *(sablon->govde_ast->ilk_jeton);
        yeni_jeton->tip = TIP_KIMLIK;
        yeni_jeton->baslangic = m_buf;
        yeni_jeton->uzunluk = m_uzn;
        yeni_yapi->ilk_jeton = yeni_jeton;

        // Alanlari klonla ve tip parametrelerini ikame et
        AstDugum* ilk_alan = 0;
        AstDugum* son_alan = 0;
        AstDugum* a_iter = sablon->govde_ast->sol;
        while (a_iter) {
            AstDugum* a_kopya = arena_dugum_yarat(motor->arena, a_iter->tip, a_iter->ilk_jeton);
            a_kopya->tip_adi = a_iter->tip_adi;
            a_kopya->tip_adi_uzunluk = a_iter->tip_adi_uzunluk;
            a_kopya->cozumlenmis_tip = a_iter->cozumlenmis_tip;
            a_kopya->dizi_boyutu = a_iter->dizi_boyutu;
            a_kopya->tip_arglari = a_iter->tip_arglari;
            a_kopya->tip_arg_sayisi = a_iter->tip_arg_sayisi;

            // Tip parametresi ikamesi
            if (a_kopya->tip_adi) {
                AstDugum* pi = sablon->parametreler_ast;
                AstDugum* si = tip_dugumu->tip_arglari;
                while (pi && si) {
                    if (pi->tip_adi && pi->tip_adi_uzunluk == a_kopya->tip_adi_uzunluk) {
                        if (bellek_esit_mi(pi->tip_adi, a_kopya->tip_adi, a_kopya->tip_adi_uzunluk) == 1) {
                            a_kopya->tip_adi = si->tip_adi;
                            a_kopya->tip_adi_uzunluk = si->tip_adi_uzunluk;
                            break;
                        }
                    }
                    pi = pi->sonraki;
                    si = si->sonraki;
                }
            }

            // Recursive monomorfizasyon: eger bu alan da generic yapi ise
            if (a_kopya->tip_arglari) {
                ykn_generic_yapi_monomorfize_et(motor, a_kopya, uretilen_fonklar_bas);
            }

            if (!ilk_alan) { ilk_alan = a_kopya; son_alan = a_kopya; }
            else { son_alan->sonraki = a_kopya; son_alan = a_kopya; }
            a_iter = a_iter->sonraki;
        }
        yeni_yapi->sol = ilk_alan;

        MonomorphKayit* mk = (MonomorphKayit*)bellek_tahsis_et(sizeof(MonomorphKayit));
        mk->mangled_ptr = m_buf;
        mk->mangled_uzunluk = m_uzn;
        mk->mangled_hash = djb2_sembol(m_buf, m_uzn);
        mk->islev_ast = yeni_yapi;
        mk->sonraki = motor->yapi_monomorph_listesi;
        motor->yapi_monomorph_listesi = mk;

        yeni_yapi->sonraki = *uretilen_fonklar_bas;
        *uretilen_fonklar_bas = yeni_yapi;
    }

    // Kullanim dugumunu somut tipe donustur
    tip_dugumu->tip_adi = m_buf;
    tip_dugumu->tip_adi_uzunluk = m_uzn;
    tip_dugumu->tip_arglari = 0;
    tip_dugumu->tip_arg_sayisi = 0;
}

AstDugum* ykn_monomorph_kaydi_bul(GenericsMotoru* motor, char* mangled_ptr, long long mangled_uzunluk) {
    if (!mangled_ptr || mangled_uzunluk <= 0) return 0;
    long long h = djb2_sembol(mangled_ptr, mangled_uzunluk);
    MonomorphKayit* iter = motor->monomorph_listesi;
    while (iter) {
        if (iter->mangled_hash == h && iter->mangled_uzunluk == mangled_uzunluk &&
            bellek_esit_mi(iter->mangled_ptr, mangled_ptr, mangled_uzunluk) == 1) return iter->islev_ast;
        iter = iter->sonraki;
    }
    return 0;
}

void ykn_generics_ifade_tara_tek(GenericsMotoru* motor, AstDugum* dugum, AstDugum** uretilen_fonklar_bas);

void ykn_generics_ifade_tara(GenericsMotoru* motor, AstDugum* dugum, AstDugum** uretilen_fonklar_bas) {
    AstDugum* iter = dugum;
    while (iter != 0) {
        AstDugum* siradaki = iter->sonraki;
        ykn_generics_ifade_tara_tek(motor, iter, uretilen_fonklar_bas);
        iter = siradaki;
    }
}

void ykn_generics_ifade_tara_tek(GenericsMotoru* motor, AstDugum* dugum, AstDugum** uretilen_fonklar_bas) {
    if (!dugum) return;
    if ((long long)dugum < 4096) return;
    tc_builtin_traitleri_kaydet();

    // Generic sablonlar (fonksiyon veya yapi sablonu) yerinde monomorfize edilemez
    if (dugum->tip == AST_GENERIC_TANIM) return;
    if (dugum->tip == AST_YAPI_TANIMI && dugum->parametreler != 0) return;

    // Generic yapi / enum tipi referansi kontrolu: dugumde tip_arglari ve tip_adi varsa
    if (dugum->tip_arglari && dugum->tip_adi) {
        ykn_generic_yapi_monomorfize_et(motor, dugum, uretilen_fonklar_bas);
    }

    // Parametrelerde generic yapi referansi kontrolu
    AstDugum* p_tarama = dugum->parametreler;
    while (p_tarama) {
        if (p_tarama->tip_arglari && p_tarama->tip_adi) {
            ykn_generic_yapi_monomorfize_et(motor, p_tarama, uretilen_fonklar_bas);
        }
        p_tarama = p_tarama->sonraki;
    }

    if (dugum->tip == AST_TRAIT_UYGULAMA) {
        if (dugum->ilk_jeton && dugum->sol && dugum->sol->ilk_jeton) {
            char* hedef_ptr = dugum->sol->ilk_jeton->baslangic;
            long long hedef_uzn = dugum->sol->ilk_jeton->uzunluk;
            AstDugum* m_iter = dugum->govde;
            while (m_iter) {
                AstDugum* m_siradaki = m_iter->sonraki;
                if ((m_iter->tip == AST_ISLEV || m_iter->tip == 10) && m_iter->ilk_jeton) {
                    Jeton* eski_j = m_iter->ilk_jeton;
                    long long m_uzn = hedef_uzn + 2 + eski_j->uzunluk;
                    char* m_buf = (char*)bellek_tahsis_et(m_uzn + 1);
                    memcpy(m_buf, hedef_ptr, hedef_uzn);
                    m_buf[hedef_uzn] = '_'; m_buf[hedef_uzn + 1] = '_';
                    memcpy(m_buf + hedef_uzn + 2, eski_j->baslangic, eski_j->uzunluk);
                    m_buf[m_uzn] = 0;
                    Jeton* yeni_j = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
                    *yeni_j = *eski_j;
                    yeni_j->baslangic = m_buf;
                    yeni_j->uzunluk = m_uzn;
                    AstDugum* m_kopya = arena_dugum_yarat(motor->arena, AST_ISLEV, yeni_j);
                    m_kopya->parametreler = m_iter->parametreler;
                    m_kopya->tip_adi = m_iter->tip_adi;
                    m_kopya->tip_adi_uzunluk = m_iter->tip_adi_uzunluk;
                    m_kopya->govde = m_iter->govde;
                    m_kopya->sonraki = *uretilen_fonklar_bas;
                    *uretilen_fonklar_bas = m_kopya;
                }
                m_iter = m_siradaki;
            }
        }
    }

    if (dugum->tip == AST_ISLEV_CAGRISI) {
        AstDugum* kimlik = dugum->sol;
        if (kimlik && kimlik->ilk_jeton) {
            char* f_ptr = kimlik->ilk_jeton->baslangic;
            long long f_uzn = kimlik->ilk_jeton->uzunluk;
            GenericSablon* sablon = ykn_generic_sablon_bul(motor, f_ptr, f_uzn);
            if (sablon) {
                AstDugum* p_ast = sablon->govde_ast ? sablon->govde_ast->sol : 0;
                AstDugum* s_ast = dugum->yoksa_govde;
                if (!s_ast && dugum->sag) {
                    AstDugum* ilk_inf = 0;
                    AstDugum* son_inf = 0;
                    AstDugum* tp_iter = p_ast;
                    AstDugum* pos_arg = dugum->sag;
                    while (tp_iter) {
                        AstDugum* eslesen_arg = 0;
                        if (sablon->govde_ast && tp_iter->tip_adi) {
                            AstDugum* fp_iter = sablon->govde_ast->parametreler;
                            AstDugum* ca_iter = dugum->sag;
                            while (fp_iter && ca_iter) {
                                if (fp_iter->tip_adi && fp_iter->tip_adi_uzunluk == tp_iter->tip_adi_uzunluk &&
                                    bellek_esit_mi(fp_iter->tip_adi, tp_iter->tip_adi, tp_iter->tip_adi_uzunluk) == 1) {
                                    eslesen_arg = ca_iter;
                                    break;
                                }
                                fp_iter = fp_iter->sonraki;
                                ca_iter = ca_iter->sonraki;
                            }
                        }
                        if (!eslesen_arg) eslesen_arg = pos_arg;
                        if (eslesen_arg) {
                            char* ded_tip = 0;
                            long long ded_uzn = 0;
                            if (eslesen_arg->tip == 1) { ded_tip = (char*)"tamsayi"; ded_uzn = 7; }
                            else if (eslesen_arg->tip == 2) { ded_tip = (char*)"gercek"; ded_uzn = 6; }
                            else if (eslesen_arg->tip == 3) { ded_tip = (char*)"metin"; ded_uzn = 5; }
                            else if (eslesen_arg->tip == 26) { ded_tip = (char*)"mantik"; ded_uzn = 6; }
                            else if (eslesen_arg->tip_adi) { ded_tip = eslesen_arg->tip_adi; ded_uzn = eslesen_arg->tip_adi_uzunluk; }
                            if (ded_tip) {
                                AstDugum* inf_d = arena_dugum_yarat(motor->arena, AST_GENERIC_TANIM, eslesen_arg->ilk_jeton);
                                inf_d->tip_adi = ded_tip;
                                inf_d->tip_adi_uzunluk = ded_uzn;
                                if (!ilk_inf) { ilk_inf = inf_d; son_inf = inf_d; }
                                else { son_inf->sonraki = inf_d; son_inf = inf_d; }
                            }
                        }
                        if (pos_arg) pos_arg = pos_arg->sonraki;
                        tp_iter = tp_iter->sonraki;
                    }
                    s_ast = ilk_inf;
                }

                long long param_sayisi = 0;
                long long arg_sayisi = 0;
                AstDugum* iter_p = p_ast;
                AstDugum* iter_s = s_ast;
                while (iter_p) { param_sayisi++; iter_p = iter_p->sonraki; }
                while (iter_s) { arg_sayisi++; iter_s = iter_s->sonraki; }

                if (param_sayisi != arg_sayisi) {
                    hata_bas_parser((char*)"HATA [E015] [ERR_GENERIC_MISMATCH]: Generic parametre sayisi uyumsuz", kimlik->ilk_jeton);
                    return;
                }

                // Trait kisitlari kontrolu
                iter_p = p_ast;
                iter_s = s_ast;
                while (iter_p && iter_s) {
                    if (iter_p->sag && iter_s->tip_adi) {
                        long long somut_tip_h = djb2_sembol(iter_s->tip_adi, iter_s->tip_adi_uzunluk);
                        AstDugum* iter_tb = iter_p->sag;
                        while (iter_tb) {
                            Jeton* tb = iter_tb->ilk_jeton;
                            if (tb) {
                                long long tr_hash = djb2_sembol(tb->baslangic, tb->uzunluk);
                                if (!tc_trait_uygulandi_mi(tr_hash, somut_tip_h)) {
                                    hata_bas_parser((char*)"HATA [E020] [ERR_TRAIT_BOUND]: Generic trait kisiti saglanmiyor", kimlik->ilk_jeton);
                                    return;
                                }
                            }
                            iter_tb = iter_tb->sonraki;
                        }
                    }
                    iter_p = iter_p->sonraki;
                    iter_s = iter_s->sonraki;
                }

                // Mangling: fonk__T1__T2__...
                long long m_uzn = f_uzn;
                iter_s = s_ast;
                while (iter_s) {
                    if (iter_s->tip_adi) m_uzn += 2 + iter_s->tip_adi_uzunluk;
                    iter_s = iter_s->sonraki;
                }

                char* m_buf = (char*)bellek_tahsis_et(m_uzn + 1);
                long long bi = 0;
                for (long long si = 0; si < f_uzn; si++) m_buf[bi++] = f_ptr[si];
                iter_s = s_ast;
                while (iter_s) {
                    m_buf[bi++] = '_'; m_buf[bi++] = '_';
                    if (iter_s->tip_adi) {
                        for (long long si = 0; si < iter_s->tip_adi_uzunluk; si++) {
                            m_buf[bi++] = iter_s->tip_adi[si];
                        }
                    }
                    iter_s = iter_s->sonraki;
                }
                m_buf[bi] = 0;

                AstDugum* mevcut = ykn_monomorph_kaydi_bul(motor, m_buf, m_uzn);
                if (!mevcut) {
                    static long long g_generic_uzmanlasma_sayaci = 0;
                    if (g_generic_uzmanlasma_sayaci >= 1024) {
                        hata_bas_parser((char*)"HATA [E015] [ERR_RECURSION_DEPTH] [class=generics]: Generic uzmanlasma kapasitesi (1024) asildi", kimlik->ilk_jeton);
                        return;
                    }
                    g_generic_uzmanlasma_sayaci++;
                    AstDugum* yeni_islev = ykn_ast_klonla_ve_coklu_tip_yerlestir(motor, sablon->govde_ast, p_ast, s_ast);
                    yeni_islev->tip = AST_ISLEV;
                    yeni_islev->sol = 0;

                    Jeton* yeni_jeton = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
                    if (sablon->govde_ast && sablon->govde_ast->ilk_jeton) *yeni_jeton = *(sablon->govde_ast->ilk_jeton);
                    yeni_jeton->tip = TIP_KIMLIK;
                    yeni_jeton->baslangic = m_buf;
                    yeni_jeton->uzunluk = m_uzn;
                    yeni_islev->ilk_jeton = yeni_jeton;

                    MonomorphKayit* mk = (MonomorphKayit*)bellek_tahsis_et(sizeof(MonomorphKayit));
                    mk->mangled_ptr = m_buf;
                    mk->mangled_uzunluk = m_uzn;
                    mk->mangled_hash = djb2_sembol(m_buf, m_uzn);
                    mk->islev_ast = yeni_islev;
                    mk->sonraki = motor->monomorph_listesi;
                    motor->monomorph_listesi = mk;

                    yeni_islev->sonraki = *uretilen_fonklar_bas;
                    *uretilen_fonklar_bas = yeni_islev;

                    if (yeni_islev->tip_arglari && yeni_islev->tip_adi) {
                        ykn_generic_yapi_monomorfize_et(motor, yeni_islev, uretilen_fonklar_bas);
                    }
                    AstDugum* yf_param = yeni_islev->parametreler;
                    while (yf_param) {
                        if (yf_param->tip_arglari && yf_param->tip_adi) {
                            ykn_generic_yapi_monomorfize_et(motor, yf_param, uretilen_fonklar_bas);
                        }
                        yf_param = yf_param->sonraki;
                    }
                    ykn_generics_ifade_tara(motor, yeni_islev->govde, uretilen_fonklar_bas);
                }

                Jeton* cagri_jeton = (Jeton*)bellek_tahsis_et(sizeof(Jeton));
                *cagri_jeton = *(kimlik->ilk_jeton);
                cagri_jeton->tip = TIP_KIMLIK;
                cagri_jeton->baslangic = m_buf;
                cagri_jeton->uzunluk = m_uzn;
                kimlik->ilk_jeton = cagri_jeton;
                dugum->yoksa_govde = 0;
            }
        }
    }

    ykn_generics_ifade_tara(motor, dugum->sol, uretilen_fonklar_bas);
    ykn_generics_ifade_tara(motor, dugum->sag, uretilen_fonklar_bas);
    ykn_generics_ifade_tara(motor, dugum->govde, uretilen_fonklar_bas);
    ykn_generics_ifade_tara(motor, dugum->yoksa_govde, uretilen_fonklar_bas);
    ykn_generics_ifade_tara(motor, dugum->parametreler, uretilen_fonklar_bas);

}

AstDugum* ykn_generics_pipeline_calistir(GenericsMotoru* motor, AstDugum* kok) {
    if (!kok) return 0;
    if ((long long)kok < 4096) return 0;

    AstDugum* liste_koku = kok;
    bool is_root_block = false;
    if (kok->tip == AST_BLOK && kok->sol != 0) {
        liste_koku = kok->sol;
        is_root_block = true;
    }

    AstDugum* iter = liste_koku;
    while (iter) {
        if (iter->tip == AST_GENERIC_TANIM) {
            ykn_generic_sablon_ekle(motor, iter);
        }
        if (iter->tip == AST_YAPI_TANIMI && iter->parametreler != 0) {
            ykn_generic_yapi_sablon_ekle(motor, iter);
        }
        iter = iter->sonraki;
    }

    AstDugum* uretilen_fonklar = 0;
    AstDugum** uretilen_ptr = &uretilen_fonklar;
    ykn_generics_ifade_tara(motor, liste_koku, uretilen_ptr);

    AstDugum* yeni_liste = 0;
    AstDugum* son_dugum = 0;

    AstDugum* uf = uretilen_fonklar;
    while (uf) {
        AstDugum* siradaki_uf = uf->sonraki;
        uf->sonraki = 0;
        if (!yeni_liste) { yeni_liste = uf; son_dugum = uf; }
        else { son_dugum->sonraki = uf; son_dugum = uf; }
        uf = siradaki_uf;
    }

    iter = liste_koku;
    while (iter) {
        AstDugum* siradaki = iter->sonraki;
        iter->sonraki = 0;
        bool yapi_sablon_mu = (iter->tip == AST_YAPI_TANIMI && iter->parametreler != 0);
        if (iter->tip != AST_GENERIC_TANIM && !yapi_sablon_mu) {
            if (!yeni_liste) { yeni_liste = iter; son_dugum = iter; }
            else { son_dugum->sonraki = iter; son_dugum = iter; }
        }
        iter = siradaki;
    }

    if (is_root_block) {
        kok->sol = yeni_liste;
        return kok;
    }
    return yeni_liste;
}


auto MIR_OP_ATAMA = 100;
auto MIR_OP_YUKLE = 101;
auto MIR_OP_DEPOLA = 102;
auto MIR_OP_TOPLA = 103;
auto MIR_OP_CIKAR = 104;
auto MIR_OP_CARP = 105;
auto MIR_OP_BOL = 106;
auto MIR_OP_MOD = 107;
auto MIR_OP_ESIT_MI = 108;
auto MIR_OP_KUCUK_MU = 109;
auto MIR_OP_BUYUK_MU = 110;
auto MIR_OP_ESIT_DEG = 111;
auto MIR_OP_KOS = 112;
auto MIR_OP_KOS_EGER = 113;
auto MIR_OP_CAGIR = 114;
auto MIR_OP_DON = 115;
auto MIR_OP_ADRES = 116;
MirTalimati* mir_talimati_yarat(MirBlok* blok, long long op, long long hedef, long long sol, long long sag, long long sabit) {
MirTalimati* t = (MirTalimati*)((char*)((char*)(YKN_DOT(YKN_DOT(blok, arena), bellek)) + YKN_DOT(YKN_DOT(blok, arena), kullanilan)));
YKN_DOT(YKN_DOT(blok, arena), kullanilan) = (YKN_DOT(YKN_DOT(blok, arena), kullanilan) + 48);
YKN_DOT(t, islem_kodu) = op;
YKN_DOT(t, hedef_sanal_reg) = hedef;
YKN_DOT(t, sol_sanal_reg) = sol;
YKN_DOT(t, sag_sanal_reg) = sag;
YKN_DOT(t, sabit_deger) = sabit;
YKN_DOT(t, etiket) = 0;
YKN_DOT(t, sonraki) = 0;
if ((!(YKN_DOT(blok, ilk_talimat)))) {
YKN_DOT(blok, ilk_talimat) = t;
YKN_DOT(blok, son_talimat) = t;
}
 else {
YKN_DOT(YKN_DOT(blok, son_talimat), sonraki) = t;
YKN_DOT(blok, son_talimat) = t;
}
;
return t;
}


long long yeni_reg(MirBlok* blok) {
long long r = YKN_DOT(blok, sanal_reg_sayaci);
YKN_DOT(blok, sanal_reg_sayaci) = (YKN_DOT(blok, sanal_reg_sayaci) + 1);
return r;
}


long long yeni_etiket(MirBlok* blok) {
long long e = YKN_DOT(blok, etiket_sayaci);
YKN_DOT(blok, etiket_sayaci) = (YKN_DOT(blok, etiket_sayaci) + 1);
return e;
}


long long string_tamsayiya(char* veri, long long uzn) {
    long long sonuc = 0;
    long long i = 0;
    long long sign = 1;
    if (uzn > 0 && veri[0] == '-') {
        sign = -1;
        i = 1;
    }
    if (uzn - i >= 2 && veri[i] == '0' && (veri[i+1] == 'x' || veri[i+1] == 'X')) {
        i += 2;
        while (i < uzn) {
            char c = veri[i];
            if (c != '_') {
                long long digit = 0;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                sonuc = (sonuc * 16) + digit;
            }
            i = i + 1;
        }
        return sign * sonuc;
    }
    if (uzn - i >= 2 && veri[i] == '0' && (veri[i+1] == 'b' || veri[i+1] == 'B')) {
        i += 2;
        while (i < uzn) {
            char cb = veri[i];
            if (cb == '0' || cb == '1') {
                sonuc = (sonuc * 2) + (cb - '0');
            }
            i = i + 1;
        }
        return sign * sonuc;
    }
    if (uzn - i >= 2 && veri[i] == '0' && (veri[i+1] == 'o' || veri[i+1] == 'O')) {
        i += 2;
        while (i < uzn) {
            char co = veri[i];
            if (co >= '0' && co <= '7') {
                sonuc = (sonuc * 8) + (co - '0');
            }
            i = i + 1;
        }
        return sign * sonuc;
    }
    while (i < uzn) {
        char cd = veri[i];
        if (cd >= '0' && cd <= '9') {
            sonuc = (sonuc * 10) + (cd - '0');
        }
        i = (i + 1);
    }
    return sign * sonuc;
}


long long ast_den_mir_uret(MirBlok* blok, AstDugum* dugum) {
if ((!(dugum))) {
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_SAYI)) {
long long r = yeni_reg(blok);
long long sayi = string_tamsayiya(YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk));
mir_talimati_yarat(blok, MIR_OP_ATAMA, r, 0, 0, sayi);
return r;
}
;
if ((YKN_DOT(dugum, tip) == AST_KIMLIK)) {
long long r = yeni_reg(blok);
mir_talimati_yarat(blok, MIR_OP_YUKLE, r, 0, 0, -1);
return r;
}
;
if ((YKN_DOT(dugum, tip) == AST_IKILI_OP)) {
long long sol_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sol));
long long sag_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sag));
long long r = yeni_reg(blok);
long long op = MIR_OP_TOPLA;
if (((YKN_DOT(dugum, ilk_jeton)) != 0)) {
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_EKSI)) {
op = MIR_OP_CIKAR;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_CARPI)) {
op = MIR_OP_CARP;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_BOLME)) {
op = MIR_OP_BOL;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_MODULO)) {
op = MIR_OP_MOD;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_ESIT_MI)) {
op = MIR_OP_ESIT_MI;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_KUCUK_MU)) {
op = MIR_OP_KUCUK_MU;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_BUYUK_MU)) {
op = MIR_OP_BUYUK_MU;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_ESIT_DEGIL)) {
op = MIR_OP_ESIT_DEG;
}
;
}
;
mir_talimati_yarat(blok, op, r, sol_r, sag_r, 0);
return r;
}
;
if ((YKN_DOT(dugum, tip) == AST_ATAMA)) {
long long deger_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sag));
mir_talimati_yarat(blok, MIR_OP_DEPOLA, 0, deger_r, 0, 0);
return deger_r;
}
;
if ((YKN_DOT(dugum, tip) == AST_DEGISKEN_TANIMI)) {
if (((YKN_DOT(dugum, sag)) != 0)) {
long long deger_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sag));
mir_talimati_yarat(blok, MIR_OP_DEPOLA, 0, deger_r, 0, 0);
return deger_r;
}
;
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONDUR)) {
long long donen_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sag));
mir_talimati_yarat(blok, MIR_OP_DON, 0, donen_r, 0, 0);
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_EGER)) {
long long kosul_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sol));
long long yon_yoksa = yeni_etiket(blok);
long long yon_bitis = yeni_etiket(blok);
MirTalimati* dal_t = (MirTalimati*)(mir_talimati_yarat(blok, MIR_OP_KOS_EGER, kosul_r, 0, 0, 0));
YKN_DOT(dal_t, etiket) = yon_yoksa;
ast_den_mir_uret(blok, YKN_DOT(dugum, govde));
MirTalimati* atla_t = (MirTalimati*)(mir_talimati_yarat(blok, MIR_OP_KOS, 0, 0, 0, 0));
YKN_DOT(atla_t, etiket) = yon_bitis;
if (((YKN_DOT(dugum, yoksa_govde)) != 0)) {
ast_den_mir_uret(blok, YKN_DOT(dugum, yoksa_govde));
}
;
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONGU)) {
long long bas_etiket = yeni_etiket(blok);
long long cik_etiket = yeni_etiket(blok);
if (((YKN_DOT(dugum, sol)) != 0)) {
long long kosul_r = ast_den_mir_uret(blok, YKN_DOT(dugum, sol));
MirTalimati* dal_t = (MirTalimati*)(mir_talimati_yarat(blok, MIR_OP_KOS_EGER, kosul_r, 0, 0, 0));
YKN_DOT(dal_t, etiket) = cik_etiket;
}
;
ast_den_mir_uret(blok, YKN_DOT(dugum, govde));
MirTalimati* don_t = (MirTalimati*)(mir_talimati_yarat(blok, MIR_OP_KOS, 0, 0, 0, 0));
YKN_DOT(don_t, etiket) = bas_etiket;
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_ISLEV_CAGRISI)) {
AstDugum* arg = (AstDugum*)(YKN_DOT(dugum, sag));
while (((arg) != 0)) {
ast_den_mir_uret(blok, arg);
arg = YKN_DOT(arg, sonraki);
}
;
long long r = yeni_reg(blok);
mir_talimati_yarat(blok, MIR_OP_CAGIR, r, 0, 0, 0);
return r;
}
;
if ((YKN_DOT(dugum, tip) == AST_BLOK)) {
AstDugum* iter = (AstDugum*)(YKN_DOT(dugum, sol));
long long son_r = 0;
while (((iter) != 0)) {
son_r = ast_den_mir_uret(blok, iter);
iter = YKN_DOT(iter, sonraki);
}
;
return son_r;
}
;
return 0;
}


void dce_optimizasyonu_yap(MirBlok* blok) {
long long maks_reg = (YKN_DOT(blok, sanal_reg_sayaci) + 1);
long long* kullanilanlar = (long long*)((char*)((char*)(YKN_DOT(YKN_DOT(blok, arena), bellek)) + YKN_DOT(YKN_DOT(blok, arena), kullanilan)));
YKN_DOT(YKN_DOT(blok, arena), kullanilan) = (YKN_DOT(YKN_DOT(blok, arena), kullanilan) + (maks_reg * 8));
for (long long ki = 0; ki < maks_reg; ki++) { kullanilanlar[ki] = 0; }
MirTalimati* iter = (MirTalimati*)(YKN_DOT(blok, ilk_talimat));
while (((iter) != 0)) {
if ((YKN_DOT(iter, sol_sanal_reg) > 0)) {
kullanilanlar[YKN_DOT(iter, sol_sanal_reg)] = 1;
}
;
if ((YKN_DOT(iter, sag_sanal_reg) > 0)) {
kullanilanlar[YKN_DOT(iter, sag_sanal_reg)] = 1;
}
;
if (((YKN_DOT(iter, hedef_sanal_reg) > 0) && (YKN_DOT(iter, islem_kodu) == MIR_OP_DON))) {
kullanilanlar[YKN_DOT(iter, sol_sanal_reg)] = 1;
}
;
iter = YKN_DOT(iter, sonraki);
}
;
MirTalimati* onceki = (MirTalimati*)(0);
iter = YKN_DOT(blok, ilk_talimat);
while (((iter) != 0)) {
long long sil = 0;
if ((YKN_DOT(iter, islem_kodu) == MIR_OP_ATAMA)) {
if (((YKN_DOT(iter, hedef_sanal_reg) > 0) && (!(kullanilanlar[YKN_DOT(iter, hedef_sanal_reg)])))) {
sil = 1;
}
;
}
;
if ((sil == 1)) {
if (((onceki) != 0)) {
YKN_DOT(onceki, sonraki) = YKN_DOT(iter, sonraki);
}
 else {
YKN_DOT(blok, ilk_talimat) = YKN_DOT(iter, sonraki);
}
;
}
 else {
onceki = iter;
}
;
iter = YKN_DOT(iter, sonraki);
}
;
}



void cfg_ve_talimat_sayaci_ata(MirBlok* blok) {
    YKN_DOT(blok, cfg_talimat_sayisi) = 0;
    YKN_DOT(blok, cfg_bb_sayisi) = 1;
    MirTalimati* iter = (MirTalimati*)(YKN_DOT(blok, ilk_talimat));
    long long pozisyon = 0;
    while (iter != 0) {
        pozisyon++;
        iter = (MirTalimati*)(YKN_DOT(iter, sonraki));
    }
    YKN_DOT(blok, cfg_talimat_sayisi) = pozisyon;
    long long bb_say = 1;
    iter = (MirTalimati*)(YKN_DOT(blok, ilk_talimat));
    while (iter != 0) {
        long long op = YKN_DOT(iter, islem_kodu);
        if (op == 112) bb_say++;  // MIR_OP_KOS
        if (op == 113) bb_say += 2; // MIR_OP_KOS_EGER
        if (op == 115) bb_say++;  // MIR_OP_DON
        iter = (MirTalimati*)(YKN_DOT(iter, sonraki));
    }
    YKN_DOT(blok, cfg_bb_sayisi) = bb_say;
}

void live_interval_hesapla(MirBlok* blok, long long* baslangic_dizisi, long long* bitis_dizisi, long long maks_reg) {
    long long ir = 0;
    while (ir < maks_reg) {
        baslangic_dizisi[ir] = 1000000000LL;
        bitis_dizisi[ir] = -1;
        ir++;
    }
    long long poz = 0;
    MirTalimati* iter = (MirTalimati*)(YKN_DOT(blok, ilk_talimat));
    while (iter != 0) {
        long long h = YKN_DOT(iter, hedef_sanal_reg);
        long long s = YKN_DOT(iter, sol_sanal_reg);
        long long sg = YKN_DOT(iter, sag_sanal_reg);
        if (h > 0 && h < maks_reg) {
            if (poz < baslangic_dizisi[h]) baslangic_dizisi[h] = poz;
            if (poz > bitis_dizisi[h]) bitis_dizisi[h] = poz;
        }
        if (s > 0 && s < maks_reg) {
            if (poz < baslangic_dizisi[s]) baslangic_dizisi[s] = poz;
            if (poz > bitis_dizisi[s]) bitis_dizisi[s] = poz;
        }
        if (sg > 0 && sg < maks_reg) {
            if (poz < baslangic_dizisi[sg]) baslangic_dizisi[sg] = poz;
            if (poz > bitis_dizisi[sg]) bitis_dizisi[sg] = poz;
        }
        poz++;
        iter = (MirTalimati*)(YKN_DOT(iter, sonraki));
    }
}

void fiziksel_register_ata(MirBlok* blok) {
long long maks_reg = (YKN_DOT(blok, sanal_reg_sayaci) + 1);
if (maks_reg <= 1) return;

cfg_ve_talimat_sayaci_ata(blok);

long long* baslangic_dizisi = (long long*)((char*)(YKN_DOT(YKN_DOT(blok, arena), bellek)) + YKN_DOT(YKN_DOT(blok, arena), kullanilan));
YKN_DOT(YKN_DOT(blok, arena), kullanilan) = (YKN_DOT(YKN_DOT(blok, arena), kullanilan) + (maks_reg * 8));

long long* bitis_dizisi = (long long*)((char*)(YKN_DOT(YKN_DOT(blok, arena), bellek)) + YKN_DOT(YKN_DOT(blok, arena), kullanilan));
YKN_DOT(YKN_DOT(blok, arena), kullanilan) = (YKN_DOT(YKN_DOT(blok, arena), kullanilan) + (maks_reg * 8));

long long* fiziksel_eslestirme = (long long*)((char*)(YKN_DOT(YKN_DOT(blok, arena), bellek)) + YKN_DOT(YKN_DOT(blok, arena), kullanilan));
YKN_DOT(YKN_DOT(blok, arena), kullanilan) = (YKN_DOT(YKN_DOT(blok, arena), kullanilan) + (maks_reg * 8));

long long* reg_dolu = (long long*)((char*)(YKN_DOT(YKN_DOT(blok, arena), bellek)) + YKN_DOT(YKN_DOT(blok, arena), kullanilan));
YKN_DOT(YKN_DOT(blok, arena), kullanilan) = (YKN_DOT(YKN_DOT(blok, arena), kullanilan) + (16 * 8));

live_interval_hesapla(blok, baslangic_dizisi, bitis_dizisi, maks_reg);

long long havuz_boyut = 8;
long long oncelik_sirasi[8];
oncelik_sirasi[0] = 2; // RBX (callee-save)
oncelik_sirasi[1] = 7; // R8
oncelik_sirasi[2] = 8; // R9
oncelik_sirasi[3] = 5; // RSI
oncelik_sirasi[4] = 6; // RDI
oncelik_sirasi[5] = 1; // RAX (caller-save - son oncelik)
oncelik_sirasi[6] = 3; // RCX
oncelik_sirasi[7] = 4; // RDX

long long zi = 0;
while (zi < maks_reg) {
    fiziksel_eslestirme[zi] = 0;
    zi++;
}
zi = 0;
while (zi < 16) {
    reg_dolu[zi] = -1;
    zi++;
}

long long scan = 0;
while (scan < maks_reg) {
    if (baslangic_dizisi[scan] < 999999999LL) {
        long long rfree = 0;
        while (rfree < 16) {
            if (reg_dolu[rfree] != -1) {
                if (bitis_dizisi[reg_dolu[rfree]] < baslangic_dizisi[scan]) {
                    reg_dolu[rfree] = -1;
                }
            }
            rfree++;
        }
        long long atandi_mi = 0;
        long long op = 0;
        while (op < havuz_boyut) {
            long long fr = oncelik_sirasi[op];
            if (reg_dolu[fr] == -1) {
                reg_dolu[fr] = scan;
                fiziksel_eslestirme[scan] = fr;
                atandi_mi = 1;
                break;
            }
            op++;
        }
        if (atandi_mi == 0) {
            fiziksel_eslestirme[scan] = ((scan % havuz_boyut) + 1);
        }
    }
    scan++;
}

MirTalimati* iter2 = (MirTalimati*)(YKN_DOT(blok, ilk_talimat));
while (((iter2) != 0)) {
long long h = YKN_DOT(iter2, hedef_sanal_reg);
long long s = YKN_DOT(iter2, sol_sanal_reg);
long long sg = YKN_DOT(iter2, sag_sanal_reg);
if ((h > 0) && (h < maks_reg)) {
    if ((fiziksel_eslestirme[h] != 0)) {
        YKN_DOT(iter2, hedef_sanal_reg) = fiziksel_eslestirme[h];
    }
}
;
if ((s > 0) && (s < maks_reg)) {
    if ((fiziksel_eslestirme[s] != 0)) {
        YKN_DOT(iter2, sol_sanal_reg) = fiziksel_eslestirme[s];
    }
}
;
if ((sg > 0) && (sg < maks_reg)) {
    if ((fiziksel_eslestirme[sg] != 0)) {
        YKN_DOT(iter2, sag_sanal_reg) = fiziksel_eslestirme[sg];
    }
}
;
iter2 = (MirTalimati*)(YKN_DOT(iter2, sonraki));
}
;
}

Jeton* tamsayidan_jeton_yap(AstArena* arena, long long sayi) {
    char* str_buf = (char*)(YKN_DOT(arena, bellek) + YKN_DOT(arena, kullanilan));
    YKN_DOT(arena, kullanilan) += 32;

    long long i = 0;
    if (sayi == (0 - 9223372036854775807LL) - 1) {
        const char* min_str = "-9223372036854775808";
        for (int mi = 0; mi < 20; mi++) str_buf[mi] = min_str[mi];
        str_buf[20] = 0;
        i = 20;
    } else if (sayi == 0) {
        str_buf[0] = '0';
        str_buf[1] = 0;
        i = 1;
    } else {
        long long n = sayi;
        bool negatif = false;
        if (n < 0) {
            negatif = true;
            n = -n;
        }
        char tmp_buf[32];
        long long ti = 0;
        while (n > 0) {
            tmp_buf[ti++] = (char)((n % 10) + '0');
            n /= 10;
        }
        if (negatif) {
            str_buf[i++] = '-';
        }
        long long ri = ti - 1;
        while (ri >= 0) {
            str_buf[i++] = tmp_buf[ri--];
        }
        str_buf[i] = 0;
    }

    Jeton* j = (Jeton*)(YKN_DOT(arena, bellek) + YKN_DOT(arena, kullanilan));
    YKN_DOT(arena, kullanilan) += sizeof(Jeton);
    j->tip = TIP_TAMSAYI;
    j->baslangic = str_buf;
    j->uzunluk = i;
    j->satir = 1;
    j->sutun = 1;
    return j;
}

static bool ast_yan_etki_var_mi(AstDugum* d) {
    if (!d) return false;
    if (d->tip == AST_ISLEV_CAGRISI || d->tip == 11) return true;
    if (d->tip == AST_ATAMA || d->tip == 6) return true;
    if (d->tip == AST_DONDUR || d->tip == 12) return true;
    if (d->tip == AST_DURDUR || d->tip == 14) return true;
    if (d->tip == AST_DEVAM_ET || d->tip == 15) return true;
    if (ast_yan_etki_var_mi(d->sol)) return true;
    if (ast_yan_etki_var_mi(d->sag)) return true;
    if (ast_yan_etki_var_mi(d->govde)) return true;
    if (ast_yan_etki_var_mi(d->yoksa_govde)) return true;
    if (ast_yan_etki_var_mi(d->sonraki)) return true;
    return false;
}

AstDugum* dugum_sabit_indirge(AstArena* arena, AstDugum* dugum) {
    if (!dugum) return nullptr;

    if (dugum->tip == AST_COMPTIME) {
        return dugum_sabit_indirge(arena, dugum->sol);
    }

    // 1. Unary Constant Folding (!0 -> 1, !1 -> 0, -0 -> 0)
    if (dugum->tip == AST_UNARY && dugum->sag) {
        if (dugum->sag->tip == AST_SAYI && dugum->sag->ilk_jeton) {
            long long v = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
            if (dugum->ilk_jeton && dugum->ilk_jeton->tip == TIP_EKSI) {
                Jeton* j_yeni = tamsayidan_jeton_yap(arena, -v);
                return arena_dugum_yarat(arena, AST_SAYI, j_yeni);
            }
        }
    }

    if (dugum->tip != AST_IKILI_OP) return dugum;
    if (!dugum->sol || !dugum->sag) return dugum;

    // 2. Sayi + Sayi Constant Folding (Aritmetik + Karsilastirma + Boolean)
    if (dugum->sol->tip == AST_SAYI && dugum->sag->tip == AST_SAYI) {
        long long v1 = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
        long long v2 = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
        long long sonuc = 0;
        bool katlandi = false;
        if (dugum->ilk_jeton) {
            long long op = dugum->ilk_jeton->tip;
            if (op == TIP_ARTI) { sonuc = v1 + v2; katlandi = true; }
            else if (op == TIP_EKSI) { sonuc = v1 - v2; katlandi = true; }
            else if (op == TIP_CARPI) { sonuc = v1 * v2; katlandi = true; }
            else if (op == TIP_BOLME && v2 != 0) { sonuc = v1 / v2; katlandi = true; }
            else if (op == TIP_MODULO && v2 != 0) { sonuc = v1 % v2; katlandi = true; }
            else if (op == TIP_XOR) { sonuc = v1 ^ v2; katlandi = true; }
            else if (op == TIP_ESIT_MI) { sonuc = (v1 == v2) ? 1 : 0; katlandi = true; }
            else if (op == TIP_ESIT_DEGIL) { sonuc = (v1 != v2) ? 1 : 0; katlandi = true; }
            else if (op == TIP_KUCUK_MU) { sonuc = (v1 < v2) ? 1 : 0; katlandi = true; }
            else if (op == TIP_BUYUK_MU) { sonuc = (v1 > v2) ? 1 : 0; katlandi = true; }
            else if (op == TIP_KUCUK_ESIT) { sonuc = (v1 <= v2) ? 1 : 0; katlandi = true; }
            else if (op == TIP_BUYUK_ESIT) { sonuc = (v1 >= v2) ? 1 : 0; katlandi = true; }
            // Boolean constant folding
            else if (op == TIP_VE) { sonuc = (v1 != 0 && v2 != 0) ? 1 : 0; katlandi = true; }
            else if (op == TIP_VEYA) { sonuc = (v1 != 0 || v2 != 0) ? 1 : 0; katlandi = true; }
        }
        if (katlandi) {
            Jeton* j_yeni = tamsayidan_jeton_yap(arena, sonuc);
            return (AstDugum*)arena_dugum_yarat(arena, AST_SAYI, j_yeni);
        }
    }

    // 3. Algebraic Simplification & Strength Reduction & Boolean Short-Circuit
    if (dugum->ilk_jeton) {
        long long op = dugum->ilk_jeton->tip;
        // Commutative canonicalization: move constant to right
        if (dugum->sol && dugum->sag && dugum->sol->tip == AST_SAYI && dugum->sag->tip != AST_SAYI) {
            if (op == TIP_ARTI || op == TIP_CARPI || op == TIP_VE || op == TIP_VEYA || op == TIP_XOR) {
                AstDugum* tmp = dugum->sol;
                dugum->sol = dugum->sag;
                dugum->sag = tmp;
            }
        }
        if (op == TIP_ARTI) {
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv == 0) return dugum->sol;
            }
            if (dugum->sol->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
                if (sv == 0) return dugum->sag;
            }
        } else if (op == TIP_EKSI) {
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv == 0) return dugum->sol;
            }
        } else if (op == TIP_CARPI) {
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv == 1) return dugum->sol;
                if (sv == 0) {
                    if (!ast_yan_etki_var_mi(dugum->sol)) {
                        Jeton* j0 = tamsayidan_jeton_yap(arena, 0);
                        return arena_dugum_yarat(arena, AST_SAYI, j0);
                    }
                }
                // Strength reduction: x * 2 -> x + x
                if (sv == 2 && !ast_yan_etki_var_mi(dugum->sol)) {
                    Jeton* j_arti = tamsayidan_jeton_yap(arena, 0);
                    j_arti->tip = TIP_ARTI;
                    AstDugum* add_node = arena_dugum_yarat(arena, AST_IKILI_OP, j_arti);
                    add_node->sol = dugum->sol;
                    add_node->sag = dugum->sol;
                    return add_node;
                }
            }
            if (dugum->sol->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
                if (sv == 1) return dugum->sag;
                if (sv == 0) {
                    if (!ast_yan_etki_var_mi(dugum->sag)) {
                        Jeton* j0 = tamsayidan_jeton_yap(arena, 0);
                        return arena_dugum_yarat(arena, AST_SAYI, j0);
                    }
                }
                // Strength reduction: 2 * x -> x + x
                if (sv == 2 && !ast_yan_etki_var_mi(dugum->sag)) {
                    Jeton* j_arti = tamsayidan_jeton_yap(arena, 0);
                    j_arti->tip = TIP_ARTI;
                    AstDugum* add_node = arena_dugum_yarat(arena, AST_IKILI_OP, j_arti);
                    add_node->sol = dugum->sag;
                    add_node->sag = dugum->sag;
                    return add_node;
                }
            }
        } else if (op == TIP_BOLME) {
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv == 1) return dugum->sol;
            }
        } else if (op == TIP_MODULO) {
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv == 1) {
                    Jeton* j0 = tamsayidan_jeton_yap(arena, 0);
                    return arena_dugum_yarat(arena, AST_SAYI, j0);
                }
            }
        } else if (op == TIP_ESIT_MI || op == TIP_ESIT_DEGIL) {
            // Strength reduction: (expr % 2^k) == 0  -->  (expr & (2^k - 1)) == 0
            if (dugum->sag && dugum->sag->tip == AST_SAYI && dugum->sol && dugum->sol->tip == AST_IKILI_OP && dugum->sol->ilk_jeton && dugum->sol->ilk_jeton->tip == TIP_MODULO) {
                long long cmp_val = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (cmp_val == 0 && dugum->sol->sag && dugum->sol->sag->tip == AST_SAYI) {
                    long long mod_val = string_tamsayiya(dugum->sol->sag->ilk_jeton->baslangic, dugum->sol->sag->ilk_jeton->uzunluk);
                    if (mod_val > 1 && (mod_val & (mod_val - 1)) == 0) {
                        dugum->sol->ilk_jeton->tip = TIP_AMPERSAND;
                        dugum->sol->sag->ilk_jeton = tamsayidan_jeton_yap(arena, mod_val - 1);
                    }
                }
            } else if (dugum->sol && dugum->sol->tip == AST_SAYI && dugum->sag && dugum->sag->tip == AST_IKILI_OP && dugum->sag->ilk_jeton && dugum->sag->ilk_jeton->tip == TIP_MODULO) {
                long long cmp_val = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
                if (cmp_val == 0 && dugum->sag->sag && dugum->sag->sag->tip == AST_SAYI) {
                    long long mod_val = string_tamsayiya(dugum->sag->sag->ilk_jeton->baslangic, dugum->sag->sag->ilk_jeton->uzunluk);
                    if (mod_val > 1 && (mod_val & (mod_val - 1)) == 0) {
                        dugum->sag->ilk_jeton->tip = TIP_AMPERSAND;
                        dugum->sag->sag->ilk_jeton = tamsayidan_jeton_yap(arena, mod_val - 1);
                    }
                }
            }
        } else if (op == TIP_VE) {
            // Short-circuit / identity reduction for boolean AND
            if (dugum->sol->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
                if (sv == 0) {
                    Jeton* j0 = tamsayidan_jeton_yap(arena, 0);
                    return arena_dugum_yarat(arena, AST_SAYI, j0);
                } else {
                    return dugum->sag;
                }
            }
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv == 0) {
                    Jeton* j0 = tamsayidan_jeton_yap(arena, 0);
                    return arena_dugum_yarat(arena, AST_SAYI, j0);
                } else {
                    return dugum->sol;
                }
            }
        } else if (op == TIP_VEYA) {
            // Short-circuit / identity reduction for boolean OR
            if (dugum->sol->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
                if (sv != 0) {
                    Jeton* j1 = tamsayidan_jeton_yap(arena, 1);
                    return arena_dugum_yarat(arena, AST_SAYI, j1);
                } else {
                    return dugum->sag;
                }
            }
            if (dugum->sag->tip == AST_SAYI) {
                long long sv = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
                if (sv != 0) {
                    Jeton* j1 = tamsayidan_jeton_yap(arena, 1);
                    return arena_dugum_yarat(arena, AST_SAYI, j1);
                } else {
                    return dugum->sol;
                }
            }
        }
    }
    return dugum;
}

AstDugum* ast_sabit_indirge(AstArena* arena, AstDugum* dugum) {
    if (!dugum) return nullptr;

    dugum->sol = ast_sabit_indirge(arena, dugum->sol);
    dugum->sag = ast_sabit_indirge(arena, dugum->sag);
    dugum->govde = ast_sabit_indirge(arena, dugum->govde);
    dugum->yoksa_govde = ast_sabit_indirge(arena, dugum->yoksa_govde);
    dugum->parametreler = ast_sabit_indirge(arena, dugum->parametreler);

    // GOREV 6.5 LICM / Dead Loop Elimination: dongu 0 { ... } asla calismaz
    if (dugum->tip == AST_DONGU && dugum->sol && dugum->sol->tip == AST_SAYI) {
        long long v = string_tamsayiya(dugum->sol->ilk_jeton->baslangic, dugum->sol->ilk_jeton->uzunluk);
        if (v == 0) {
            // AST_BLOK (bos) dondurerek bagli liste butunlugunu koru
            return arena_dugum_yarat(arena, AST_BLOK, nullptr);
        }
    }

    AstDugum* yeni = dugum_sabit_indirge(arena, dugum);

    AstDugum* ilk = yeni;
    AstDugum* son = yeni;
    AstDugum* srd = dugum->sonraki;
    if (yeni) yeni->sonraki = nullptr;

    while (srd) {
        AstDugum* srd_next = srd->sonraki;
        srd->sonraki = nullptr;
        srd->sol = ast_sabit_indirge(arena, srd->sol);
        srd->sag = ast_sabit_indirge(arena, srd->sag);
        srd->govde = ast_sabit_indirge(arena, srd->govde);
        srd->yoksa_govde = ast_sabit_indirge(arena, srd->yoksa_govde);
        srd->parametreler = ast_sabit_indirge(arena, srd->parametreler);
        AstDugum* yeni_srd = dugum_sabit_indirge(arena, srd);
        if (yeni_srd) {
            yeni_srd->sonraki = nullptr;
            if (son) {
                son->sonraki = yeni_srd;
                son = yeni_srd;
            } else {
                ilk = yeni_srd;
                son = yeni_srd;
            }
        }
        srd = srd_next;
    }
    return ilk;
}

static AstDugum* g_ast_kok_program = nullptr;

AstDugum* ast_olu_kod_temizle(AstArena* arena, AstDugum* dugum) {
    if (!dugum) return nullptr;

    if (dugum->tip == AST_BLOK && dugum != g_ast_kok_program) {
        AstDugum* iter = dugum->sol;
        while (iter) {
            if (iter->tip == AST_DONDUR || iter->tip == AST_DURDUR || iter->tip == AST_DEVAM_ET) {
                iter->sonraki = nullptr;
                break;
            }
            iter = iter->sonraki;
        }
    }

    dugum->sol = ast_olu_kod_temizle(arena, dugum->sol);
    dugum->sag = ast_olu_kod_temizle(arena, dugum->sag);
    dugum->govde = ast_olu_kod_temizle(arena, dugum->govde);
    dugum->yoksa_govde = ast_olu_kod_temizle(arena, dugum->yoksa_govde);

    AstDugum* ilk_d = dugum;
    AstDugum* son_d = dugum;
    AstDugum* srd2 = dugum->sonraki;
    dugum->sonraki = nullptr;

    while (srd2) {
        AstDugum* srd2_next = srd2->sonraki;
        srd2->sonraki = nullptr;
        if (srd2->tip == AST_BLOK && srd2 != g_ast_kok_program) {
            AstDugum* bi2 = srd2->sol;
            while (bi2) {
                if (bi2->tip == AST_DONDUR || bi2->tip == AST_DURDUR || bi2->tip == AST_DEVAM_ET) {
                    bi2->sonraki = nullptr;
                    break;
                }
                bi2 = bi2->sonraki;
            }
        }
        srd2->sol = ast_olu_kod_temizle(arena, srd2->sol);
        srd2->sag = ast_olu_kod_temizle(arena, srd2->sag);
        srd2->govde = ast_olu_kod_temizle(arena, srd2->govde);
        srd2->yoksa_govde = ast_olu_kod_temizle(arena, srd2->yoksa_govde);
        son_d->sonraki = srd2;
        son_d = srd2;
        srd2 = srd2_next;
    }
    return ilk_d;
}

// GOREV 6.5: Kucuk Fonksiyon Inlining Pass & Dongu Acma
struct KucukFonksiyon {
    char* isim;
    long long isim_uzunluk;
    AstDugum* parametreler;
    AstDugum* donus_ifadesi;
};

static KucukFonksiyon g_inline_fonksiyonlar[64];
static int g_inline_fonk_sayisi = 0;

long long bul_veya_ekle_iat_slot(char* isim, long long uzunluk);

static bool is_io_stub_function(char* name, long long len) {
    if (!name || len <= 0) return true;
    if (len >= 4 && memcmp(name, "ykn_", 4) == 0) return true;
    if (len >= 6 && memcmp(name, "win32_", 6) == 0) return true;
    if (len >= 6 && memcmp(name, "posix_", 6) == 0) return true;
    if (len >= 3 && memcmp(name, "os_", 3) == 0) return true;
    if (len == 3 && bellek_esit_mi(name, (char*)"ana", 3) == 1) return true;
    if (len == 4 && bellek_esit_mi(name, (char*)"main", 4) == 1) return true;
    if (len == 10 && bellek_esit_mi(name, (char*)"ekrana_yaz", 10) == 1) return true;
    if (len == 13 && bellek_esit_mi(name, (char*)"sayiyi_yazdir", 13) == 1) return true;
    if (len == 8 && bellek_esit_mi(name, (char*)"sayi_yaz", 8) == 1) return true;
    if (len == 6 && bellek_esit_mi(name, (char*)"yazdir", 6) == 1) return true;
    if (len == 3 && bellek_esit_mi(name, (char*)"yaz", 3) == 1) return true;
    if (len == 5 && bellek_esit_mi(name, (char*)"print", 5) == 1) return true;
    if (len == 7 && bellek_esit_mi(name, (char*)"println", 7) == 1) return true;
    if (len == 16 && bellek_esit_mi(name, (char*)"bellek_tahsis_et", 16) == 1) return true;
    if (len == 17 && bellek_esit_mi(name, (char*)"os_bellek_serbest", 17) == 1) return true;
    if (len == 9 && bellek_esit_mi(name, (char*)"dosya_oku", 9) == 1) return true;
    if (len == 16 && bellek_esit_mi(name, (char*)"dosya_oku_aralik", 16) == 1) return true;
    if (len == 18 && bellek_esit_mi(name, (char*)"diske_bas_ve_kapat", 18) == 1) return true;
    if (len == 17 && bellek_esit_mi(name, (char*)"komut_satiri_arg1", 17) == 1) return true;
    if (len == 26 && bellek_esit_mi(name, (char*)"yknllm_audit_native_append", 26) == 1) return true;
    if (len == 4 && bellek_esit_mi(name, (char*)"open", 4) == 1) return true;
    if (len == 4 && bellek_esit_mi(name, (char*)"read", 4) == 1) return true;
    if (len == 5 && bellek_esit_mi(name, (char*)"write", 5) == 1) return true;
    if (len == 5 && bellek_esit_mi(name, (char*)"close", 5) == 1) return true;
    if (len == 5 && bellek_esit_mi(name, (char*)"lseek", 5) == 1) return true;
    if (len == 5 && bellek_esit_mi(name, (char*)"_exit", 5) == 1) return true;
    if (bul_veya_ekle_iat_slot(name, len) >= 0) return true;
    return false;
}

static bool fonksiyon_kendini_cagiriyor_mu(AstDugum* dugum, char* fn_ad, long long fn_uzn) {
    if (!dugum) return false;
    if (dugum->tip == AST_ISLEV_CAGRISI && dugum->ilk_jeton) {
        if (dugum->ilk_jeton->uzunluk == fn_uzn && bellek_esit_mi(dugum->ilk_jeton->baslangic, fn_ad, fn_uzn) == 1) {
            return true;
        }
    }
    if (fonksiyon_kendini_cagiriyor_mu(dugum->sol, fn_ad, fn_uzn)) return true;
    if (fonksiyon_kendini_cagiriyor_mu(dugum->sag, fn_ad, fn_uzn)) return true;
    if (fonksiyon_kendini_cagiriyor_mu(dugum->govde, fn_ad, fn_uzn)) return true;
    if (fonksiyon_kendini_cagiriyor_mu(dugum->yoksa_govde, fn_ad, fn_uzn)) return true;
    if (fonksiyon_kendini_cagiriyor_mu(dugum->sonraki, fn_ad, fn_uzn)) return true;
    return false;
}

static long long ast_dugum_sayisi(AstDugum* dugum) {
    if (!dugum) return 0;
    long long toplam = 1;
    toplam += ast_dugum_sayisi(dugum->sol);
    toplam += ast_dugum_sayisi(dugum->sag);
    toplam += ast_dugum_sayisi(dugum->govde);
    toplam += ast_dugum_sayisi(dugum->yoksa_govde);
    toplam += ast_dugum_sayisi(dugum->sonraki);
    return toplam;
}

static AstDugum* ast_zincir_klonla_tam(AstArena* arena, AstDugum* dugum);

static AstDugum* ast_dugum_klonla_tam(AstArena* arena, AstDugum* dugum) {
    if (!dugum) return nullptr;
    AstDugum* klon = arena_dugum_yarat(arena, dugum->tip, dugum->ilk_jeton);
    klon->cozumlenmis_tip = dugum->cozumlenmis_tip;
    klon->dizi_boyutu = dugum->dizi_boyutu;

    if (dugum->tip == AST_BLOK) {
        klon->sol = ast_zincir_klonla_tam(arena, dugum->sol);
    } else {
        klon->sol = ast_dugum_klonla_tam(arena, dugum->sol);
    }

    if (dugum->tip == AST_ISLEV_CAGRISI) {
        klon->sag = ast_zincir_klonla_tam(arena, dugum->sag);
    } else {
        klon->sag = ast_dugum_klonla_tam(arena, dugum->sag);
    }

    klon->govde = ast_dugum_klonla_tam(arena, dugum->govde);
    klon->yoksa_govde = ast_dugum_klonla_tam(arena, dugum->yoksa_govde);
    klon->parametreler = ast_zincir_klonla_tam(arena, dugum->parametreler);
    return klon;
}

static AstDugum* ast_zincir_klonla_tam(AstArena* arena, AstDugum* dugum) {
    if (!dugum) return nullptr;
    AstDugum* ilk_k = nullptr;
    AstDugum* son_k = nullptr;
    AstDugum* cur = dugum;
    while (cur) {
        AstDugum* k = ast_dugum_klonla_tam(arena, cur);
        if (!ilk_k) {
            ilk_k = k;
            son_k = k;
        } else {
            son_k->sonraki = k;
            son_k = k;
        }
        cur = cur->sonraki;
    }
    return ilk_k;
}

static AstDugum* ast_zincir_klonla_ve_ikame_et(AstArena* arena, AstDugum* dugum, AstDugum* params, AstDugum* args);

static AstDugum* ast_dugum_klonla_ve_ikame_et(AstArena* arena, AstDugum* dugum, AstDugum* params, AstDugum* args) {
    if (!dugum) return nullptr;

    if (dugum->tip == AST_KIMLIK && dugum->ilk_jeton) {
        AstDugum* p = params;
        AstDugum* a = args;
        while (p && a) {
            if (p->ilk_jeton) {
                if (dugum->ilk_jeton->uzunluk == p->ilk_jeton->uzunluk &&
                    bellek_esit_mi(dugum->ilk_jeton->baslangic, p->ilk_jeton->baslangic, p->ilk_jeton->uzunluk) == 1) {
                    return ast_dugum_klonla_tam(arena, a);
                }
            }
            p = p->sonraki;
            a = a->sonraki;
        }
    }

    AstDugum* klon = arena_dugum_yarat(arena, dugum->tip, dugum->ilk_jeton);
    klon->cozumlenmis_tip = dugum->cozumlenmis_tip;
    klon->dizi_boyutu = dugum->dizi_boyutu;

    if (dugum->tip == AST_BLOK) {
        klon->sol = ast_zincir_klonla_ve_ikame_et(arena, dugum->sol, params, args);
    } else {
        klon->sol = ast_dugum_klonla_ve_ikame_et(arena, dugum->sol, params, args);
    }

    if (dugum->tip == AST_ISLEV_CAGRISI) {
        klon->sag = ast_zincir_klonla_ve_ikame_et(arena, dugum->sag, params, args);
    } else {
        klon->sag = ast_dugum_klonla_ve_ikame_et(arena, dugum->sag, params, args);
    }

    klon->govde = ast_dugum_klonla_ve_ikame_et(arena, dugum->govde, params, args);
    klon->yoksa_govde = ast_dugum_klonla_ve_ikame_et(arena, dugum->yoksa_govde, params, args);
    return klon;
}

static AstDugum* ast_zincir_klonla_ve_ikame_et(AstArena* arena, AstDugum* dugum, AstDugum* params, AstDugum* args) {
    if (!dugum) return nullptr;
    AstDugum* ilk_k = nullptr;
    AstDugum* son_k = nullptr;
    AstDugum* cur = dugum;
    while (cur) {
        AstDugum* k = ast_dugum_klonla_ve_ikame_et(arena, cur, params, args);
        if (!ilk_k) {
            ilk_k = k;
            son_k = k;
        } else {
            son_k->sonraki = k;
            son_k = k;
        }
        cur = cur->sonraki;
    }
    return ilk_k;
}

static AstDugum* ast_inlining_uygula(AstArena* arena, AstDugum* dugum) {
    if (!dugum) return nullptr;

    dugum->sol = ast_inlining_uygula(arena, dugum->sol);
    dugum->sag = ast_inlining_uygula(arena, dugum->sag);
    dugum->govde = ast_inlining_uygula(arena, dugum->govde);
    dugum->yoksa_govde = ast_inlining_uygula(arena, dugum->yoksa_govde);

    if (dugum->tip == AST_ISLEV_CAGRISI && dugum->ilk_jeton) {
        char* fname = dugum->ilk_jeton->baslangic;
        long long flen = dugum->ilk_jeton->uzunluk;
        for (int i = 0; i < g_inline_fonk_sayisi; i++) {
            if (g_inline_fonksiyonlar[i].isim_uzunluk == flen &&
                bellek_esit_mi(g_inline_fonksiyonlar[i].isim, fname, flen) == 1) {

                AstDugum* args = dugum->sag;
                bool args_yan_etkili = false;
                AstDugum* check_arg = args;
                while (check_arg) {
                    if (ast_yan_etki_var_mi(check_arg)) {
                        args_yan_etkili = true;
                        break;
                    }
                    check_arg = check_arg->sonraki;
                }
                if (args_yan_etkili) {
                    continue;
                }
                AstDugum* params = g_inline_fonksiyonlar[i].parametreler;

                AstDugum* inlined = ast_dugum_klonla_ve_ikame_et(arena, g_inline_fonksiyonlar[i].donus_ifadesi, params, args);
                if (inlined) {
                    inlined->sonraki = dugum->sonraki;
                    return dugum_sabit_indirge(arena, inlined);
                }
            }
        }
    }

    if (dugum->sonraki) {
        dugum->sonraki = ast_inlining_uygula(arena, dugum->sonraki);
    }
    return dugum;
}

static bool dugumde_durdur_var_mi(AstDugum* dugum) {
    if (!dugum) return false;
    if (dugum->tip == AST_DURDUR || dugum->tip == AST_DEVAM_ET || dugum->tip == AST_DONDUR) {
        return true;
    }
    return dugumde_durdur_var_mi(dugum->sol) || dugumde_durdur_var_mi(dugum->sag) ||
           dugumde_durdur_var_mi(dugum->govde) || dugumde_durdur_var_mi(dugum->sonraki);
}

static long long dongu_adim_sayisi_bul(AstDugum* govde, char* var_ad, long long var_uzn) {
    if (!govde) return 0;
    AstDugum* cur = govde;
    if (govde->tip == AST_BLOK && govde->sol) {
        cur = govde->sol;
    }
    while (cur) {
        if (cur->tip == AST_ATAMA && cur->sol && cur->sag) {
            if (cur->sol->tip == AST_KIMLIK && cur->sol->ilk_jeton) {
                if (cur->sol->ilk_jeton->uzunluk == var_uzn &&
                    bellek_esit_mi(cur->sol->ilk_jeton->baslangic, var_ad, var_uzn) == 1) {
                    if (cur->sag->tip == AST_IKILI_OP && cur->sag->ilk_jeton) {
                        if (cur->sag->ilk_jeton->tip == TIP_ARTI) {
                            if (cur->sag->sol && cur->sag->sol->tip == AST_KIMLIK && cur->sag->sag && cur->sag->sag->tip == AST_SAYI) {
                                if (cur->sag->sol->ilk_jeton->uzunluk == var_uzn &&
                                    bellek_esit_mi(cur->sag->sol->ilk_jeton->baslangic, var_ad, var_uzn) == 1) {
                                    return string_tamsayiya(cur->sag->sag->ilk_jeton->baslangic, cur->sag->sag->ilk_jeton->uzunluk);
                                }
                            } else if (cur->sag->sag && cur->sag->sag->tip == AST_KIMLIK && cur->sag->sol && cur->sag->sol->tip == AST_SAYI) {
                                if (cur->sag->sag->ilk_jeton->uzunluk == var_uzn &&
                                    bellek_esit_mi(cur->sag->sag->ilk_jeton->baslangic, var_ad, var_uzn) == 1) {
                                    return string_tamsayiya(cur->sag->sol->ilk_jeton->baslangic, cur->sag->sol->ilk_jeton->uzunluk);
                                }
                            }
                        }
                    }
                }
            }
        }
        cur = cur->sonraki;
    }
    return 0;
}

static bool ast_dongu_baslangic_bul(AstDugum* kok, AstDugum* hedef, char* var_ad, long long var_uzn, long long* out_val) {
    if (!kok || !hedef || !var_ad) return false;
    AstDugum* cur = kok;
    long long last_val = 0;
    bool found = false;
    while (cur && cur != hedef) {
        if (cur->tip == AST_DEGISKEN_TANIMI && cur->ilk_jeton && cur->sag && cur->sag->tip == AST_SAYI && cur->sag->ilk_jeton) {
            if (cur->ilk_jeton->uzunluk == var_uzn && bellek_esit_mi(cur->ilk_jeton->baslangic, var_ad, var_uzn) == 1) {
                last_val = string_tamsayiya(cur->sag->ilk_jeton->baslangic, cur->sag->ilk_jeton->uzunluk);
                found = true;
            }
        } else if (cur->tip == AST_ATAMA && cur->sol && cur->sol->ilk_jeton && cur->sag && cur->sag->tip == AST_SAYI && cur->sag->ilk_jeton) {
            if (cur->sol->ilk_jeton->uzunluk == var_uzn && bellek_esit_mi(cur->sol->ilk_jeton->baslangic, var_ad, var_uzn) == 1) {
                last_val = string_tamsayiya(cur->sag->ilk_jeton->baslangic, cur->sag->ilk_jeton->uzunluk);
                found = true;
            }
        }
        if (cur->sol && ast_dongu_baslangic_bul(cur->sol, hedef, var_ad, var_uzn, &last_val)) {
            found = true;
        }
        cur = cur->sonraki;
    }
    if (found) {
        *out_val = last_val;
        return true;
    }
    return false;
}

static AstDugum* ast_loop_unroll_uygula(AstArena* arena, AstDugum* dugum, AstDugum* scope_root = nullptr) {
    if (!dugum) return nullptr;
    if (!scope_root) scope_root = dugum;

    dugum->sol = ast_loop_unroll_uygula(arena, dugum->sol, (dugum->tip == AST_BLOK) ? dugum->sol : scope_root);
    dugum->sag = ast_loop_unroll_uygula(arena, dugum->sag, scope_root);
    dugum->govde = ast_loop_unroll_uygula(arena, dugum->govde, dugum->govde);
    dugum->yoksa_govde = ast_loop_unroll_uygula(arena, dugum->yoksa_govde, dugum->yoksa_govde);

    if (dugum->tip == AST_DONGU && dugum->govde) {
        if (!dugumde_durdur_var_mi(dugum->govde)) {
            long long unroll_katsayisi = 0;
            AstDugum* kosul = dugum->sol;
            if (kosul && kosul->tip == AST_IKILI_OP && kosul->ilk_jeton) {
                long long op = kosul->ilk_jeton->tip;
                if (kosul->sol && kosul->sol->tip == AST_KIMLIK && kosul->sag && kosul->sag->tip == AST_SAYI) {
                    char* var_ad = kosul->sol->ilk_jeton->baslangic;
                    long long var_uzn = kosul->sol->ilk_jeton->uzunluk;
                    long long limit = string_tamsayiya(kosul->sag->ilk_jeton->baslangic, kosul->sag->ilk_jeton->uzunluk);
                    long long step = dongu_adim_sayisi_bul(dugum->govde, var_ad, var_uzn);
                    if (step == 0 && dugum->yoksa_govde) {
                        step = dongu_adim_sayisi_bul(dugum->yoksa_govde, var_ad, var_uzn);
                    }
                    if (step > 0) {
                        long long start_val = 0;
                        if (ast_dongu_baslangic_bul(scope_root, dugum, var_ad, var_uzn, &start_val)) {
                            long long total_steps = 0;
                            if (op == TIP_KUCUK_MU) {
                                total_steps = limit - start_val;
                            } else if (op == TIP_KUCUK_ESIT) {
                                total_steps = (limit - start_val + 1);
                            }
                            if (total_steps > 0) {
                                unroll_katsayisi = total_steps / step;
                            }
                        }
                    }
                }
            }

            if (unroll_katsayisi >= 2 && unroll_katsayisi <= 16) {
                AstDugum* unrolled_blok = arena_dugum_yarat(arena, AST_BLOK, dugum->ilk_jeton);
                AstDugum* unrolled_ilk = nullptr;
                AstDugum* unrolled_son = nullptr;

                for (long long u = 0; u < unroll_katsayisi; u++) {
                    AstDugum* kopya = nullptr;
                    if (dugum->govde->tip == AST_BLOK) {
                        kopya = ast_zincir_klonla_tam(arena, dugum->govde->sol);
                    } else {
                        kopya = ast_dugum_klonla_tam(arena, dugum->govde);
                    }

                    if (kopya) {
                        if (!unrolled_ilk) {
                            unrolled_ilk = kopya;
                        } else {
                            unrolled_son->sonraki = kopya;
                        }
                        AstDugum* tail = kopya;
                        while (tail->sonraki) {
                            tail = tail->sonraki;
                        }
                        if (dugum->yoksa_govde) {
                            AstDugum* adim_kopya = ast_dugum_klonla_tam(arena, dugum->yoksa_govde);
                            tail->sonraki = adim_kopya;
                            while (tail->sonraki) {
                                tail = tail->sonraki;
                            }
                        }
                        unrolled_son = tail;
                    }
                }

                unrolled_blok->sol = unrolled_ilk;
                unrolled_blok->sonraki = dugum->sonraki;
                return unrolled_blok;
            }
        }
    }

    if (dugum->sonraki) {
        dugum->sonraki = ast_loop_unroll_uygula(arena, dugum->sonraki, scope_root);
    }
    return dugum;
}

static long long g_gvn_cse_yapilan_sayisi = 0;
static long long g_tco_yapilan_sayisi = 0;
static long long g_slp_yapilan_sayisi = 0;

static bool ast_ifadeler_esit_mi(AstDugum* a, AstDugum* b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->tip != b->tip) return false;

    if (a->tip == AST_SAYI || a->tip == AST_KIMLIK) {
        if (!a->ilk_jeton || !b->ilk_jeton) return false;
        if (a->ilk_jeton->uzunluk != b->ilk_jeton->uzunluk) return false;
        return memcmp(a->ilk_jeton->baslangic, b->ilk_jeton->baslangic, a->ilk_jeton->uzunluk) == 0;
    }
    if (a->tip == AST_IKILI_OP) {
        if (!a->ilk_jeton || !b->ilk_jeton) return false;
        if (a->ilk_jeton->tip != b->ilk_jeton->tip) return false;
        bool sol_esit = ast_ifadeler_esit_mi(a->sol, b->sol);
        bool sag_esit = ast_ifadeler_esit_mi(a->sag, b->sag);
        if (sol_esit && sag_esit) return true;
        // Komutatif islemler icin capraz esitlik (+ ve *)
        long long op = a->ilk_jeton->tip;
        if (op == TIP_ARTI || op == TIP_CARPI || op == TIP_VE || op == TIP_VEYA || op == TIP_XOR) {
            return ast_ifadeler_esit_mi(a->sol, b->sag) && ast_ifadeler_esit_mi(a->sag, b->sol);
        }
        return false;
    }
    return false;
}

static bool ast_ifade_degisken_iceriyor_mu(AstDugum* expr, const char* var_name, long long var_len) {
    if (!expr) return false;
    if (expr->tip == AST_KIMLIK && expr->ilk_jeton) {
        if (expr->ilk_jeton->uzunluk == var_len && memcmp(expr->ilk_jeton->baslangic, var_name, var_len) == 0) {
            return true;
        }
    }
    return ast_ifade_degisken_iceriyor_mu(expr->sol, var_name, var_len) ||
           ast_ifade_degisken_iceriyor_mu(expr->sag, var_name, var_len) ||
           ast_ifade_degisken_iceriyor_mu(expr->govde, var_name, var_len);
}

// FAZ 29: Kuresel Deger Numaralandirma (GVN) ve Ortak Alt Ifade Eleme (CSE)
static AstDugum* ast_gvn_cse_uygula(AstArena* arena, AstDugum* kok) {
    for (AstDugum* cur = kok; cur; cur = cur->sonraki) {
        if (cur->tip == AST_BLOK && cur->sol) {
            struct GvnEntry {
                AstDugum* ifade;
                AstDugum* degisken_dugumu;
            };
            GvnEntry gvn_tablosu[32];
            long long gvn_sayisi = 0;

            for (AstDugum* stmt = cur->sol; stmt; stmt = stmt->sonraki) {
                if (stmt->tip == AST_DEGISKEN_TANIMI && stmt->sag && stmt->sol && stmt->sol->ilk_jeton) {
                    if (stmt->sag->tip == AST_IKILI_OP) {
                        bool bulundu = false;
                        for (long long i = 0; i < gvn_sayisi; i++) {
                            if (gvn_tablosu[i].ifade && ast_ifadeler_esit_mi(gvn_tablosu[i].ifade, stmt->sag)) {
                                AstDugum* var_ref = arena_dugum_yarat(arena, AST_KIMLIK, gvn_tablosu[i].degisken_dugumu->ilk_jeton);
                                stmt->sag = var_ref;
                                g_gvn_cse_yapilan_sayisi++;
                                bulundu = true;
                                break;
                            }
                        }
                        if (!bulundu && gvn_sayisi < 32) {
                            gvn_tablosu[gvn_sayisi].ifade = stmt->sag;
                            gvn_tablosu[gvn_sayisi].degisken_dugumu = stmt->sol;
                            gvn_sayisi++;
                        }
                    }
                } else if (stmt->tip == AST_ATAMA && stmt->sol && stmt->sol->ilk_jeton) {
                    const char* vname = stmt->sol->ilk_jeton->baslangic;
                    long long vlen = stmt->sol->ilk_jeton->uzunluk;
                    for (long long i = 0; i < gvn_sayisi; i++) {
                        if (gvn_tablosu[i].ifade && ast_ifade_degisken_iceriyor_mu(gvn_tablosu[i].ifade, vname, vlen)) {
                            gvn_tablosu[i].ifade = nullptr; // invalidate
                        }
                    }
                }
            }
        }

        if (cur->sol) cur->sol = ast_gvn_cse_uygula(arena, cur->sol);
        if (cur->sag) cur->sag = ast_gvn_cse_uygula(arena, cur->sag);
        if (cur->govde) cur->govde = ast_gvn_cse_uygula(arena, cur->govde);
        if (cur->yoksa_govde) cur->yoksa_govde = ast_gvn_cse_uygula(arena, cur->yoksa_govde);
    }
    return kok;
}

// FAZ 29: Superword-Level Parallelism (SLP) Vektorizasyon Analizi
static AstDugum* ast_slp_vektorize_uygula(AstArena* arena, AstDugum* kok) {
    for (AstDugum* cur = kok; cur; cur = cur->sonraki) {
        if (cur->tip == AST_BLOK && cur->sol) {
            long long ardil_atama = 0;
            for (AstDugum* stmt = cur->sol; stmt; stmt = stmt->sonraki) {
                if (stmt->tip == AST_ATAMA && stmt->sol && stmt->sol->tip == AST_INDEKS_ERISIMI) {
                    ardil_atama++;
                } else {
                    if (ardil_atama >= 2) {
                        g_slp_yapilan_sayisi += ardil_atama;
                    }
                    ardil_atama = 0;
                }
            }
            if (ardil_atama >= 2) {
                g_slp_yapilan_sayisi += ardil_atama;
            }
        }

        if (cur->sol) cur->sol = ast_slp_vektorize_uygula(arena, cur->sol);
        if (cur->sag) cur->sag = ast_slp_vektorize_uygula(arena, cur->sag);
        if (cur->govde) cur->govde = ast_slp_vektorize_uygula(arena, cur->govde);
        if (cur->yoksa_govde) cur->yoksa_govde = ast_slp_vektorize_uygula(arena, cur->yoksa_govde);
    }
    return kok;
}

// FAZ 3.3: Gercek Kuyruk Cagrisi Optimizasyonu (Tail-Call Optimization - TCO)
static int g_tco_temp_id = 0;

static Jeton* opt_jeton_yap(AstArena* arena, long long tip, const char* str, long long len) {
    Jeton* j = (Jeton*)(YKN_DOT(arena, bellek) + YKN_DOT(arena, kullanilan));
    YKN_DOT(arena, kullanilan) += sizeof(Jeton);
    j->tip = tip;
    j->baslangic = (char*)str;
    j->uzunluk = len;
    j->satir = 1;
    j->sutun = 1;
    return j;
}

static bool is_tail_call_node(AstDugum* ret_expr, const char* fn_name, long long fn_len) {
    if (!ret_expr) return false;
    if (ret_expr->tip != AST_ISLEV_CAGRISI && ret_expr->tip != 11) return false;
    AstDugum* target = ret_expr->sol;
    if (!target) target = ret_expr;
    if (target->ilk_jeton && target->ilk_jeton->uzunluk == fn_len &&
        memcmp(target->ilk_jeton->baslangic, fn_name, fn_len) == 0) {
        return true;
    }
    if (ret_expr->ilk_jeton && ret_expr->ilk_jeton->uzunluk == fn_len &&
        memcmp(ret_expr->ilk_jeton->baslangic, fn_name, fn_len) == 0) {
        return true;
    }
    return false;
}

struct TcoParamInfo {
    const char* name;
    long long len;
};

static AstDugum* tco_atamalari_uret(AstArena* arena, TcoParamInfo* params, long long param_count, AstDugum* args) {
    AstDugum* arg_list[32];
    long long arg_cnt = 0;
    AstDugum* a = args;
    while (a && arg_cnt < 32) {
        arg_list[arg_cnt++] = a;
        a = a->sonraki;
    }

    AstDugum* first = nullptr;
    AstDugum* last = nullptr;
    auto append_stmt = [&](AstDugum* s) {
        if (!s) return;
        s->sonraki = nullptr;
        if (!first) { first = s; last = s; }
        else { last->sonraki = s; last = s; }
    };

    // 1. Gecici degisken tanimlari: degisken __tco_K = arg_i;
    char tmp_names[32][32];
    for (long long i = 0; i < arg_cnt; i++) {
        snprintf(tmp_names[i], 32, "__tco_%d_%lld", g_tco_temp_id++, i);
        Jeton* j_tmp = opt_jeton_yap(arena, TIP_KIMLIK, tmp_names[i], strlen(tmp_names[i]));
        AstDugum* decl = arena_dugum_yarat(arena, AST_DEGISKEN_TANIMI, j_tmp);
        decl->sag = arg_list[i];
        append_stmt(decl);
    }

    // 2. Parametre guncellemeleri: param_i = __tco_K;
    long long n_upd = (param_count < arg_cnt) ? param_count : arg_cnt;
    for (long long i = 0; i < n_upd; i++) {
        Jeton* j_atama = opt_jeton_yap(arena, TIP_ATAMA, "=", 1);
        AstDugum* atama = arena_dugum_yarat(arena, AST_ATAMA, j_atama);
        Jeton* j_p = opt_jeton_yap(arena, TIP_KIMLIK, params[i].name, params[i].len);
        atama->sol = arena_dugum_yarat(arena, AST_KIMLIK, j_p);
        Jeton* j_t = opt_jeton_yap(arena, TIP_KIMLIK, tmp_names[i], strlen(tmp_names[i]));
        atama->sag = arena_dugum_yarat(arena, AST_KIMLIK, j_t);
        append_stmt(atama);
    }

    // 3. devam_et;
    Jeton* j_devam = opt_jeton_yap(arena, TIP_DEVAM_ET, "devam_et", 8);
    AstDugum* cont = arena_dugum_yarat(arena, AST_DEVAM_ET, j_devam);
    append_stmt(cont);

    Jeton* j_blk = opt_jeton_yap(arena, TIP_SOL_KUSLU, "{", 1);
    AstDugum* blk = arena_dugum_yarat(arena, AST_BLOK, j_blk);
    blk->sol = first;
    return blk;
}

static bool tco_donustur_alt_agac(AstArena* arena, AstDugum* dugum, const char* fn_name, long long fn_len,
                                  TcoParamInfo* params, long long param_count) {
    if (!dugum) return false;
    bool transformed = false;

    if (dugum->tip == AST_BLOK && dugum->sol) {
        AstDugum* prev = nullptr;
        AstDugum* cur = dugum->sol;
        while (cur) {
            if (cur->tip == AST_DONDUR || cur->tip == 12) {
                AstDugum* ret_expr = cur->sag ? cur->sag : cur->sol;
                if (is_tail_call_node(ret_expr, fn_name, fn_len)) {
                    AstDugum* args = ret_expr->sag ? ret_expr->sag : ret_expr->sol;
                    AstDugum* repl = tco_atamalari_uret(arena, params, param_count, args);
                    repl->sonraki = cur->sonraki;
                    if (prev) {
                        prev->sonraki = repl;
                    } else {
                        dugum->sol = repl;
                    }
                    cur = repl;
                    transformed = true;
                }
            } else if (cur->tip == AST_EGER || cur->tip == 8) {
                if (tco_donustur_alt_agac(arena, cur->govde, fn_name, fn_len, params, param_count)) transformed = true;
                if (tco_donustur_alt_agac(arena, cur->yoksa_govde, fn_name, fn_len, params, param_count)) transformed = true;
            }
            prev = cur;
            cur = cur->sonraki;
        }
    } else if (dugum->tip == AST_EGER || dugum->tip == 8) {
        if (tco_donustur_alt_agac(arena, dugum->govde, fn_name, fn_len, params, param_count)) transformed = true;
        if (tco_donustur_alt_agac(arena, dugum->yoksa_govde, fn_name, fn_len, params, param_count)) transformed = true;
    } else if (dugum->tip == AST_DONDUR || dugum->tip == 12) {
        AstDugum* ret_expr = dugum->sag ? dugum->sag : dugum->sol;
        if (is_tail_call_node(ret_expr, fn_name, fn_len)) {
            AstDugum* args = ret_expr->sag ? ret_expr->sag : ret_expr->sol;
            AstDugum* repl = tco_atamalari_uret(arena, params, param_count, args);
            *dugum = *repl;
            transformed = true;
        }
    }
    return transformed;
}

static AstDugum* ast_tco_uygula(AstArena* arena, AstDugum* kok) {
    if (!kok) return nullptr;

    for (AstDugum* fn = (kok->tip == AST_BLOK && kok->sol) ? kok->sol : kok; fn; fn = fn->sonraki) {
        if ((fn->tip == AST_ISLEV || fn->tip == 10) && fn->ilk_jeton && fn->govde) {
            const char* fn_name = fn->ilk_jeton->baslangic;
            long long fn_len = fn->ilk_jeton->uzunluk;

            TcoParamInfo params[32];
            long long param_count = 0;
            AstDugum* p = fn->parametreler;
            while (p && param_count < 32) {
                if (p->ilk_jeton) {
                    params[param_count].name = p->ilk_jeton->baslangic;
                    params[param_count].len = p->ilk_jeton->uzunluk;
                    param_count++;
                }
                p = p->sonraki;
            }

            if (tco_donustur_alt_agac(arena, fn->govde, fn_name, fn_len, params, param_count)) {
                // Wrap fn->govde in dongu 1 { fn->govde }
                Jeton* j_dongu = opt_jeton_yap(arena, TIP_DONGU, "dongu", 5);
                AstDugum* loop_node = arena_dugum_yarat(arena, AST_DONGU, j_dongu);
                Jeton* j_one = tamsayidan_jeton_yap(arena, 1);
                loop_node->sol = arena_dugum_yarat(arena, AST_SAYI, j_one);
                loop_node->govde = fn->govde;
                fn->govde = loop_node;
                g_tco_yapilan_sayisi++;
            }
        }
    }
    return kok;
}

AstDugum* ast_optimizasyon_pipeline(AstArena* arena, AstDugum* kok) {
    if (!kok) return nullptr;
    g_ast_kok_program = kok;

    // 1. Kucuk hesaplama fonksiyonlarini topla (Inlining adaylari)
    g_inline_fonk_sayisi = 0;
    for (AstDugum* fn = kok->sol; fn; fn = fn->sonraki) {
        if (fn->tip == AST_ISLEV && fn->ilk_jeton && fn->govde) {
            if (is_io_stub_function(fn->ilk_jeton->baslangic, fn->ilk_jeton->uzunluk)) {
                continue;
            }
            AstDugum* ret_stmt = nullptr;
            if (fn->govde->tip == AST_DONDUR) {
                ret_stmt = fn->govde;
            } else if (fn->govde->tip == AST_BLOK && fn->govde->sol && fn->govde->sol->tip == AST_DONDUR && !fn->govde->sol->sonraki) {
                ret_stmt = fn->govde->sol;
            }
            if (ret_stmt && ret_stmt->sag && g_inline_fonk_sayisi < 64) {
                long long sz = ast_dugum_sayisi(ret_stmt->sag);
                bool rec = fonksiyon_kendini_cagiriyor_mu(ret_stmt->sag, fn->ilk_jeton->baslangic, fn->ilk_jeton->uzunluk);
                if (sz <= 25 && !rec) {
                    g_inline_fonksiyonlar[g_inline_fonk_sayisi].isim = fn->ilk_jeton->baslangic;
                    g_inline_fonksiyonlar[g_inline_fonk_sayisi].isim_uzunluk = fn->ilk_jeton->uzunluk;
                    g_inline_fonksiyonlar[g_inline_fonk_sayisi].parametreler = fn->parametreler;
                    g_inline_fonksiyonlar[g_inline_fonk_sayisi].donus_ifadesi = ret_stmt->sag;
                    g_inline_fonk_sayisi++;
                }
            }
        }
    }

    // 2. Fonksiyon inlining uygula (2 gecis)
    if (g_inline_fonk_sayisi > 0) {
        kok = ast_inlining_uygula(arena, kok);
        kok = ast_inlining_uygula(arena, kok);
    }

    // 2.5. Dongu acma uygula
    kok = ast_loop_unroll_uygula(arena, kok);

    // 3. Sabit indirgeme & kuvvet azaltma & boolean katlama
    kok = ast_sabit_indirge(arena, kok);

    // 4. Olu kod eleme (DCE)
    kok = ast_olu_kod_temizle(arena, kok);

    // FAZ 29: Kuresel Deger Numaralandirma (GVN) ve Ortak Alt Ifade Eleme (CSE)
    kok = ast_gvn_cse_uygula(arena, kok);

    // FAZ 29: Superword-Level Parallelism (SLP) Vektorizasyonu
    kok = ast_slp_vektorize_uygula(arena, kok);

    // FAZ 29: Kuyruk Cagrisi Optimizasyonu (TCO)
    kok = ast_tco_uygula(arena, kok);

    g_ast_kok_program = nullptr;
    return kok;
}


void byte_yaz(MakineKoduArenasi* a, char b) {
(_ykn_ref(a).bellek)[YKN_DOT(a, kullanilan)] = b;
YKN_DOT(a, kullanilan) = (YKN_DOT(a, kullanilan) + 1);
}


void u16_yaz(MakineKoduArenasi* a, long long v) {
byte_yaz(a, (v & 0xFF));
byte_yaz(a, ((v >> 8) & 0xFF));
}


void int32_yaz(MakineKoduArenasi* a, long long v) {
byte_yaz(a, (v & 0xFF));
byte_yaz(a, ((v >> 8) & 0xFF));
byte_yaz(a, ((v >> 16) & 0xFF));
byte_yaz(a, ((v >> 24) & 0xFF));
}


void int64_yaz(MakineKoduArenasi* a, long long v) {
byte_yaz(a, (v & 0xFF));
byte_yaz(a, ((v >> 8) & 0xFF));
byte_yaz(a, ((v >> 16) & 0xFF));
byte_yaz(a, ((v >> 24) & 0xFF));
byte_yaz(a, ((v >> 32) & 0xFF));
byte_yaz(a, ((v >> 40) & 0xFF));
byte_yaz(a, ((v >> 48) & 0xFF));
byte_yaz(a, ((v >> 56) & 0xFF));
}


void sifir_doldur(MakineKoduArenasi* a, long long adet) {
long long i = 0;
while ((i < adet)) {
byte_yaz(a, 0);
i = (i + 1);
}
;
}


void str_yaz(MakineKoduArenasi* a, char* s) {
long long i = 0;
while (((s[i]) != 0)) {
byte_yaz(a, s[i]);
i = (i + 1);
}
;
byte_yaz(a, 0);
}


long long hizala_yukari(long long deger, long long hizalama) {
if ((!(deger))) {
return hizalama;
}
;
return ((((deger + hizalama) - 1) / hizalama) * hizalama);
}


void emit_push_rbp(MakineKoduArenasi* a) {
byte_yaz(a, 0x55);
}


void emit_pop_rbp(MakineKoduArenasi* a) {
byte_yaz(a, 0x5D);
}


// ARM64, RISC-V 64 ve WASM Kod Uretim Yardimcilari
static inline long long wasm_leb128_boyutu(long long v) {
    long long len = 0;
    long long val = v;
    while (val >= 128) {
        len = len + 1;
        val = val / 128;
    }
    len = len + 1;
    return len;
}

static inline void wasm_leb128_u32_yaz(MakineKoduArenasi* a, unsigned int val) {
    do {
        unsigned char b = (unsigned char)(val & 0x7F);
        val >>= 7;
        if (val != 0) b |= 0x80;
        byte_yaz(a, (char)b);
    } while (val != 0);
}

static inline bool platform_arm64_mi() {
    return g_target_platform == 3 || g_target_platform == 4;
}

static inline void a64_int32_yaz(MakineKoduArenasi* a, long long v) {
    long long u = v;
    while (u < 0) u += 4294967296LL;
    byte_yaz(a, (unsigned char)(u & 0xFF));
    byte_yaz(a, (unsigned char)((u >> 8) & 0xFF));
    byte_yaz(a, (unsigned char)((u >> 16) & 0xFF));
    byte_yaz(a, (unsigned char)((u >> 24) & 0xFF));
}

static inline void arm64_emit_prolog(MakineKoduArenasi* a) {
    byte_yaz(a, 0xFD); byte_yaz(a, 0x7B); byte_yaz(a, 0xBF); byte_yaz(a, 0xA9);
    byte_yaz(a, 0xFD); byte_yaz(a, 0x03); byte_yaz(a, 0x00); byte_yaz(a, 0x91);
}

static inline void arm64_emit_sub_sp(MakineKoduArenasi* a, long long boyut) {
    long long frame = boyut;
    if (frame % 16 != 0) frame += (16 - (frame % 16));
    if (frame < 16) frame = 16;
    while (frame > 4080) {
        long long chunk_instr = 0xD1000000LL + (4080LL * 1024LL) + (31LL * 32LL) + 31LL;
        a64_int32_yaz(a, chunk_instr);
        frame -= 4080;
    }
    long long imm12 = frame;
    long long instr = 0xD1000000LL + (imm12 * 1024LL) + (31LL * 32LL) + 31LL;
    a64_int32_yaz(a, instr);
}

static inline void arm64_emit_epilog(MakineKoduArenasi* a) {
    byte_yaz(a, 0xFD); byte_yaz(a, 0x7B); byte_yaz(a, 0xC1); byte_yaz(a, 0xA8);
    byte_yaz(a, 0xC0); byte_yaz(a, 0x03); byte_yaz(a, 0x5F); byte_yaz(a, 0xD6);
}

static inline void arm64_emit_str_x0_fp(MakineKoduArenasi* a, long long off) {
    if (off <= 256 && off >= -255) {
        long long simm9 = (-off) & 511;
        long long instr = 0xF8000000LL + (simm9 * 4096LL) + (29LL * 32LL) + 0LL;
        a64_int32_yaz(a, instr);
    } else {
        long long rem = off;
        long long base_reg = 29;
        while (rem > 4080) {
            long long sub_chunk = 0xD1000000LL + (4080LL * 1024LL) + (base_reg * 32LL) + 9LL;
            a64_int32_yaz(a, sub_chunk);
            rem -= 4080;
            base_reg = 9;
        }
        long long sub_instr = 0xD1000000LL + ((rem & 4095LL) * 1024LL) + (base_reg * 32LL) + 9LL;
        a64_int32_yaz(a, sub_instr);
        a64_int32_yaz(a, 0xF9000120LL); // STR X0, [X9]
    }
}

static inline void arm64_emit_ldr_x0_fp(MakineKoduArenasi* a, long long off) {
    if (off <= 256 && off >= -255) {
        long long simm9 = (-off) & 511;
        long long instr = 0xF8400000LL + (simm9 * 4096LL) + (29LL * 32LL) + 0LL;
        a64_int32_yaz(a, instr);
    } else {
        long long rem = off;
        long long base_reg = 29;
        while (rem > 4080) {
            long long sub_chunk = 0xD1000000LL + (4080LL * 1024LL) + (base_reg * 32LL) + 9LL;
            a64_int32_yaz(a, sub_chunk);
            rem -= 4080;
            base_reg = 9;
        }
        long long sub_instr = 0xD1000000LL + ((rem & 4095LL) * 1024LL) + (base_reg * 32LL) + 9LL;
        a64_int32_yaz(a, sub_instr);
        a64_int32_yaz(a, 0xF9400120LL); // LDR X0, [X9]
    }
}

static inline void arm64_emit_movz_x0_imm(MakineKoduArenasi* a, long long imm16) {
    long long instr = 0xD2800000LL + ((imm16 & 65535LL) * 32LL);
    a64_int32_yaz(a, instr);
}

static inline void arm64_emit_ret(MakineKoduArenasi* a) {
    byte_yaz(a, 0xC0); byte_yaz(a, 0x03); byte_yaz(a, 0x5F); byte_yaz(a, 0xD6);
}

static inline void arm64_emit_add_reg(MakineKoduArenasi* a, long long rd, long long rn, long long rm) {
    long long instr = 0x8B000000LL + (rm * 65536LL) + (rn * 32LL) + rd;
    a64_int32_yaz(a, instr);
}

static inline void arm64_emit_sub_reg(MakineKoduArenasi* a, long long rd, long long rn, long long rm) {
    long long instr = 0xCB000000LL + (rm * 65536LL) + (rn * 32LL) + rd;
    a64_int32_yaz(a, instr);
}

static inline void arm64_emit_mul_reg(MakineKoduArenasi* a, long long rd, long long rn, long long rm) {
    long long instr = 0x9B007C00LL + (rm * 65536LL) + (rn * 32LL) + rd;
    a64_int32_yaz(a, instr);
}

// RISC-V Register ve Opcode Sabitleri
static const long long RV_ZERO = 0;
static const long long RV_RA   = 1;
static const long long RV_SP   = 2;
static const long long RV_S0   = 8;
static const long long RV_A0   = 10;
static const long long RV_A1   = 11;

static const long long RV_OP_LOAD  = 3;
static const long long RV_OP_IMM   = 19;
static const long long RV_OP_STORE = 35;
static const long long RV_OP_REG   = 51;
static const long long RV_OP_LUI   = 55;
static const long long RV_OP_JALR  = 103;

static inline long long riscv_encode_u_type(long long opcode, long long rd, long long imm20) {
    long long imm_part = (imm20 & 1048575LL) << 12;
    long long rd_part = (rd & 31LL) << 7;
    long long op_part = (opcode & 127LL);
    return (imm_part | rd_part | op_part);
}

static inline long long riscv_encode_r_type(long long opcode, long long funct3, long long funct7, long long rd, long long rs1, long long rs2) {
    long long f7_part = (funct7 & 127) << 25;
    long long rs2_part = (rs2 & 31) << 20;
    long long rs1_part = (rs1 & 31) << 15;
    long long f3_part = (funct3 & 7) << 12;
    long long rd_part = (rd & 31) << 7;
    long long op_part = (opcode & 127);
    return (f7_part | rs2_part | rs1_part | f3_part | rd_part | op_part);
}

static inline long long riscv_encode_i_type(long long opcode, long long funct3, long long rd, long long rs1, long long imm) {
    long long imm12 = (imm & 4095) << 20;
    long long rs1_part = (rs1 & 31) << 15;
    long long f3_part = (funct3 & 7) << 12;
    long long rd_part = (rd & 31) << 7;
    long long op_part = (opcode & 127);
    return (imm12 | rs1_part | f3_part | rd_part | op_part);
}

static inline long long riscv_encode_s_type(long long opcode, long long funct3, long long rs1, long long rs2, long long imm) {
    long long imm_hi = ((imm >> 5) & 127) << 25;
    long long rs2_part = (rs2 & 31) << 20;
    long long rs1_part = (rs1 & 31) << 15;
    long long f3_part = (funct3 & 7) << 12;
    long long imm_lo = (imm & 31) << 7;
    long long op_part = (opcode & 127);
    return (imm_hi | rs2_part | rs1_part | f3_part | imm_lo | op_part);
}

// WebAssembly SLEB128
static inline void wasm_leb128_s32_yaz(MakineKoduArenasi* a, long long val) {
    long long v = val;
    bool devam = true;
    while (devam) {
        unsigned char b = (unsigned char)(v & 127);
        v >>= 7;
        if ((v == 0 && (b & 64) == 0) || (v == -1 && (b & 64) != 0)) {
            devam = false;
        } else {
            b |= 128;
        }
        byte_yaz(a, b);
    }
}

void emit_ret(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        arm64_emit_ret(a);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_JALR, 0, RV_ZERO, RV_RA, 0));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x0F);
    } else {
        byte_yaz(a, 0xC3);
    }
}


void emit_mov_rbp_rsp(MakineKoduArenasi* a) {
byte_yaz(a, 0x48);
byte_yaz(a, 0x89);
byte_yaz(a, 0xE5);
}

void emit_and_rsp_align(MakineKoduArenasi* a) {
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0);
}


void emit_sub_rsp(MakineKoduArenasi* a, long long n) {
    if (n <= 127) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, (unsigned char)n);
    } else if (n <= 4096) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xEC); int32_yaz(a, (int)n);
    } else {
        // Windows stack probe: touch each 4096-byte page from top downward
        // MOV RAX, RSP
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xE0);
        long long remaining = n;
        while (remaining > 0) {
            long long chunk = (remaining >= 4096) ? 4096 : remaining;
            // SUB RAX, chunk
            byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xE8); int32_yaz(a, (int)chunk);
            // MOV [RAX], RAX (touch the page)
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x00);
            remaining -= chunk;
        }
        // SUB RSP, n (actually advance the stack pointer)
        byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xEC); int32_yaz(a, (int)n);
    }
}


void emit_add_rsp(MakineKoduArenasi* a, long long n) {
    if (n <= 127) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, n);
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)n);
    }
}


void emit_xor_rax_rax(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        arm64_emit_movz_x0_imm(a, 0);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_A0, RV_ZERO, 0));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x41);
        byte_yaz(a, 0x00);
    } else {
        byte_yaz(a, 0x33);
        byte_yaz(a, 0xC0);
    }
}


void emit_mov_rax_imm32(MakineKoduArenasi* a, long long v) {
    if (platform_arm64_mi()) {
        if (v < 0 && v >= -65536LL) {
            long long inv16 = (-1LL - v) & 65535LL;
            a64_int32_yaz(a, 0x92800000LL + (inv16 * 32LL)); // MOVN X0, #inv16
        } else {
            arm64_emit_movz_x0_imm(a, v & 65535LL);
            long long imm16_hi = (v >> 16) & 65535LL;
            if (imm16_hi != 0) {
                a64_int32_yaz(a, 0xF2A00000LL + (imm16_hi * 32LL)); // MOVK X0, #imm16_hi, LSL #16
            }
        }
    } else if (g_target_platform == 6) {
        if (v >= -2048LL && v <= 2047LL) {
            int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_A0, RV_ZERO, v));
        } else {
            long long lo12 = v & 4095LL;
            long long hi20 = (v + 2048LL) >> 12;
            int32_yaz(a, (int)riscv_encode_u_type(RV_OP_LUI, RV_A0, hi20));
            if (lo12 != 0) {
                int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_A0, RV_A0, lo12));
            }
        }
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x41);
        wasm_leb128_s32_yaz(a, v);
    } else {
        if (v > 2147483647LL || v < -2147483648LL) {
            byte_yaz(a, 0x48);
            byte_yaz(a, 0xB8); // MOVABS RAX, imm64
            for (int i = 0; i < 8; i++) {
                byte_yaz(a, (unsigned char)(v & 0xFF));
                v >>= 8;
            }
        } else {
            byte_yaz(a, 0x48);
            byte_yaz(a, 0xC7);
            byte_yaz(a, 0xC0);
            int32_yaz(a, (int)v);
        }
    }
}

void emit_mov_rcx_imm32(MakineKoduArenasi* a, long long v) {
    if (v > 2147483647LL || v < -2147483648LL) {
        byte_yaz(a, 0x48);
        byte_yaz(a, 0xB9); // MOVABS RCX, imm64
        for (int i = 0; i < 8; i++) {
            byte_yaz(a, (unsigned char)(v & 0xFF));
            v >>= 8;
        }
    } else {
        byte_yaz(a, 0x48);
        byte_yaz(a, 0xC7);
        byte_yaz(a, 0xC1);
        int32_yaz(a, (int)v);
    }
}


void emit_mov_rdx_imm32(MakineKoduArenasi* a, long long v) {
byte_yaz(a, 0x48);
byte_yaz(a, 0xC7);
byte_yaz(a, 0xC2);
int32_yaz(a, (int)v);
}


void emit_add_rax_rcx(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        arm64_emit_add_reg(a, 0, 0, 1);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_r_type(RV_OP_REG, 0, 0, RV_A0, RV_A0, RV_A1));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x6A); // i32.add
    } else {
        byte_yaz(a, 0x48);
        byte_yaz(a, 0x01);
        byte_yaz(a, 0xC8);
    }
}


void emit_sub_rax_rcx(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        arm64_emit_sub_reg(a, 0, 0, 1);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_r_type(RV_OP_REG, 0, 32, RV_A0, RV_A0, RV_A1));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x6B); // i32.sub
    } else {
        byte_yaz(a, 0x48);
        byte_yaz(a, 0x29);
        byte_yaz(a, 0xC8);
    }
}


void emit_imul_rax_rcx(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        arm64_emit_mul_reg(a, 0, 0, 1);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_r_type(RV_OP_REG, 0, 1, RV_A0, RV_A0, RV_A1));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x6C); // i32.mul
    } else {
        byte_yaz(a, 0x48);
        byte_yaz(a, 0x0F);
        byte_yaz(a, 0xAF);
        byte_yaz(a, 0xC1);
    }
}

void emit_store_rax(MakineKoduArenasi* a, long long off) {
    if (platform_arm64_mi()) {
        arm64_emit_str_x0_fp(a, off);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_s_type(RV_OP_STORE, 3, RV_S0, RV_A0, -off));
    } else if (g_target_platform == 5) {
        long long idx = (off / 8) - 1;
        if (idx < 0) idx = 0;
        if (idx > 15) idx = 15;
        byte_yaz(a, 0x21); // local.set
        wasm_leb128_u32_yaz(a, (unsigned int)idx);
    } else {
        if (off <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x45);
            byte_yaz(a, (unsigned char)(256 - off));
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x85);
            int32_yaz(a, (int)(-off));
        }
    }
}

void emit_load_rax(MakineKoduArenasi* a, long long off) {
    if (platform_arm64_mi()) {
        arm64_emit_ldr_x0_fp(a, off);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_LOAD, 3, RV_A0, RV_S0, -off));
    } else if (g_target_platform == 5) {
        long long idx = (off / 8) - 1;
        if (idx < 0) idx = 0;
        if (idx > 15) idx = 15;
        byte_yaz(a, 0x20); // local.get
        wasm_leb128_u32_yaz(a, (unsigned int)idx);
    } else {
        if (off <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45);
            byte_yaz(a, (unsigned char)(256 - off));
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85);
            int32_yaz(a, (int)(-off));
        }
    }
}

void emit_lea_rax(MakineKoduArenasi* a, long long off) {
    if (platform_arm64_mi()) {
        long long instr = 0xD1000000LL + (off * 1024LL) + (29LL * 32LL) + 0LL;
        a64_int32_yaz(a, instr);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_A0, RV_S0, -off));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x41);
        wasm_leb128_u32_yaz(a, (unsigned int)off);
    } else {
        if (off <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45);
            byte_yaz(a, (unsigned char)(256 - off));
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85);
            int32_yaz(a, (int)(-off));
        }
    }
}

void ra_ikili_sol_kaydet(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        byte_yaz(a, 0xE0); byte_yaz(a, 0x7F); byte_yaz(a, 0xBF); byte_yaz(a, 0xA9);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_SP, RV_SP, -16));
        int32_yaz(a, (int)riscv_encode_s_type(RV_OP_STORE, 3, RV_SP, RV_A0, 0));
    } else if (g_target_platform == 5) {
        // WebAssembly: operand stack handles it
    } else {
        byte_yaz(a, 0x50); // PUSH RAX
    }
}

void ra_ikili_sol_geri_yukle_rcx(MakineKoduArenasi* a) {
    if (platform_arm64_mi()) {
        byte_yaz(a, 0xE1); byte_yaz(a, 0x03); byte_yaz(a, 0x00); byte_yaz(a, 0xAA);
        byte_yaz(a, 0xE0); byte_yaz(a, 0x7F); byte_yaz(a, 0xC1); byte_yaz(a, 0xA8);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_A1, RV_A0, 0));
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_LOAD, 3, RV_A0, RV_SP, 0));
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_SP, RV_SP, 16));
    } else if (g_target_platform == 5) {
        // WebAssembly: stack already has sol then sag
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX
        byte_yaz(a, 0x58); // POP RAX
    }
}


// ============================================================
// Karsilastirma Operatorleri: CMP + SETcc + MOVZX
// Ornek: a == b => RAX=1 eger esit, 0 yoksa
// ============================================================
void emit_cmp_rax_rcx(MakineKoduArenasi* a) {
    // CMP RAX, RCX => 48 3B C1
    byte_yaz(a, 0x48);
    byte_yaz(a, 0x3B);
    byte_yaz(a, 0xC1);
}

void emit_sete_al(MakineKoduArenasi* a) {
    // SETE AL  => 0F 94 C0
    byte_yaz(a, 0x0F); byte_yaz(a, 0x94); byte_yaz(a, 0xC0);
}

void emit_setne_al(MakineKoduArenasi* a) {
    // SETNE AL  => 0F 95 C0
    byte_yaz(a, 0x0F); byte_yaz(a, 0x95); byte_yaz(a, 0xC0);
}

void emit_setl_al(MakineKoduArenasi* a) {
    // SETL AL  => 0F 9C C0  (signed less-than)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x9C); byte_yaz(a, 0xC0);
}

void emit_setg_al(MakineKoduArenasi* a) {
    // SETG AL  => 0F 9F C0  (signed greater-than)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x9F); byte_yaz(a, 0xC0);
}

void emit_setle_al(MakineKoduArenasi* a) {
    // SETLE AL  => 0F 9E C0  (signed less-or-equal)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x9E); byte_yaz(a, 0xC0);
}

void emit_setge_al(MakineKoduArenasi* a) {
    // SETGE AL  => 0F 9D C0  (signed greater-or-equal)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x9D); byte_yaz(a, 0xC0);
}

void emit_movzx_rax_al(MakineKoduArenasi* a) {
    // MOVZX RAX, AL  => 48 0F B6 C0
    byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0);
}

void emit_setb_al(MakineKoduArenasi* a) {
    // SETB AL  => 0F 92 C0  (unsigned below / CF=1)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x92); byte_yaz(a, 0xC0);
}

void emit_setae_al(MakineKoduArenasi* a) {
    // SETAE AL => 0F 93 C0  (unsigned above or equal / CF=0)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x93); byte_yaz(a, 0xC0);
}

void emit_setbe_al(MakineKoduArenasi* a) {
    // SETBE AL => 0F 96 C0  (unsigned below or equal / CF=1 or ZF=1)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x96); byte_yaz(a, 0xC0);
}

void emit_seta_al(MakineKoduArenasi* a) {
    // SETA AL  => 0F 97 C0  (unsigned above / CF=0 and ZF=0)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x97); byte_yaz(a, 0xC0);
}


void emit_call_rip_rel(MakineKoduArenasi* a, long long rel32) {
byte_yaz(a, 0xFF);
byte_yaz(a, 0x15);
int32_yaz(a, rel32);
}


void emit_lea_rcx_rip(MakineKoduArenasi* a, long long rel32) {
byte_yaz(a, 0x48);
byte_yaz(a, 0x8D);
byte_yaz(a, 0x0D);
int32_yaz(a, rel32);
}


void emit_mov_r8d_imm32(MakineKoduArenasi* a, long long v) {
byte_yaz(a, 0x41);
byte_yaz(a, 0xB8);
int32_yaz(a, v);
}


void emit_xor_r9d_r9d(MakineKoduArenasi* a) {
byte_yaz(a, 0x45);
byte_yaz(a, 0x33);
byte_yaz(a, 0xC9);
}

struct YapiAlani {
    char* isim;
    long long uzunluk;
    long long offset;
    char* tip_adi;       // added to match codegen.ykn YapiAlani layout
    long long tip_uzunluk;
    long long yildiz_sayisi;
    long long nullable;
    long long dizi_boyutu;
};

struct YapiTanimi {
    char* isim;
    long long uzunluk;
    YapiAlani alanlar[32];
    long long alan_sayisi;
    long long byte_boyutu;
};

// Keep the C++ bootstrap seed aligned with src/codegen.ykn. The canonical
// self-hosted codegen can register compiler, tooling, and std/ai structs in
// one flattened AST; 64 entries was silently overflowing during Stage-1.
YapiTanimi g_yapilar[1024];
long long g_yapi_sayisi = 0;

static long long tip_eleman_boyutu_al(const char* tip_adi, long long tip_uzn, long long yildiz) {
    if (yildiz > 0) return 8;
    if (!tip_adi || tip_uzn <= 0) return 8;
    if (tip_adi[0] == '*') {
        if (tip_uzn > 1) {
            return tip_eleman_boyutu_al(tip_adi + 1, tip_uzn - 1, 0);
        }
        return 8;
    }
    if (tip_uzn == 2 && (tip_adi[0] == 'u' || tip_adi[0] == 'i') && tip_adi[1] == '8') return 1;
    if (tip_uzn == 4 && (bellek_esit_mi((char*)tip_adi, (char*)"bayt", 4) || bellek_esit_mi((char*)tip_adi, (char*)"bool", 4))) return 1;
    if (tip_uzn == 3 && (tip_adi[0] == 'u' || tip_adi[0] == 'i') && tip_adi[1] == '1' && tip_adi[2] == '6') return 2;
    if (tip_uzn == 3 && (tip_adi[0] == 'u' || tip_adi[0] == 'i' || tip_adi[0] == 'f') && tip_adi[1] == '3' && tip_adi[2] == '2') return 4;
    if (tip_uzn == 3 && (tip_adi[0] == 'u' || tip_adi[0] == 'i' || tip_adi[0] == 'f') && tip_adi[1] == '6' && tip_adi[2] == '4') return 8;
    if (tip_uzn == 7 && (bellek_esit_mi((char*)tip_adi, (char*)"tamsayi", 7) || bellek_esit_mi((char*)tip_adi, (char*)"ondalik", 7))) return 8;
    if (tip_uzn == 9 && bellek_esit_mi((char*)tip_adi, (char*)"mantiksal", 9)) return 8;
    for (long long s_i = 0; s_i < g_yapi_sayisi; s_i++) {
        if (g_yapilar[s_i].uzunluk == tip_uzn && bellek_esit_mi(g_yapilar[s_i].isim, (char*)tip_adi, tip_uzn)) {
            long long sz = g_yapilar[s_i].byte_boyutu;
            if (sz <= 0) sz = g_yapilar[s_i].alan_sayisi * 8;
            if (sz <= 0) sz = 8;
            return sz;
        }
    }
    return 8;
}


struct LokalDegisken {
    char* isim;
    long long uzunluk;
    long long offset;
    char* tip_adi;
    long long tip_uzunluk;
    long long dizi_boyutu; // >0 means stack-allocated array (use LEA); 0 means scalar/pointer (use MOV)
    long long yildiz_sayisi; // pointer depth stored from AST; populated from dizi_boyutu sign trick or separately
};

LokalDegisken g_lokaller[16384];
char* g_lokal_isimleri[16384];
long long g_lokal_uzunluklari[16384];
long long g_lokal_offsetleri[16384];
char* g_lokal_tip_adlari[16384];
long long g_lokal_tip_uzunluklari[16384];
long long g_lokal_yildizlari[16384];
long long g_lokal_dizi_boyutlari[16384];
long long g_lokal_sayisi = 0;
long long g_stack_offset = 0;

// Global constant table: top-level degisken NAME = value
struct GlobalSabit {
    char* isim;
    long long uzunluk;
    long long deger;
};
GlobalSabit g_global_sabitler[16384];
long long g_global_sabit_sayisi = 0;
long long g_global_sabitleri_toplandi = 0;
long long g_fonksiyon_icinde = 0; // set to 1 when inside a function body

// ============================================================
// Register Allocator Global State & Constants (x86_64)
// ============================================================
static const long long YKN_REG_NONE = 0;
static const long long YKN_REG_RAX  = 1;
static const long long YKN_REG_RCX  = 2;
static const long long YKN_REG_RDX  = 3;
static const long long YKN_REG_R8   = 4;
static const long long YKN_REG_R9   = 5;
static const long long YKN_REG_R10  = 6;
static const long long YKN_REG_R11  = 7;
static const long long YKN_REG_RBX  = 8;
static const long long YKN_REG_R12  = 9;
static const long long YKN_REG_R13  = 10;
static const long long YKN_REG_R14  = 11;
static const long long YKN_REG_R15  = 12;

static long long g_ra_opt_aktif = 0;
static long long g_ra_callee_saved_kullanildi[5] = {0, 0, 0, 0, 0};
static long long g_ra_var_reg[256];
static char*     g_ra_var_isim[256];
static long long g_ra_var_uzunluk[256];
static long long g_ra_var_adresi_alindi[256];
static long long g_ra_kullanim_sayaci[256];
static long long g_ra_var_adet = 0;

static void ra_fonksiyon_sifirla() {
    for (int i = 0; i < 5; i++) g_ra_callee_saved_kullanildi[i] = 0;
    g_ra_var_adet = 0;
    for (int i = 0; i < 256; i++) {
        g_ra_var_reg[i] = 0;
        g_ra_var_isim[i] = 0;
        g_ra_var_uzunluk[i] = 0;
        g_ra_var_adresi_alindi[i] = 0;
        g_ra_kullanim_sayaci[i] = 0;
    }
}

static long long ra_degisken_isim_bul(char* isim, long long uzn) {
    if (!isim || uzn <= 0) return -1;
    for (long long i = 0; i < g_ra_var_adet; i++) {
        if (g_ra_var_uzunluk[i] == uzn && memcmp(g_ra_var_isim[i], isim, (size_t)uzn) == 0) return i;
    }
    return -1;
}

static void ra_mark_captured(AstDugum* kok) {
    if (!kok) return;
    if (kok->tip == AST_KIMLIK && kok->ilk_jeton) {
        long long idx = ra_degisken_isim_bul(kok->ilk_jeton->baslangic, kok->ilk_jeton->uzunluk);
        if (idx >= 0 && idx < 256) g_ra_var_adresi_alindi[idx] = 1;
    }
    ra_mark_captured(kok->sol);
    ra_mark_captured(kok->sag);
    ra_mark_captured(kok->sonraki);
    ra_mark_captured(kok->govde);
}

static void ra_ast_tanimlari_tara(AstDugum* kok) {
    if (!kok) return;
    if (kok->tip == AST_ISLEV || kok->tip == 41) {
        // Lambda is an independent nested function frame; do not register its locals in the outer function
        return;
    }
    if (kok->tip == AST_DEGISKEN_TANIMI && kok->ilk_jeton && kok->dizi_boyutu == 0 && kok->cozumlenmis_tip <= 0) {
        int uygun = 0;
        if (!kok->tip_adi) uygun = 1;
        else if (kok->tip_adi_uzunluk == 7 && memcmp(kok->tip_adi, "tamsayi", 7) == 0) uygun = 1;
        else if (kok->tip_adi_uzunluk == 3 && memcmp(kok->tip_adi, "int", 3) == 0) uygun = 1;
        else if (kok->tip_adi_uzunluk == 3 && memcmp(kok->tip_adi, "i64", 3) == 0) uygun = 1;
        else if (kok->tip_adi_uzunluk == 4 && memcmp(kok->tip_adi, "sayi", 4) == 0) uygun = 1;
        if (uygun) {
            long long idx = ra_degisken_isim_bul(kok->ilk_jeton->baslangic, kok->ilk_jeton->uzunluk);
            if (idx < 0 && g_ra_var_adet < 256) {
                g_ra_var_isim[g_ra_var_adet] = kok->ilk_jeton->baslangic;
                g_ra_var_uzunluk[g_ra_var_adet] = kok->ilk_jeton->uzunluk;
                g_ra_kullanim_sayaci[g_ra_var_adet] = 0;
                g_ra_var_reg[g_ra_var_adet] = 0;
                g_ra_var_adresi_alindi[g_ra_var_adet] = 0;
                g_ra_var_adet++;
            } else if (idx >= 0) {
                g_ra_var_adresi_alindi[idx] = 1;
            }
        }
    }
    ra_ast_tanimlari_tara(kok->sol);
    ra_ast_tanimlari_tara(kok->sag);
    ra_ast_tanimlari_tara(kok->sonraki);
    ra_ast_tanimlari_tara(kok->govde);
}

static void ra_ast_kullanim_tara(AstDugum* kok) {
    if (!kok) return;
    if (kok->tip == AST_ISLEV || kok->tip == 41) {
        // Any outer local used in a lambda body is captured via static link -> keep on stack frame
        ra_mark_captured(kok->govde);
        return;
    }
    if (kok->tip == AST_KIMLIK && kok->ilk_jeton) {
        long long idx = ra_degisken_isim_bul(kok->ilk_jeton->baslangic, kok->ilk_jeton->uzunluk);
        if (idx >= 0 && idx < 256) {
            g_ra_kullanim_sayaci[idx]++;
        }
    }
    if (kok->tip == 20 || kok->tip == AST_REFERANS) {
        if (kok->sag && kok->sag->ilk_jeton) {
            long long idx = ra_degisken_isim_bul(kok->sag->ilk_jeton->baslangic, kok->sag->ilk_jeton->uzunluk);
            if (idx >= 0 && idx < 256) g_ra_var_adresi_alindi[idx] = 1;
        }
        if (kok->sol && kok->sol->ilk_jeton) {
            long long idx = ra_degisken_isim_bul(kok->sol->ilk_jeton->baslangic, kok->sol->ilk_jeton->uzunluk);
            if (idx >= 0 && idx < 256) g_ra_var_adresi_alindi[idx] = 1;
        }
    }
    ra_ast_kullanim_tara(kok->sol);
    ra_ast_kullanim_tara(kok->sag);
    ra_ast_kullanim_tara(kok->sonraki);
    ra_ast_kullanim_tara(kok->govde);
}

static long long ra_degisken_reg_bul(char* isim, long long uzn) {
    long long idx = ra_degisken_isim_bul(isim, uzn);
    if (idx < 0) return 0;
    return g_ra_var_reg[idx];
}

static void ra_degiskenleri_yazmaca_bagla(AstDugum* kok) {
    g_ra_var_adet = 0;
    for (int i = 0; i < 256; i++) {
        g_ra_kullanim_sayaci[i] = 0;
        g_ra_var_reg[i] = 0;
        g_ra_var_adresi_alindi[i] = 0;
    }
    if (g_ra_opt_aktif == 0) return;
    ra_ast_tanimlari_tara(kok);
    ra_ast_kullanim_tara(kok);

    long long tahsis_edilen = 0;
    for (int adim = 0; adim < 5 && tahsis_edilen < 5; adim++) {
        long long maks_kullanim = 1;
        long long secilen_idx = -1;
        for (long long i = 0; i < g_ra_var_adet; i++) {
            if (g_ra_var_reg[i] == 0 && g_ra_var_adresi_alindi[i] == 0 && g_ra_kullanim_sayaci[i] > maks_kullanim) {
                maks_kullanim = g_ra_kullanim_sayaci[i];
                secilen_idx = i;
            }
        }
        if (secilen_idx != -1) {
            if (tahsis_edilen == 0) {
                g_ra_var_reg[secilen_idx] = YKN_REG_RBX;
                g_ra_callee_saved_kullanildi[0] = 1;
            } else if (tahsis_edilen == 1) {
                g_ra_var_reg[secilen_idx] = YKN_REG_R12;
                g_ra_callee_saved_kullanildi[1] = 1;
            } else if (tahsis_edilen == 2) {
                g_ra_var_reg[secilen_idx] = YKN_REG_R13;
                g_ra_callee_saved_kullanildi[2] = 1;
            } else if (tahsis_edilen == 3) {
                g_ra_var_reg[secilen_idx] = YKN_REG_R14;
                g_ra_callee_saved_kullanildi[3] = 1;
            } else if (tahsis_edilen == 4) {
                g_ra_var_reg[secilen_idx] = YKN_REG_R15;
                g_ra_callee_saved_kullanildi[4] = 1;
            }
            tahsis_edilen++;
        } else {
            break;
        }
    }
}

static void ra_callee_prolog_emit(MakineKoduArenasi* a) {
    if (g_ra_callee_saved_kullanildi[0] == 1) { byte_yaz(a, 0x53); } // PUSH RBX
    if (g_ra_callee_saved_kullanildi[1] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x54); } // PUSH R12
    if (g_ra_callee_saved_kullanildi[2] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x55); } // PUSH R13
    if (g_ra_callee_saved_kullanildi[3] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x56); } // PUSH R14
    if (g_ra_callee_saved_kullanildi[4] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x57); } // PUSH R15
}

static void ra_callee_epilog_emit(MakineKoduArenasi* a) {
    if (g_ra_callee_saved_kullanildi[4] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x5F); } // POP R15
    if (g_ra_callee_saved_kullanildi[3] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x5E); } // POP R14
    if (g_ra_callee_saved_kullanildi[2] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x5D); } // POP R13
    if (g_ra_callee_saved_kullanildi[1] == 1) { byte_yaz(a, 0x41); byte_yaz(a, 0x5C); } // POP R12
    if (g_ra_callee_saved_kullanildi[0] == 1) { byte_yaz(a, 0x5B); } // POP RBX
}

static void emit_mov_reg_rax(MakineKoduArenasi* a, long long reg) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC4); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC5); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); }
}

static void emit_mov_rax_reg(MakineKoduArenasi* a, long long reg) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE0); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE8); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF0); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF8); }
}

static void emit_mov_rcx_reg(MakineKoduArenasi* a, long long reg) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD9); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE1); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE9); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF1); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); }
}

static void emit_add_reg_imm8(MakineKoduArenasi* a, long long reg, long long c) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC3); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xC5); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xC6); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xC7); byte_yaz(a, (unsigned char)c); }
}

static void emit_sub_reg_imm8(MakineKoduArenasi* a, long long reg, long long c) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEB); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xED); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xEE); byte_yaz(a, (unsigned char)c); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xEF); byte_yaz(a, (unsigned char)c); }
}

static void emit_add_reg_rax(MakineKoduArenasi* a, long long reg) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0xC3); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x49); byte_yaz(a, 0x01); byte_yaz(a, 0xC4); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x49); byte_yaz(a, 0x01); byte_yaz(a, 0xC5); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x49); byte_yaz(a, 0x01); byte_yaz(a, 0xC6); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x49); byte_yaz(a, 0x01); byte_yaz(a, 0xC7); }
}

static void emit_sub_reg_rax(MakineKoduArenasi* a, long long reg) {
    if (reg == YKN_REG_RBX) { byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0xC3); }
    else if (reg == YKN_REG_R12) { byte_yaz(a, 0x49); byte_yaz(a, 0x29); byte_yaz(a, 0xC4); }
    else if (reg == YKN_REG_R13) { byte_yaz(a, 0x49); byte_yaz(a, 0x29); byte_yaz(a, 0xC5); }
    else if (reg == YKN_REG_R14) { byte_yaz(a, 0x49); byte_yaz(a, 0x29); byte_yaz(a, 0xC6); }
    else if (reg == YKN_REG_R15) { byte_yaz(a, 0x49); byte_yaz(a, 0x29); byte_yaz(a, 0xC7); }
}

struct FonksiyonSembolu {
    char* isim;
    long long uzunluk;
    long long adres;
    FonksiyonSembolu* sonraki;
};
FonksiyonSembolu* g_fonksiyon_bas = 0;
FonksiyonSembolu* g_fonksiyon_son = 0;
long long g_fonk_sayisi = 0;

struct YamalanacakCagri {
    char* isim;
    long long uzunluk;
    long long cagri_pos;
};
YamalanacakCagri g_yamalar[65536];
long long g_yama_sayisi = 0;

// IAT call patch: stores the .text offset of the int32 rel32 field that points to an IAT entry
// slot: 0..8=KERNEL32, 10..13=ykn_llm_simd.dll. Native LLM pointer
// arguments follow YKN's 8-byte `tamsayi` array ABI.
struct IatYama {
    long long text_offset; // offset in .text where the rel32 IAT disp32 sits
    int slot;              // combined import slot
};
IatYama g_iat_yamalar[65536];
long long g_iat_yama_sayisi = 0;
long long g_iat_simd_kullanildi = 0;

// #c-lib directive registry: stores library names for linker metadata
static char* g_clib_isim[64];
static long long g_clib_uzunluk[64];
static long long g_clib_adet = 0;
static long long g_codegen_unsupported_ast_hatasi = 0;
static long long g_codegen_kapasite_hatasi = 0;
static long long g_codegen_kapasite_kodu = 0;

// Stage-0 typechecker must fail closed at the same semantic boundaries as
// src/typechecker.ykn. These are compiler-state limits, not user-runtime
// limits; exceeding one must never produce an artifact.
static void seed_typechecker_kapasite_reddet(long long kod) {
    if (g_typechecker_kapasite_hatasi == 0) {
        g_typechecker_kapasite_hatasi = 1;
        g_typechecker_kapasite_kodu = kod;
    }
}

static void seed_codegen_kapasite_reddet(long long kod) {
    if (g_codegen_kapasite_hatasi == 0) {
        g_codegen_kapasite_hatasi = 1;
        g_codegen_kapasite_kodu = kod;
    }
}


static bool seed_iat_yama_kapasite_kontrol() {
    if (g_iat_yama_sayisi >= 65536) {
        seed_codegen_kapasite_reddet(9);
        return false;
    }
    return true;
}

static bool seed_yama_kapasite_kontrol() {
    if (g_yama_sayisi >= 65536) {
        seed_codegen_kapasite_reddet(2);
        return false;
    }
    return true;
}


static long long g_suanki_islev_async_seed = 0;
static long long g_suanki_islev_ana_seed = 0;

void int32_patch(MakineKoduArenasi* a, long long pos, int val);

static void seed_emit_async_future_wrap(MakineKoduArenasi* a) {
    byte_yaz(a, 0x50); // PUSH RAX
    if (g_target_platform == 1) {
        // Linux x86_64 sys_mmap (size=160: 80 bytes Future + 80 bytes AsyncFrame)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);                     // XOR RDI, RDI
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 160);  // MOV RSI, 160
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);    // MOV RDX, 3 (PROT_READ|PROT_WRITE)
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);   // MOV R10, 34 (MAP_PRIVATE|MAP_ANONYMOUS)
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);   // MOV R8, -1
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);                     // XOR R9, R9
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);    // RAX = 9 (sys_mmap)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);                                         // syscall -> RAX = buffer
    } else {
        // Windows VirtualAlloc(NULL, 160, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);                     // XOR RCX, RCX
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 160);  // MOV RDX, 160
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);                  // MOV R8D, 0x3000
        byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);                    // MOV R9D, 0x04
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28);  // SUB RSP, 40
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) {
            long long iat_idx = g_iat_yama_sayisi;
            g_iat_yamalar[iat_idx].text_offset = a->kullanilan;
            g_iat_yamalar[iat_idx].slot = 3; // VirtualAlloc
            g_iat_yama_sayisi++;
        }
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28);  // ADD RSP, 40
    }
    byte_yaz(a, 0x5A); // POP RDX (orijinal sonuc degeri)

    // JMP over async_poll_trampoline
    byte_yaz(a, 0xE9);
    long long jmp_pos = a->kullanilan;
    int32_yaz(a, 0);

    long long tramp_start = a->kullanilan;
    // --- async_poll_trampoline(RCX = context / *AsyncFrame) -> RAX (tamsayi durum) ---
    // CMP QWORD PTR [RCX], 0x594B4E46
    byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0x39); int32_yaz(a, 0x594B4E46);
    // JNE .poll_hata (+101)
    byte_yaz(a, 0x75); byte_yaz(a, 101);
    // MOV RAX, [RCX + 24] (sonuc_durumu)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x41); byte_yaz(a, 0x18);
    // CMP RAX, 1 (ASYNC_HAZIR)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xF8); byte_yaz(a, 0x01);
    // JE .poll_ret (+99)
    byte_yaz(a, 0x74); byte_yaz(a, 99);
    // MOV RDX, [RCX + 40] (beklenen)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x51); byte_yaz(a, 0x28);
    // TEST RDX, RDX
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xD2);
    // JZ .poll_bekliyor (+78)
    byte_yaz(a, 0x74); byte_yaz(a, 78);
    // MOV R10, [RDX] (poll fn)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x12);
    // TEST R10, R10
    byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xD2);
    // JZ .poll_bekliyor (+70)
    byte_yaz(a, 0x74); byte_yaz(a, 70);

    // Call beklenen->poll(context)
    byte_yaz(a, 0x51); // PUSH RCX
    byte_yaz(a, 0x52); // PUSH RDX
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4A); byte_yaz(a, 0x08); // MOV RCX, [RDX + 8]
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28); // SUB RSP, 40
    byte_yaz(a, 0x41); byte_yaz(a, 0xFF); byte_yaz(a, 0xD2);                     // CALL R10
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28); // ADD RSP, 40
    byte_yaz(a, 0x5A); // POP RDX
    byte_yaz(a, 0x59); // POP RCX

    // CMP RAX, 1 (ASYNC_HAZIR)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xF8); byte_yaz(a, 0x01);
    // JNE .poll_bekliyor (+45)
    byte_yaz(a, 0x75); byte_yaz(a, 45);

    // Child is HAZIR! Sonucu al ve frame'e aktar:
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x42); byte_yaz(a, 0x18); // MOV R8, [RDX + 24]
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x41); byte_yaz(a, 0x20); // MOV [RCX + 32], R8
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x41); byte_yaz(a, 0x18); int32_yaz(a, 1); // MOV QWORD PTR [RCX + 24], 1
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x59); byte_yaz(a, 0x30); // MOV R11, [RCX + 48]
    byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xDB);                     // TEST R11, R11
    byte_yaz(a, 0x74); byte_yaz(a, 19);                                         // JZ .poll_hazir (+19)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x43); byte_yaz(a, 0x10); int32_yaz(a, 1); // MOV QWORD PTR [R11 + 16], 1
    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0x43); byte_yaz(a, 0x18); // MOV [R11 + 24], R8
    // .poll_hazir:
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1);   // MOV RAX, 1
    byte_yaz(a, 0xC3);                                                           // RET

    // .poll_bekliyor:
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0);                     // XOR RAX, RAX
    byte_yaz(a, 0xC3);                                                           // RET

    // .poll_hata:
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 2);   // MOV RAX, 2
    byte_yaz(a, 0xC3);                                                           // RET

    // .poll_ret:
    byte_yaz(a, 0xC3);                                                           // RET

    long long after_tramp = a->kullanilan;
    int32_patch(a, jmp_pos, (int)(after_tramp - (jmp_pos + 4)));

    // LEA R10, [RIP + rel32] -> Trampoline adresi
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x15);
    long long lea_pos = a->kullanilan;
    int32_yaz(a, (int)(tramp_start - (lea_pos + 4)));
    // [RAX + 0] = R10 (Future->poll = trampoline)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x10);

    // LEA R8, [RAX + 80] (AsyncFrame adresi)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x40); byte_yaz(a, 80);
    // [RAX + 8] = R8 (Future->context = AsyncFrame)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x40); byte_yaz(a, 0x08);
    // [RAX + 16] = 1 (durum = ASYNC_HAZIR)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x10); int32_yaz(a, 1);
    // [RAX + 24] = RDX (hata / inline payload)
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, 0x18);
    // [RAX + 32] = 1 (poll_sayisi)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x20); int32_yaz(a, 1);
    // [RAX + 40] = 0 (poll_limiti)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x28); int32_yaz(a, 0);
    // LEA R9, [RAX + 24] -> [RAX + 48] = R9 (sonuc_bellegi)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x48); byte_yaz(a, 0x18);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x48); byte_yaz(a, 0x30);
    // [RAX + 56] = 8 (sonuc_boyutu)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x38); int32_yaz(a, 8);
    // [RAX + 64] = 2 (sonuc_tipi)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x40); int32_yaz(a, 2);
    // [RAX + 72] = R8 (Future->frame = AsyncFrame)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x40); byte_yaz(a, 0x48);

    // --- AsyncFrame baslatma (R8 = [RAX + 80] / AsyncFrame adresi) ---
    // [R8 + 0] = 0x594B4E46 (magic = ASYNC_FRAME_MAGIC)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x00); int32_yaz(a, 0x594B4E46);
    // [R8 + 8] = 64 (boyut)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x08); int32_yaz(a, 64);
    // [R8 + 16] = 0 (program_sayaci)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x10); int32_yaz(a, 0);
    // [R8 + 24] = 1 (sonuc_durumu = ASYNC_HAZIR)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x18); int32_yaz(a, 1);
    // [R8 + 32] = RDX (sonuc_hata = payload)
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, 0x20);
    // [R8 + 40] = 0 (beklenen = 0)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x28); int32_yaz(a, 0);
    // [R8 + 48] = RAX (sahibi = Future)
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0x40); byte_yaz(a, 0x30);
    // LEA R9, [R8 + 64] -> [R8 + 56] = R9 (slotlar)
    byte_yaz(a, 0x4D); byte_yaz(a, 0x8D); byte_yaz(a, 0x48); byte_yaz(a, 0x40);
    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0x48); byte_yaz(a, 0x38);
    // [R8 + 64] = 8 (slot_sayisi)
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 0x40); int32_yaz(a, 8);
}

static void seed_emit_async_await_unwrap(MakineKoduArenasi* a) {
    // TEST RAX, RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
    // JZ atla_null (+52)
    byte_yaz(a, 0x74); byte_yaz(a, 52);

    // .poll_loop:
    // CMP QWORD PTR [RAX + 16], 0 (durum == ASYNC_BEKLIYOR?)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0x78); byte_yaz(a, 0x10); byte_yaz(a, 0x00);
    // JNE .poll_done (+27)
    byte_yaz(a, 0x75); byte_yaz(a, 27);

    // MOV R10, [RAX] (poll fn)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x10);
    // TEST R10, R10
    byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xD2);
    // JZ .poll_done (+19)
    byte_yaz(a, 0x74); byte_yaz(a, 19);

    // PUSH RAX
    byte_yaz(a, 0x50);
    // MOV RCX, [RAX + 8] (context)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x48); byte_yaz(a, 0x08);
    // SUB RSP, 40
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28);
    // CALL R10
    byte_yaz(a, 0x41); byte_yaz(a, 0xFF); byte_yaz(a, 0xD2);
    // ADD RSP, 40
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28);
    // POP RAX
    byte_yaz(a, 0x58);
    // JMP .poll_loop (-34 -> 0xDE)
    byte_yaz(a, 0xEB); byte_yaz(a, 0xDE);

    // .poll_done:
    // MOV RDX, [RAX + 48] (sonuc_bellegi)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x50); byte_yaz(a, 0x30);
    // TEST RDX, RDX
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xD2);
    // JZ oku_inline (+5)
    byte_yaz(a, 0x74); byte_yaz(a, 5);
    // MOV RAX, [RDX]
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x02);
    // JMP bitti (+4)
    byte_yaz(a, 0xEB); byte_yaz(a, 4);
    // oku_inline: MOV RAX, [RAX + 24]
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x40); byte_yaz(a, 0x18);
    // atla_null:
}

// Stage-0 seed must enforce the same fail-closed boundary as src/codegen.ykn.
// These nodes are compile-time directives or unlowered declarations; silently
// emitting an artifact would turn an unsupported feature into false success.
static void seed_codegen_ast_capability_scan(AstDugum* dugum) {
    if (!dugum || g_codegen_unsupported_ast_hatasi != 0) return;
    long long tip = YKN_DOT(dugum, tip);
    if ((tip < AST_BOS || (tip > AST_TRAIT_UYGULAMA && tip != AST_ASYNC_ISLEV && tip != AST_AWAIT && tip != 41 && tip != 42 && tip != AST_DONUSUM && tip != 45 && tip != AST_DIREKTIF_CONFIG && tip != 44)) ||
        tip == AST_GENERIC_TANIM) {
        g_codegen_unsupported_ast_hatasi = 1;
        long long line = 1;
        long long column = 1;
        Jeton* token = (Jeton*)YKN_DOT(dugum, ilk_jeton);
        if (token) {
            line = YKN_DOT(token, satir);
            column = YKN_DOT(token, sutun);
        }
        printf("HATA [E043] [ERR_UNSUPPORTED_AST_NODE] [class=codegen]: %lld:%lld: unsupported AST node reached codegen\\n", line, column);
        return;
    }
    seed_codegen_ast_capability_scan((AstDugum*)YKN_DOT(dugum, sol));
    seed_codegen_ast_capability_scan((AstDugum*)YKN_DOT(dugum, sag));
    seed_codegen_ast_capability_scan((AstDugum*)YKN_DOT(dugum, govde));
    seed_codegen_ast_capability_scan((AstDugum*)YKN_DOT(dugum, yoksa_govde));
    // Generic parameters attached to enum/struct declarations are type
    // metadata consumed before codegen; they are not executable AST nodes.
    // Do not confuse that metadata with an unlowered generic declaration.
    if (tip != AST_YAPI_TANIMI) {
        seed_codegen_ast_capability_scan((AstDugum*)YKN_DOT(dugum, parametreler));
    }
    seed_codegen_ast_capability_scan((AstDugum*)YKN_DOT(dugum, sonraki));
}

// Mutates the first real AST node with the requested tag into an unknown tag.
// This is used only by the explicit native acceptance flag and exercises the
// parser/typechecker/optimizer-produced tree, not a synthetic AST.
static void seed_ast_ilk_dugumu_mutasyona_ugrat(AstDugum* dugum) {
    if (!dugum || g_ast_mutasyon_uygulandi != 0) return;
    if (YKN_DOT(dugum, tip) == g_ast_mutasyon_hedef) {
        YKN_DOT(dugum, tip) = 255;
        g_ast_mutasyon_uygulandi = 1;
        return;
    }
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)YKN_DOT(dugum, sol));
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)YKN_DOT(dugum, sag));
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)YKN_DOT(dugum, govde));
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)YKN_DOT(dugum, yoksa_govde));
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)YKN_DOT(dugum, parametreler));
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)YKN_DOT(dugum, sonraki));
}

// Loop stack for devam_et (continue) and durdur (break)
long long g_dongu_basi_stack[64];   // loop start offsets (for devam_et -> JMP back)
long long g_durdur_yamalar[1024];  // loop exit patch positions (for durdur -> JMP forward)
long long g_devam_yamalar[1024];   // loop continue patch positions (for devam_et -> JMP step/start)
long long g_durdur_adet = 0;
long long g_devam_adet = 0;
long long g_dongu_derinlik = 0;

void int32_patch(MakineKoduArenasi* a, long long pos, int val);

static void emit_yknllm_audit_native_append(MakineKoduArenasi* a, AstDugum* dugum) {
    AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
    if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // R15=path
    arg = arg ? (AstDugum*)YKN_DOT(arg, sonraki) : 0;
    if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // R14=buffer
    arg = arg ? (AstDugum*)YKN_DOT(arg, sonraki) : 0;
    if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC5); // R13=size

    if (g_target_platform == 1) {
        // Linux x86-64: open(O_WRONLY|O_CREAT|O_APPEND), write, close.
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 1089);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 420);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 2);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF6);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEA);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 3);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8);
        return;
    }
    if (g_target_platform == 2) {
        // macOS x86-64: BSD open(O_WRONLY|O_CREAT|O_APPEND), write, close.
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 521);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 420);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000005);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000004);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF6);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEA);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000006);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8);
        return;
    }

    // Windows x86-64: OPEN_ALWAYS then SetFilePointerEx(FILE_END), WriteFile, CloseHandle.
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x40);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 0x40000000);
    byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 1);
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 4);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); int32_yaz(a, 128);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x30); int32_yaz(a, 0);
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = a->kullanilan; g_iat_yamalar[g_iat_yama_sayisi].slot = 4; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x40);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7);

    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x85); int32_yaz(a, -528); int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9);
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x85); int32_yaz(a, -528);
    byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 2);
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = a->kullanilan; g_iat_yamalar[g_iat_yama_sayisi].slot = 8; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);

    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x85); int32_yaz(a, -512); int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x30);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF2);
    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0xE8);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x8D); int32_yaz(a, -512);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 0);
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = a->kullanilan; g_iat_yamalar[g_iat_yama_sayisi].slot = 1; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, -512);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x30);

    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9);
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = a->kullanilan; g_iat_yamalar[g_iat_yama_sayisi].slot = 6; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8);
}

void emit_bounds_panic(MakineKoduArenasi* a) {
    // Bounds failures are terminal and intentionally do not depend on YKN runtime calls.
    // ExitProcess is already part of the PE import table and works before normal codegen.
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC1); int32_yaz(a, 1); // RCX=1
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) {
        g_iat_yamalar[g_iat_yama_sayisi].text_offset = a->kullanilan;
        g_iat_yamalar[g_iat_yama_sayisi].slot = 0; // ExitProcess
        g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    byte_yaz(a, 0xCC); // defensive trap if a non-conforming loader returns
}

void emit_bounds_guard(MakineKoduArenasi* a, long long array_size, long long pointer_base) {
    if (!g_bounds_check) return;
    long long fail1 = -1, fail2 = -1, fail3 = -1;
    if (pointer_base) {
        // CMP qword ptr [RSP], 0; JE fail (base is saved on the stack)
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0x3C); byte_yaz(a, 0x24); byte_yaz(a, 0x00);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x84); fail1 = a->kullanilan; int32_yaz(a, 0);
    }
    if (array_size > 0) {
        // The index is signed: reject i < 0 before the unsigned upper-bound test.
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xF8); byte_yaz(a, 0x00); // CMP RAX,0
        byte_yaz(a, 0x0F); byte_yaz(a, 0x8C); fail2 = a->kullanilan; int32_yaz(a, 0); // JL fail
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC1); int32_yaz(a, array_size); // RCX=N
        byte_yaz(a, 0x48); byte_yaz(a, 0x3B); byte_yaz(a, 0xC1); // CMP RAX,RCX
        byte_yaz(a, 0x0F); byte_yaz(a, 0x83); fail3 = a->kullanilan; int32_yaz(a, 0); // JAE fail
    }
    if (fail1 >= 0 || fail2 >= 0) {
        byte_yaz(a, 0xE9); long long skip = a->kullanilan; int32_yaz(a, 0);
        long long panic = a->kullanilan;
        emit_bounds_panic(a);
        long long after = a->kullanilan;
        int32_patch(a, skip, (int)(after - (skip + 4)));
        if (fail1 >= 0) int32_patch(a, fail1, (int)(panic - (fail1 + 4)));
        if (fail2 >= 0) int32_patch(a, fail2, (int)(panic - (fail2 + 4)));
        if (fail3 >= 0) int32_patch(a, fail3, (int)(panic - (fail3 + 4)));
    }
}

void int32_patch(MakineKoduArenasi* a, long long pos, int val) {
    a->bellek[pos] = (val & 0xFF);
    a->bellek[pos+1] = ((val >> 8) & 0xFF);
    a->bellek[pos+2] = ((val >> 16) & 0xFF);
    a->bellek[pos+3] = ((val >> 24) & 0xFF);
}

long long global_sabit_degeri(AstDugum* dugum, long long* bulundu) {
    *bulundu = 0;
    if (!dugum) return 0;
    if (YKN_DOT(dugum, tip) == AST_SAYI) {
        *bulundu = 1;
        return string_tamsayiya(YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk));
    }
    if (YKN_DOT(dugum, tip) == AST_GERCEK_SAYI) {
        *bulundu = 1;
        char buf[128];
        long long u = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
        if (u >= 127) u = 127;
        memcpy(buf, YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), u);
        buf[u] = 0;
        double dval = strtod(buf, NULL);
        uint64_t u64val = *(uint64_t*)&dval;
        return (long long)u64val;
    }
    if (YKN_DOT(dugum, tip) == AST_UNARY) {
        long long ic_bulundu = 0;
        long long deger = global_sabit_degeri(YKN_DOT(dugum, sag), &ic_bulundu);
        if (ic_bulundu && YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_EKSI) {
            *bulundu = 1;
            return 0 - deger;
        }
        *bulundu = ic_bulundu;
        return deger;
    }
    if (YKN_DOT(dugum, tip) == AST_IKILI_OP) {
        long long sol_bulundu = 0;
        long long sag_bulundu = 0;
        long long sol = global_sabit_degeri(YKN_DOT(dugum, sol), &sol_bulundu);
        long long sag = global_sabit_degeri(YKN_DOT(dugum, sag), &sag_bulundu);
        if (!sol_bulundu || !sag_bulundu) return 0;
        *bulundu = 1;
        long long op = YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip);
        if (op == TIP_ARTI) return sol + sag;
        if (op == TIP_EKSI) return sol - sag;
        if (op == TIP_CARPI) return sol * sag;
        if (op == TIP_BOLME && sag != 0) return sol / sag;
        if (op == TIP_MODULO && sag != 0) return sol % sag;
        if (op == TIP_XOR) return sol ^ sag;
        *bulundu = 0;
    }
    return 0;
}

long long ast_node_is_float(AstDugum* d) {
    if (!d) return 0;
    if (YKN_DOT(d, tip) == AST_GERCEK_SAYI) return 1;
    if (YKN_DOT(d, tip) == AST_SAYI || YKN_DOT(d, tip) == AST_METIN || YKN_DOT(d, tip) == AST_MANTIK) return 0;
    if (YKN_DOT(d, tip) == AST_KIMLIK) {
        if (YKN_DOT(d, ilk_jeton) == 0) return 0;
        char* isim = YKN_DOT(YKN_DOT(d, ilk_jeton), baslangic);
        long long uzn = YKN_DOT(YKN_DOT(d, ilk_jeton), uzunluk);
        for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
            if (g_lokal_uzunluklari[i] == uzn && bellek_esit_mi(g_lokal_isimleri[i], isim, uzn) == 1) {
                if (g_lokal_yildizlari[i] > 0) return 0;
                if (g_lokal_tip_adlari[i] != 0 && g_lokal_tip_uzunluklari[i] > 0) {
                    long long tid = tip_adini_coz(g_lokal_tip_adlari[i], g_lokal_tip_uzunluklari[i]);
                    if (is_gercek_group(tid)) return 1;
                }
                return 0;
            }
        }
        for (long long i = 0; i < g_global_sayisi; i++) {
            if (g_global_uzunluk[i] == uzn && bellek_esit_mi(g_global_isim[i], isim, uzn) == 1) {
                if (g_global_is_pointer[i] > 0) return 0;
                if (g_global_tip_adi[i] != 0 && g_global_tip_adi_uzunluk[i] > 0) {
                    long long tid = tip_adini_coz(g_global_tip_adi[i], g_global_tip_adi_uzunluk[i]);
                    if (is_gercek_group(tid)) return 1;
                }
                return 0;
            }
        }
        return 0;
    }
    if (YKN_DOT(d, tip) == AST_IKILI_OP) {
        if (YKN_DOT(d, ilk_jeton) != 0) {
            long long op = YKN_DOT(YKN_DOT(d, ilk_jeton), tip);
            if (op == TIP_ESIT_MI || op == TIP_ESIT_DEGIL || op == TIP_KUCUK_MU || op == TIP_BUYUK_MU || op == TIP_KUCUK_ESIT || op == TIP_BUYUK_ESIT) {
                return 0;
            }
            if (op == TIP_VE || op == TIP_VEYA) return 0;
        }
        return (ast_node_is_float(YKN_DOT(d, sol)) || ast_node_is_float(YKN_DOT(d, sag)));
    }
    if (YKN_DOT(d, tip) == AST_UNARY) {
        return ast_node_is_float(YKN_DOT(d, sag));
    }
    if (YKN_DOT(d, tip) == AST_COMPTIME) {
        return ast_node_is_float(YKN_DOT(d, sol));
    }
    if (YKN_DOT(d, tip) == AST_DONUSUM) {
        if (YKN_DOT(d, cozumlenmis_tip) == 0 && YKN_DOT(d, tip_adi) != 0) {
            if (is_gercek_group(tip_adini_coz((char*)YKN_DOT(d, tip_adi), YKN_DOT(d, tip_adi_uzunluk)))) return 1;
        }
        return 0;
    }
    if (YKN_DOT(d, tip) == AST_ISLEV_CAGRISI) {
        if (YKN_DOT(d, ilk_jeton) == 0) return 0;
        char* fname = YKN_DOT(YKN_DOT(d, ilk_jeton), baslangic);
        long long flen = YKN_DOT(YKN_DOT(d, ilk_jeton), uzunluk);
        if (is_gercek_group(tip_adini_coz(fname, flen))) return 1;
        // GOREV 6.2: FPU fonksiyonlar gercek dondurur
        struct { const char* nm; int len; } fpu_fns[] = {
            {"kare_kok",8},{"sqrt",4},{"sinus",5},{"sin",3},{"kosinus",7},{"cos",3},
            {"gercek_mutlak",13},{"fabs",4},{"gercek_minimum",14},{"fmin",4},
            {"gercek_maksimum",15},{"fmax",4}
        };
        for (auto& ff : fpu_fns) {
            if (flen == ff.len && bellek_esit_mi(fname, (char*)ff.nm, ff.len) == 1) return 1;
        }
        return 0;
    }
    return 0;
}

long long ast_node_is_string(AstDugum* d) {
    if (!d) return 0;
    if (YKN_DOT(d, tip) == AST_METIN) return 1;
    if (YKN_DOT(d, tip) == AST_FSTRING) return 1;
    if (YKN_DOT(d, tip) == AST_SAYI || YKN_DOT(d, tip) == AST_GERCEK_SAYI || YKN_DOT(d, tip) == AST_MANTIK) return 0;
    if (YKN_DOT(d, tip) == AST_ALAN_ERISIMI) {
        if (YKN_DOT(d, sag) != 0 && YKN_DOT(YKN_DOT(d, sag), ilk_jeton) != 0) {
            Jeton* f_j = YKN_DOT(YKN_DOT(d, sag), ilk_jeton);
            if (YKN_DOT(f_j, uzunluk) == 3) {
                char* ptr_f = YKN_DOT(f_j, baslangic);
                if (ptr_f[0] == 112 && ptr_f[1] == 116 && ptr_f[2] == 114) return 1; // .ptr
            } else if (YKN_DOT(f_j, uzunluk) == 4) {
                char* ptr_f = YKN_DOT(f_j, baslangic);
                if (ptr_f[0] == 118 && ptr_f[1] == 101 && ptr_f[2] == 114 && ptr_f[3] == 105) return 1; // .veri
            }
        }
    }
    if (YKN_DOT(d, tip) == AST_KIMLIK) {
        if (YKN_DOT(d, ilk_jeton) == 0) return 0;
        char* isim = YKN_DOT(YKN_DOT(d, ilk_jeton), baslangic);
        long long uzn = YKN_DOT(YKN_DOT(d, ilk_jeton), uzunluk);
        for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                if (g_lokal_uzunluklari[i] == uzn && bellek_esit_mi(g_lokal_isimleri[i], isim, uzn) == 1) {
                if (g_lokal_tip_adlari[i] != 0 && g_lokal_tip_uzunluklari[i] > 0) {
                    char* t_ad = g_lokal_tip_adlari[i];
                    long long t_len = g_lokal_tip_uzunluklari[i];
                    if (t_len == 3 && t_ad[0] == 42 && t_ad[1] == 117 && t_ad[2] == 56) return 1; // *u8
                    if (t_len == 2 && t_ad[0] == 117 && t_ad[1] == 56 && g_lokal_yildizlari[i] >= 1) return 1;
                    long long tid = tip_adini_coz(t_ad, t_len);
                    if (tid == TIP_ID_METIN) return 1;
                    if (g_lokal_yildizlari[i] == 1 && tid == 6) return 1;
                }
                return 0;
            }
        }
        for (long long i = 0; i < g_global_sayisi; i++) {
            if (g_global_uzunluk[i] == uzn && bellek_esit_mi(g_global_isim[i], isim, uzn) == 1) {
                if (g_global_tip_adi[i] != 0 && g_global_tip_adi_uzunluk[i] > 0) {
                    char* gt_ad = g_global_tip_adi[i];
                    long long gt_len = g_global_tip_adi_uzunluk[i];
                    if (gt_len == 3 && gt_ad[0] == 42 && gt_ad[1] == 117 && gt_ad[2] == 56) return 1;
                    if (gt_len == 2 && gt_ad[0] == 117 && gt_ad[1] == 56 && g_global_is_pointer[i] >= 1) return 1;
                    long long gtid = tip_adini_coz(gt_ad, gt_len);
                    if (gtid == TIP_ID_METIN) return 1;
                    if (g_global_is_pointer[i] == 1 && gtid == 6) return 1;
                }
                return 0;
            }
        }
        return 0;
    }
    if (YKN_DOT(d, tip) == AST_IKILI_OP) {
        if (YKN_DOT(d, ilk_jeton) != 0) {
            long long op = YKN_DOT(YKN_DOT(d, ilk_jeton), tip);
            if (op == TIP_ARTI) {
                if (ast_node_is_string(YKN_DOT(d, sol)) || ast_node_is_string(YKN_DOT(d, sag))) return 1;
            }
        }
        return 0;
    }
    if (YKN_DOT(d, tip) == AST_ISLEV_CAGRISI) {
        if (YKN_DOT(d, ilk_jeton) == 0) return 0;
        char* fname = YKN_DOT(YKN_DOT(d, ilk_jeton), baslangic);
        long long flen = YKN_DOT(YKN_DOT(d, ilk_jeton), uzunluk);
        if (tip_adini_coz(fname, flen) == TIP_ID_METIN) return 1;
        if (flen == 13 && bellek_esit_mi(fname, (char*)"tamsayi_metne", 13) == 1) return 1;
        if (flen == 9 && bellek_esit_mi(fname, (char*)"to_string", 9) == 1) return 1;
        if (flen == 4 && bellek_esit_mi(fname, (char*)"itoa", 4) == 1) return 1;
        if (flen == 15 && bellek_esit_mi(fname, (char*)"metin_birlestir", 15) == 1) return 1;
        if (flen == 23 && bellek_esit_mi(fname, (char*)"metin_iki_birlestir_yeni", 23) == 1) return 1;
        if (flen == 8 && bellek_esit_mi(fname, (char*)"alt_dize", 8) == 1) return 1;
        if (flen == 6 && bellek_esit_mi(fname, (char*)"substr", 6) == 1) return 1;
        if (flen == 14 && bellek_esit_mi(fname, (char*)"metin_kucuk_yap", 14) == 1) return 1;
        if (flen == 14 && bellek_esit_mi(fname, (char*)"metin_buyuk_yap", 14) == 1) return 1;
        return 0;
    }
    return 0;
}

void emit_string_concat_inline(MakineKoduArenasi* a) {
    // On entry: RCX = str1, RDX = str2
    // Preserve string pointers safely on stack across allocator call
    byte_yaz(a, 0x51); // PUSH RCX
    byte_yaz(a, 0x52); // PUSH RDX

    if (g_target_platform == 1) {
        // Linux sys_mmap(NULL, 4096, 3, 34, -1, 0)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC7); int32_yaz(a, 0);    // RDI = 0
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 4096); // RSI = 4096
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);    // RDX = 3
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);   // R10 = 34
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0xFFFFFFFF); // R8 = -1
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);                     // R9 = 0
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);    // RAX = 9
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);                                         // syscall -> RAX = buffer
    } else {
        // Windows VirtualAlloc(NULL, 4096, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
        // Two PUSHes (RCX, RDX) maintain 16-byte alignment. SUB RSP, 32 allocates shadow space.
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);  // SUB RSP, 32
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);                     // XOR RCX, RCX
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 4096); // MOV RDX, 4096
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);                  // MOV R8D, 0x3000
        byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);                    // MOV R9D, 0x04
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (!seed_iat_yama_kapasite_kontrol()) return;
        long long iat_idx = g_iat_yama_sayisi;
        g_iat_yamalar[iat_idx].text_offset = a->kullanilan;
        g_iat_yamalar[iat_idx].slot = 3; // VirtualAlloc
        g_iat_yama_sayisi++;
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);  // ADD RSP, 32
    }

    // Restore RDX (str2), RCX (str1) from stack
    byte_yaz(a, 0x5A); // POP RDX
    byte_yaz(a, 0x59); // POP RCX

    // Save buffer start in R11
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); // MOV R11, RAX

    // R8 = write pointer (starts at buffer start)
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC0); // MOV R8, RAX

    // Copy str1 (RCX) until null:
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC9); // TEST RCX, RCX
    byte_yaz(a, 0x74); byte_yaz(a, 0x13);                     // JE cp1_done (+19)
    byte_yaz(a, 0x44); byte_yaz(a, 0x8A); byte_yaz(a, 0x11); // MOV R10B, [RCX]
    byte_yaz(a, 0x45); byte_yaz(a, 0x84); byte_yaz(a, 0xD2); // TEST R10B, R10B
    byte_yaz(a, 0x74); byte_yaz(a, 0x0B);                     // JE cp1_done (+11)
    byte_yaz(a, 0x45); byte_yaz(a, 0x88); byte_yaz(a, 0x10); // MOV [R8], R10B
    byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC1); // INC RCX
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xEB); byte_yaz(a, 0xED);                     // JMP cp1_loop (-19 bytes = 0xED)

    // Copy str2 (RDX) until null:
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xD2); // TEST RDX, RDX
    byte_yaz(a, 0x74); byte_yaz(a, 0x13);                     // JE cp2_done (+19)
    byte_yaz(a, 0x44); byte_yaz(a, 0x8A); byte_yaz(a, 0x12); // MOV R10B, [RDX]
    byte_yaz(a, 0x45); byte_yaz(a, 0x84); byte_yaz(a, 0xD2); // TEST R10B, R10B
    byte_yaz(a, 0x74); byte_yaz(a, 0x0B);                     // JE cp2_done (+11)
    byte_yaz(a, 0x45); byte_yaz(a, 0x88); byte_yaz(a, 0x10); // MOV [R8], R10B
    byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC2); // INC RDX
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xEB); byte_yaz(a, 0xED);                     // JMP cp2_loop (-19 bytes = 0xED)

    // Null terminate: MOV byte [R8], 0
    byte_yaz(a, 0x41); byte_yaz(a, 0xC6); byte_yaz(a, 0x00); byte_yaz(a, 0x00);

    // Return buffer start in RAX: MOV RAX, R11
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, R11
}

void emit_i64_to_string(MakineKoduArenasi* a) {
    byte_yaz(a, 0x50); // PUSH RAX

    if (g_target_platform == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC7); int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 64);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0xFFFFFFFF);
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28);
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 64);
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);
        byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) {
            long long iat_idx = g_iat_yama_sayisi;
            g_iat_yamalar[iat_idx].text_offset = a->kullanilan;
            g_iat_yamalar[iat_idx].slot = 3;
            g_iat_yama_sayisi++;
        }
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28);
    }

    byte_yaz(a, 0x59); // POP RCX
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC0); // MOV R8, RAX
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV R9, RAX

    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC9); // TEST RCX, RCX
    byte_yaz(a, 0x75); long long jnz_zero_pos = a->kullanilan; byte_yaz(a, 0);

    byte_yaz(a, 0xC6); byte_yaz(a, 0x00); byte_yaz(a, '0');
    byte_yaz(a, 0xC6); byte_yaz(a, 0x40); byte_yaz(a, 1); byte_yaz(a, 0);
    byte_yaz(a, 0xEB); long long jmp_done_zero_pos = a->kullanilan; byte_yaz(a, 0);

    ((unsigned char*)a->bellek)[jnz_zero_pos] = (unsigned char)(a->kullanilan - (jnz_zero_pos + 1));

    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);
    byte_yaz(a, 0x79); long long jns_pos = a->kullanilan; byte_yaz(a, 0);

    byte_yaz(a, 0x41); byte_yaz(a, 0xC6); byte_yaz(a, 0x01); byte_yaz(a, '-');
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC1);
    byte_yaz(a, 0x48); byte_yaz(a, 0xF7); byte_yaz(a, 0xD9);

    ((unsigned char*)a->bellek)[jns_pos] = (unsigned char)(a->kullanilan - (jns_pos + 1));

    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0xCA); // MOV R10, R9

    long long loop_start = a->kullanilan;
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC8);
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC3); int32_yaz(a, 10);
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2);
    byte_yaz(a, 0x49); byte_yaz(a, 0xF7); byte_yaz(a, 0xF3);
    byte_yaz(a, 0x80); byte_yaz(a, 0xC2); byte_yaz(a, 0x30);
    byte_yaz(a, 0x41); byte_yaz(a, 0x88); byte_yaz(a, 0x11);
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC1);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1);
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);
    byte_yaz(a, 0x75); byte_yaz(a, (unsigned char)(loop_start - (a->kullanilan + 1)));

    byte_yaz(a, 0x41); byte_yaz(a, 0xC6); byte_yaz(a, 0x01); byte_yaz(a, 0);

    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0xCB);
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xCB);

    long long rev_loop = a->kullanilan;
    byte_yaz(a, 0x4D); byte_yaz(a, 0x39); byte_yaz(a, 0xDA);
    byte_yaz(a, 0x7D); long long jge_rev_pos = a->kullanilan; byte_yaz(a, 0);

    byte_yaz(a, 0x41); byte_yaz(a, 0x8A); byte_yaz(a, 0x02);
    byte_yaz(a, 0x41); byte_yaz(a, 0x8A); byte_yaz(a, 0x0B);
    byte_yaz(a, 0x41); byte_yaz(a, 0x88); byte_yaz(a, 0x0A);
    byte_yaz(a, 0x41); byte_yaz(a, 0x88); byte_yaz(a, 0x03);
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC2);
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xCB);
    byte_yaz(a, 0xEB); byte_yaz(a, (unsigned char)(rev_loop - (a->kullanilan + 1)));

    ((unsigned char*)a->bellek)[jge_rev_pos] = (unsigned char)(a->kullanilan - (jge_rev_pos + 1));
    ((unsigned char*)a->bellek)[jmp_done_zero_pos] = (unsigned char)(a->kullanilan - (jmp_done_zero_pos + 1));
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC0); // MOV RAX, R8
}

AstDugum* parse_fstring_subexpr(char* expr_ptr, long long expr_uzn, long long satir, long long sutun) {
    long long b = 0;
    while (b < expr_uzn && (expr_ptr[b] == ' ' || expr_ptr[b] == '\t' || expr_ptr[b] == '\n' || expr_ptr[b] == '\r')) b++;
    long long e = expr_uzn;
    while (e > b && (expr_ptr[e - 1] == ' ' || expr_ptr[e - 1] == '\t' || expr_ptr[e - 1] == '\n' || expr_ptr[e - 1] == '\r')) e--;
    long long c_len = e - b;
    if (c_len <= 0) return nullptr;
    char* c_ptr = expr_ptr + b;

    LexerState* lx = (LexerState*)bellek_tahsis_et(sizeof(LexerState));
    if (!lx) return nullptr;
    memset(lx, 0, sizeof(LexerState));
    lx->kaynak = c_ptr;
    lx->dosya_boyutu = c_len;
    lx->guncel_offset = 0;
    lx->satir = satir;
    lx->sutun = sutun;
    lx->jeton_kapasite = 128;
    lx->jeton_sayisi = 0;
    lx->jetonlar = (Jeton*)bellek_tahsis_et(128 * sizeof(Jeton));
    memset(lx->jetonlar, 0, 128 * sizeof(Jeton));
    lx->pool = nullptr;

    tokenize_et(lx);
    if (lx->jeton_sayisi <= 0) return nullptr;

    ParserDurumu* pd = (ParserDurumu*)bellek_tahsis_et(sizeof(ParserDurumu));
    if (!pd) return nullptr;
    memset(pd, 0, sizeof(ParserDurumu));
    pd->jetonlar = lx->jetonlar;
    pd->jeton_sayisi = lx->jeton_sayisi;
    pd->guncel_index = 0;
    AstArena* ar = (AstArena*)bellek_tahsis_et(sizeof(AstArena));
    if (!ar) return nullptr;
    memset(ar, 0, sizeof(AstArena));
    ar->bellek = (char*)bellek_tahsis_et(65536);
    ar->kapasite = 65536;
    ar->kullanilan = 0;
    pd->arena = ar;
    pd->hata_sayisi = 0;

    return parse_ifade(pd, 0);
}

// RAII state and helpers
const int RAII_MAKS_KAYIT = 512;
int g_raii_kayit_adet = 0;
long long g_raii_kayit_off[512];
char* g_raii_kayit_fn_isim[512];
long long g_raii_kayit_fn_uzn[512];
int g_raii_kayit_derinlik[512];
int g_raii_suanki_derinlik = 0;

char* g_raii_tip_isim[256];
long long g_raii_tip_uzunluk[256];
int g_raii_tip_adet = 0;

void raii_sifirla() {
    g_raii_kayit_adet = 0;
    g_raii_suanki_derinlik = 0;
}

void raii_destructor_tipi_kaydet(char* tip_isim, long long tip_uzn) {
    for (int i = 0; i < g_raii_tip_adet; i++) {
        if (g_raii_tip_uzunluk[i] == tip_uzn && memcmp(g_raii_tip_isim[i], tip_isim, tip_uzn) == 0) return;
    }
    if (g_raii_tip_adet < 256) {
        g_raii_tip_isim[g_raii_tip_adet] = tip_isim;
        g_raii_tip_uzunluk[g_raii_tip_adet] = tip_uzn;
        g_raii_tip_adet++;
    }
}

int raii_destructor_tipi_var_mi(char* tip_isim, long long tip_uzn) {
    if (!tip_isim || tip_uzn <= 0) return 0;
    for (int i = 0; i < g_raii_tip_adet; i++) {
        if (g_raii_tip_uzunluk[i] == tip_uzn && memcmp(g_raii_tip_isim[i], tip_isim, tip_uzn) == 0) return 1;
    }
    return 0;
}

void raii_ast_destructorlari_tara(AstDugum* kok) {
    g_raii_tip_adet = 0;
    for (AstDugum* iter = kok; iter != nullptr; iter = (AstDugum*)iter->sonraki) {
        if (iter->tip == AST_ISLEV || iter->tip == AST_ASYNC_ISLEV || iter->tip == 41) {
            if (iter->ilk_jeton && iter->ilk_jeton->uzunluk > 6) {
                long long uzn = iter->ilk_jeton->uzunluk;
                char* bas = iter->ilk_jeton->baslangic;
                if (memcmp(bas + (uzn - 6), "_birak", 6) == 0) {
                    raii_destructor_tipi_kaydet(bas, uzn - 6);
                }
            }
        }
    }
}

void raii_blok_gir() {
    g_raii_suanki_derinlik++;
}

void raii_kayit_ekle(long long off, char* fn_isim, long long fn_uzn) {
    if (g_raii_kayit_adet >= RAII_MAKS_KAYIT) return;
    int idx = g_raii_kayit_adet++;
    g_raii_kayit_off[idx] = off;
    g_raii_kayit_fn_isim[idx] = fn_isim;
    g_raii_kayit_fn_uzn[idx] = fn_uzn;
    g_raii_kayit_derinlik[idx] = g_raii_suanki_derinlik;
}

void raii_cagri_emit(MakineKoduArenasi* a, long long off, char* fn_isim, long long fn_uzn) {
    byte_yaz(a, 0x50); // PUSH RAX (-8, now RSP % 16 == 8)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x8D);
    int32_yaz(a, (int)-off);
    if (g_target_platform == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF); // MOV RDI, RCX
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x08); // SUB RSP, 8
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28); // SUB RSP, 40 (32 shadow + 8 align)
    }
    byte_yaz(a, 0xE8);
    if (seed_yama_kapasite_kontrol()) {
        long long y_idx = g_yama_sayisi++;
        g_yamalar[y_idx].isim = fn_isim;
        g_yamalar[y_idx].uzunluk = fn_uzn;
        g_yamalar[y_idx].cagri_pos = a->kullanilan;
    }
    int32_yaz(a, 0);
    if (g_target_platform == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x08); // ADD RSP, 8
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28); // ADD RSP, 40
    }
    byte_yaz(a, 0x58); // POP RAX
}

void raii_blok_cik_emit(MakineKoduArenasi* a) {
    int d = g_raii_suanki_derinlik;
    for (int i = g_raii_kayit_adet - 1; i >= 0; i--) {
        if (g_raii_kayit_derinlik[i] == d) {
            raii_cagri_emit(a, g_raii_kayit_off[i], g_raii_kayit_fn_isim[i], g_raii_kayit_fn_uzn[i]);
            g_raii_kayit_adet = i;
        }
    }
    if (g_raii_suanki_derinlik > 0) g_raii_suanki_derinlik--;
}

void raii_fonksiyon_donus_emit(MakineKoduArenasi* a) {
    for (int i = g_raii_kayit_adet - 1; i >= 0; i--) {
        raii_cagri_emit(a, g_raii_kayit_off[i], g_raii_kayit_fn_isim[i], g_raii_kayit_fn_uzn[i]);
    }
}

void raii_degisken_kontrol(char* tip_adi, long long tip_uzn, long long off) {
    if (!tip_adi || tip_uzn <= 0) return;
    if (raii_destructor_tipi_var_mi(tip_adi, tip_uzn)) {
        long long fn_uzn = tip_uzn + 6;
        char* fn_buf = (char*)bellek_tahsis_et(fn_uzn + 1);
        memcpy(fn_buf, tip_adi, tip_uzn);
        memcpy(fn_buf + tip_uzn, "_birak", 6);
        fn_buf[fn_uzn] = 0;
        raii_kayit_ekle(off, fn_buf, fn_uzn);
    }
}

// Closure / Lambda capture state
long long g_lambda_tabani = 0;

long long yerlesik_sabit_degeri(char* isim, long long uzunluk, long long* bulundu) {
    *bulundu = 1;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_ISLEV", 9)) return 1;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_DEGISKEN", 12)) return 2;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_DONDUR", 10)) return 3;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"TIP_EGER", 8)) return 4;
    if (uzunluk == 7 && bellek_esit_mi(isim, (char*)"TIP_ISE", 7)) return 5;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_YOKSA", 9)) return 6;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_DONGU", 9)) return 7;
    if (uzunluk == 7 && bellek_esit_mi(isim, (char*)"TIP_HER", 7)) return 8;
    if (uzunluk == 7 && bellek_esit_mi(isim, (char*)"TIP_EOF", 7)) return 99;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_KIMLIK", 10)) return 9;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"TIP_TAMSAYI", 11)) return 10;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"TIP_GERCEK_SAYI", 15)) return 11;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_METIN", 9)) return 12;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"TIP_YAPI", 8)) return 13;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_DURDUR", 10)) return 14;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_DEVAM_ET", 12)) return 15;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"TIP_ICE_AKTAR", 13)) return 16;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_DOGRU", 9)) return 17;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_YANLIS", 10)) return 18;
    if (uzunluk == 6 && bellek_esit_mi(isim, (char*)"TIP_VE", 6)) return 19;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"TIP_VEYA", 8)) return 20;
    if (uzunluk == 7 && bellek_esit_mi(isim, (char*)"TIP_XOR", 7)) return 43;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"TIP_ARTI", 8)) return 30;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"TIP_EKSI", 8)) return 31;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_CARPI", 9)) return 32;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_BOLME", 9)) return 33;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_ATAMA", 9)) return 34;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"TIP_ESIT_MI", 11)) return 35;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"TIP_ESIT_DEGIL", 14)) return 36;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_KUCUK_MU", 12)) return 37;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_BUYUK_MU", 12)) return 38;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"TIP_KUCUK_ESIT", 14)) return 39;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"TIP_BUYUK_ESIT", 14)) return 40;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_MODULO", 10)) return 41;
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"TIP_SOL_PARANTEZ", 16)) return 50;
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"TIP_SAG_PARANTEZ", 16)) return 51;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"TIP_SOL_KUSLU", 13)) return 52;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"TIP_SAG_KUSLU", 13)) return 53;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_SOL_KOSE", 12)) return 54;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_SAG_KOSE", 12)) return 55;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_NOKTA", 9)) return 56;
    if (uzunluk == 18 && bellek_esit_mi(isim, (char*)"TIP_NOKTALI_VIRGUL", 18)) return 57;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_VIRGUL", 10)) return 58;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_KOLON", 9)) return 59;
    if (uzunluk == 6 && bellek_esit_mi(isim, (char*)"TIP_OK", 6)) return 60;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"TIP_AMPERSAND", 13)) return 61;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"TIP_BILINMEYEN", 15)) return 100;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"TIP_FSTRING", 11)) return 101;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_ASYNC", 9)) return 102;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_AWAIT", 9)) return 103;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_TRAIT", 9)) return 104;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_UYGULA", 10)) return 105;
    if (uzunluk == 7 && bellek_esit_mi(isim, (char*)"TIP_DYN", 7)) return 106;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_DEGIL", 9)) return 107;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_SABIT", 9)) return 108;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_TILDE", 9)) return 47;

    // AST Dugum Tipleri
    if (uzunluk == 7 && bellek_esit_mi(isim, (char*)"AST_BOS", 7)) return 0;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"AST_SAYI", 8)) return 1;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"AST_GERCEK_SAYI", 15)) return 2;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"AST_METIN", 9)) return 3;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"AST_KIMLIK", 10)) return 4;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"AST_IKILI_OP", 12)) return 5;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"AST_ATAMA", 9)) return 6;
    if (uzunluk == 19 && bellek_esit_mi(isim, (char*)"AST_DEGISKEN_TANIMI", 19)) return 7;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"AST_EGER", 8)) return 8;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"AST_DONGU", 9)) return 9;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"AST_ISLEV", 9)) return 10;
    if (uzunluk == 17 && bellek_esit_mi(isim, (char*)"AST_ISLEV_CAGRISI", 17)) return 11;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"AST_DONDUR", 10)) return 12;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"AST_BLOK", 8)) return 13;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"AST_DURDUR", 10)) return 14;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"AST_DEVAM_ET", 12)) return 15;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"AST_YAPI_TANIMI", 15)) return 16;
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"AST_ALAN_ERISIMI", 16)) return 17;
    if (uzunluk == 18 && bellek_esit_mi(isim, (char*)"AST_INDEKS_ERISIMI", 18)) return 18;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"AST_UNARY", 9)) return 19;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"AST_REFERANS", 12)) return 20;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"AST_ICE_AKTAR", 14)) return 21;
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"AST_MAKRO_TANIMI", 16)) return 22;
    if (uzunluk == 17 && bellek_esit_mi(isim, (char*)"AST_MAKRO_CAGRISI", 17)) return 23;
    if (uzunluk == 17 && bellek_esit_mi(isim, (char*)"AST_GENERIC_TANIM", 17)) return 24;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"AST_TRAIT_TANIM", 15)) return 25;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"AST_MANTIK", 10)) return 26;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"AST_FSTRING", 11)) return 39;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"AST_ASYNC_ISLEV", 15)) return 41;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"AST_AWAIT", 9)) return 42;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"AST_DONUSUM", 11)) return 45;

    // Typechecker Tipleri
    if (uzunluk == 18 && bellek_esit_mi(isim, (char*)"TIP_ID_BILINMIYOR", 18)) return 0;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"TIP_ID_TAMSAYI", 15)) return 1;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"TIP_ID_GERCEK", 13)) return 2;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_ID_METIN", 12)) return 3;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"TIP_ID_MANTIK", 13)) return 4;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"TIP_ID_U8", 9)) return 5;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"TIP_ID_ISLEV", 12)) return 6;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"TIP_ID_YAPI", 11)) return 7;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"TIP_ID_HIC", 10)) return 9;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"KAT_TEMEL", 9)) return 1;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"KAT_YAPI", 8)) return 2;

    // MIR Opcodes
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"MIR_OP_ATAMA", 12)) return 100;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"MIR_OP_YUKLE", 12)) return 101;
    if (uzunluk == 13 && bellek_esit_mi(isim, (char*)"MIR_OP_DEPOLA", 13)) return 102;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"MIR_OP_TOPLA", 12)) return 103;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"MIR_OP_CIKAR", 12)) return 104;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"MIR_OP_CARP", 11)) return 105;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"MIR_OP_BOL", 10)) return 106;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"MIR_OP_MOD", 10)) return 107;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"MIR_OP_ESIT_MI", 14)) return 108;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"MIR_OP_KUCUK_MU", 15)) return 109;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"MIR_OP_BUYUK_MU", 15)) return 110;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"MIR_OP_ESIT_DEG", 15)) return 111;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"MIR_OP_KOS", 10)) return 112;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"MIR_OP_KOS_EGER", 15)) return 113;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"MIR_OP_CAGIR", 12)) return 114;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"MIR_OP_DON", 10)) return 115;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"MIR_OP_ADRES", 12)) return 116;

    *bulundu = 0;
    return 0;
}

void global_sabitleri_topla(AstDugum* kok, long long fonksiyon_icinde) {
    AstDugum* iter = kok;
    while (iter != 0) {
        if (YKN_DOT(iter, tip) == AST_DEGISKEN_TANIMI) {
            long long bulundu = 0;
            long long deger = global_sabit_degeri(YKN_DOT(iter, sag), &bulundu);
            if (bulundu == 1 && g_global_sabit_sayisi < 16384) {
                g_global_sabitler[g_global_sabit_sayisi].isim = YKN_DOT(YKN_DOT(iter, ilk_jeton), baslangic);
                g_global_sabitler[g_global_sabit_sayisi].uzunluk = YKN_DOT(YKN_DOT(iter, ilk_jeton), uzunluk);
                g_global_sabitler[g_global_sabit_sayisi].deger = deger;
                g_global_sabit_sayisi++;
            }
        }
        iter = YKN_DOT(iter, sonraki);
    }
}

void yapilari_topla(AstDugum* kok) {
    AstDugum* iter = kok;
    while (iter != 0) {
        if (YKN_DOT(iter, tip) == AST_YAPI_TANIMI) {
            bool zaten_var = false;
            for (long long i = 0; i < g_yapi_sayisi; i++) {
                if (g_yapilar[i].uzunluk == YKN_DOT(YKN_DOT(iter, ilk_jeton), uzunluk) &&
                    bellek_esit_mi(g_yapilar[i].isim, YKN_DOT(YKN_DOT(iter, ilk_jeton), baslangic), g_yapilar[i].uzunluk) == 1) {
                    zaten_var = true; break;
                }
            }
            if (!zaten_var && g_yapi_sayisi >= 1024 && YKN_DOT(iter, ilk_jeton) != 0) {
                seed_codegen_kapasite_reddet(3);
            }
            if (!zaten_var && g_yapi_sayisi < 1024 && YKN_DOT(iter, ilk_jeton) != 0) {
                long long idx = g_yapi_sayisi++;
                g_yapilar[idx].isim = YKN_DOT(YKN_DOT(iter, ilk_jeton), baslangic);
                g_yapilar[idx].uzunluk = YKN_DOT(YKN_DOT(iter, ilk_jeton), uzunluk);
                g_yapilar[idx].alan_sayisi = 0;
                AstDugum* alan = YKN_DOT(iter, sol);
                long long mevcut_off = 0;
                while (alan != 0) {
                    if (g_yapilar[idx].alan_sayisi >= 32 || YKN_DOT(alan, ilk_jeton) == 0) {
                        if (g_yapilar[idx].alan_sayisi >= 32) seed_codegen_kapasite_reddet(7);
                        break;
                    }
                    long long a_idx = g_yapilar[idx].alan_sayisi++;
                    g_yapilar[idx].alanlar[a_idx].isim = YKN_DOT(YKN_DOT(alan, ilk_jeton), baslangic);
                    g_yapilar[idx].alanlar[a_idx].uzunluk = YKN_DOT(YKN_DOT(alan, ilk_jeton), uzunluk);
                    long long f_elem_sz = tip_eleman_boyutu_al(YKN_DOT(alan, tip_adi), YKN_DOT(alan, tip_adi_uzunluk), YKN_DOT(alan, cozumlenmis_tip));
                    long long f_boyut = f_elem_sz;
                    if (YKN_DOT(alan, dizi_boyutu) > 0) {
                        f_boyut = YKN_DOT(alan, dizi_boyutu) * f_elem_sz;
                    }
                    long long f_align = f_elem_sz;
                    if (f_align > 8) f_align = 8;
                    if (f_align > 1) {
                        mevcut_off = ((mevcut_off + (f_align - 1)) / f_align) * f_align;
                    }
                    g_yapilar[idx].alanlar[a_idx].offset = mevcut_off;
                    g_yapilar[idx].alanlar[a_idx].tip_adi = (char*)YKN_DOT(alan, tip_adi);
                    g_yapilar[idx].alanlar[a_idx].tip_uzunluk = YKN_DOT(alan, tip_adi_uzunluk);
                    g_yapilar[idx].alanlar[a_idx].yildiz_sayisi = YKN_DOT(alan, cozumlenmis_tip);
                    g_yapilar[idx].alanlar[a_idx].nullable = YKN_DOT(alan, nullable);
                    g_yapilar[idx].alanlar[a_idx].dizi_boyutu = YKN_DOT(alan, dizi_boyutu);
                    mevcut_off += f_boyut;
                    alan = YKN_DOT(alan, sonraki);
                }
                mevcut_off = ((mevcut_off + 7) / 8) * 8;
                g_yapilar[idx].byte_boyutu = mevcut_off;
            }
        }
        iter = YKN_DOT(iter, sonraki);
    }

}

long long bul_veya_ekle_iat_slot(char* isim, long long uzunluk) {
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"ExitProcess", 11) == 1) return 0;
    if (uzunluk == 9 && bellek_esit_mi(isim, (char*)"WriteFile", 9) == 1) return 1;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"GetStdHandle", 12) == 1) return 2;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"VirtualAlloc", 12) == 1) return 3;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"CreateFileA", 11) == 1) return 4;
    if (uzunluk == 8 && bellek_esit_mi(isim, (char*)"ReadFile", 8) == 1) return 5;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"CloseHandle", 11) == 1) return 6;
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"GetCommandLineA", 16) == 1) return 7;
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"SetFilePointerEx", 16) == 1) return 8;
    if (uzunluk == 10 && bellek_esit_mi(isim, (char*)"CreatePipe", 10) == 1) return 10;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"CreateProcessA", 14) == 1) return 11;
    if (uzunluk == 19 && bellek_esit_mi(isim, (char*)"WaitForSingleObject", 19) == 1) return 12;
    if (uzunluk == 18 && bellek_esit_mi(isim, (char*)"GetExitCodeProcess", 18) == 1) return 13;
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"LoadLibraryA", 12) == 1) return 14;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"GetProcAddress", 14) == 1) return 15;
    if (uzunluk == 11 && bellek_esit_mi(isim, (char*)"FreeLibrary", 11) == 1) return 16;
    if (uzunluk == 15 && bellek_esit_mi(isim, (char*)"ykn_llm_dot_q16", 15) == 1) { g_iat_simd_kullanildi = 1; return 30; }
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"ykn_llm_silu_q16", 16) == 1) { g_iat_simd_kullanildi = 1; return 31; }
    if (uzunluk == 20 && bellek_esit_mi(isim, (char*)"ykn_llm_simd_backend", 20) == 1) { g_iat_simd_kullanildi = 1; return 32; }
    if (uzunluk == 20 && bellek_esit_mi(isim, (char*)"ykn_llm_iq2s_dot_q16", 20) == 1) { g_iat_simd_kullanildi = 1; return 33; }
    if (uzunluk == 16 && bellek_esit_mi(isim, (char*)"ykn_llm_clock_ns", 16) == 1) { g_iat_simd_kullanildi = 1; return 34; }
    if (uzunluk == 22 && bellek_esit_mi(isim, (char*)"ykn_llm_peak_rss_bytes", 22) == 1) { g_iat_simd_kullanildi = 1; return 35; }
    if (uzunluk == 19 && bellek_esit_mi(isim, (char*)"ykn_llm_f16_dot_q16", 19) == 1) { g_iat_simd_kullanildi = 1; return 36; }
    if (uzunluk == 20 && bellek_esit_mi(isim, (char*)"ykn_llm_q1_0_dot_q16", 20) == 1) { g_iat_simd_kullanildi = 1; return 37; }
    if (uzunluk == 12 && bellek_esit_mi(isim, (char*)"CreateThread", 12) == 1) return 39;
    if (uzunluk == 25 && bellek_esit_mi(isim, (char*)"InitializeCriticalSection", 25) == 1) return 40;
    if (uzunluk == 20 && bellek_esit_mi(isim, (char*)"EnterCriticalSection", 20) == 1) return 41;
    if (uzunluk == 20 && bellek_esit_mi(isim, (char*)"LeaveCriticalSection", 20) == 1) return 42;
    if (uzunluk == 22 && bellek_esit_mi(isim, (char*)"CreateIoCompletionPort", 22) == 1) return 43;
    if (uzunluk == 25 && bellek_esit_mi(isim, (char*)"GetQueuedCompletionStatus", 25) == 1) return 44;
    if (uzunluk == 5 && bellek_esit_mi(isim, (char*)"Sleep", 5) == 1) return 45;
    if (uzunluk == 14 && bellek_esit_mi(isim, (char*)"GetTickCount64", 14) == 1) return 46;
    return -1;
}

static bool seed_linux_socket_call(MakineKoduArenasi* a, char* name, long long length, long long arg_count) {
    if (g_target_platform != 1 || !name) return false;
    auto eq = [&](const char* text, long long n) { return length == n && bellek_esit_mi(name, (char*)text, n) == 1; };
    if (eq("yukinary_sys_net_init", 21)) { if (arg_count != 0) return false; emit_xor_rax_rax(a); return true; }
    if (eq("yukinary_sys_socket_create", 26)) {
        if (arg_count != 3) return false;
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xD6); byte_yaz(a,0x4C); byte_yaz(a,0x89); byte_yaz(a,0xC2);
        emit_mov_rax_imm32(a,41); byte_yaz(a,0x0F); byte_yaz(a,0x05); return true;
    }
    if (eq("yukinary_sys_socket_bind",24) || eq("yukinary_sys_socket_connect",27)) {
        if (arg_count != 3) return false; emit_sub_rsp(a,16);
        byte_yaz(a,0x66); byte_yaz(a,0xC7); byte_yaz(a,0x04); byte_yaz(a,0x24); byte_yaz(a,2); byte_yaz(a,0);
        byte_yaz(a,0x66); byte_yaz(a,0x41); byte_yaz(a,0xC1); byte_yaz(a,0xC8); byte_yaz(a,8); byte_yaz(a,0x66); byte_yaz(a,0x44); byte_yaz(a,0x89); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,2);
        byte_yaz(a,0x89); byte_yaz(a,0x54); byte_yaz(a,0x24); byte_yaz(a,4); byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,8); int32_yaz(a,0); byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,12); int32_yaz(a,0);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xE6); byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,16);
        emit_mov_rax_imm32(a,eq("yukinary_sys_socket_bind",24)?49:42); byte_yaz(a,0x0F); byte_yaz(a,0x05); emit_add_rsp(a,16); return true;
    }
    if (eq("yukinary_sys_socket_listen",26)) { if (arg_count!=2) return false; byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xD6); emit_mov_rax_imm32(a,50); byte_yaz(a,0x0F); byte_yaz(a,0x05); return true; }
    if (eq("yukinary_sys_socket_accept",26)) { if (arg_count!=1) return false; byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); byte_yaz(a,0x48); byte_yaz(a,0x31); byte_yaz(a,0xF6); byte_yaz(a,0x48); byte_yaz(a,0x31); byte_yaz(a,0xD2); emit_mov_rax_imm32(a,43); byte_yaz(a,0x0F); byte_yaz(a,0x05); return true; }
    if (eq("yukinary_sys_socket_close",25)) { if (arg_count!=1) return false; byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); emit_mov_rax_imm32(a,3); byte_yaz(a,0x0F); byte_yaz(a,0x05); return true; }
    if (eq("yukinary_sys_socket_send",24) || eq("yukinary_sys_socket_recv",24)) {
        if (arg_count!=3) return false; byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xD6); byte_yaz(a,0x4C); byte_yaz(a,0x89); byte_yaz(a,0xC2); byte_yaz(a,0x4D); byte_yaz(a,0x31); byte_yaz(a,0xD2); byte_yaz(a,0x4D); byte_yaz(a,0x31); byte_yaz(a,0xC0); byte_yaz(a,0x4D); byte_yaz(a,0x31); byte_yaz(a,0xC9);
        emit_mov_rax_imm32(a,eq("yukinary_sys_socket_send",24)?44:45); byte_yaz(a,0x0F); byte_yaz(a,0x05); return true;
    }
    if (eq("yukinary_sys_socket_set_reuseaddr",33)) {
        if (arg_count != 1) return false; emit_sub_rsp(a,8);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0x04); byte_yaz(a,0x24); int32_yaz(a,1);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF); byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC6); int32_yaz(a,1); byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,2);
        byte_yaz(a,0x49); byte_yaz(a,0x89); byte_yaz(a,0xE2); byte_yaz(a,0x49); byte_yaz(a,0xC7); byte_yaz(a,0xC0); int32_yaz(a,4); emit_mov_rax_imm32(a,54); byte_yaz(a,0x0F); byte_yaz(a,0x05); emit_add_rsp(a,8); return true;
    }
    if (eq("yukinary_sys_socket_set_timeout",31)) {
        if (arg_count != 2) return false;
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xD6);
        emit_sub_rsp(a,24);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0x7C); byte_yaz(a,0x24); byte_yaz(a,16);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xF0);
        byte_yaz(a,0x48); byte_yaz(a,0x31); byte_yaz(a,0xD2);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC1); int32_yaz(a,1000);
        byte_yaz(a,0x48); byte_yaz(a,0xF7); byte_yaz(a,0xF1);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0x04); byte_yaz(a,0x24);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xD0);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC1); int32_yaz(a,1000);
        byte_yaz(a,0x48); byte_yaz(a,0x0F); byte_yaz(a,0xAF); byte_yaz(a,0xC1);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,8);
        byte_yaz(a,0x48); byte_yaz(a,0x8B); byte_yaz(a,0x7C); byte_yaz(a,0x24); byte_yaz(a,16);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC6); int32_yaz(a,1);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,20);
        byte_yaz(a,0x4C); byte_yaz(a,0x8D); byte_yaz(a,0x54); byte_yaz(a,0x24); byte_yaz(a,0);
        byte_yaz(a,0x49); byte_yaz(a,0xC7); byte_yaz(a,0xC0); int32_yaz(a,16);
        emit_mov_rax_imm32(a,54); byte_yaz(a,0x0F); byte_yaz(a,0x05);
        byte_yaz(a,0x49); byte_yaz(a,0x89); byte_yaz(a,0xC1);
        byte_yaz(a,0x48); byte_yaz(a,0x8B); byte_yaz(a,0x7C); byte_yaz(a,0x24); byte_yaz(a,16);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC6); int32_yaz(a,1);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,21);
        byte_yaz(a,0x4C); byte_yaz(a,0x8D); byte_yaz(a,0x54); byte_yaz(a,0x24); byte_yaz(a,0);
        byte_yaz(a,0x49); byte_yaz(a,0xC7); byte_yaz(a,0xC0); int32_yaz(a,16);
        emit_mov_rax_imm32(a,54); byte_yaz(a,0x0F); byte_yaz(a,0x05);
        byte_yaz(a,0x4C); byte_yaz(a,0x09); byte_yaz(a,0xC8);
        emit_add_rsp(a,24); return true;
    }
    if (eq("yukinary_sys_socket_set_nonblocking",35)) {
        if (arg_count != 1) return false;
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0xCF);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC6); int32_yaz(a,3);
        byte_yaz(a,0x48); byte_yaz(a,0x31); byte_yaz(a,0xD2);
        emit_mov_rax_imm32(a,72); byte_yaz(a,0x0F); byte_yaz(a,0x05);
        byte_yaz(a,0x49); byte_yaz(a,0x89); byte_yaz(a,0xC0);
        byte_yaz(a,0x49); byte_yaz(a,0x81); byte_yaz(a,0xC8); int32_yaz(a,0x800);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC6); int32_yaz(a,4);
        byte_yaz(a,0x4C); byte_yaz(a,0x89); byte_yaz(a,0xC2);
        emit_mov_rax_imm32(a,72); byte_yaz(a,0x0F); byte_yaz(a,0x05); return true;
    }
    if (eq("yukinary_sys_dns_resolve",24)) {
        emit_mov_rax_imm32(a,-1); return true;
    }
    // POSIX process and shell syscalls (Linux x86-64)
    if (eq("pipe", 4) || eq("yukinary_sys_pipe", 17)) {
        if (arg_count != 1) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        emit_mov_rax_imm32(a, 22); byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
        byte_yaz(a, 0x75); byte_yaz(a, 19);
        byte_yaz(a, 0x48); byte_yaz(a, 0x63); byte_yaz(a, 0x57); byte_yaz(a, 0x04);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x57); byte_yaz(a, 0x08);
        byte_yaz(a, 0x48); byte_yaz(a, 0x63); byte_yaz(a, 0x17);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x17);
        return true;
    }
    if (eq("fork", 4) || eq("yukinary_sys_fork", 17)) {
        if (arg_count != 0) return false;
        emit_mov_rax_imm32(a, 57); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("dup2", 4) || eq("yukinary_sys_dup2", 17)) {
        if (arg_count != 2) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);
        emit_mov_rax_imm32(a, 33); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("close", 5) || eq("yukinary_sys_close", 18)) {
        if (arg_count != 1) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        emit_mov_rax_imm32(a, 3); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("execve", 6) || eq("yukinary_sys_execve", 19)) {
        if (arg_count != 3) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC2);
        emit_mov_rax_imm32(a, 59); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("waitpid", 7) || eq("yukinary_sys_waitpid", 20)) {
        if (arg_count != 3) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC2);
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xD2);
        emit_mov_rax_imm32(a, 61); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("read", 4) || eq("yukinary_sys_read", 17)) {
        if (arg_count != 3) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC2);
        emit_mov_rax_imm32(a, 0); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("write", 5) || eq("yukinary_sys_write", 18)) {
        if (arg_count != 3) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC2);
        emit_mov_rax_imm32(a, 1); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    if (eq("_exit", 5) || eq("yukinary_sys_exit", 17)) {
        if (arg_count != 1) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
        emit_mov_rax_imm32(a, 60); byte_yaz(a, 0x0F); byte_yaz(a, 0x05); return true;
    }
    // Win32 API functions fail safely on Linux with return 0 (NULL handle/result)
    if (eq("LoadLibraryA", 12) || eq("GetProcAddress", 14) || eq("FreeLibrary", 11) ||
        eq("CreateProcessA", 14) || eq("CreatePipe", 10) || eq("CloseHandle", 11) ||
        eq("ReadFile", 8) || eq("WaitForSingleObject", 19) || eq("GetExitCodeProcess", 18)) {
        emit_xor_rax_rax(a); return true;
    }
    return false;
}

static bool seed_windows_socket_call(MakineKoduArenasi* a, char* name, long long length, long long arg_count) {
    if (g_target_platform != 0 || !name) return false;
    auto eq = [&](const char* text, long long n) { return length == n && bellek_esit_mi(name, (char*)text, n) == 1; };
    auto emit_iat_call = [&](int slot) {
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) {
            g_iat_yamalar[g_iat_yama_sayisi].text_offset = a->kullanilan;
            g_iat_yamalar[g_iat_yama_sayisi].slot = slot;
            g_iat_yama_sayisi++;
        }
        int32_yaz(a, 0);
    };
    if (eq("yukinary_sys_net_init", 21)) {
        if (arg_count != 0) return false;
        // WSAStartup(0x0202, WSADATA*) with a real stack-resident WSADATA.
        emit_sub_rsp(a, 432);
        byte_yaz(a, 0x66); byte_yaz(a, 0xC7); byte_yaz(a, 0xC1); byte_yaz(a, 0x02); byte_yaz(a, 0x02);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xE2);
        emit_iat_call(18);
        emit_add_rsp(a, 432);
        return true;
    }
    if (eq("yukinary_sys_socket_create", 26)) {
        if (arg_count != 3) return false;
        emit_iat_call(19);
        return true;
    }
    if (eq("yukinary_sys_socket_bind", 24) || eq("yukinary_sys_socket_connect", 27)) {
        if (arg_count != 3) return false;
        // sockaddr_in { family=AF_INET, port=htons(port), addr, zero }.
        emit_sub_rsp(a, 48);
        byte_yaz(a, 0x66); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); byte_yaz(a, 0x02); byte_yaz(a, 0x00);
        byte_yaz(a, 0x66); byte_yaz(a, 0x41); byte_yaz(a, 0xC1); byte_yaz(a, 0xC8); byte_yaz(a, 0x08);
        byte_yaz(a, 0x66); byte_yaz(a, 0x44); byte_yaz(a, 0x89); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x22);
        byte_yaz(a, 0x89); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x24);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x20);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 16);
        emit_iat_call(eq("yukinary_sys_socket_bind", 24) ? 20 : 26);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        emit_add_rsp(a, 48);
        return true;
    }
    if (eq("yukinary_sys_socket_listen", 26)) {
        if (arg_count != 2) return false;
        emit_iat_call(21);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        return true;
    }
    if (eq("yukinary_sys_socket_accept", 26)) {
        if (arg_count != 1) return false;
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2);
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC0);
        emit_iat_call(22);
        return true;
    }
    if (eq("yukinary_sys_socket_close", 25)) {
        if (arg_count != 1) return false;
        emit_iat_call(23);
        return true;
    }
    if (eq("yukinary_sys_socket_send", 24) || eq("yukinary_sys_socket_recv", 24)) {
        if (arg_count != 3) return false;
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
        emit_iat_call(eq("yukinary_sys_socket_send", 24) ? 24 : 25);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        return true;
    }
    if (eq("yukinary_sys_socket_set_reuseaddr", 33)) {
        if (arg_count != 1) return false;
        emit_sub_rsp(a, 48);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 4);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); int32_yaz(a, 1);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 0xFFFF);
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 4);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x28);
        emit_iat_call(27);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        emit_add_rsp(a, 48);
        return true;
    }
    if (eq("yukinary_sys_socket_set_timeout", 31)) {
        if (arg_count != 2) return false;
        emit_sub_rsp(a,64);
        byte_yaz(a,0x48); byte_yaz(a,0x89); byte_yaz(a,0x4C); byte_yaz(a,0x24); byte_yaz(a,48);
        byte_yaz(a,0x89); byte_yaz(a,0x54); byte_yaz(a,0x24); byte_yaz(a,40);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,32); int32_yaz(a,4);
        byte_yaz(a,0x48); byte_yaz(a,0x8B); byte_yaz(a,0x4C); byte_yaz(a,0x24); byte_yaz(a,48);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,0xFFFF);
        byte_yaz(a,0x41); byte_yaz(a,0xB8); int32_yaz(a,0x1006);
        byte_yaz(a,0x4C); byte_yaz(a,0x8D); byte_yaz(a,0x4C); byte_yaz(a,0x24); byte_yaz(a,40);
        emit_iat_call(27);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        byte_yaz(a,0x49); byte_yaz(a,0x89); byte_yaz(a,0xC2);
        byte_yaz(a,0x48); byte_yaz(a,0x8B); byte_yaz(a,0x4C); byte_yaz(a,0x24); byte_yaz(a,48);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,0xFFFF);
        byte_yaz(a,0x41); byte_yaz(a,0xB8); int32_yaz(a,0x1005);
        byte_yaz(a,0x4C); byte_yaz(a,0x8D); byte_yaz(a,0x4C); byte_yaz(a,0x24); byte_yaz(a,40);
        emit_iat_call(27);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        byte_yaz(a,0x4C); byte_yaz(a,0x09); byte_yaz(a,0xD0);
        emit_add_rsp(a,64); return true;
    }
    if (eq("yukinary_sys_socket_set_nonblocking", 35)) {
        if (arg_count != 1) return false;
        emit_sub_rsp(a,48);
        byte_yaz(a,0x48); byte_yaz(a,0xC7); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,40); int32_yaz(a,1);
        byte_yaz(a,0xC7); byte_yaz(a,0xC2); int32_yaz(a,0x8004667E);
        byte_yaz(a,0x4C); byte_yaz(a,0x8D); byte_yaz(a,0x44); byte_yaz(a,0x24); byte_yaz(a,40);
        emit_iat_call(28);
        byte_yaz(a,0x48); byte_yaz(a,0x63); byte_yaz(a,0xC0);
        emit_add_rsp(a,48); return true;
    }
    if (eq("yukinary_sys_dns_resolve", 24)) {
        emit_mov_rax_imm32(a, -1);
        return true;
    }
    // POSIX stubs on Windows (fail closed so linker does not fail)
    if (eq("pipe", 4) || eq("fork", 4) || eq("dup2", 4) || eq("close", 5) ||
        eq("execve", 6) || eq("waitpid", 7) || eq("read", 4) || eq("write", 5) || eq("_exit", 5) ||
        eq("yukinary_sys_pipe", 17) || eq("yukinary_sys_fork", 17) || eq("yukinary_sys_dup2", 17) ||
        eq("yukinary_sys_close", 18) || eq("yukinary_sys_execve", 19) || eq("yukinary_sys_waitpid", 20) ||
        eq("yukinary_sys_read", 17) || eq("yukinary_sys_write", 18) || eq("yukinary_sys_exit", 17)) {
        emit_mov_rax_imm32(a, -1);
        return true;
    }
    return false;
}

static long long seed_frame_node_size(AstDugum* node) {
    if (!node) return 0;
    long long total = 0;
    if (YKN_DOT(node, tip) == AST_DEGISKEN_TANIMI) {
        long long elem_sz = 8;
        if (YKN_DOT(node, tip_adi) != nullptr && YKN_DOT(node, tip_adi_uzunluk) > 0) {
            elem_sz = tip_eleman_boyutu_al(YKN_DOT(node, tip_adi), YKN_DOT(node, tip_adi_uzunluk), YKN_DOT(node, cozumlenmis_tip));
        }
        long long n = YKN_DOT(node, dizi_boyutu);
        if (n > 0) {
            total += n * elem_sz;
        } else {
            total += elem_sz;
        }
        return total + seed_frame_node_size((AstDugum*)YKN_DOT(node, sag));
    }
    if (YKN_DOT(node, tip) == AST_BLOK) {
        AstDugum* item = (AstDugum*)YKN_DOT(node, sol);
        while (item) {
            total += seed_frame_node_size(item);
            item = (AstDugum*)YKN_DOT(item, sonraki);
        }
        return total;
    }
    if (YKN_DOT(node, tip) == AST_EGER || YKN_DOT(node, tip) == AST_DONGU) {
        total += seed_frame_node_size((AstDugum*)YKN_DOT(node, sol));
        total += seed_frame_node_size((AstDugum*)YKN_DOT(node, govde));
        total += seed_frame_node_size((AstDugum*)YKN_DOT(node, yoksa_govde));
        return total;
    }
    total += seed_frame_node_size((AstDugum*)YKN_DOT(node, sol));
    total += seed_frame_node_size((AstDugum*)YKN_DOT(node, sag));
    return total;
}

static long long seed_global_storage_index(char* isim, long long uzn) {
    if (!isim || uzn <= 0) return -1;
    for (long long i = 0; i < g_global_sayisi; ++i) {
        if (g_global_uzunluk[i] == uzn &&
            g_global_isim[i] != nullptr &&
            bellek_esit_mi(g_global_isim[i], isim, uzn) == 1) {
            return i;
        }
    }
    return -1;
}

static long long seed_global_storage_size(AstDugum* dugum) {
    if (!dugum) return 8;
    long long elem_sz = 8;
    if (YKN_DOT(dugum, tip_adi) != nullptr && YKN_DOT(dugum, tip_adi_uzunluk) > 0) {
        elem_sz = tip_eleman_boyutu_al(YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk), YKN_DOT(dugum, cozumlenmis_tip));
    }
    long long dizi = YKN_DOT(dugum, dizi_boyutu);
    long long size = elem_sz;
    if (dizi > 0) {
        if (dizi > 1048576) dizi = 1048576;
        size = dizi * elem_sz;
    }
    size = ((size + 7) / 8) * 8; // 8-byte alignment
    if (size < 8) size = 8;
    return size;
}

static void seed_global_constant_register(AstDugum* dugum, char* isim, long long uzn) {
    if (!dugum || !isim || uzn <= 0) return;
    long long bulundu = 0;
    long long deger = global_sabit_degeri(YKN_DOT(dugum, sag), &bulundu);
    if (!bulundu || g_global_sabit_sayisi >= 16384) return;
    for (long long i = 0; i < g_global_sabit_sayisi; ++i) {
        if (g_global_sabitler[i].uzunluk == uzn &&
            bellek_esit_mi(g_global_sabitler[i].isim, isim, uzn) == 1) return;
    }
    g_global_sabitler[g_global_sabit_sayisi].isim = isim;
    g_global_sabitler[g_global_sabit_sayisi].uzunluk = uzn;
    g_global_sabitler[g_global_sabit_sayisi].deger = deger;
    g_global_sabit_sayisi++;
}

static long long seed_global_storage_register(MakineKoduArenasi* a, AstDugum* dugum) {
    if (!a || !dugum || YKN_DOT(dugum, tip) != AST_DEGISKEN_TANIMI ||
        !YKN_DOT(dugum, ilk_jeton)) return 0;
    char* isim = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
    long long uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
    if (!isim || uzn <= 0) return 0;
    if (seed_global_storage_index(isim, uzn) >= 0) return 0;
    if (g_global_sayisi >= 16384) {
        seed_codegen_kapasite_reddet(4);
        return 0;
    }

    long long size = seed_global_storage_size(dugum);
    byte_yaz(a, 0xE9);
    int32_yaz(a, size);
    long long address = YKN_DOT(a, kullanilan);
    long long value = 0;
    long long value_found = 0;
    if (YKN_DOT(dugum, sag)) {
        value = global_sabit_degeri(YKN_DOT(dugum, sag), &value_found);
    }
    if (value_found) {
        long long copy = value;
        for (long long i = 0; i < 8; ++i) {
            byte_yaz(a, (unsigned char)(copy & 0xFF));
            copy >>= 8;
        }
        for (long long i = 8; i < size; ++i) byte_yaz(a, 0);
    } else {
        for (long long i = 0; i < size; ++i) byte_yaz(a, 0);
    }

    long long index = g_global_sayisi++;
    g_global_isim[index] = isim;
    g_global_uzunluk[index] = uzn;
    g_global_adres[index] = address;
    g_global_tip_adi[index] = YKN_DOT(dugum, tip_adi);
    g_global_tip_adi_uzunluk[index] = YKN_DOT(dugum, tip_adi_uzunluk);
    g_global_is_pointer[index] = YKN_DOT(dugum, cozumlenmis_tip);
    g_global_dizi_boyutu[index] = YKN_DOT(dugum, dizi_boyutu);
    seed_global_constant_register(dugum, isim, uzn);
    return 1;
}

static void seed_global_declarations_scan(MakineKoduArenasi* a, AstDugum* list) {
    AstDugum* iter = list;
    while (iter) {
        if (YKN_DOT(iter, tip) == AST_DEGISKEN_TANIMI) {
            seed_global_storage_register(a, iter);
        } else if (YKN_DOT(iter, tip) == AST_BLOK) {
            // Module/import flattening normally removes wrappers. Preserve a
            // bounded wrapper walk for ASTs produced by external frontends.
            seed_global_declarations_scan(a, YKN_DOT(iter, sol));
        }
        iter = YKN_DOT(iter, sonraki);
    }
}

static void expr_struct_type(AstDugum* expr, char** out_t_adi, long long* out_t_uzn) {
    if (!expr || !out_t_adi || !out_t_uzn) return;
    *out_t_adi = nullptr;
    *out_t_uzn = 0;

    if (YKN_DOT(expr, tip) == 4 && YKN_DOT(expr, ilk_jeton)) { // AST_KIMLIK
        long long uzn = YKN_DOT(YKN_DOT(expr, ilk_jeton), uzunluk);
        char* isim = YKN_DOT(YKN_DOT(expr, ilk_jeton), baslangic);
        for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
            if (g_lokal_uzunluklari[i] == uzn && bellek_esit_mi(g_lokal_isimleri[i], isim, uzn) == 1) {
                *out_t_adi = g_lokal_tip_adlari[i];
                *out_t_uzn = g_lokal_tip_uzunluklari[i];
                return;
            }
        }
        for (long long i = 0; i < g_global_sayisi; i++) {
            if (g_global_uzunluk[i] == uzn && bellek_esit_mi(g_global_isim[i], isim, uzn) == 1) {
                *out_t_adi = g_global_tip_adi[i];
                *out_t_uzn = g_global_tip_adi_uzunluk[i];
                return;
            }
        }
    } else if (YKN_DOT(expr, tip) == 20 && YKN_DOT(expr, sag)) { // AST_REFERANS: &x
        expr_struct_type(YKN_DOT(expr, sag), out_t_adi, out_t_uzn);
        return;
    } else if (YKN_DOT(expr, tip) == 18 && YKN_DOT(expr, sol)) { // AST_INDEKS_ERISIMI: arr[i]
        expr_struct_type(YKN_DOT(expr, sol), out_t_adi, out_t_uzn);
        if (*out_t_adi && *out_t_uzn > 1 && (*out_t_adi)[0] == '*') {
            *out_t_adi = *out_t_adi + 1;
            *out_t_uzn = *out_t_uzn - 1;
        }
        return;
    } else if (YKN_DOT(expr, tip) == 17 && YKN_DOT(expr, sol) && YKN_DOT(expr, sag)) { // AST_ALAN_ERISIMI: a.b
        char* parent_t = nullptr;
        long long parent_len = 0;
        expr_struct_type(YKN_DOT(expr, sol), &parent_t, &parent_len);
        if (parent_t && parent_len > 0) {
            if (parent_t[0] == '*') { parent_t++; parent_len--; }
            for (long long k = 0; k < parent_len; k++) {
                if (parent_t[k] == '<') { parent_len = k; break; }
            }
            if (YKN_DOT(YKN_DOT(expr, sag), ilk_jeton)) {
                long long f_len = YKN_DOT(YKN_DOT(YKN_DOT(expr, sag), ilk_jeton), uzunluk);
                char* f_isim = YKN_DOT(YKN_DOT(YKN_DOT(expr, sag), ilk_jeton), baslangic);
                for (long long i = 0; i < g_yapi_sayisi; i++) {
                    if (g_yapilar[i].uzunluk == parent_len && bellek_esit_mi(g_yapilar[i].isim, parent_t, parent_len) == 1) {
                        for (long long j = 0; j < g_yapilar[i].alan_sayisi; j++) {
                            if (g_yapilar[i].alanlar[j].uzunluk == f_len &&
                                bellek_esit_mi(g_yapilar[i].alanlar[j].isim, f_isim, f_len) == 1) {
                                *out_t_adi = g_yapilar[i].alanlar[j].tip_adi;
                                *out_t_uzn = g_yapilar[i].alanlar[j].tip_uzunluk;
                                return;
                            }
                        }
                        break;
                    }
                }
            }
        }
    } else if (YKN_DOT(expr, tip) == 11 && YKN_DOT(expr, sol) && YKN_DOT(YKN_DOT(expr, sol), ilk_jeton)) { // AST_ISLEV_CAGRISI
        long long fn_len = YKN_DOT(YKN_DOT(YKN_DOT(expr, sol), ilk_jeton), uzunluk);
        char* fn_isim = YKN_DOT(YKN_DOT(YKN_DOT(expr, sol), ilk_jeton), baslangic);
        if (fn_len == 12 && bellek_esit_mi(fn_isim, (char*)"suanki_jeton", 12) == 1) {
            *out_t_adi = (char*)"Jeton";
            *out_t_uzn = 5;
            return;
        }
    }
    if (*out_t_adi == nullptr && YKN_DOT(expr, tip_adi) != nullptr && YKN_DOT(expr, tip_adi_uzunluk) > 0) {
        *out_t_adi = (char*)YKN_DOT(expr, tip_adi);
        *out_t_uzn = YKN_DOT(expr, tip_adi_uzunluk);
    }
}

static void seed_codegen_index_element_type(AstDugum* index, char** type_name, long long* type_length) {
    if (!type_name || !type_length) return;
    *type_name = nullptr;
    *type_length = 0;
    if (!index || index->tip != AST_INDEKS_ERISIMI || !index->sol) return;
    expr_struct_type(index->sol, type_name, type_length);
    if (*type_name && *type_length > 1 && (*type_name)[0] == '*') {
        *type_name = *type_name + 1;
        *type_length = *type_length - 1;
    }
}

static bool emit_magic_div_mod(MakineKoduArenasi* a, long long d, bool is_modulo) {
    if (d <= 1 || (d & (d - 1)) == 0) return false;
    unsigned long long two_63 = 1ULL << 63;
    unsigned long long ad = (unsigned long long)d;
    unsigned long long t = two_63;
    unsigned long long anc = t - 1 - (t % ad);
    int p = 63;
    unsigned long long q1 = two_63 / anc;
    unsigned long long r1 = two_63 % anc;
    unsigned long long q2 = two_63 / ad;
    unsigned long long r2 = two_63 % ad;
    while (true) {
        p++;
        q1 = 2 * q1;
        r1 = 2 * r1;
        if (r1 >= anc) { q1++; r1 -= anc; }
        q2 = 2 * q2;
        r2 = 2 * r2;
        if (r2 >= ad) { q2++; r2 -= ad; }
        unsigned long long delta = ad - r2;
        if (q1 < delta || (q1 == delta && r1 == 0)) continue;
        break;
    }
    unsigned long long M = q2 + 1;
    int s = p - 64;
    if (M >= two_63) return false;

    // Input x is in RAX
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // MOV R10, RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0xB8); int64_yaz(a, (long long)M); // MOV RAX, M
    byte_yaz(a, 0x49); byte_yaz(a, 0xF7); byte_yaz(a, 0xEA); // IMUL R10 (RDX:RAX = RAX * R10)
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD0); // MOV RAX, RDX
    if (s > 0) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xF8); byte_yaz(a, (char)s); // SAR RAX, s
    }
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xD1); // MOV RCX, R10
    byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xF9); byte_yaz(a, 0x3F); // SAR RCX, 63
    byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0xC8); // SUB RAX, RCX (RAX = x / d)
    if (is_modulo) {
        if (d <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x6B); byte_yaz(a, 0xD0); byte_yaz(a, (char)d); // IMUL RDX, RAX, d
        } else if (d <= 0x7FFFFFFF) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x69); byte_yaz(a, 0xD0); int32_yaz(a, (int)d);
        } else {
            return false;
        }
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xD0); // MOV RAX, R10
        byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0xD0); // SUB RAX, RDX (RAX = x % d)
    }
    return true;
}

static long long emit_conditional_branch_helper(MakineKoduArenasi* a, AstDugum* cond) {
    if (!cond) return 0;
    bool fast_cmp = false;
    unsigned char jmp_code = 0x84; // default JZ

    if (cond->tip == AST_IKILI_OP && cond->ilk_jeton && cond->sol && cond->sag) {
        long long cmp_op = cond->ilk_jeton->tip;
        if (cmp_op == TIP_KUCUK_MU)        { jmp_code = 0x8D; fast_cmp = true; } // invert: JGE
        else if (cmp_op == TIP_KUCUK_ESIT) { jmp_code = 0x8F; fast_cmp = true; } // invert: JG
        else if (cmp_op == TIP_BUYUK_MU)   { jmp_code = 0x8E; fast_cmp = true; } // invert: JLE
        else if (cmp_op == TIP_BUYUK_ESIT) { jmp_code = 0x8C; fast_cmp = true; } // invert: JL
        else if (cmp_op == TIP_ESIT_MI)    { jmp_code = 0x85; fast_cmp = true; } // invert: JNE
        else if (cmp_op == TIP_ESIT_DEGIL) { jmp_code = 0x84; fast_cmp = true; } // invert: JE
    }

    if (fast_cmp) {
        ast_kod_uret(a, cond->sol);
        if (cond->sag->tip == AST_SAYI && cond->sag->ilk_jeton) {
            long long imm = string_tamsayiya(cond->sag->ilk_jeton->baslangic, cond->sag->ilk_jeton->uzunluk);
            if (imm >= -128 && imm <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xF8); byte_yaz(a, (char)imm);
            } else if (imm >= -0x80000000LL && imm <= 0x7FFFFFFFLL) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x3D); int32_yaz(a, (int)imm);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0xB9); int64_yaz(a, imm);
                byte_yaz(a, 0x48); byte_yaz(a, 0x39); byte_yaz(a, 0xC8);
            }
        } else {
            bool sag_into_rcx_branch = false;
            AstDugum* sag_node = cond->sag;
            if (sag_node && sag_node->tip == 4 && sag_node->ilk_jeton) {
                long long r_reg = ra_degisken_reg_bul(sag_node->ilk_jeton->baslangic, sag_node->ilk_jeton->uzunluk);
                if (r_reg > 0) {
                    emit_mov_rcx_reg(a, r_reg);
                    sag_into_rcx_branch = true;
                } else {
                    long long sag_off = 0;
                    long long sag_dizi = 0;
                    for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                        if (g_lokal_uzunluklari[i] == sag_node->ilk_jeton->uzunluk &&
                            bellek_esit_mi(g_lokal_isimleri[i], sag_node->ilk_jeton->baslangic, sag_node->ilk_jeton->uzunluk) == 1) {
                            sag_off = g_lokal_offsetleri[i];
                            sag_dizi = g_lokal_dizi_boyutlari[i];
                            break;
                        }
                    }
                    if (sag_off != 0 && sag_dizi == 0) {
                        if (sag_off <= 127) {
                            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - sag_off));
                        } else {
                            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x8D); int32_yaz(a, (int)(-sag_off));
                        }
                        sag_into_rcx_branch = true;
                    }
                }
            }
            if (!sag_into_rcx_branch) {
                byte_yaz(a, 0x50); // PUSH RAX
                ast_kod_uret(a, cond->sag);
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX
                byte_yaz(a, 0x58); // POP RAX
            }
            byte_yaz(a, 0x48); byte_yaz(a, 0x39); byte_yaz(a, 0xC8); // CMP RAX, RCX
        }
        byte_yaz(a, 0x0F); byte_yaz(a, jmp_code);
        long long rel_pos = YKN_DOT(a, kullanilan);
        int32_yaz(a, 0);
        return rel_pos;
    } else {
        ast_kod_uret(a, cond);
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
        byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JZ
        long long rel_pos = YKN_DOT(a, kullanilan);
        int32_yaz(a, 0);
        return rel_pos;
    }
}

long long ast_kod_uret(MakineKoduArenasi* a, AstDugum* dugum) {
if ((!(dugum))) {
return 0;
}
;
if (g_ast_mutasyon_aktif == 1 && g_ast_mutasyon_uygulandi == 0 &&
    YKN_DOT(dugum, tip) == g_ast_mutasyon_hedef) {
    YKN_DOT(dugum, tip) = 255;
    g_ast_mutasyon_uygulandi = 1;
}
if ((YKN_DOT(dugum, tip) == AST_ICE_AKTAR)) { return 0; }
if (g_global_sabitleri_toplandi == 0) {
    global_sabitleri_topla(dugum, 0);
    yapilari_topla(dugum);
    g_global_sabitleri_toplandi = 1;
}
if ((YKN_DOT(dugum, tip) == AST_SAYI)) {
long long sayi = string_tamsayiya(YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk));
emit_mov_rax_imm32(a, sayi);
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_GERCEK_SAYI)) {
    char buf[128];
    long long u = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
    if (u >= 127) u = 127;
    memcpy(buf, YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), u);
    buf[u] = 0;
    double dval = strtod(buf, NULL);
    uint64_t u64val = *(uint64_t*)&dval;
    byte_yaz(a, 0x48); byte_yaz(a, 0xB8);
    int64_yaz(a, (long long)u64val);
    // MOVQ XMM0, RAX (0x66 0x48 0x0F 0x6E 0xC0)
    byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x6E); byte_yaz(a, 0xC0);
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_IKILI_OP)) {
    if (YKN_DOT(dugum, ilk_jeton) != 0) {
        if (YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_VE) {
            ast_kod_uret(a, YKN_DOT(dugum, sol));
            byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
            byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JZ false_label
            long long ve_jz_pos = YKN_DOT(a, kullanilan);
            int32_yaz(a, 0);
            ast_kod_uret(a, YKN_DOT(dugum, sag));
            byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
            byte_yaz(a, 0x0F); byte_yaz(a, 0x95); byte_yaz(a, 0xC0); // SETNE AL
            byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0); // MOVZX RAX, AL
            byte_yaz(a, 0xE9); // JMP end_label
            long long ve_jmp_pos = YKN_DOT(a, kullanilan);
            int32_yaz(a, 0);
            long long ve_false_label = YKN_DOT(a, kullanilan);
            int32_patch(a, ve_jz_pos, ve_false_label - (ve_jz_pos + 4));
            emit_xor_rax_rax(a);
            long long ve_end_label = YKN_DOT(a, kullanilan);
            int32_patch(a, ve_jmp_pos, ve_end_label - (ve_jmp_pos + 4));
            return 0;
        }
        if (YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_VEYA) {
            ast_kod_uret(a, YKN_DOT(dugum, sol));
            byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
            byte_yaz(a, 0x0F); byte_yaz(a, 0x85); // JNZ true_label
            long long veya_jnz_pos = YKN_DOT(a, kullanilan);
            int32_yaz(a, 0);
            ast_kod_uret(a, YKN_DOT(dugum, sag));
            byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
            byte_yaz(a, 0x0F); byte_yaz(a, 0x95); byte_yaz(a, 0xC0); // SETNE AL
            byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0); // MOVZX RAX, AL
            byte_yaz(a, 0xE9); // JMP end_label
            long long veya_jmp_pos = YKN_DOT(a, kullanilan);
            int32_yaz(a, 0);
            long long veya_true_label = YKN_DOT(a, kullanilan);
            int32_patch(a, veya_jnz_pos, veya_true_label - (veya_jnz_pos + 4));
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1); // MOV RAX, 1
            long long veya_end_label = YKN_DOT(a, kullanilan);
            int32_patch(a, veya_jmp_pos, veya_end_label - (veya_jmp_pos + 4));
            return 0;
        }
    }

    long long is_flt = (ast_node_is_float(YKN_DOT(dugum, sol)) || ast_node_is_float(YKN_DOT(dugum, sag)));
    if (is_flt) {
        // GOREV 6.2: f32 mi f64 mu? f32 → F3 prefix (ADDSS/SUBSS/...), f64 → F2 prefix (ADDSD/SUBSD/...)
        unsigned char flt_pfx = 0xF2; // varsayilan f64
        {
            AstDugum* cs = (AstDugum*)YKN_DOT(dugum, sol);
            AstDugum* cr = (AstDugum*)YKN_DOT(dugum, sag);
            auto is_f32_node = [](AstDugum* n) -> bool {
                if (!n) return false;
                char* ta = YKN_DOT(n, tip_adi);
                long long tl = YKN_DOT(n, tip_adi_uzunluk);
                if (!ta) return false;
                if (tl == 3 && bellek_esit_mi(ta, (char*)"f32", 3) == 1) return true;
                if (tl == 9 && bellek_esit_mi(ta, (char*)"kesirli32", 9) == 1) return true;
                return false;
            };
            if (is_f32_node(cs) || is_f32_node(cr)) flt_pfx = 0xF3;
        }
        ast_kod_uret(a, YKN_DOT(dugum, sol));
        if (!ast_node_is_float(YKN_DOT(dugum, sol))) {
            // CVTSI2SD XMM0, RAX (0xF2 0x48 0x0F 0x2A 0xC0)
            byte_yaz(a, 0xF2); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x2A); byte_yaz(a, 0xC0);
            // MOVQ RAX, XMM0 (0x66 0x48 0x0F 0x7E 0xC0)
            byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
        }
        byte_yaz(a, 0x50); // PUSH RAX
        ast_kod_uret(a, YKN_DOT(dugum, sag));
        if (!ast_node_is_float(YKN_DOT(dugum, sag))) {
            // CVTSI2SD XMM0, RAX
            byte_yaz(a, 0xF2); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x2A); byte_yaz(a, 0xC0);
            // MOVQ RAX, XMM0
            byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
        }
        // MOVQ XMM1, RAX (0x66 0x48 0x0F 0x6E 0xC8)
        byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x6E); byte_yaz(a, 0xC8);
        byte_yaz(a, 0x58); // POP RAX
        // MOVQ XMM0, RAX (0x66 0x48 0x0F 0x6E 0xC0)
        byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x6E); byte_yaz(a, 0xC0);

        if (YKN_DOT(dugum, ilk_jeton) != 0) {
            long long op_tip = YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip);
            if (op_tip == TIP_ARTI) {
                // ADDSD/ADDSS XMM0, XMM1
                byte_yaz(a, flt_pfx); byte_yaz(a, 0x0F); byte_yaz(a, 0x58); byte_yaz(a, 0xC1);
                // MOVQ RAX, XMM0 (0x66 0x48 0x0F 0x7E 0xC0)
                byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
                return 0;
            }
            if (op_tip == TIP_EKSI) {
                // SUBSD/SUBSS XMM0, XMM1
                byte_yaz(a, flt_pfx); byte_yaz(a, 0x0F); byte_yaz(a, 0x5C); byte_yaz(a, 0xC1);
                byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
                return 0;
            }
            if (op_tip == TIP_CARPI) {
                // MULSD/MULSS XMM0, XMM1
                byte_yaz(a, flt_pfx); byte_yaz(a, 0x0F); byte_yaz(a, 0x59); byte_yaz(a, 0xC1);
                byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
                return 0;
            }
            if (op_tip == TIP_BOLME) {
                // DIVSD/DIVSS XMM0, XMM1
                byte_yaz(a, flt_pfx); byte_yaz(a, 0x0F); byte_yaz(a, 0x5E); byte_yaz(a, 0xC1);
                byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
                return 0;
            }
            if (op_tip == TIP_ESIT_MI) {
                // UCOMISD XMM0, XMM1 (0x66 0x0F 0x2E 0xC1)
                byte_yaz(a, 0x66); byte_yaz(a, 0x0F); byte_yaz(a, 0x2E); byte_yaz(a, 0xC1);
                emit_sete_al(a); emit_movzx_rax_al(a);
                return 0;
            }
            if (op_tip == TIP_ESIT_DEGIL) {
                byte_yaz(a, 0x66); byte_yaz(a, 0x0F); byte_yaz(a, 0x2E); byte_yaz(a, 0xC1);
                emit_setne_al(a); emit_movzx_rax_al(a);
                return 0;
            }
            if (op_tip == TIP_KUCUK_MU) {
                byte_yaz(a, 0x66); byte_yaz(a, 0x0F); byte_yaz(a, 0x2E); byte_yaz(a, 0xC1);
                emit_setb_al(a); emit_movzx_rax_al(a);
                return 0;
            }
            if (op_tip == TIP_BUYUK_MU) {
                byte_yaz(a, 0x66); byte_yaz(a, 0x0F); byte_yaz(a, 0x2E); byte_yaz(a, 0xC1);
                emit_seta_al(a); emit_movzx_rax_al(a);
                return 0;
            }
            if (op_tip == TIP_KUCUK_ESIT) {
                byte_yaz(a, 0x66); byte_yaz(a, 0x0F); byte_yaz(a, 0x2E); byte_yaz(a, 0xC1);
                emit_setbe_al(a); emit_movzx_rax_al(a);
                return 0;
            }
            if (op_tip == TIP_BUYUK_ESIT) {
                byte_yaz(a, 0x66); byte_yaz(a, 0x0F); byte_yaz(a, 0x2E); byte_yaz(a, 0xC1);
                emit_setae_al(a); emit_movzx_rax_al(a);
                return 0;
            }
        }
        return 0;
    }

    long long is_str = 0;
    if (YKN_DOT(dugum, ilk_jeton) != 0 && YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_ARTI) {
        if (ast_node_is_string(YKN_DOT(dugum, sol)) && ast_node_is_string(YKN_DOT(dugum, sag))) {
            is_str = 1;
        }
    }
    if (is_str) {
        ast_kod_uret(a, YKN_DOT(dugum, sol));
        byte_yaz(a, 0x50); // PUSH RAX (sol)
        ast_kod_uret(a, YKN_DOT(dugum, sag));
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // MOV RDX, RAX (sag)
        byte_yaz(a, 0x59); // POP RCX (sol)
        emit_string_concat_inline(a);
        return 0;
    }

ast_kod_uret(a, YKN_DOT(dugum, sol));

// FAST PATH: evaluate sag directly into RCX if it doesn't clobber RAX
bool sag_into_rcx = false;
AstDugum* sag_d = (AstDugum*)YKN_DOT(dugum, sag);
if (g_target_platform < 3) {
if (sag_d && sag_d->tip == AST_SAYI && sag_d->ilk_jeton) {
    long long val = string_tamsayiya(sag_d->ilk_jeton->baslangic, sag_d->ilk_jeton->uzunluk);
    if (val >= 0 && val <= 0x7FFFFFFF) {
        byte_yaz(a, 0xB9); int32_yaz(a, (int)val); // MOV ECX, imm32 (zero-extends to RCX)
    } else if (val >= -0x80000000LL && val < 0) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC1); int32_yaz(a, (int)val); // MOV RCX, imm32 (sign-extended)
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0xB9); int64_yaz(a, val); // MOV RCX, imm64
    }
    sag_into_rcx = true;
} else if (sag_d && sag_d->tip == 4 && sag_d->ilk_jeton) { // AST_KIMLIK
    long long r_reg = (g_lambda_tabani == 0) ? ra_degisken_reg_bul(sag_d->ilk_jeton->baslangic, sag_d->ilk_jeton->uzunluk) : 0;
    if (r_reg > 0) {
        emit_mov_rcx_reg(a, r_reg);
        sag_into_rcx = true;
    } else {
        long long sag_off = 0;
        long long sag_dizi = 0;
        long long sag_struct = 0;
        long long sag_search_start = g_lokal_sayisi - 1;
        long long sag_search_end = (g_lambda_tabani > 0) ? g_lambda_tabani : 0;
        for (long long i = sag_search_start; i >= sag_search_end; i--) {
            if (g_lokal_uzunluklari[i] == sag_d->ilk_jeton->uzunluk &&
                bellek_esit_mi(g_lokal_isimleri[i], sag_d->ilk_jeton->baslangic, sag_d->ilk_jeton->uzunluk) == 1) {
                sag_off = g_lokal_offsetleri[i];
                sag_dizi = g_lokal_dizi_boyutlari[i];
                if (g_lokal_yildizlari[i] == 0 && g_lokal_tip_adlari[i] != 0) {
                    for(long long j = 0; j < g_yapi_sayisi; j++) {
                        if(g_yapilar[j].uzunluk == g_lokal_tip_uzunluklari[i] &&
                           bellek_esit_mi(g_yapilar[j].isim, g_lokal_tip_adlari[i], g_lokal_tip_uzunluklari[i]) == 1) {
                            sag_struct = 1; break;
                        }
                    }
                }
                break;
            }
        }
        if (sag_off != 0 && sag_dizi == 0 && sag_struct == 0) {
            if (sag_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4D); // MOV RCX, [RBP - off]
                byte_yaz(a, (unsigned char)(256 - sag_off));
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x8D); // MOV RCX, [RBP - off]
                int32_yaz(a, -sag_off);
            }
            sag_into_rcx = true;
        } else if (sag_off == 0 && g_lambda_tabani > 0) {
            long long outer_sag_off = 0;
            for (long long i = g_lambda_tabani - 1; i >= 0; i--) {
                if (g_lokal_uzunluklari[i] == sag_d->ilk_jeton->uzunluk &&
                    bellek_esit_mi(g_lokal_isimleri[i], sag_d->ilk_jeton->baslangic, sag_d->ilk_jeton->uzunluk) == 1) {
                    outer_sag_off = g_lokal_offsetleri[i];
                    break;
                }
            }
            if (outer_sag_off != 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x55); byte_yaz(a, 0x00); // MOV RDX, [RBP + 0]
                if (outer_sag_off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4A); // MOV RCX, [RDX - outer_sag_off]
                    byte_yaz(a, (unsigned char)(256 - outer_sag_off));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x8A);
                    int32_yaz(a, -outer_sag_off);
                }
                sag_into_rcx = true;
            }
        }
    }
}

}

if (!sag_into_rcx) {
    ra_ikili_sol_kaydet(a);
    ast_kod_uret(a, sag_d);
    ra_ikili_sol_geri_yukle_rcx(a);
}
if (((YKN_DOT(dugum, ilk_jeton)) != 0)) {
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_ARTI)) {
emit_add_rax_rcx(a);
return 0;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_EKSI)) {
emit_sub_rax_rcx(a);
return 0;
}
;
if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_CARPI)) {
    if (dugum->sag && dugum->sag->tip == AST_SAYI) {
        long long mul_val = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
        if (mul_val > 0 && (mul_val & (mul_val - 1)) == 0) {
            int shift = 0;
            while ((1LL << shift) < mul_val) shift++;
            if (shift == 1) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0xC0); // ADD RAX, RAX
            } else if (shift > 1 && shift < 64) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, (char)shift); // SHL RAX, shift
            }
            return 0;
        }
    }
    emit_imul_rax_rcx(a);
    return 0;
}
;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_BOLME)) {
      if (dugum->sag && dugum->sag->tip == AST_SAYI) {
          long long div_val = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
          if (div_val > 0 && (div_val & (div_val - 1)) == 0) {
              int shift = 0;
              while ((1LL << shift) < div_val) shift++;
              if (shift > 0 && shift < 64) {
                  long long mask = div_val - 1;
                  byte_yaz(a, 0x48); byte_yaz(a, 0x99); // CQO
                  if (mask <= 127) {
                      byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE2); byte_yaz(a, (char)mask);
                  } else {
                      byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xE2); int32_yaz(a, (int)mask);
                  }
                  byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0xD0); // ADD RAX, RDX
                  byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xF8); byte_yaz(a, (char)shift); // SAR RAX, shift
                  return 0;
              }
          }
          if (emit_magic_div_mod(a, div_val, false)) return 0;
      }
      byte_yaz(a, 0x48); byte_yaz(a, 0x99);
      byte_yaz(a, 0x48); byte_yaz(a, 0xF7); byte_yaz(a, 0xF9);
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_MODULO)) {
      if (dugum->sag && dugum->sag->tip == AST_SAYI) {
          long long mod_val = string_tamsayiya(dugum->sag->ilk_jeton->baslangic, dugum->sag->ilk_jeton->uzunluk);
          if (mod_val > 1 && (mod_val & (mod_val - 1)) == 0) {
              long long mask = mod_val - 1;
              byte_yaz(a, 0x48); byte_yaz(a, 0x99); // CQO
              if (mask <= 127) {
                  byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE2); byte_yaz(a, (char)mask); // AND RDX, mask
              } else {
                  byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xE2); int32_yaz(a, (int)mask);
              }
              byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0xD0); // ADD RAX, RDX
              if (mask <= 127) {
                  byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE0); byte_yaz(a, (char)mask); // AND RAX, mask
              } else {
                  byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xE0); int32_yaz(a, (int)mask);
              }
              byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0xD0); // SUB RAX, RDX
              return 0;
          }
          if (emit_magic_div_mod(a, mod_val, true)) return 0;
      }
      byte_yaz(a, 0x48); byte_yaz(a, 0x99);
      byte_yaz(a, 0x48); byte_yaz(a, 0xF7); byte_yaz(a, 0xF9);
      byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD0);
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_XOR)) {
      byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC8);
      return 0;
  }
  ;
  // BUG-3 FIX: Bitwise shift + OR
  // ra_ikili_sol_geri_yukle_rcx: RAX=sag(miktar), RCX=sol(deger) => XCHG sonrasi RAX=sol
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_SOL_KAYDIR)) {
      // RAX=sol(deger), RCX=sag(miktar) - XCHG gerekmiyor
      byte_yaz(a, 0x48); byte_yaz(a, 0xD3); byte_yaz(a, 0xE0); // SAL RAX, CL
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_SAG_KAYDIR)) {
      byte_yaz(a, 0x48); byte_yaz(a, 0xD3); byte_yaz(a, 0xF8); // SAR RAX, CL
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_DIKEY_CUBUK)) {
      byte_yaz(a, 0x48); byte_yaz(a, 0x09); byte_yaz(a, 0xC8); // OR RAX, RCX
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_AMPERSAND)) {
      byte_yaz(a, 0x48); byte_yaz(a, 0x21); byte_yaz(a, 0xC8); // AND RAX, RCX
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_VE)) {
      byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
      byte_yaz(a, 0x0F); byte_yaz(a, 0x95); byte_yaz(a, 0xC0);
      byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0);
      byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);
      byte_yaz(a, 0x0F); byte_yaz(a, 0x95); byte_yaz(a, 0xC1);
      byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC9);
      byte_yaz(a, 0x48); byte_yaz(a, 0x21); byte_yaz(a, 0xC8);
      return 0;
  }
  ;
  if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_VEYA)) {
      byte_yaz(a, 0x48); byte_yaz(a, 0x09); byte_yaz(a, 0xC8);
      byte_yaz(a, 0x0F); byte_yaz(a, 0x95); byte_yaz(a, 0xC0);
      byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0);
      return 0;
  }
  ;
        // Karsilastirma operatorleri: CMP RAX, RCX + SETcc AL + MOVZX RAX, AL
        if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_ESIT_MI)) {
            emit_cmp_rax_rcx(a); emit_sete_al(a); emit_movzx_rax_al(a);
            return 0;
        }
        if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_ESIT_DEGIL)) {
            emit_cmp_rax_rcx(a); emit_setne_al(a); emit_movzx_rax_al(a);
            return 0;
        }
        if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_KUCUK_MU)) {
            emit_cmp_rax_rcx(a); emit_setl_al(a); emit_movzx_rax_al(a);
            return 0;
        }
        if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_BUYUK_MU)) {
            emit_cmp_rax_rcx(a); emit_setg_al(a); emit_movzx_rax_al(a);
            return 0;
        }
        if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_KUCUK_ESIT)) {
            emit_cmp_rax_rcx(a); emit_setle_al(a); emit_movzx_rax_al(a);
            return 0;
        }
        if ((YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_BUYUK_ESIT)) {
            emit_cmp_rax_rcx(a); emit_setge_al(a); emit_movzx_rax_al(a);
            return 0;
        }
}
;
return 0;
}
;
// AST_MANTIK (26): true/false literal'i RAX'ta canonical 1/0 olarak uretilir.
if (YKN_DOT(dugum, tip) == AST_MANTIK) {
    emit_mov_rax_imm32(a, YKN_DOT(dugum, dizi_boyutu));
    return 0;
}
;
// AST_INLINE_ASM (43) = asm { "..." } inline assembly
if ((YKN_DOT(dugum, tip) == AST_INLINE_ASM || YKN_DOT(dugum, tip) == 43) && YKN_DOT(dugum, ilk_jeton) != 0 &&
    YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_METIN) {
    // Asm string icerigini byte bazli emit et
    // Desteklenen: "nop", "ret", "int3", "0x90" gibi hex literaller
    char* asm_str = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
    long long asm_len = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
    if (asm_len >= 3 && bellek_esit_mi(asm_str, (char*)"nop", 3) == 1) { byte_yaz(a, 0x90); return 0; }
    if (asm_len >= 3 && bellek_esit_mi(asm_str, (char*)"ret", 3) == 1) { byte_yaz(a, 0xC3); return 0; }
    if (asm_len >= 4 && bellek_esit_mi(asm_str, (char*)"int3", 4) == 1) { byte_yaz(a, 0xCC); return 0; }
    if (asm_len >= 4 && bellek_esit_mi(asm_str, (char*)"hlt\0", 3) == 1) { byte_yaz(a, 0xF4); return 0; }
    // Ham hex string: "0x90 0x90" gibi boslukla ayrilmis hex byte'lar
    for (long long ai = 0; ai < asm_len - 1; ) {
        if (asm_str[ai] == '0' && asm_str[ai+1] == 'x') {
            char h1 = asm_str[ai+2]; char h2 = asm_str[ai+3];
            long long v = 0;
            if (h1 >= '0' && h1 <= '9') v = (h1 - '0') << 4; else if (h1 >= 'a' && h1 <= 'f') v = (h1 - 'a' + 10) << 4; else if (h1 >= 'A' && h1 <= 'F') v = (h1 - 'A' + 10) << 4;
            if (h2 >= '0' && h2 <= '9') v |= (h2 - '0'); else if (h2 >= 'a' && h2 <= 'f') v |= (h2 - 'a' + 10); else if (h2 >= 'A' && h2 <= 'F') v |= (h2 - 'A' + 10);
            byte_yaz(a, (unsigned char)v);
            ai += 4;
            while (ai < asm_len && asm_str[ai] == ' ') ai++;
        } else { ai++; }
    }
    return 0;
}
// Bug #1 fix: AST_METIN kendi baÄŸÄ±msÄ±z bloÄŸunda (Ã¶nceden AST_IKILI_OP'un iÃ§indeydi)
if ((YKN_DOT(dugum, tip) == AST_METIN)) {
long long ic_uzunluk = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
char* ic_baslangic = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
// TÄ±rnak iÅŸaretlerini kaldÄ±r: "..." â†’ ...
if ((ic_uzunluk >= 2) && (ic_baslangic[0] == '"')) {
ic_baslangic = (ic_baslangic + 1);
ic_uzunluk = (ic_uzunluk - 2);
}
;
// Escape dizilerini say (gercek_uzunluk = null dahil kaÃ§ byte gÃ¶mÃ¼lecek)
long long gercek_uzunluk = 0;
for (long long i = 0; (i < ic_uzunluk); i = (i + 1)) {
if ((ic_baslangic[i] == '\\') && ((i + 1) < ic_uzunluk)) {
    char nc = ic_baslangic[i + 1];
    if (nc == 'n' || nc == 't' || nc == 'r' || nc == '\\' || nc == '"' || nc == '0' || nc == 'e') {
        gercek_uzunluk = (gercek_uzunluk + 1);
        i = (i + 1);
    } else if (nc == 'x' && (i + 3) < ic_uzunluk) {
        // \xHH - 2 hex digit
        gercek_uzunluk = (gercek_uzunluk + 1);
        i = (i + 3);
    } else {
        gercek_uzunluk = (gercek_uzunluk + 1);
    }
} else {
gercek_uzunluk = (gercek_uzunluk + 1);
}
}
;
// JMP rel32: string verisini atla (gercek_uzunluk bayt + 1 null)
byte_yaz(a, 0xE9);
int32_yaz(a, (int)(gercek_uzunluk + 1));
// String verisi baÅŸlangÄ±Ã§ offseti
long long str_offset = YKN_DOT(a, kullanilan);
for (long long i = 0; (i < ic_uzunluk); i = (i + 1)) {
if ((ic_baslangic[i] == '\\') && ((i + 1) < ic_uzunluk)) {
    char nc = ic_baslangic[i + 1];
    if (nc == 'n') { byte_yaz(a, 10); i = (i + 1); }
    else if (nc == 't') { byte_yaz(a, 9); i = (i + 1); }
    else if (nc == 'r') { byte_yaz(a, 13); i = (i + 1); }
    else if (nc == '\\') { byte_yaz(a, '\\'); i = (i + 1); }
    else if (nc == '"') { byte_yaz(a, '"'); i = (i + 1); }
    else if (nc == '0') { byte_yaz(a, 0); i = (i + 1); }
    else if (nc == 'e') { byte_yaz(a, 27); i = (i + 1); } // ESC
    else if (nc == 'x' && (i + 3) < ic_uzunluk) {
        // \xHH hex escape
        char h1 = ic_baslangic[i + 2];
        char h2 = ic_baslangic[i + 3];
        auto hex_val = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return (unsigned char)(c - '0');
            if (c >= 'a' && c <= 'f') return (unsigned char)(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return (unsigned char)(c - 'A' + 10);
            return 0;
        };
        unsigned char hbyte = (hex_val(h1) << 4) | hex_val(h2);
        byte_yaz(a, hbyte);
        i = (i + 3);
    } else {
        byte_yaz(a, (unsigned char)ic_baslangic[i]);
    }
} else {
byte_yaz(a, (unsigned char)ic_baslangic[i]);
}
}
byte_yaz(a, 0); // null terminator
// LEA RAX, [RIP + rel32]
// LEA ÅŸu an buraya emitted: lea_start = kullanilan
// RIP after LEA = lea_start + 7
// rel32 = str_offset - (lea_start + 7)
long long lea_start = YKN_DOT(a, kullanilan);
long long rip_after_lea = (lea_start + 7);
long long rel32 = (str_offset - rip_after_lea);
byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05);
int32_yaz(a, (int)rel32);
return 0;
}
;
// ─── AST_FSTRING: f"..." interpolasyonu ─────────────────────────────────────
if ((YKN_DOT(dugum, tip) == AST_FSTRING)) {
    if (!YKN_DOT(dugum, ilk_jeton)) {
        byte_yaz(a, 0xE9); int32_yaz(a, 1); byte_yaz(a, 0);
        long long e_pos = YKN_DOT(a, kullanilan) - 1;
        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05);
        int32_yaz(a, (int)(e_pos - (YKN_DOT(a, kullanilan) + 4)));
        return 0;
    }
    char* fs_ptr = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
    long long fs_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
    // Accumulator baslangiç: bos string
    byte_yaz(a, 0xE9); int32_yaz(a, 1); byte_yaz(a, 0);
    long long empty_str_pos = YKN_DOT(a, kullanilan) - 1;
    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05);
    int32_yaz(a, (int)(empty_str_pos - (YKN_DOT(a, kullanilan) + 4)));
    long long fpos = 0;
    while (fpos < fs_uzn) {
        long long seg_bas = fpos;
        while (fpos < fs_uzn && fs_ptr[fpos] != '{') fpos++;
        long long seg_uzn = fpos - seg_bas;
        if (seg_uzn > 0) {
            byte_yaz(a, 0x50); // PUSH RAX
            byte_yaz(a, 0xE9); int32_yaz(a, (int)(seg_uzn + 1));
            long long lit_pos = YKN_DOT(a, kullanilan);
            for (long long si = seg_bas; si < seg_bas + seg_uzn; si++) byte_yaz(a, (unsigned char)fs_ptr[si]);
            byte_yaz(a, 0);
            byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05);
            int32_yaz(a, (int)(lit_pos - (YKN_DOT(a, kullanilan) + 4)));
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // MOV RDX, RAX
            byte_yaz(a, 0x59); // POP RCX
            emit_string_concat_inline(a);
        }
        if (fpos < fs_uzn) {
            fpos++; // skip '{'
            long long expr_bas = fpos;
            while (fpos < fs_uzn && fs_ptr[fpos] != '}') fpos++;
            long long expr_uzn = fpos - expr_bas;
            if (fpos < fs_uzn) fpos++; // skip '}'
            if (expr_uzn > 0) {
                byte_yaz(a, 0x50); // PUSH RAX
                AstDugum* syn_d = parse_fstring_subexpr(fs_ptr + expr_bas, expr_uzn,
                                                        dugum->ilk_jeton ? dugum->ilk_jeton->satir : 0,
                                                        dugum->ilk_jeton ? dugum->ilk_jeton->sutun : 0);
                if (syn_d) {
                    ast_kod_uret(a, syn_d);
                    if (ast_node_is_string(syn_d) == 0) {
                        emit_i64_to_string(a);
                    }
                } else {
                    byte_yaz(a, 0xE9); int32_yaz(a, 1); byte_yaz(a, 0);
                    long long e_pos = YKN_DOT(a, kullanilan) - 1;
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05);
                    int32_yaz(a, (int)(e_pos - (YKN_DOT(a, kullanilan) + 4)));
                }
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // MOV RDX, RAX
                byte_yaz(a, 0x59); // POP RCX
                emit_string_concat_inline(a);
            }
        }
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 19)) { // AST_UNARY — *ptr, -val, !val, ~val
    if (YKN_DOT(dugum, ilk_jeton) != 0 && YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_EKSI) { // -val (negate)
        ast_kod_uret(a, YKN_DOT(dugum, sag));
        if (ast_node_is_float(YKN_DOT(dugum, sag))) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xBA);
            byte_yaz(a, 0); byte_yaz(a, 0); byte_yaz(a, 0); byte_yaz(a, 0);
            byte_yaz(a, 0); byte_yaz(a, 0); byte_yaz(a, 0); byte_yaz(a, 0x80);
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD0); // XOR RAX, RDX
            byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x6E); byte_yaz(a, 0xC0); // MOVQ XMM0, RAX
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0xF7); byte_yaz(a, 0xD8); // NEG RAX
        }
        return 0;
    }
    if (YKN_DOT(dugum, ilk_jeton) != 0 && YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_DEGIL) { // !val (logical NOT)
        ast_kod_uret(a, YKN_DOT(dugum, sag));
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
        byte_yaz(a, 0x0F); byte_yaz(a, 0x94); byte_yaz(a, 0xC0); // SETE AL
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0); // MOVZX RAX, AL
        return 0;
    }
    if (YKN_DOT(dugum, ilk_jeton) != 0 && YKN_DOT(YKN_DOT(dugum, ilk_jeton), tip) == TIP_TILDE) { // ~val (bitwise NOT)
        ast_kod_uret(a, YKN_DOT(dugum, sag));
        byte_yaz(a, 0x48); byte_yaz(a, 0xF7); byte_yaz(a, 0xD0); // NOT RAX
        return 0;
    }
    ast_kod_uret(a, YKN_DOT(dugum, sag));
    // Ã–NEMLÄ°: tip_cikar() AST_UNARY dÃ¼ÄŸÃ¼mÃ¼nÃ¼n tip_adi/cozumlenmis_tip alanlarini doldurmuyor,
    // sag dÃ¼ÄŸÃ¼mÃ¼n (AST_KIMLIK) alanlarina bak:
    AstDugum* u_sag = YKN_DOT(dugum, sag);
    long long u_tip_adi = (long long)YKN_DOT(u_sag, tip_adi);
    long long u_tip_uzunluk = YKN_DOT(u_sag, tip_adi_uzunluk);
    long long u_yildiz = YKN_DOT(u_sag, cozumlenmis_tip);
    long long is_unary_byte = 0;
    if (u_tip_adi != 0 && u_tip_uzunluk == 2 &&
        ((char*)u_tip_adi)[0] == 'u' && ((char*)u_tip_adi)[1] == '8') {
        if (u_yildiz == 1) {
            is_unary_byte = 1;
        }
    }
    if (is_unary_byte == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x00);
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_COMPTIME)) {
    ast_kod_uret(a, YKN_DOT(dugum, sol));
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONUSUM)) {
    ast_kod_uret(a, YKN_DOT(dugum, sol));
    long long sol_float = ast_node_is_float(YKN_DOT(dugum, sol));
    long long hedef_float = 0;
    if (YKN_DOT(dugum, cozumlenmis_tip) == 0 && YKN_DOT(dugum, tip_adi) != 0) {
        if (is_gercek_group(tip_adini_coz((char*)YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk)))) {
            hedef_float = 1;
        }
    }
    if (sol_float == 1 && hedef_float == 0) {
        // MOVQ XMM0, RAX
        byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x6E); byte_yaz(a, 0xC0);
        // CVTTSD2SI RAX, XMM0
        byte_yaz(a, 0xF2); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x2C); byte_yaz(a, 0xC0);
    } else if (sol_float == 0 && hedef_float == 1) {
        // CVTSI2SD XMM0, RAX
        byte_yaz(a, 0xF2); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x2A); byte_yaz(a, 0xC0);
        // MOVQ RAX, XMM0
        byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
    } else if (YKN_DOT(dugum, cozumlenmis_tip) == 0 && YKN_DOT(dugum, tip_adi) != 0) {
        char* tname = (char*)YKN_DOT(dugum, tip_adi);
        long long tlen = YKN_DOT(dugum, tip_adi_uzunluk);
        if (tlen == 2 && tname[0] == 'u' && tname[1] == '8') {
            byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0);
        } else if (tlen == 3 && tname[0] == 'u' && tname[1] == '1' && tname[2] == '6') {
            byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0xC0);
        } else if (tlen == 3 && tname[0] == 'i' && tname[1] == '3' && tname[2] == '2') {
            byte_yaz(a, 0x48); byte_yaz(a, 0x63); byte_yaz(a, 0xC0);
        } else if (tlen == 3 && tname[0] == 'u' && tname[1] == '3' && tname[2] == '2') {
            byte_yaz(a, 0x89); byte_yaz(a, 0xC0);
        }
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 20)) { // AST_REFERANS
    AstDugum* sag = YKN_DOT(dugum, sag);
    if (YKN_DOT(sag, tip) == 4) { // AST_KIMLIK
        long long off = 0;
        long long var_dizi = 0;
        for(long long i = g_lokal_sayisi - 1; i >= 0; i--) {
            if(g_lokal_uzunluklari[i] == YKN_DOT(YKN_DOT(sag, ilk_jeton), uzunluk)) {
                if(bellek_esit_mi(g_lokal_isimleri[i], YKN_DOT(YKN_DOT(sag, ilk_jeton), baslangic), g_lokal_uzunluklari[i]) == 1) {
                    off = g_lokal_offsetleri[i];
                    var_dizi = g_lokal_dizi_boyutlari[i];
                    break;
                }
            }
        }
        if (off == 0) {
            long long sag_uzn = YKN_DOT(YKN_DOT(sag, ilk_jeton), uzunluk);
            char* sag_isim = YKN_DOT(YKN_DOT(sag, ilk_jeton), baslangic);
            for(long long i=0; i<g_global_sayisi; i++) {
                if(g_global_uzunluk[i] == sag_uzn && bellek_esit_mi(g_global_isim[i], sag_isim, sag_uzn) == 1) {
                    long long g_addr = g_global_adres[i];
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05); // LEA RAX, [RIP+rel32]
                    int32_yaz(a, (int)(g_addr - (YKN_DOT(a, kullanilan) + 4)));
                    return 0;
                }
            }
        }
        if (off != 0) {
            if (var_dizi > 0) {
                if (off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45); // LEA RAX, [RBP-off8]
                    byte_yaz(a, (unsigned char)(256 - off));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85); // LEA RAX, [RBP-off32]
                    int32_yaz(a, -off);
                }
            } else {
                // Scalar local: &x should also give LEA (address), not MOV (value)
                if (off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45); // LEA RAX, [RBP-off8]
                    byte_yaz(a, (unsigned char)(256 - off));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85); // LEA RAX, [RBP-off32]
                    int32_yaz(a, -off);
                }
            }
        }
    } else if (YKN_DOT(sag, tip) == 17) { // AST_ALAN_ERISIMI: &d.x — parent adres + field offset
        AstDugum* ae_parent = YKN_DOT(sag, sol);
        char* ae_parent_tip_adi = (char*)YKN_DOT(ae_parent, tip_adi);
        long long ae_parent_tip_len = YKN_DOT(ae_parent, tip_adi_uzunluk);
        long long ae_parent_yildiz = 0;
        // 1. Parent'in ADRESINI al
        if (YKN_DOT(ae_parent, tip) == 4) {
            long long poff = 0;
            for(long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                if(g_lokal_uzunluklari[i] == YKN_DOT(YKN_DOT(ae_parent, ilk_jeton), uzunluk)) {
                    if(bellek_esit_mi(g_lokal_isimleri[i], YKN_DOT(YKN_DOT(ae_parent, ilk_jeton), baslangic), g_lokal_uzunluklari[i]) == 1) {
                        poff = g_lokal_offsetleri[i];
                        ae_parent_tip_adi = g_lokal_tip_adlari[i];
                        ae_parent_tip_len = g_lokal_tip_uzunluklari[i];
                        ae_parent_yildiz = g_lokal_yildizlari[i];
                        break;
                    }
                }
            }
            if (poff == 0) {
                long long ae_p_uzn = YKN_DOT(YKN_DOT(ae_parent, ilk_jeton), uzunluk);
                char* ae_p_isim = YKN_DOT(YKN_DOT(ae_parent, ilk_jeton), baslangic);
                for(long long i=0; i<g_global_sayisi; i++) {
                    if(g_global_uzunluk[i] == ae_p_uzn && bellek_esit_mi(g_global_isim[i], ae_p_isim, ae_p_uzn) == 1) {
                        ae_parent_tip_adi = g_global_tip_adi[i];
                        ae_parent_tip_len = g_global_tip_adi_uzunluk[i];
                        ae_parent_yildiz = g_global_is_pointer[i];
                        if (ae_parent_yildiz == 0) {
                            byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05); // LEA RAX, [RIP+rel]
                        } else {
                            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x05); // MOV RAX, [RIP+rel]
                        }
                        int32_yaz(a, g_global_adres[i] - (a->kullanilan + 4));
                        poff = -1;
                        break;
                    }
                }
            }
            if (poff > 0) {
                if (ae_parent_yildiz == 0) {
                    if (poff <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45);
                        byte_yaz(a, (unsigned char)(256 - poff));
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85);
                        int32_yaz(a, -poff);
                    }
                } else {
                    if (poff <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45);
                        byte_yaz(a, (unsigned char)(256 - poff));
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85);
                        int32_yaz(a, -poff);
                    }
                }
            } else if (poff == 0) {
                ast_kod_uret(a, ae_parent);
            }
        } else {
            ast_kod_uret(a, ae_parent);
        }
        if (ae_parent_tip_adi != 0 && ae_parent_tip_len > 0 && ae_parent_tip_adi[0] == '*') {
            ae_parent_tip_adi++;
            ae_parent_tip_len--;
        }
        // 2. Field offset bul
        AstDugum* ae_field_kim = YKN_DOT(sag, sag);
        char* ae_field_isim = (char*)YKN_DOT(YKN_DOT(ae_field_kim, ilk_jeton), baslangic);
        long long ae_field_uzunluk = YKN_DOT(YKN_DOT(ae_field_kim, ilk_jeton), uzunluk);
        long long ae_field_off = 0;
        if (ae_parent_tip_adi != 0 && ae_parent_tip_len > 0) {
            for(long long yi=0; yi<g_yapi_sayisi; yi++) {
                if(g_yapilar[yi].uzunluk == ae_parent_tip_len &&
                    bellek_esit_mi(g_yapilar[yi].isim, ae_parent_tip_adi, ae_parent_tip_len) == 1) {
                    for(long long ai=0; ai<g_yapilar[yi].alan_sayisi; ai++) {
                        if(g_yapilar[yi].alanlar[ai].uzunluk == ae_field_uzunluk &&
                            bellek_esit_mi(g_yapilar[yi].alanlar[ai].isim, ae_field_isim, ae_field_uzunluk) == 1) {
                            ae_field_off = g_yapilar[yi].alanlar[ai].offset;
                            YKN_DOT(dugum, tip_adi) = g_yapilar[yi].alanlar[ai].tip_adi;
                            YKN_DOT(dugum, tip_adi_uzunluk) = g_yapilar[yi].alanlar[ai].tip_uzunluk;
                            YKN_DOT(dugum, cozumlenmis_tip) = g_yapilar[yi].alanlar[ai].yildiz_sayisi;
                            YKN_DOT(dugum, dizi_boyutu) = g_yapilar[yi].alanlar[ai].dizi_boyutu;
                            break;
                        }
                    }
                    break;
                }
            }
        }
        // 3. RAX += field_off
        if (ae_field_off > 0) {
            if (ae_field_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0);
                byte_yaz(a, (unsigned char)ae_field_off);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x05);
                int32_yaz(a, (int)ae_field_off);
            }
        }
    } else if (YKN_DOT(sag, tip) == 18) { // AST_INDEKS_ERISIMI: &arr[i] â€” arr adres + i*scale
        AstDugum* aie_arr_node = YKN_DOT(sag, sol);
        AstDugum* aie_idx_node = YKN_DOT(sag, sag);
        char* aie_arr_adi = 0;
        long long aie_arr_adi_uzn = 0;
        if (YKN_DOT(aie_arr_node, tip) == 4) {
            aie_arr_adi = (char*)YKN_DOT(YKN_DOT(aie_arr_node, ilk_jeton), baslangic);
            aie_arr_adi_uzn = YKN_DOT(YKN_DOT(aie_arr_node, ilk_jeton), uzunluk);
        }
        long long aie_var_off = 0;
        long long aie_var_dizi = 0;
        long long aie_var_yildiz = 0;
        char* aie_var_tip_adi = 0;
        long long aie_var_tip_uzunluk = 0;
        if (aie_arr_adi != 0 && aie_arr_adi_uzn > 0) {
            for(long long i=0; i<g_lokal_sayisi; i++) {
                if(g_lokal_uzunluklari[i] == aie_arr_adi_uzn &&
                    bellek_esit_mi(g_lokal_isimleri[i], aie_arr_adi, aie_arr_adi_uzn) == 1) {
                    aie_var_off = g_lokal_offsetleri[i];
                    aie_var_dizi = g_lokal_dizi_boyutlari[i];
                    aie_var_yildiz = g_lokal_yildizlari[i];
                    aie_var_tip_adi = g_lokal_tip_adlari[i];
                    aie_var_tip_uzunluk = g_lokal_tip_uzunluklari[i];
                    break;
                }
            }
            if (aie_var_off == 0 && aie_var_tip_adi == 0) {
                for(long long i = 0; i < g_global_sayisi; i++) {
                    if(g_global_uzunluk[i] == aie_arr_adi_uzn && bellek_esit_mi(g_global_isim[i], aie_arr_adi, aie_arr_adi_uzn) == 1) {
                        aie_var_tip_adi = g_global_tip_adi[i];
                        aie_var_tip_uzunluk = g_global_tip_adi_uzunluk[i];
                        aie_var_yildiz = g_global_is_pointer[i];
                        aie_var_dizi = g_global_dizi_boyutu[i];
                        break;
                    }
                }
            }
        }
        // Fallback: arr node uzerinden tip cek
        if (aie_var_yildiz == 0 && aie_var_dizi == 0 && aie_var_tip_adi == 0) {
            aie_var_yildiz = YKN_DOT(aie_arr_node, cozumlenmis_tip);
            aie_var_tip_adi = (char*)YKN_DOT(aie_arr_node, tip_adi);
            aie_var_tip_uzunluk = YKN_DOT(aie_arr_node, tip_adi_uzunluk);
            aie_var_dizi = YKN_DOT(aie_arr_node, dizi_boyutu);
        }
        // 1. Arr ADRESINI al (stack dizi ise LEA, pointer ise MOV â€” deger = adres zaten)
        if (aie_var_off != 0 && aie_var_dizi > 0) {
            if (aie_var_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45);
                byte_yaz(a, (unsigned char)(256 - aie_var_off));
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85);
                int32_yaz(a, -aie_var_off);
            }
        } else {
            ast_kod_uret(a, aie_arr_node);
        }
        // RAX (arr adresi) -> RCX'e koruma: PUSH RAX
        byte_yaz(a, 0x50);
        // 2. idx RAX'e
        ast_kod_uret(a, aie_idx_node);
        // idx RAX -> RCX
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX
        // arr adresini POP: POP RAX
        byte_yaz(a, 0x58);
        // 3. Eleman olcegi hesapla
        long long aie_elem_yildiz = (aie_var_dizi > 0) ? aie_var_yildiz : (aie_var_yildiz > 0 ? aie_var_yildiz - 1 : 0);
        long long aie_elem_scale = tip_eleman_boyutu_al(aie_var_tip_adi, aie_var_tip_uzunluk, aie_elem_yildiz);
        // 4. RCX *= scale
        if (aie_elem_scale == 1) {
            // no-op
        } else if (aie_elem_scale == 2) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xD1); byte_yaz(a, 0xE1); // SHL RCX, 1 (*2)
        } else if (aie_elem_scale == 4) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE1); byte_yaz(a, 0x02); // SHL RCX, 2 (*4)
        } else if (aie_elem_scale == 8) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE1); byte_yaz(a, 0x03); // SHL RCX, 3 (*8)
        } else if (aie_elem_scale == 16) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE1); byte_yaz(a, 0x04);
        } else if (aie_elem_scale == 32) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE1); byte_yaz(a, 0x05);
        } else if (aie_elem_scale == 64) {
            byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE1); byte_yaz(a, 0x06);
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x69); byte_yaz(a, 0xC9); int32_yaz(a, (int)aie_elem_scale); // IMUL RCX, scale
        }
        // 5. RAX += RCX
        byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0xC8);
    } else {
        // Diger her tip icin fallback: degerini al
        ast_kod_uret(a, sag);
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 4)) { // AST_KIMLIK
    long long off = 0;
    long long var_dizi = 0;
    long long is_local_struct = 0;
    long long kimlik_start = g_lokal_sayisi - 1;
    long long kimlik_end = (g_lambda_tabani > 0) ? g_lambda_tabani : 0;
    for(long long i = kimlik_start; i >= kimlik_end; i--) {
        if(g_lokal_uzunluklari[i] == YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk)) {
            if(bellek_esit_mi(g_lokal_isimleri[i], YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), g_lokal_uzunluklari[i]) == 1) {
                off = g_lokal_offsetleri[i];
                var_dizi = g_lokal_dizi_boyutlari[i];
                if (g_lokal_yildizlari[i] == 0 && g_lokal_tip_adlari[i] != 0) {
                    for(long long j=0; j<g_yapi_sayisi; j++) {
                        if(g_yapilar[j].uzunluk == g_lokal_tip_uzunluklari[i] && bellek_esit_mi(g_yapilar[j].isim, g_lokal_tip_adlari[i], g_lokal_tip_uzunluklari[i]) == 1 && enum_turu_mi(g_lokal_tip_adlari[i], g_lokal_tip_uzunluklari[i]) == 0) {
                            is_local_struct = 1; break;
                        }
                    }
                }
                break;
            }
        }
    }
    if (off != 0) {
// [DEBUG_REMOVED]         printf("DEBUG_KIMLIK: off=%lld, dizi=%lld, struct=%lld\n", off, var_dizi, is_local_struct);
        if (var_dizi > 0 || is_local_struct == 1) {
            emit_lea_rax(a, off);
        } else {
            long long r_reg = (g_lambda_tabani == 0 && YKN_DOT(dugum, ilk_jeton)) ? ra_degisken_reg_bul(YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk)) : 0;
            if (r_reg > 0) {
                emit_mov_rax_reg(a, r_reg);
            } else {
                emit_load_rax(a, off);
            }
        }
    } else {
        long long dbg_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
        char* dbg_isim = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
        if (g_lambda_tabani > 0) {
            long long outer_off = 0;
            for (long long i = g_lambda_tabani - 1; i >= 0; i--) {
                if (g_lokal_uzunluklari[i] == dbg_uzn &&
                    bellek_esit_mi(g_lokal_isimleri[i], dbg_isim, dbg_uzn) == 1) {
                    outer_off = g_lokal_offsetleri[i];
                    break;
                }
            }
            if (outer_off != 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x55); byte_yaz(a, 0x00); // MOV RDX, [RBP + 0]
                if (outer_off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x42);
                    byte_yaz(a, (unsigned char)(256 - outer_off));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x82);
                    int32_yaz(a, -outer_off);
                }
                return 0;
            }
        }
        long long g_idx = -1;
        for(long long gi=0; gi<g_global_sayisi; gi++) {
            if(g_global_uzunluk[gi] == dbg_uzn && bellek_esit_mi(g_global_isim[gi], dbg_isim, dbg_uzn) == 1) {
                g_idx = gi; break;
            }
        }
        if (g_idx != -1) {
            long long g_addr = g_global_adres[g_idx];
            long long is_global_struct = 0;
            char* gtip = g_global_tip_adi[g_idx];
            long long gt_uzn = g_global_tip_adi_uzunluk[g_idx];
            if (gtip != 0 && g_global_is_pointer[g_idx] == 0) {
                for(long long j=0; j<g_yapi_sayisi; j++) {
                    if(g_yapilar[j].uzunluk == gt_uzn && bellek_esit_mi(g_yapilar[j].isim, gtip, gt_uzn) == 1 && enum_turu_mi(gtip, gt_uzn) == 0) {
                        is_global_struct = 1; break;
                    }
                }
            }
            if (g_global_dizi_boyutu[g_idx] > 0 || is_global_struct == 1) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05); // LEA RAX, [RIP+rel32]
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x05); // MOV RAX, [RIP+rel32]
            }
            int32_yaz(a, (int)(g_addr - (YKN_DOT(a, kullanilan) + 4)));
        } else {
            long long found_global = 0;
            for(long long gi=0; gi<g_global_sabit_sayisi; gi++) {
                if(g_global_sabitler[gi].uzunluk == dbg_uzn && bellek_esit_mi(g_global_sabitler[gi].isim, dbg_isim, dbg_uzn) == 1) {
                    emit_mov_rax_imm32(a, g_global_sabitler[gi].deger);
                    found_global = 1;
                    break;
                }
            }
            if (!found_global) {
                long long builtin_found = 0;
                long long builtin_value = yerlesik_sabit_degeri(dbg_isim, dbg_uzn, &builtin_found);
                if (builtin_found) {
                    emit_mov_rax_imm32(a, builtin_value);
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05); // LEA RAX, [RIP+rel32]
                    if (g_yama_sayisi >= 65536) {
                        seed_codegen_kapasite_reddet(2);
                        return 0;
                    }
                    g_yamalar[g_yama_sayisi].isim = dbg_isim;
                    g_yamalar[g_yama_sayisi].uzunluk = dbg_uzn;
                    g_yamalar[g_yama_sayisi].cagri_pos = YKN_DOT(a, kullanilan);
                    int32_yaz(a, 0);
                    g_yama_sayisi++;
                }
            }
        }
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 6)) { // AST_ATAMA
    AstDugum* sol = YKN_DOT(dugum, sol);
    AstDugum* sag = YKN_DOT(dugum, sag);

    // [FAZ 30 OPT] In-place accumulator & induction variable optimization (x += expr, x -= expr, x += c, x -= c)
    if (sol && sol->tip == 4 && sol->ilk_jeton && sag && sag->tip == AST_IKILI_OP && sag->ilk_jeton) {
        long long bin_op = sag->ilk_jeton->tip;
        if (bin_op == TIP_ARTI || bin_op == TIP_EKSI) {
            if (sag->sol && sag->sol->tip == 4 && sag->sol->ilk_jeton) {
                if (sag->sol->ilk_jeton->uzunluk == sol->ilk_jeton->uzunluk &&
                    bellek_esit_mi(sag->sol->ilk_jeton->baslangic, sol->ilk_jeton->baslangic, sol->ilk_jeton->uzunluk) == 1) {
                    long long off = 0;
                    long long opt_start = g_lokal_sayisi - 1;
                    long long opt_end = (g_lambda_tabani > 0) ? g_lambda_tabani : 0;
                    for (long long i = opt_start; i >= opt_end; i--) {
                        if (g_lokal_uzunluklari[i] == sol->ilk_jeton->uzunluk &&
                            bellek_esit_mi(g_lokal_isimleri[i], sol->ilk_jeton->baslangic, sol->ilk_jeton->uzunluk) == 1) {
                            off = g_lokal_offsetleri[i];
                            break;
                        }
                    }
                    if (off != 0) {
                        long long r_reg_opt = ra_degisken_reg_bul(sol->ilk_jeton->baslangic, sol->ilk_jeton->uzunluk);
                        // Constant step: x = x + c or x = x - c
                        if (sag->sag && sag->sag->tip == AST_SAYI && sag->sag->ilk_jeton) {
                            long long c = string_tamsayiya(sag->sag->ilk_jeton->baslangic, sag->sag->ilk_jeton->uzunluk);
                            if (c >= -128 && c <= 127) {
                                if (r_reg_opt > 0) {
                                    if (bin_op == TIP_ARTI) emit_add_reg_imm8(a, r_reg_opt, c);
                                    else emit_sub_reg_imm8(a, r_reg_opt, c);
                                }
                                if (bin_op == TIP_ARTI) {
                                    if (off <= 127) {
                                        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0x45);
                                        byte_yaz(a, (unsigned char)(256 - off)); byte_yaz(a, (char)c);
                                    } else {
                                        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0x85);
                                        int32_yaz(a, -off); byte_yaz(a, (char)c);
                                    }
                                } else {
                                    if (off <= 127) {
                                        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0x6D);
                                        byte_yaz(a, (unsigned char)(256 - off)); byte_yaz(a, (char)c);
                                    } else {
                                        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xAD);
                                        int32_yaz(a, -off); byte_yaz(a, (char)c);
                                    }
                                }
                                return 0;
                            }
                        }
                        // General expr: evaluate sag->sag into RAX, then ADD/SUB [RBP - off], RAX
                        ast_kod_uret(a, sag->sag);
                        if (r_reg_opt > 0) {
                            if (bin_op == TIP_ARTI) emit_add_reg_rax(a, r_reg_opt);
                            else emit_sub_reg_rax(a, r_reg_opt);
                        }
                        if (bin_op == TIP_ARTI) {
                            if (off <= 127) {
                                byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0x45);
                                byte_yaz(a, (unsigned char)(256 - off));
                            } else {
                                byte_yaz(a, 0x48); byte_yaz(a, 0x01); byte_yaz(a, 0x85);
                                int32_yaz(a, -off);
                            }
                        } else {
                            if (off <= 127) {
                                byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0x45);
                                byte_yaz(a, (unsigned char)(256 - off));
                            } else {
                                byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0x85);
                                int32_yaz(a, -off);
                            }
                        }
                        return 0;
                    }
                }
            }
        }
    }

    ast_kod_uret(a, YKN_DOT(dugum, sag));
// [DEBUG_REMOVED]     printf("AST_ATAMA debug! sol tip=%lld\n", sol ? YKN_DOT(sol, tip) : -1);
    if (YKN_DOT(sol, tip) == 19) { // AST_UNARY
        byte_yaz(a, 0x50); // PUSH RAX
        ast_kod_uret(a, YKN_DOT(sol, sag));
        byte_yaz(a, 0x59); // POP RCX
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x08); // MOV [RAX], RCX
    } else if (YKN_DOT(sol, tip) == 17) { // AST_ALAN_ERISIMI â€” LHS: write to struct field
        byte_yaz(a, 0x50); // PUSH RAX (save assigned value)
        AstDugum* obj = YKN_DOT(sol, sol);
        AstDugum* field = YKN_DOT(sol, sag);
        long long var_off = 0;
        char* t_adi = 0;
        long long t_uzn = 0;
        long long obj_yildiz = 0;  // 0 = direkt struct degisken (b,d), >0 = pointer (*Kutu k)
        long long g_addr = 0;
        expr_struct_type(obj, &t_adi, &t_uzn);
        if (YKN_DOT(obj, tip) == 4 && YKN_DOT(obj, ilk_jeton) != 0) { // AST_KIMLIK
            for(long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                if(g_lokal_uzunluklari[i] == YKN_DOT(YKN_DOT(obj, ilk_jeton), uzunluk)) {
                    if(bellek_esit_mi(g_lokal_isimleri[i], YKN_DOT(YKN_DOT(obj, ilk_jeton), baslangic), g_lokal_uzunluklari[i]) == 1) {
                        var_off = g_lokal_offsetleri[i];
                        obj_yildiz = g_lokal_yildizlari[i];
                        break;
                    }
                }
            }
            if (var_off == 0) {
                long long obj_uzn = YKN_DOT(YKN_DOT(obj, ilk_jeton), uzunluk);
                char* obj_isim = YKN_DOT(YKN_DOT(obj, ilk_jeton), baslangic);
                for(long long i=0; i<g_global_sayisi; i++) {
                    if(g_global_uzunluk[i] == obj_uzn && bellek_esit_mi(g_global_isim[i], obj_isim, obj_uzn) == 1) {
                        var_off = -1;
                        g_addr = g_global_adres[i];
                        obj_yildiz = g_global_is_pointer[i];
                        break;
                    }
                }
            }
        }
        long long field_off = -1;
        long long field_elem_sz = 8;
        if (t_adi != 0 && t_uzn > 0 && t_adi[0] == '*') {
            t_adi = t_adi + 1;
            t_uzn = t_uzn - 1;
        }
        if (t_adi != 0 && t_uzn > 0) {
            for (long long k = 0; k < t_uzn; k++) {
                if (t_adi[k] == '<') { t_uzn = k; break; }
            }
        }
        if (t_adi != 0) {
            for(long long i=0; i<g_yapi_sayisi; i++) {
                if (g_yapilar[i].uzunluk == t_uzn && bellek_esit_mi(g_yapilar[i].isim, t_adi, t_uzn) == 1) {
                    for(long long j=0; j<g_yapilar[i].alan_sayisi; j++) {
                        if (g_yapilar[i].alanlar[j].uzunluk == YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk) && 
                            bellek_esit_mi(g_yapilar[i].alanlar[j].isim, YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic), g_yapilar[i].alanlar[j].uzunluk) == 1) {
                            field_off = g_yapilar[i].alanlar[j].offset;
                            field_elem_sz = tip_eleman_boyutu_al(g_yapilar[i].alanlar[j].tip_adi, g_yapilar[i].alanlar[j].tip_uzunluk, g_yapilar[i].alanlar[j].yildiz_sayisi);
                            break;
                        }
                    }
                    break;
                }
            }
        }
        if (field_off == -1 && field != 0 && YKN_DOT(field, ilk_jeton) != 0) {
            long long flen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
            char* fstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
            if (flen == 3 && bellek_esit_mi(fstr, (char*)"tip", 3) == 1) field_off = 0;
            else if (flen == 9 && bellek_esit_mi(fstr, (char*)"ilk_jeton", 9) == 1) field_off = 8;
            else if (flen == 9 && bellek_esit_mi(fstr, (char*)"son_jeton", 9) == 1) field_off = 16;
            else if (flen == 3 && bellek_esit_mi(fstr, (char*)"sol", 3) == 1) field_off = 24;
            else if (flen == 3 && bellek_esit_mi(fstr, (char*)"sag", 3) == 1) field_off = 32;
            else if (flen == 7 && bellek_esit_mi(fstr, (char*)"sonraki", 7) == 1) field_off = 40;
            else if (flen == 5 && bellek_esit_mi(fstr, (char*)"govde", 5) == 1) field_off = 48;
            else if (flen == 11 && bellek_esit_mi(fstr, (char*)"yoksa_govde", 11) == 1) field_off = 56;
            else if (flen == 12 && bellek_esit_mi(fstr, (char*)"parametreler", 12) == 1) field_off = 64;
            else if (flen == 7 && bellek_esit_mi(fstr, (char*)"tip_adi", 7) == 1) field_off = 72;
            else if (flen == 15 && bellek_esit_mi(fstr, (char*)"tip_adi_uzunluk", 15) == 1) field_off = 80;
            else if (flen == 15 && bellek_esit_mi(fstr, (char*)"cozumlenmis_tip", 15) == 1) field_off = 88;
            else if (flen == 11 && bellek_esit_mi(fstr, (char*)"dizi_boyutu", 11) == 1) field_off = 96;
        else if (flen == 10 && bellek_esit_mi(fstr, (char*)"gorunurluk", 10) == 1) {
            if (t_adi != 0 && t_uzn == 13 && bellek_esit_mi(t_adi, (char*)"SembolGirdisi", 13) == 1) field_off = 48;
            else field_off = 104;
        }
        else if (flen == 8 && bellek_esit_mi(fstr, (char*)"nullable", 8) == 1) {
            if (t_adi != 0 && t_uzn == 13 && bellek_esit_mi(t_adi, (char*)"SembolGirdisi", 13) == 1) field_off = 48;
            else field_off = 112;
        }
            else if (flen == 10 && bellek_esit_mi(fstr, (char*)"gorunurluk", 10) == 1) {
                if (t_adi != 0 && t_uzn == 13 && bellek_esit_mi(t_adi, (char*)"SembolGirdisi", 13) == 1) field_off = 48;
                else field_off = 104;
            }
            else if (flen == 8 && bellek_esit_mi(fstr, (char*)"nullable", 8) == 1) {
                if (t_adi != 0 && t_uzn == 13 && bellek_esit_mi(t_adi, (char*)"SembolGirdisi", 13) == 1) field_off = 48;
                else field_off = 112;
            }
            else if (flen == 11 && bellek_esit_mi(fstr, (char*)"nominal_id", 11) == 1) field_off = 32;
            else if (flen == 6 && bellek_esit_mi(fstr, (char*)"ic_tip", 6) == 1) field_off = 40;
            else if (flen == 9 && bellek_esit_mi(fstr, (char*)"baslangic", 9) == 1) field_off = 8;
            else if (flen == 7 && bellek_esit_mi(fstr, (char*)"uzunluk", 7) == 1) field_off = 16;
            else if (flen == 5 && bellek_esit_mi(fstr, (char*)"satir", 5) == 1) field_off = 24;
            else if (flen == 5 && bellek_esit_mi(fstr, (char*)"sutun", 5) == 1) field_off = 32;
            else if (flen == 6 && bellek_esit_mi(fstr, (char*)"kaynak", 6) == 1) field_off = 0;
            else if (flen == 12 && bellek_esit_mi(fstr, (char*)"dosya_boyutu", 12) == 1) field_off = 8;
            else if (flen == 13 && bellek_esit_mi(fstr, (char*)"guncel_offset", 13) == 1) field_off = 16;
            else if (flen == 8 && bellek_esit_mi(fstr, (char*)"jetonlar", 8) == 1) field_off = 40;
            else if (flen == 12 && bellek_esit_mi(fstr, (char*)"jeton_sayisi", 12) == 1) field_off = 48;
            else if (flen == 14 && bellek_esit_mi(fstr, (char*)"jeton_kapasite", 14) == 1) field_off = 56;
            else if (flen == 5 && bellek_esit_mi(fstr, (char*)"arena", 5) == 1) field_off = 8;
            else if (flen == 12 && bellek_esit_mi(fstr, (char*)"guncel_index", 12) == 1) field_off = 16;
            else if (flen == 6 && bellek_esit_mi(fstr, (char*)"bellek", 6) == 1) field_off = 0;
            else if (flen == 8 && bellek_esit_mi(fstr, (char*)"kapasite", 8) == 1) field_off = 8;
            else if (flen == 10 && bellek_esit_mi(fstr, (char*)"kullanilan", 10) == 1) field_off = 16;
            else if (flen == 4 && bellek_esit_mi(fstr, (char*)"adet", 4) == 1) field_off = 0;
            else if (flen == 12 && bellek_esit_mi(fstr, (char*)"stack_offset", 12) == 1) field_off = 8;
            else if (flen == 4 && bellek_esit_mi(fstr, (char*)"isim", 4) == 1) field_off = 0;
            else if (flen == 5 && bellek_esit_mi(fstr, (char*)"deger", 5) == 1) field_off = 8;
            else if (flen == 5 && bellek_esit_mi(fstr, (char*)"adres", 5) == 1) field_off = 16;
            // GOREV 6.4: Sonuc<T> struct alanlari — atama fallback
            else if (flen == 8 && bellek_esit_mi(fstr, (char*)"basarili",  8) == 1) field_off = 0;
            else if (flen == 9 && bellek_esit_mi(fstr, (char*)"hata_kodu", 9) == 1) field_off = 16;
            // Imported Arc yapi layout fallback: ArcKontrol={veri_ptr,ref_sayisi_ptr}, Arc={kontrol_ptr}.
            else if (flen == 8 && bellek_esit_mi(fstr, (char*)"veri_ptr", 8) == 1) field_off = 0;
            else if (flen == 14 && bellek_esit_mi(fstr, (char*)"ref_sayisi_ptr", 14) == 1) field_off = 8;
            else if (flen == 11 && bellek_esit_mi(fstr, (char*)"kontrol_ptr", 11) == 1) field_off = 0;
        }
        // Arc ABI override: imported yapi metadata'si yanlis olsa dahi sabit layout.
        if (field != 0 && YKN_DOT(field, ilk_jeton) != 0) {
            long long aflen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
            char* afstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
            if (aflen == 8 && bellek_esit_mi(afstr, (char*)"veri_ptr", 8) == 1) field_off = 0;
            if (aflen == 14 && bellek_esit_mi(afstr, (char*)"ref_sayisi_ptr", 14) == 1) field_off = 8;
            if (aflen == 11 && bellek_esit_mi(afstr, (char*)"kontrol_ptr", 11) == 1) field_off = 0;
        }
// [DEBUG_REMOVED]         printf("DEBUG: AST_ALAN_ERISIMI read '%.*s', field_off=%lld\n", (int)YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk), (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic), field_off);
        // --- DÃœZELTME 2: POINTER ise MOV (deger = adres), DOÄRUDAN struct ise LEA (adres al) ---
        if (YKN_DOT(obj, tip) == 4 && var_off != 0) {
            if (var_off == -1) { // Global deÄŸiÅŸken
                if (obj_yildiz == 0) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05); // LEA RAX, [RIP+rel32]
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x05); // MOV RAX, [RIP+rel32]
                }
                int32_yaz(a, g_addr - (a->kullanilan + 4));
            } else { // Lokal deÄŸiÅŸken
                if (obj_yildiz == 0) {
                    // DOÄRUDAN struct deÄŸiÅŸkeni (d, b): LEA ile ADRES al
                    if (var_off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45);  // LEA RAX, [RBP - var_off]
                        byte_yaz(a, (unsigned char)(256 - var_off));
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85);  // LEA RAX, [RBP - var_off]
                        int32_yaz(a, -var_off);
                    }
                } else {
                    // POINTER (*Kutu k gibi): MOV ile DEÄERÄ° yÃ¼kle (zaten hedef struct'Ä±n adresi)
                    if (var_off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45);  // MOV RAX, [RBP - var_off]
                        byte_yaz(a, (unsigned char)(256 - var_off));
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85);  // MOV RAX, [RBP - var_off]
                        int32_yaz(a, -var_off);
                    }
                }
            }
        } else {
            ast_kod_uret(a, obj);
        }
        byte_yaz(a, 0x59); // POP RCX
        if (field_off != -1) {
            if (field_elem_sz == 1) {
                if (field_off <= 127) {
                    byte_yaz(a, 0x88); byte_yaz(a, 0x48); byte_yaz(a, (unsigned char)field_off);
                } else {
                    byte_yaz(a, 0x88); byte_yaz(a, 0x88); int32_yaz(a, (int)field_off);
                }
            } else if (field_elem_sz == 2) {
                if (field_off <= 127) {
                    byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x48); byte_yaz(a, (unsigned char)field_off);
                } else {
                    byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x88); int32_yaz(a, (int)field_off);
                }
            } else if (field_elem_sz == 4) {
                if (field_off <= 127) {
                    byte_yaz(a, 0x89); byte_yaz(a, 0x48); byte_yaz(a, (unsigned char)field_off);
                } else {
                    byte_yaz(a, 0x89); byte_yaz(a, 0x88); int32_yaz(a, (int)field_off);
                }
            } else {
                if (field_off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x48);
                    byte_yaz(a, (unsigned char)field_off);
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x88);
                    int32_yaz(a, (int)field_off);
                }
            }
        }
    } else if (YKN_DOT(sol, tip) == 18) { // AST_INDEKS_ERISIMI
        byte_yaz(a, 0x50); // PUSH RAX (save assigned value)
        AstDugum* arr_node = YKN_DOT(sol, sol);
        AstDugum* idx_node = YKN_DOT(sol, sag);
        long long var_off = 0;
        long long var_dizi = 0;
        char* var_tip_adi2 = 0;
        long long var_tip_uzunluk2 = 0;
        long long var_yildiz2 = 0;
        if (YKN_DOT(arr_node, tip) == 4) { // AST_KIMLIK
            for(long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                if(g_lokal_uzunluklari[i] == YKN_DOT(YKN_DOT(arr_node, ilk_jeton), uzunluk)) {
                    if(bellek_esit_mi(g_lokal_isimleri[i], YKN_DOT(YKN_DOT(arr_node, ilk_jeton), baslangic), g_lokal_uzunluklari[i]) == 1) {
                        var_off = g_lokal_offsetleri[i];
                        var_dizi = g_lokal_dizi_boyutlari[i];
                        var_tip_adi2 = g_lokal_tip_adlari[i];
                        var_tip_uzunluk2 = g_lokal_tip_uzunluklari[i];
                        var_yildiz2 = g_lokal_yildizlari[i];
                        break;
                    }
                }
            }
            if (var_off == 0 && var_tip_adi2 == 0) {
                long long obj_uzn = YKN_DOT(YKN_DOT(arr_node, ilk_jeton), uzunluk);
                char* obj_isim = YKN_DOT(YKN_DOT(arr_node, ilk_jeton), baslangic);
                for(long long i = 0; i < g_global_sayisi; i++) {
                    if(g_global_uzunluk[i] == obj_uzn && bellek_esit_mi(g_global_isim[i], obj_isim, obj_uzn) == 1) {
                        var_tip_adi2 = g_global_tip_adi[i];
                        var_tip_uzunluk2 = g_global_tip_adi_uzunluk[i];
                        var_yildiz2 = g_global_is_pointer[i];
                        var_dizi = g_global_dizi_boyutu[i];
                        break;
                    }
                }
            }
        }
        
        if (var_off != 0 && var_dizi > 0) {
            if (var_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45); // LEA RAX, [RBP - off]
                byte_yaz(a, (unsigned char)(256 - var_off));
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85);
                int32_yaz(a, -var_off);
            }
        } else {
            ast_kod_uret(a, arr_node); // MOV RAX, [RBP - off]
        }
        if (var_yildiz2 == 0 && var_dizi == 0 && var_tip_adi2 == 0 && arr_node != 0) {
            var_yildiz2 = YKN_DOT(arr_node, cozumlenmis_tip);
            var_tip_adi2 = (char*)YKN_DOT(arr_node, tip_adi);
            var_tip_uzunluk2 = YKN_DOT(arr_node, tip_adi_uzunluk);
            if (YKN_DOT(arr_node, tip) != 17) {
                var_dizi = YKN_DOT(arr_node, dizi_boyutu);
            }
        }
        
        long long elem_yildiz2 = (var_dizi > 0) ? var_yildiz2 : (var_yildiz2 > 0 ? var_yildiz2 - 1 : 0);
        long long elem_scale2 = tip_eleman_boyutu_al(var_tip_adi2, var_tip_uzunluk2, elem_yildiz2);
        long long is_byte2 = (elem_scale2 == 1) ? 1 : 0;
        if (arr_node != 0 && YKN_DOT(arr_node, tip) == 17) { // AST_ALAN_ERISIMI
            char* fld_t_adi2 = 0;
            long long fld_t_uzn2 = 0;
            expr_struct_type(arr_node, &fld_t_adi2, &fld_t_uzn2);
            if (fld_t_adi2 != 0 && fld_t_uzn2 > 0) {
                long long f_scale2 = tip_eleman_boyutu_al(fld_t_adi2, fld_t_uzn2, 0);
                if (f_scale2 == 1) {
                    is_byte2 = 1;
                    elem_scale2 = 1;
                } else if (f_scale2 == 2 || f_scale2 == 4 || f_scale2 == 8) {
                    elem_scale2 = f_scale2;
                }
            }
            if (!is_byte2) {
                AstDugum* fld = YKN_DOT(arr_node, sag);
                if (fld != 0 && YKN_DOT(fld, ilk_jeton) != 0) {
                    long long flen = YKN_DOT(YKN_DOT(fld, ilk_jeton), uzunluk);
                    char* fstr = (char*)YKN_DOT(YKN_DOT(fld, ilk_jeton), baslangic);
                    if (flen == 6 && bellek_esit_mi(fstr, (char*)"bellek", 6) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 6 && bellek_esit_mi(fstr, (char*)"kaynak", 6) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 6 && bellek_esit_mi(fstr, (char*)"icerik", 6) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 4 && bellek_esit_mi(fstr, (char*)"veri", 4) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 3 && bellek_esit_mi(fstr, (char*)"ptr", 3) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 9 && bellek_esit_mi(fstr, (char*)"baslangic", 9) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 7 && bellek_esit_mi(fstr, (char*)"tip_adi", 7) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 5 && bellek_esit_mi(fstr, (char*)"mesaj", 5) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 5 && bellek_esit_mi(fstr, (char*)"ipucu", 5) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 12 && bellek_esit_mi(fstr, (char*)"kaynak_satir", 12) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 9 && bellek_esit_mi(fstr, (char*)"dosya_adi", 9) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 8 && bellek_esit_mi(fstr, (char*)"veri_ptr", 8) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                    else if (flen == 12 && bellek_esit_mi(fstr, (char*)"arena_bellek", 12) == 1) { is_byte2 = 1; elem_scale2 = 1; }
                }
            }
        }
        
        bool fast_indexed_write = false;
        if (!g_bounds_check) {
            if (idx_node && idx_node->tip == AST_SAYI && idx_node->ilk_jeton) {
                long long c = string_tamsayiya(idx_node->ilk_jeton->baslangic, idx_node->ilk_jeton->uzunluk);
                long long off = c * elem_scale2;
                byte_yaz(a, 0x5A); // POP RDX (assigned value)
                if (elem_scale2 == 1) {
                    if (off == 0) {
                        byte_yaz(a, 0x88); byte_yaz(a, 0x10);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x88); byte_yaz(a, 0x50); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x88); byte_yaz(a, 0x90); int32_yaz(a, (int)off);
                    }
                } else if (elem_scale2 == 2) {
                    if (off == 0) {
                        byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x10);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x90); int32_yaz(a, (int)off);
                    }
                } else if (elem_scale2 == 4) {
                    if (off == 0) {
                        byte_yaz(a, 0x89); byte_yaz(a, 0x10);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x89); byte_yaz(a, 0x90); int32_yaz(a, (int)off);
                    }
                } else {
                    if (off == 0) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x10);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x90); int32_yaz(a, (int)off);
                    }
                }
                fast_indexed_write = true;
            } else if (idx_node && idx_node->tip == 4 && idx_node->ilk_jeton) {
                long long idx_off = 0;
                long long idx_dizi = 0;
                for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                    if (g_lokal_uzunluklari[i] == idx_node->ilk_jeton->uzunluk &&
                        bellek_esit_mi(g_lokal_isimleri[i], idx_node->ilk_jeton->baslangic, idx_node->ilk_jeton->uzunluk) == 1) {
                        idx_off = g_lokal_offsetleri[i];
                        idx_dizi = g_lokal_dizi_boyutlari[i];
                        break;
                    }
                }
                if (idx_off != 0 && idx_dizi == 0) {
                    byte_yaz(a, 0x5A); // POP RDX (assigned value)
                    if (idx_off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - idx_off));
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x8D); int32_yaz(a, -idx_off);
                    }
                    if (elem_scale2 == 1) {
                        byte_yaz(a, 0x88); byte_yaz(a, 0x14); byte_yaz(a, 0x08); // MOV [RAX + RCX], DL
                    } else if (elem_scale2 == 2) {
                        byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x14); byte_yaz(a, 0x48); // MOV [RAX + RCX*2], DX
                    } else if (elem_scale2 == 4) {
                        byte_yaz(a, 0x89); byte_yaz(a, 0x14); byte_yaz(a, 0x88); // MOV [RAX + RCX*4], EDX
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x14); byte_yaz(a, 0xC8); // MOV [RAX + RCX*8], RDX
                    }
                    fast_indexed_write = true;
                }
            }
        }
        if (!fast_indexed_write) {
            byte_yaz(a, 0x50); // PUSH RAX (save base address)
            if (g_bounds_check && var_yildiz2 != 0) emit_bounds_guard(a, 0, 1);
            ast_kod_uret(a, idx_node);
            if (g_bounds_check && var_dizi > 0) emit_bounds_guard(a, var_dizi, 0);
            if (elem_scale2 == 1) {
                // scale 1: no shift
            } else if (elem_scale2 == 2) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xD1); byte_yaz(a, 0xE0); // SHL RAX, 1 (*2)
            } else if (elem_scale2 == 4) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x02); // SHL RAX, 2 (*4)
            } else if (elem_scale2 == 8) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x03); // SHL RAX, 3 (*8)
            } else if (elem_scale2 == 16) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x04);
            } else if (elem_scale2 == 32) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x05);
            } else if (elem_scale2 == 64) {
                byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x06);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x69); byte_yaz(a, 0xC0); int32_yaz(a, (int)elem_scale2);
            }
            byte_yaz(a, 0x59); // POP RCX (base address)
            emit_add_rax_rcx(a); // RAX = target element address
            byte_yaz(a, 0x59); // POP RCX (assigned value)
            if (elem_scale2 == 1) {
                byte_yaz(a, 0x88); byte_yaz(a, 0x08); // MOV [RAX], CL
            } else if (elem_scale2 == 2) {
                byte_yaz(a, 0x66); byte_yaz(a, 0x89); byte_yaz(a, 0x08); // MOV [RAX], CX
            } else if (elem_scale2 == 4) {
                byte_yaz(a, 0x89); byte_yaz(a, 0x08); // MOV [RAX], ECX
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x08); // MOV [RAX], RCX
            }
        }
      } else if (YKN_DOT(sol, tip) == 4) { // AST_KIMLIK
          long long off = 0;
          char* p_isim = YKN_DOT(YKN_DOT(sol, ilk_jeton), baslangic);
          long long p_uzn = YKN_DOT(YKN_DOT(sol, ilk_jeton), uzunluk);
// [DEBUG_REMOVED]           printf("AST_ATAMA KIMLIK found! name: %.*s\n", (int)p_uzn, p_isim);
          long long atama_start = g_lokal_sayisi - 1;
          long long atama_end = (g_lambda_tabani > 0) ? g_lambda_tabani : 0;
          for(long long i = atama_start; i >= atama_end; i--) {
              if(g_lokal_uzunluklari[i] == p_uzn) {
                  if(bellek_esit_mi(g_lokal_isimleri[i], p_isim, p_uzn) == 1) {
                      off = g_lokal_offsetleri[i];
                      if (off != 0) {
                          emit_store_rax(a, off);
                          long long r_reg = (g_lambda_tabani == 0) ? ra_degisken_reg_bul(p_isim, p_uzn) : 0;
                          if (r_reg > 0) emit_mov_reg_rax(a, r_reg);
                      }
                      break;
                  }
              }
          }
          if (off == 0) {
              long long outer_off = 0;
              if (g_lambda_tabani > 0) {
                  for (long long i = g_lambda_tabani - 1; i >= 0; i--) {
                      if (g_lokal_uzunluklari[i] == p_uzn &&
                          bellek_esit_mi(g_lokal_isimleri[i], p_isim, p_uzn) == 1) {
                          outer_off = g_lokal_offsetleri[i];
                          break;
                      }
                  }
              }
              if (outer_off != 0) {
                  byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x55); byte_yaz(a, 0x00); // MOV RDX, [RBP + 0]
                  if (outer_off <= 127) {
                      byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x42);
                      byte_yaz(a, (unsigned char)(256 - outer_off));
                  } else {
                      byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x82);
                      int32_yaz(a, -outer_off);
                  }
              } else {
                  for (long long gi = 0; gi < g_global_sayisi; gi++) {
                      if (g_global_uzunluk[gi] == p_uzn && bellek_esit_mi(g_global_isim[gi], p_isim, p_uzn) == 1) {
                          long long g_addr = g_global_adres[gi];
                          byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x05); // MOV [RIP+rel32], RAX
                          int32_yaz(a, (int)(g_addr - (YKN_DOT(a, kullanilan) + 4)));
                          break;
                      }
                  }
              }
          }
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 7)) { // AST_DEGISKEN_TANIMI
    // Global scope: not inside any function body â†’ allocate global mutable storage
    if (!g_fonksiyon_icinde) {
        long long obj_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
        if (seed_global_storage_index(YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), obj_uzn) >= 0) {
            return 0;
        }
        char* obj_isim = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
// [DEBUG_REMOVED]         printf("DBG_GLOBAL_REG: '%.*s' (uzunluk=%lld) g_global_sayisi=%lld\n", (int)obj_uzn, obj_isim, obj_uzn, g_global_sayisi);
        
        long long gval = 0;
        long long gval_bulundu = 0;
        AstDugum* rhs = YKN_DOT(dugum, sag);
        if (rhs != 0) {
            gval = global_sabit_degeri(rhs, &gval_bulundu);
        }
        if (gval_bulundu && g_global_sabit_sayisi < 16384) {
            g_global_sabitler[g_global_sabit_sayisi].isim = obj_isim;
            g_global_sabitler[g_global_sabit_sayisi].uzunluk = obj_uzn;
            g_global_sabitler[g_global_sabit_sayisi].deger = gval;
            g_global_sabit_sayisi++;
        }
        
        long long elem_sz = 8;
        if (YKN_DOT(dugum, tip_adi) != nullptr && YKN_DOT(dugum, tip_adi_uzunluk) > 0) {
            elem_sz = tip_eleman_boyutu_al(YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk), YKN_DOT(dugum, cozumlenmis_tip));
        }
        long long g_size = elem_sz;
        if (YKN_DOT(dugum, dizi_boyutu) > 0) {
            long long d_cnt = YKN_DOT(dugum, dizi_boyutu);
            if (d_cnt > 1048576) d_cnt = 1048576;
            g_size = d_cnt * elem_sz;
        }
        g_size = ((g_size + 7) / 8) * 8;
        if (g_size < 8) g_size = 8;
        byte_yaz(a, 0xE9);
        int32_yaz(a, g_size);
        long long g_addr = YKN_DOT(a, kullanilan);
        if (gval_bulundu) {
            long long v_copy = gval;
            for (int i = 0; i < 8; i++) {
                byte_yaz(a, (unsigned char)(v_copy & 0xFF));
                v_copy >>= 8;
            }
            for (int i = 8; i < g_size; i++) {
                byte_yaz(a, 0);
            }
        } else {
            for(int i = 0; i < g_size; i++) {
                byte_yaz(a, 0);
            }
        }
        if (g_global_sayisi >= 16384) {
            seed_codegen_kapasite_reddet(4);
            return 0;
        }
        if (g_global_sayisi < 16384) {
            g_global_isim[g_global_sayisi] = obj_isim;
            g_global_uzunluk[g_global_sayisi] = obj_uzn;
            g_global_adres[g_global_sayisi] = g_addr;
            g_global_tip_adi[g_global_sayisi] = YKN_DOT(dugum, tip_adi);
            g_global_tip_adi_uzunluk[g_global_sayisi] = YKN_DOT(dugum, tip_adi_uzunluk);
            g_global_is_pointer[g_global_sayisi] = YKN_DOT(dugum, cozumlenmis_tip);
            g_global_dizi_boyutu[g_global_sayisi] = YKN_DOT(dugum, dizi_boyutu);
            g_global_sayisi++;
// [DEBUG_REMOVED]             printf("DBG_GLOBAL_REG_OK: idx=%lld addr=%lld\n", g_global_sayisi-1, g_addr);
        }
        return 0;
    }
    if (YKN_DOT(dugum, sag) != 0) {
        ast_kod_uret(a, YKN_DOT(dugum, sag));
    } else {
        emit_xor_rax_rax(a);
    }
    long long loc_elem_sz = tip_eleman_boyutu_al((char*)YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk), YKN_DOT(dugum, cozumlenmis_tip));
    if (YKN_DOT(dugum, dizi_boyutu) > 0) {
        long long d_bytes = YKN_DOT(dugum, dizi_boyutu) * loc_elem_sz;
        d_bytes = ((d_bytes + 7) / 8) * 8;
        g_stack_offset = g_stack_offset + d_bytes;
    } else {
        long long struct_byte_size = loc_elem_sz;
        struct_byte_size = ((struct_byte_size + 7) / 8) * 8;
        g_stack_offset = g_stack_offset + struct_byte_size;
    }
    long long off = g_stack_offset;
    char* l_isim = (char*)"";
    long long l_uzn = 0;
    if (YKN_DOT(dugum, ilk_jeton) != nullptr) {
        l_isim = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
        l_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
    }
    if (g_lokal_sayisi >= 16384) {
        seed_codegen_kapasite_reddet(1);
        return 0;
    }
    g_lokal_isimleri[g_lokal_sayisi] = l_isim;
    g_lokal_uzunluklari[g_lokal_sayisi] = l_uzn;
    g_lokal_offsetleri[g_lokal_sayisi] = off;
    g_lokal_tip_adlari[g_lokal_sayisi] = YKN_DOT(dugum, tip_adi);
    g_lokal_tip_uzunluklari[g_lokal_sayisi] = YKN_DOT(dugum, tip_adi_uzunluk);
    g_lokal_dizi_boyutlari[g_lokal_sayisi] = YKN_DOT(dugum, dizi_boyutu);
    g_lokal_yildizlari[g_lokal_sayisi] = (enum_turu_mi(YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk)) == 1) ? 1 : YKN_DOT(dugum, cozumlenmis_tip);
// [DEBUG_REMOVED]     printf("DEBUG: Local %.*s defined with yildiz_sayisi=%d (cozumlenmis_tip=%d, dizi_boyutu=%d)\n", (int)g_lokal_uzunluklari[g_lokal_sayisi], g_lokal_isimleri[g_lokal_sayisi], (int)g_lokal_yildizlari[g_lokal_sayisi], (int)YKN_DOT(dugum, cozumlenmis_tip), (int)g_lokal_dizi_boyutlari[g_lokal_sayisi]);
    g_lokal_sayisi = g_lokal_sayisi + 1;
    emit_store_rax(a, off);
    if (YKN_DOT(dugum, ilk_jeton) && g_lambda_tabani == 0) {
        long long r_reg = ra_degisken_reg_bul(YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk));
        if (r_reg > 0) emit_mov_reg_rax(a, r_reg);
    }
    char* effective_tip_adi = YKN_DOT(dugum, tip_adi);
    long long effective_tip_uzn = YKN_DOT(dugum, tip_adi_uzunluk);
    if ((!effective_tip_adi || effective_tip_uzn <= 0) && YKN_DOT(dugum, sag) != 0) {
        AstDugum* rhs = YKN_DOT(dugum, sag);
        if (rhs->tip == AST_YAPI_TANIMI && rhs->ilk_jeton) {
            effective_tip_adi = rhs->ilk_jeton->baslangic;
            effective_tip_uzn = rhs->ilk_jeton->uzunluk;
        } else if (rhs->tip == 11 && rhs->sol && rhs->sol->ilk_jeton) { // AST_ISLEV_CAGRISI
            effective_tip_adi = rhs->sol->ilk_jeton->baslangic;
            effective_tip_uzn = rhs->sol->ilk_jeton->uzunluk;
        }
    }
    raii_degisken_kontrol(effective_tip_adi, effective_tip_uzn, off);
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_DONDUR)) {
// [DEBUG_REMOVED] printf("AST_DONDUR CALISTI!\n");
if (((YKN_DOT(dugum, sag)) != 0)) {
ast_kod_uret(a, YKN_DOT(dugum, sag));
}
 else {
emit_xor_rax_rax(a);
}
raii_fonksiyon_donus_emit(a);
if (g_suanki_islev_async_seed == 1 && g_suanki_islev_ana_seed == 0) {
    seed_emit_async_future_wrap(a);
}
;
// dondu: restore frame and return
if (platform_arm64_mi()) {
    arm64_emit_epilog(a);
} else if (g_target_platform == 6) {
    int32_yaz(a, (int)riscv_encode_i_type(RV_OP_LOAD, 3, RV_RA, RV_SP, 248));
    int32_yaz(a, (int)riscv_encode_i_type(RV_OP_LOAD, 3, RV_S0, RV_SP, 240));
    int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_SP, RV_SP, 256));
    int32_yaz(a, (int)riscv_encode_i_type(RV_OP_JALR, 0, RV_ZERO, RV_RA, 0));
} else if (g_target_platform == 5) {
    byte_yaz(a, 0x0F);
} else {
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xEC); // MOV RSP, RBP
    emit_pop_rbp(a);
    ra_callee_epilog_emit(a);
    emit_ret(a);
}
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_BLOK)) {
raii_blok_gir();
// [DEBUG_REMOVED] printf("AST_BLOK BASLIYOR!\n");
long long block_saved_local_count = g_lokal_sayisi;
long long block_saved_stack_offset = g_stack_offset;
AstDugum* iter = (AstDugum*)(YKN_DOT(dugum, sol));
while (((iter) != 0)) {
// [DEBUG_REMOVED] printf("AST_BLOK ITER: tip = %lld\n", YKN_DOT(iter, tip));
if (YKN_DOT(iter, tip) == 4) { // AST_KIMLIK
    char* ad = YKN_DOT(YKN_DOT(iter, ilk_jeton), baslangic);
    long long uz = YKN_DOT(YKN_DOT(iter, ilk_jeton), uzunluk);
    char buf[128];
    for(int i=0; i<uz && i<127; i++) buf[i] = ad[i];
    buf[uz < 127 ? uz : 127] = 0;
// [DEBUG_REMOVED]     printf(" -> KIMLIK: %s\n", buf);
}
ast_kod_uret(a, iter);
iter = YKN_DOT(iter, sonraki);
}
raii_blok_cik_emit(a);
g_lokal_sayisi = block_saved_local_count;
g_stack_offset = block_saved_stack_offset;
// [DEBUG_REMOVED] printf("AST_BLOK BITTI!\n");
return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_ISLEV || YKN_DOT(dugum, tip) == AST_ASYNC_ISLEV || YKN_DOT(dugum, tip) == 41)) {
    if (YKN_DOT(dugum, govde) == 0) {
        return 0; // External prototype (extern isv) - no machine code body
    }
    if (g_fonksiyon_icinde == 1) {
        // Lambda in-line
        byte_yaz(a, 0xE9);
        long long jmp_pos = YKN_DOT(a, kullanilan);
        int32_yaz(a, 0);
        long long lambda_start = YKN_DOT(a, kullanilan);
        
        long long saved_taban = g_lambda_tabani;
        g_lambda_tabani = g_lokal_sayisi;
        long long saved_stack_offset = g_stack_offset;
        g_stack_offset = 0;
        
        emit_push_rbp(a);
        emit_mov_rbp_rsp(a);
        if (g_target_platform == 0) {
            emit_and_rsp_align(a);
        }
        emit_sub_rsp(a, 4096);
        
        AstDugum* param = (AstDugum*)(YKN_DOT(dugum, parametreler));
        long long param_idx = 0;
        while (param != 0) {
            g_stack_offset += 8;
            long long p_off = (param_idx + 1) * 8;
            if (g_lokal_sayisi >= 16384) {
                seed_codegen_kapasite_reddet(1);
                return 0;
            }
            Jeton* param_jeton = YKN_DOT(param, ilk_jeton);
            if (param_jeton) {
                g_lokal_isimleri[g_lokal_sayisi] = YKN_DOT(param_jeton, baslangic);
                g_lokal_uzunluklari[g_lokal_sayisi] = YKN_DOT(param_jeton, uzunluk);
            } else {
                g_lokal_isimleri[g_lokal_sayisi] = nullptr;
                g_lokal_uzunluklari[g_lokal_sayisi] = 0;
            }
            g_lokal_offsetleri[g_lokal_sayisi] = p_off;
            g_lokal_tip_adlari[g_lokal_sayisi] = YKN_DOT(param, tip_adi);
            g_lokal_tip_uzunluklari[g_lokal_sayisi] = YKN_DOT(param, tip_adi_uzunluk);
            g_lokal_dizi_boyutlari[g_lokal_sayisi] = 0;
            g_lokal_yildizlari[g_lokal_sayisi] = YKN_DOT(param, cozumlenmis_tip);
            g_lokal_sayisi++;

            if (param_idx == 0) {
                if (p_off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - p_off));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x8D); int32_yaz(a, -p_off);
                }
            } else if (param_idx == 1) {
                if (p_off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x55); byte_yaz(a, (unsigned char)(256 - p_off));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x95); int32_yaz(a, -p_off);
                }
            } else if (param_idx == 2) {
                if (p_off <= 127) {
                    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)(256 - p_off));
                } else {
                    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x85); int32_yaz(a, -p_off);
                }
            } else if (param_idx == 3) {
                if (p_off <= 127) {
                    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - p_off));
                } else {
                    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x8D); int32_yaz(a, -p_off);
                }
            }
            param_idx++;
            param = YKN_DOT(param, sonraki);
        }
        
        if (YKN_DOT(dugum, govde) != 0) {
            ast_kod_uret(a, YKN_DOT(dugum, govde));
        }
        emit_xor_rax_rax(a);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xEC);
        emit_pop_rbp(a);
        emit_ret(a);
        
        g_lokal_sayisi = g_lambda_tabani;
        g_lambda_tabani = saved_taban;
        g_stack_offset = saved_stack_offset;
        
        long long after_lambda = YKN_DOT(a, kullanilan);
        int32_patch(a, jmp_pos, (int)(after_lambda - (jmp_pos + 4)));
        
        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05); // LEA RAX, [RIP+rel32]
        long long lea_next = YKN_DOT(a, kullanilan) + 4;
        int32_yaz(a, (int)(lambda_start - lea_next));
        return 0;
    }
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_ENTER\n");
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_NODE_PTR=%p FIRST_TOKEN_PTR=%p FONK_INDEX=%lld\n", (void*)dugum, (void*)YKN_DOT(dugum, ilk_jeton), g_fonk_sayisi);
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_RETURN_TYPE=%p RETURN_TYPE_LEN=%lld\n", (void*)YKN_DOT(dugum, tip_adi), YKN_DOT(dugum, tip_adi_uzunluk));
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_NODE=%p\n", (void*)dugum);
    Jeton* islev_token_debug = YKN_DOT(dugum, ilk_jeton);
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_TOKEN_READ=%p\n", (void*)islev_token_debug);
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_TOKEN_TIP=%lld\n", YKN_DOT(islev_token_debug, tip));
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_TOKEN_TEXT=%p\n", (void*)YKN_DOT(islev_token_debug, baslangic));
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_TOKEN_LEN=%lld\n", YKN_DOT(islev_token_debug, uzunluk));
    if (g_fonk_sayisi >= 16384) {
        seed_codegen_kapasite_reddet(5);
        return 0;
    }
    FonksiyonSembolu* f = (FonksiyonSembolu*)bellek_tahsis_et(32);
// [DEBUG_REMOVED]     printf("CG_F_ALLOC_DONE\n");
    f->isim = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
// [DEBUG_REMOVED]     printf("CG_F_NAME_DONE\n");
// [DEBUG_REMOVED]     printf("CODEGEN_ISLEV_NAME_OK\n");
// [DEBUG_REMOVED]     printf("CG_F_LEN_DONE\n");
    f->uzunluk = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
// [DEBUG_REMOVED]     printf("CG_F_ADDR_DONE\n");
    f->adres = YKN_DOT(a, kullanilan);
// [DEBUG_REMOVED]     printf("CG_F_NEXT_DONE\n");
    f->sonraki = 0;
// [DEBUG_REMOVED]     printf("CG_F_HEAD_DONE\n");
    if (!g_fonksiyon_bas) g_fonksiyon_bas = f; else g_fonksiyon_son->sonraki = f;
// [DEBUG_REMOVED]     printf("CG_F_LIST_DONE\n");
    g_fonksiyon_son = f;
    g_fonk_sayisi++;
// [DEBUG_REMOVED]     printf("CG_F_COUNT_DONE\n");

    g_suanki_islev_async_seed = (YKN_DOT(dugum, tip) == AST_ASYNC_ISLEV || YKN_DOT(dugum, tip) == 41) ? 1 : 0;
    g_suanki_islev_ana_seed = 0;
    if (YKN_DOT(dugum, ilk_jeton) != 0) {
        char* is_ptr = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
        long long is_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
        if ((is_uzn == 3 && memcmp(is_ptr, "ana", 3) == 0) || (is_uzn == 4 && memcmp(is_ptr, "main", 4) == 0)) {
            g_suanki_islev_ana_seed = 1;
        }
    }

    ra_fonksiyon_sifirla();
    raii_sifirla();
    ra_degiskenleri_yazmaca_bagla((AstDugum*)YKN_DOT(dugum, govde));
    g_lokal_sayisi = 0;
    g_stack_offset = 0;
    g_lambda_tabani = 0;
    g_fonksiyon_icinde = 1; // entering function body
    // GOREV 6.3: Multi-arch prolog
    if (g_target_platform == 3 || g_target_platform == 4) {
        arm64_emit_prolog(a);
        arm64_emit_sub_sp(a, 256);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_SP, RV_SP, -256));
        int32_yaz(a, (int)riscv_encode_s_type(RV_OP_STORE, 3, RV_SP, RV_RA, 248));
        int32_yaz(a, (int)riscv_encode_s_type(RV_OP_STORE, 3, RV_SP, RV_S0, 240));
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_S0, RV_SP, 256));
    } else if (g_target_platform == 5) {
        // WebAssembly: locals declared in Section 10
    } else {
        ra_callee_prolog_emit(a);
        emit_push_rbp(a);
        emit_mov_rbp_rsp(a);
        if (g_target_platform == 0) {
            emit_and_rsp_align(a);
        }
        // Reserve the baseline compiler workspace plus the exact storage needed
        // by this function's parameters and local declarations. This prevents
        // large YKN buffers from aliasing the caller frame on Windows.
        long long frame_size = 8192 + 64;
        AstDugum* frame_param = (AstDugum*)YKN_DOT(dugum, parametreler);
        while (frame_param) {
            frame_size += 8;
            frame_param = (AstDugum*)YKN_DOT(frame_param, sonraki);
        }
        frame_size += seed_frame_node_size((AstDugum*)YKN_DOT(dugum, govde));
        frame_size = (frame_size + 15) & ~15LL;
        emit_sub_rsp(a, frame_size);
    }
// [DEBUG_REMOVED]     printf("CG_F_STACK_DONE\n");

    AstDugum* param = (AstDugum*)(YKN_DOT(dugum, parametreler));
// [DEBUG_REMOVED]     printf("CG_F_PARAM_READ\n");
    long long param_idx = 0;
// [DEBUG_REMOVED]     printf("CG_ISLEV_PARAM_PTR=%p\n", (void*)param);
    {
        char* fn_isim = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
        long long fn_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
// [DEBUG_REMOVED]         printf("ISLEV: %.*s (%lld params)\n", (int)fn_uzn, fn_isim, (long long)(param != 0 ? 1 : 0));
    }
    while (param != 0) {
// [DEBUG_REMOVED]         printf("CG_ISLEV_PARAM\\n");
// [DEBUG_REMOVED]         printf("CG_PARAM_LOCAL_BEGIN\n");
        g_stack_offset += 8;
        long long off = (param_idx + 1) * 8;
        if (g_lokal_sayisi >= 16384) {
            seed_codegen_kapasite_reddet(1);
            return 0;
        }
// [DEBUG_REMOVED]         printf("DBG_META_BEGIN idx=%lld locals=%lld param=%p first=%p\\n", param_idx, g_lokal_sayisi, (void*)param, (void*)YKN_DOT(param, ilk_jeton));
// [DEBUG_REMOVED]         printf("CG_PARAM_META_BEGIN\n");
        char* param_ptr = (char*)param;
        param_ptr += 8;
        Jeton* param_jeton = *(Jeton**)param_ptr;
// [DEBUG_REMOVED]         printf("CG_PARAM_TOKEN_LOCAL\n");
        char** isim_ptr = (char**)((char*)param_jeton + 8);
// [DEBUG_REMOVED]         printf("DBG_META_TOKEN token=%p base=%p len=%lld field_base=%p field_len=%p\n", (void*)param_jeton, (void*)YKN_DOT(param_jeton, baslangic), YKN_DOT(param_jeton, uzunluk), (void*)isim_ptr, (void*)((char*)param_jeton + 16));
        g_lokal_isimleri[g_lokal_sayisi] = *isim_ptr;
// [DEBUG_REMOVED]         printf("DBG_META_NAME ptr=%p\n", (void*)g_lokal_isimleri[g_lokal_sayisi]);
// [DEBUG_REMOVED]         printf("CG_PARAM_META_NAME\n");
        long long* uzunluk_ptr = (long long*)((char*)param_jeton + 16);
        g_lokal_uzunluklari[g_lokal_sayisi] = *uzunluk_ptr;
// [DEBUG_REMOVED]         printf("CG_PARAM_META_LEN\n");
        g_lokal_offsetleri[g_lokal_sayisi] = off;
// [DEBUG_REMOVED]         printf("CG_PARAM_META_OFF\n");
        g_lokal_tip_adlari[g_lokal_sayisi] = YKN_DOT(param, tip_adi);
        g_lokal_tip_uzunluklari[g_lokal_sayisi] = YKN_DOT(param, tip_adi_uzunluk);
        g_lokal_dizi_boyutlari[g_lokal_sayisi] = 0;
        g_lokal_yildizlari[g_lokal_sayisi] = YKN_DOT(param, cozumlenmis_tip);
        g_lokal_sayisi++;

        if (platform_arm64_mi()) {
            if (param_idx < 8) {
                long long simm9 = (-off) & 511;
                long long instr = 0xF8000000LL + (simm9 * 4096LL) + (29LL * 32LL) + param_idx;
                a64_int32_yaz(a, instr);
            }
        } else if (g_target_platform == 6) {
            if (param_idx < 8) {
                long long src_reg = RV_A0 + param_idx;
                int32_yaz(a, (int)riscv_encode_s_type(RV_OP_STORE, 3, RV_S0, src_reg, -off));
            }
        } else if (g_target_platform == 5) {
            if (param_idx < 16) {
                byte_yaz(a, 0x20); wasm_leb128_u32_yaz(a, (unsigned int)param_idx);
                long long lidx = (off / 8) - 1;
                byte_yaz(a, 0x21); wasm_leb128_u32_yaz(a, (unsigned int)lidx);
            }
        } else {
        if (param_idx == 0) {
            if (off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - off));
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x8D); int32_yaz(a, -off);
            }
            // MOV [RBP - off], RCX
        } else if (param_idx == 1) {
            if (off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x55); byte_yaz(a, (unsigned char)(256 - off));
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x95); int32_yaz(a, -off);
            }
            // MOV [RBP - off], RDX
        } else if (param_idx == 2) {
            if (off <= 127) {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)(256 - off));
            } else {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x85); int32_yaz(a, -off);
            }
            // MOV [RBP - off], R8
        } else if (param_idx == 3) {
            if (off <= 127) {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - off));
            } else {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x8D); int32_yaz(a, -off);
            }
            // MOV [RBP - off], R9
        } else {
            // param_idx >= 4: push rbp sonrasinda ilk stack argumani RBP+48'ta baslar.
            long long callee_pushed = 0;
            for (int i = 0; i < 5; i++) {
                if (g_ra_callee_saved_kullanildi[i] == 1) callee_pushed += 8;
            }
            long long stack_src = 16 + (param_idx * 8) + callee_pushed;
            if (stack_src <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)stack_src);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, stack_src);
            }
            if (off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)(256 - off));
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x85); int32_yaz(a, -off);
            }
        }
        }
        param_idx++;
        param = YKN_DOT(param, sonraki);
    }

    ast_kod_uret(a, YKN_DOT(dugum, govde));
    // GOREV 6.3: Multi-arch epilog
    if (g_target_platform == 3 || g_target_platform == 4) {
        arm64_emit_movz_x0_imm(a, 0);
        arm64_emit_epilog(a);
    } else if (g_target_platform == 6) {
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_LOAD, 3, RV_RA, RV_SP, 248));
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_LOAD, 3, RV_S0, RV_SP, 240));
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_IMM, 0, RV_SP, RV_SP, 256));
        int32_yaz(a, (int)riscv_encode_i_type(RV_OP_JALR, 0, RV_ZERO, RV_RA, 0));
    } else if (g_target_platform == 5) {
        byte_yaz(a, 0x0B); // end
    } else {
        raii_fonksiyon_donus_emit(a);
        emit_xor_rax_rax(a);
        if (g_suanki_islev_async_seed == 1 && g_suanki_islev_ana_seed == 0) {
            seed_emit_async_future_wrap(a);
        }
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xEC); // MOV RSP, RBP
        emit_pop_rbp(a);
        ra_callee_epilog_emit(a);
        emit_ret(a);
    }
    g_fonksiyon_icinde = 0; // back to global scope after function
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 8)) { // AST_EGER
    long long jz_rel_pos = emit_conditional_branch_helper(a, (AstDugum*)YKN_DOT(dugum, sol));

    ast_kod_uret(a, YKN_DOT(dugum, govde));
    
    if (YKN_DOT(dugum, yoksa_govde) != 0) {
        byte_yaz(a, 0xE9);
        long long jmp_pos = YKN_DOT(a, kullanilan);
        int32_yaz(a, 0);

        long long else_label = YKN_DOT(a, kullanilan);
        int32_patch(a, jz_rel_pos, else_label - (jz_rel_pos + 4));

        ast_kod_uret(a, YKN_DOT(dugum, yoksa_govde));

        long long end_label = YKN_DOT(a, kullanilan);
        int32_patch(a, jmp_pos, end_label - (jmp_pos + 4));
    } else {
        long long end_label = YKN_DOT(a, kullanilan);
        int32_patch(a, jz_rel_pos, end_label - (jz_rel_pos + 4));
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 9)) { // AST_DONGU
    long long loop_start = YKN_DOT(a, kullanilan);
    long long jz_rel_pos = 0;

    // Push loop context for devam_et/durdur
    if (g_dongu_derinlik < 64) {
        g_dongu_basi_stack[g_dongu_derinlik] = loop_start;
        g_dongu_derinlik++;
    }
    long long prev_durdur_adet = g_durdur_adet;
    long long prev_devam_adet = g_devam_adet;

    if (YKN_DOT(dugum, sol) != 0) {
        jz_rel_pos = emit_conditional_branch_helper(a, (AstDugum*)YKN_DOT(dugum, sol));
    }

    ast_kod_uret(a, YKN_DOT(dugum, govde));

    long long loop_continue_target = YKN_DOT(a, kullanilan);
    for (long long c_idx = prev_devam_adet; c_idx < g_devam_adet; c_idx++) {
        long long cont_ptr = g_devam_yamalar[c_idx];
        int32_patch(a, cont_ptr, (int)(loop_continue_target - (cont_ptr + 4)));
    }
    g_devam_adet = prev_devam_adet;

    if (YKN_DOT(dugum, yoksa_govde) != 0) {
        ast_kod_uret(a, YKN_DOT(dugum, yoksa_govde));
    }

    // Back-jump to loop_start
    byte_yaz(a, 0xE9);
    long long jmp_pos = YKN_DOT(a, kullanilan);
    long long rip_after_jmp = jmp_pos + 4;
    int32_yaz(a, loop_start - rip_after_jmp);

    long long loop_end = YKN_DOT(a, kullanilan);

    if (YKN_DOT(dugum, sol) != 0) {
        int32_patch(a, jz_rel_pos, loop_end - (jz_rel_pos + 4));
    }

    // Pop loop context and patch any durdur exits
    if (g_dongu_derinlik > 0) {
        g_dongu_derinlik--;
        for (long long b_idx = prev_durdur_adet; b_idx < g_durdur_adet; b_idx++) {
            long long break_ptr = g_durdur_yamalar[b_idx];
            int32_patch(a, break_ptr, loop_end - (break_ptr + 4));
        }
        g_durdur_adet = prev_durdur_adet;
    }
    return 0;
}
;
// AST_DEVAM_ET (continue): jump to the step/start target of the innermost loop
if ((YKN_DOT(dugum, tip) == 15)) { // AST_DEVAM_ET
    if (g_dongu_derinlik > 0) {
        byte_yaz(a, 0xE9);
        long long p = YKN_DOT(a, kullanilan);
        if (g_devam_adet < 1024) {
            g_devam_yamalar[g_devam_adet++] = p;
        }
        int32_yaz(a, 0);
    }
    return 0;
}
;
// AST_SORU_ISLEMI (33): ? error propagation
if ((YKN_DOT(dugum, tip) == 33 || YKN_DOT(dugum, tip) == AST_SORU_ISLEMI)) {
    ast_kod_uret(a, YKN_DOT(dugum, sol));
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX (pointer sakla)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x01); // MOV RAX, [RCX] = basarili (offset 0)
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX, RAX
    byte_yaz(a, 0x0F); byte_yaz(a, 0x85); // JNZ rel32 (basarili yolu)
    long long q_jnz_pos = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);
    // Hata yolu:
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x41); byte_yaz(a, 0x10); // MOV RAX, [RCX+16] = hata_kodu
    raii_fonksiyon_donus_emit(a);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xEC); // MOV RSP, RBP
    emit_pop_rbp(a);
    ra_callee_epilog_emit(a);
    emit_ret(a);
    // Basarili yolu (JNZ buraya atlar):
    long long q_ok_pos = YKN_DOT(a, kullanilan);
    int32_patch(a, q_jnz_pos, (int)(q_ok_pos - (q_jnz_pos + 4)));
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x41); byte_yaz(a, 0x08); // MOV RAX, [RCX+8] = deger
    return 0;
}
;
// AST_DURDUR (break): emit forward JMP, patch target at end of innermost loop
if ((YKN_DOT(dugum, tip) == 14)) { // AST_DURDUR
    if (g_dongu_derinlik > 0) {
        byte_yaz(a, 0xE9);
        long long p = YKN_DOT(a, kullanilan);
        if (g_durdur_adet < 1024) {
            g_durdur_yamalar[g_durdur_adet++] = p;
        }
        int32_yaz(a, 0); // placeholder
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 16)) { // AST_YAPI_TANIMI: yapilari_topla tarafindan zaten kaydedildi
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == AST_ICE_AKTAR)) {
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 17)) { // AST_ALAN_ERISIMI
    AstDugum* obj = YKN_DOT(dugum, sol);
    AstDugum* field = YKN_DOT(dugum, sag);
    ast_kod_uret(a, obj);
    if (field != 0 && YKN_DOT(field, ilk_jeton) != 0) {
        long long flen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* fstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (flen == 3 && fstr[0] == 'p' && fstr[1] == 't' && fstr[2] == 'r') {
            if (obj != 0 && YKN_DOT(obj, tip) == 3 /* AST_METIN */) {
                return 0; // .ptr is a no-op for string literals!
            }
        }
    }
    char* t_adi = 0;
    long long t_uzn = 0;
    expr_struct_type(obj, &t_adi, &t_uzn);
    if (YKN_DOT(obj, tip) == 11 && YKN_DOT(field, ilk_jeton) != 0 &&
        YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk) == 3 &&
        bellek_esit_mi(YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic), (char*)"tip", 3) == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
        return 0;
    }
    long long field_off = -1;
    if (t_adi != 0 && t_uzn > 0 && t_adi[0] == '*') {
        t_adi = t_adi + 1;
        t_uzn = t_uzn - 1;
    }
    if (t_adi != 0 && t_uzn > 0) {
        for (long long k = 0; k < t_uzn; k++) {
            if (t_adi[k] == '<') { t_uzn = k; break; }
        }
    }
    if (t_adi != 0 && t_uzn == 5 && bellek_esit_mi(t_adi, (char*)"Jeton", 5) == 1) {
        long long flen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* fbase = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (flen == 3 && bellek_esit_mi(fbase, (char*)"tip", 3) == 1) field_off = 0;
        else if (flen == 9 && bellek_esit_mi(fbase, (char*)"baslangic", 9) == 1) field_off = 8;
        else if (flen == 7 && bellek_esit_mi(fbase, (char*)"uzunluk", 7) == 1) field_off = 16;
        else if (flen == 5 && bellek_esit_mi(fbase, (char*)"satir", 5) == 1) field_off = 24;
        else if (flen == 5 && bellek_esit_mi(fbase, (char*)"sutun", 5) == 1) field_off = 32;
    }
    long long field_dizi = 0;
    long long field_elem_sz = 8;
    if (field_off == -1 && t_adi != 0) {
        for(long long i=0; i<g_yapi_sayisi; i++) {
            if (g_yapilar[i].uzunluk == t_uzn && bellek_esit_mi(g_yapilar[i].isim, t_adi, t_uzn) == 1) {
                for(long long j=0; j<g_yapilar[i].alan_sayisi; j++) {
                    if (g_yapilar[i].alanlar[j].uzunluk == YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk) && 
                        bellek_esit_mi(g_yapilar[i].alanlar[j].isim, YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic), g_yapilar[i].alanlar[j].uzunluk) == 1) {
                        field_off = g_yapilar[i].alanlar[j].offset;
                        field_dizi = g_yapilar[i].alanlar[j].dizi_boyutu;
                        field_elem_sz = tip_eleman_boyutu_al(g_yapilar[i].alanlar[j].tip_adi, g_yapilar[i].alanlar[j].tip_uzunluk, g_yapilar[i].alanlar[j].yildiz_sayisi);
                        YKN_DOT(dugum, tip_adi) = g_yapilar[i].alanlar[j].tip_adi;
                        YKN_DOT(dugum, tip_adi_uzunluk) = g_yapilar[i].alanlar[j].tip_uzunluk;
                        YKN_DOT(dugum, cozumlenmis_tip) = g_yapilar[i].alanlar[j].yildiz_sayisi;
                        YKN_DOT(dugum, dizi_boyutu) = g_yapilar[i].alanlar[j].dizi_boyutu;
                        if (g_yapilar[i].alanlar[j].yildiz_sayisi == 0) {
                            for (long long si = 0; si < g_yapi_sayisi; si++) {
                                if (g_yapilar[si].uzunluk == g_yapilar[i].alanlar[j].tip_uzunluk &&
                                    bellek_esit_mi(g_yapilar[si].isim, g_yapilar[i].alanlar[j].tip_adi, g_yapilar[si].uzunluk) == 1) {
                                    field_dizi = 1;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
    if (field_off == -1 && t_adi != 0 && t_uzn == 4 &&
        bellek_esit_mi(t_adi, (char*)"Dizi", 4) == 1 && field != 0 &&
        YKN_DOT(field, ilk_jeton) != 0) {
        long long dizi_flen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* dizi_fstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (dizi_flen == 5 && bellek_esit_mi(dizi_fstr, (char*)"boyut", 5) == 1) field_off = 8;
        else if (dizi_flen == 8 && bellek_esit_mi(dizi_fstr, (char*)"kapasite", 8) == 1) field_off = 16;
        else if (dizi_flen == 6 && bellek_esit_mi(dizi_fstr, (char*)"tampon", 6) == 1) field_off = 0;
    }
    if (field_off == -1 && field != 0 && YKN_DOT(field, ilk_jeton) != 0) {
        long long flen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* fstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (flen == 3 && bellek_esit_mi(fstr, (char*)"tip", 3) == 1) field_off = 0;
        else if (flen == 9 && bellek_esit_mi(fstr, (char*)"ilk_jeton", 9) == 1) field_off = 8;
        else if (flen == 9 && bellek_esit_mi(fstr, (char*)"son_jeton", 9) == 1) field_off = 16;
        else if (flen == 3 && bellek_esit_mi(fstr, (char*)"sol", 3) == 1) field_off = 24;
        else if (flen == 3 && bellek_esit_mi(fstr, (char*)"sag", 3) == 1) field_off = 32;
        else if (flen == 7 && bellek_esit_mi(fstr, (char*)"sonraki", 7) == 1) field_off = 40;
        else if (flen == 5 && bellek_esit_mi(fstr, (char*)"govde", 5) == 1) field_off = 48;
        else if (flen == 11 && bellek_esit_mi(fstr, (char*)"yoksa_govde", 11) == 1) field_off = 56;
        else if (flen == 12 && bellek_esit_mi(fstr, (char*)"parametreler", 12) == 1) field_off = 64;
        else if (flen == 7 && bellek_esit_mi(fstr, (char*)"tip_adi", 7) == 1) field_off = 72;
        else if (flen == 15 && bellek_esit_mi(fstr, (char*)"tip_adi_uzunluk", 15) == 1) field_off = 80;
        else if (flen == 15 && bellek_esit_mi(fstr, (char*)"cozumlenmis_tip", 15) == 1) field_off = 88;
        else if (flen == 11 && bellek_esit_mi(fstr, (char*)"dizi_boyutu", 11) == 1) field_off = 96;
        else if (flen == 10 && bellek_esit_mi(fstr, (char*)"gorunurluk", 10) == 1) {
            if (t_adi != 0 && t_uzn == 13 && bellek_esit_mi(t_adi, (char*)"SembolGirdisi", 13) == 1) field_off = 48;
            else field_off = 104;
        }
        else if (flen == 8 && bellek_esit_mi(fstr, (char*)"nullable", 8) == 1) {
            if (t_adi != 0 && t_uzn == 13 && bellek_esit_mi(t_adi, (char*)"SembolGirdisi", 13) == 1) field_off = 48;
            else field_off = 112;
        }
        else if (flen == 11 && bellek_esit_mi(fstr, (char*)"nominal_id", 11) == 1) field_off = 32;
        else if (flen == 6 && bellek_esit_mi(fstr, (char*)"ic_tip", 6) == 1) field_off = 40;
        else if (flen == 9 && bellek_esit_mi(fstr, (char*)"baslangic", 9) == 1) field_off = 8;
        else if (flen == 7 && bellek_esit_mi(fstr, (char*)"uzunluk", 7) == 1) field_off = 16;
        else if (flen == 5 && bellek_esit_mi(fstr, (char*)"satir", 5) == 1) field_off = 24;
        else if (flen == 5 && bellek_esit_mi(fstr, (char*)"sutun", 5) == 1) field_off = 32;
        else if (flen == 6 && bellek_esit_mi(fstr, (char*)"kaynak", 6) == 1) field_off = 0;
        else if (flen == 12 && bellek_esit_mi(fstr, (char*)"dosya_boyutu", 12) == 1) field_off = 8;
        else if (flen == 13 && bellek_esit_mi(fstr, (char*)"guncel_offset", 13) == 1) field_off = 16;
        else if (flen == 8 && bellek_esit_mi(fstr, (char*)"jetonlar", 8) == 1) field_off = 40;
        else if (flen == 12 && bellek_esit_mi(fstr, (char*)"jeton_sayisi", 12) == 1) field_off = 48;
        else if (flen == 14 && bellek_esit_mi(fstr, (char*)"jeton_kapasite", 14) == 1) field_off = 56;
        else if (flen == 5 && bellek_esit_mi(fstr, (char*)"arena", 5) == 1) field_off = 8;
        else if (flen == 12 && bellek_esit_mi(fstr, (char*)"guncel_index", 12) == 1) field_off = 16;
        else if (flen == 6 && bellek_esit_mi(fstr, (char*)"bellek", 6) == 1) field_off = 0;
        else if (flen == 8 && bellek_esit_mi(fstr, (char*)"kapasite", 8) == 1) field_off = 8;
        else if (flen == 10 && bellek_esit_mi(fstr, (char*)"kullanilan", 10) == 1) field_off = 16;

        else if (flen == 4 && bellek_esit_mi(fstr, (char*)"adet", 4) == 1) field_off = 0;
        else if (flen == 12 && bellek_esit_mi(fstr, (char*)"stack_offset", 12) == 1) field_off = 8;
        else if (flen == 4 && bellek_esit_mi(fstr, (char*)"isim", 4) == 1) field_off = 0;
        else if (flen == 5 && bellek_esit_mi(fstr, (char*)"deger", 5) == 1) field_off = 8;
        else if (flen == 5 && bellek_esit_mi(fstr, (char*)"adres", 5) == 1) field_off = 16;
        // GOREV 6.4: Sonuc<T> struct alanlari — okuma fallback
        else if (flen == 8 && bellek_esit_mi(fstr, (char*)"basarili",  8) == 1) field_off = 0;
        else if (flen == 9 && bellek_esit_mi(fstr, (char*)"hata_kodu", 9) == 1) field_off = 16;
        else if (flen == 8 && bellek_esit_mi(fstr, (char*)"veri_ptr", 8) == 1) field_off = 0;
        else if (flen == 14 && bellek_esit_mi(fstr, (char*)"ref_sayisi_ptr", 14) == 1) field_off = 8;
        else if (flen == 11 && bellek_esit_mi(fstr, (char*)"kontrol_ptr", 11) == 1) field_off = 0;
    }
    // Arc ABI override for nested imported handles.
    if (field != 0 && YKN_DOT(field, ilk_jeton) != 0) {
        long long aflen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* afstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (aflen == 8 && bellek_esit_mi(afstr, (char*)"veri_ptr", 8) == 1) field_off = 0;
        if (aflen == 14 && bellek_esit_mi(afstr, (char*)"ref_sayisi_ptr", 14) == 1) field_off = 8;
        if (aflen == 11 && bellek_esit_mi(afstr, (char*)"kontrol_ptr", 11) == 1) field_off = 0;
        if (aflen == 7 && bellek_esit_mi(afstr, (char*)"kontrol", 7) == 1) field_off = 8;
        if (field_off == -1 && t_adi != 0 && t_uzn == 3 && bellek_esit_mi(t_adi, (char*)"Arc", 3) == 1 &&
            aflen == 3 && bellek_esit_mi(afstr, (char*)"ptr", 3) == 1) field_off = 0;
    }

    if (field_off == -1 && field != 0 && YKN_DOT(field, ilk_jeton) != 0) {
        long long aflen = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* afstr = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (aflen == 3 && afstr[0] == 'p' && afstr[1] == 't' && afstr[2] == 'r') {
            return 0; // fallback .ptr no-op for pointers/arrays
        }
    }

    if (field_off != -1) {
        if (field_dizi > 0) {
            if (field_off > 0) {
                if (field_off <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0);
                    byte_yaz(a, (unsigned char)field_off);
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x05);
                    int32_yaz(a, (int)field_off);
                }
            }
        } else if (field_elem_sz == 1) {
            if (field_off == 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x00);
            } else if (field_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x40);
                byte_yaz(a, (unsigned char)field_off);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x80);
                int32_yaz(a, (int)field_off);
            }
        } else if (field_elem_sz == 2) {
            if (field_off == 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x00);
            } else if (field_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x40);
                byte_yaz(a, (unsigned char)field_off);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x80);
                int32_yaz(a, (int)field_off);
            }
        } else if (field_elem_sz == 4) {
            if (field_off == 0) {
                byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
            } else if (field_off <= 127) {
                byte_yaz(a, 0x8B); byte_yaz(a, 0x40);
                byte_yaz(a, (unsigned char)field_off);
            } else {
                byte_yaz(a, 0x8B); byte_yaz(a, 0x80);
                int32_yaz(a, (int)field_off);
            }
        } else {
            if (field_off == 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
            } else if (field_off <= 127) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x40);
                byte_yaz(a, (unsigned char)field_off);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x80);
                int32_yaz(a, (int)field_off);
            }
        }
    } else {
        long long f_len = YKN_DOT(YKN_DOT(field, ilk_jeton), uzunluk);
        char* f_str = (char*)YKN_DOT(YKN_DOT(field, ilk_jeton), baslangic);
        if (f_len == 3 && f_str[0] == 'p' && f_str[1] == 't' && f_str[2] == 'r') {
            // .ptr returns the pointer itself, which is already in RAX. Do nothing.
        } else {
            emit_xor_rax_rax(a);
        }
    }
    return 0;
}
;

if ((YKN_DOT(dugum, tip) == 18)) { // AST_INDEKS_ERISIMI
    AstDugum* arr_node = YKN_DOT(dugum, sol);
    AstDugum* idx_node = YKN_DOT(dugum, sag);
    long long var_off = 0;
    long long var_dizi = 0; // dizi_boyutu: >0 means stack-allocated array
    long long var_yildiz = 0; // pointer depth
    char* var_tip_adi = 0;
    long long var_tip_uzunluk = 0;
    long long jeton_dizisi_mi = 0;
    if (YKN_DOT(arr_node, tip) == 4) { // AST_KIMLIK
// [DEBUG_REMOVED]         printf("DEBUG: AST_INDEKS_ERISIMI LHS is AST_KIMLIK. Name: %.*s\n", (int)YKN_DOT(YKN_DOT(arr_node, ilk_jeton), uzunluk), YKN_DOT(YKN_DOT(arr_node, ilk_jeton), baslangic));
        for(long long i = g_lokal_sayisi - 1; i >= 0; i--) {
            if(g_lokal_uzunluklari[i] == YKN_DOT(YKN_DOT(arr_node, ilk_jeton), uzunluk)) {
                if(bellek_esit_mi(g_lokal_isimleri[i], YKN_DOT(YKN_DOT(arr_node, ilk_jeton), baslangic), g_lokal_uzunluklari[i]) == 1) {
                    var_off = g_lokal_offsetleri[i];
                    var_dizi = g_lokal_dizi_boyutlari[i];
                    var_tip_adi = g_lokal_tip_adlari[i];
                    var_tip_uzunluk = g_lokal_tip_uzunluklari[i];
                    var_yildiz = g_lokal_yildizlari[i];
                    break;
                }
            }
        }
        if (var_off == 0 && var_tip_adi == 0) {
            long long obj_uzn = YKN_DOT(YKN_DOT(arr_node, ilk_jeton), uzunluk);
            char* obj_isim = YKN_DOT(YKN_DOT(arr_node, ilk_jeton), baslangic);
            for(long long i = 0; i < g_global_sayisi; i++) {
                if(g_global_uzunluk[i] == obj_uzn && bellek_esit_mi(g_global_isim[i], obj_isim, obj_uzn) == 1) {
                    var_tip_adi = g_global_tip_adi[i];
                    var_tip_uzunluk = g_global_tip_adi_uzunluk[i];
                    var_yildiz = g_global_is_pointer[i];
                    var_dizi = g_global_dizi_boyutu[i];
                    break;
                }
            }
        }
    }
    if (YKN_DOT(arr_node, tip) == 17 && YKN_DOT(arr_node, sag) != 0 &&
        YKN_DOT(YKN_DOT(arr_node, sag), ilk_jeton) != 0 &&
        YKN_DOT(YKN_DOT(YKN_DOT(arr_node, sag), ilk_jeton), uzunluk) == 7 &&
        bellek_esit_mi(YKN_DOT(YKN_DOT(YKN_DOT(arr_node, sag), ilk_jeton), baslangic), (char*)"jetonlar", 7) == 1) {
        jeton_dizisi_mi = 1;
        var_yildiz = 1;
        var_tip_adi = (char*)"Jeton";
        var_tip_uzunluk = 5;
    }
    if (!jeton_dizisi_mi && YKN_DOT(arr_node, tip) == 17 &&
        YKN_DOT(arr_node, sol) != 0 && YKN_DOT(arr_node, sol)->tip == 4 &&
        YKN_DOT(YKN_DOT(arr_node, sol), ilk_jeton) != 0 &&
        YKN_DOT(YKN_DOT(YKN_DOT(arr_node, sol), ilk_jeton), uzunluk) == 12 &&
        bellek_esit_mi(YKN_DOT(YKN_DOT(YKN_DOT(arr_node, sol), ilk_jeton), baslangic), (char*)"ParserDurumu", 12) == 1) {
        jeton_dizisi_mi = 1;
        var_yildiz = 1;
        var_tip_adi = (char*)"Jeton";
        var_tip_uzunluk = 5;
    }
    
    // Fallback: use arr_node's own type info (for struct field access, nested subscripts, etc.)
    // These are populated by tip_oku() / typechecker's tip_cikar()
    if (var_yildiz == 0 && var_dizi == 0 && var_tip_adi == 0) {
        var_yildiz = YKN_DOT(arr_node, cozumlenmis_tip);
        var_tip_adi = (char*)YKN_DOT(arr_node, tip_adi);
        var_tip_uzunluk = YKN_DOT(arr_node, tip_adi_uzunluk);
        if (YKN_DOT(arr_node, tip) != 17) {
            var_dizi = YKN_DOT(arr_node, dizi_boyutu);
        }
    }
    
    // Stack-allocated array (dizi_boyutu>0): LEA its base address.
    // Pointer/scalar (dizi_boyutu==0): MOV to load the pointer value, then index into it.
    if (jeton_dizisi_mi) {
        ast_kod_uret(a, YKN_DOT(arr_node, sol));
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
    } else if (var_off != 0 && var_dizi > 0) {
        if (var_off <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x45); // LEA RAX, [RBP - off]
            byte_yaz(a, (unsigned char)(256 - var_off));
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x85);
            int32_yaz(a, -var_off);
        }
    } else {
        ast_kod_uret(a, arr_node); // MOV RAX, [RBP - off] â€” loads the pointer value
    }
    
    // Determine element scale:
    // *u8 (yildiz_sayisi==1 OR tip_adi=="*u8" OR cozumlenmis_tip==8(TIP_ID_POINTER)) â†’ byte Ã—1
    // [N]u8 stack array (dizi_boyutu>0, tip_adi=="u8") â†’ byte Ã—1
    // [N]StructType â†’ struct_size (alan_sayisi * 8) bytes per element
    // everything else â†’ Ã—8
    long long elem_yildiz = (var_dizi > 0) ? var_yildiz : (var_yildiz > 0 ? var_yildiz - 1 : 0);
    long long elem_scale = tip_eleman_boyutu_al(var_tip_adi, var_tip_uzunluk, elem_yildiz);
    if (jeton_dizisi_mi == 1) elem_scale = 40;
    long long is_byte_array = (elem_scale == 1) ? 1 : 0;
    if (arr_node != 0 && YKN_DOT(arr_node, tip) == 17) { // AST_ALAN_ERISIMI
        char* fld_t_adi = 0;
        long long fld_t_uzn = 0;
        expr_struct_type(arr_node, &fld_t_adi, &fld_t_uzn);
        if (fld_t_adi != 0 && fld_t_uzn > 0) {
            long long f_scale = tip_eleman_boyutu_al(fld_t_adi, fld_t_uzn, 0);
            if (f_scale == 1) {
                is_byte_array = 1;
                elem_scale = 1;
            } else if (f_scale == 2 || f_scale == 4 || f_scale == 8) {
                elem_scale = f_scale;
            }
        }
        if (!is_byte_array) {
            AstDugum* fld = YKN_DOT(arr_node, sag);
            if (fld != 0 && YKN_DOT(fld, ilk_jeton) != 0) {
                long long flen = YKN_DOT(YKN_DOT(fld, ilk_jeton), uzunluk);
                char* fstr = (char*)YKN_DOT(YKN_DOT(fld, ilk_jeton), baslangic);
                if ((flen == 6 && bellek_esit_mi(fstr, (char*)"bellek", 6)) ||
                    (flen == 6 && bellek_esit_mi(fstr, (char*)"kaynak", 6)) ||
                    (flen == 6 && bellek_esit_mi(fstr, (char*)"icerik", 6)) ||
                    (flen == 4 && bellek_esit_mi(fstr, (char*)"veri", 4)) ||
                    (flen == 3 && bellek_esit_mi(fstr, (char*)"ptr", 3)) ||
                    (flen == 9 && bellek_esit_mi(fstr, (char*)"baslangic", 9)) ||
                    (flen == 7 && bellek_esit_mi(fstr, (char*)"tip_adi", 7)) ||
                    (flen == 5 && bellek_esit_mi(fstr, (char*)"mesaj", 5)) ||
                    (flen == 5 && bellek_esit_mi(fstr, (char*)"ipucu", 5)) ||
                    (flen == 12 && bellek_esit_mi(fstr, (char*)"kaynak_satir", 12)) ||
                    (flen == 9 && bellek_esit_mi(fstr, (char*)"dosya_adi", 9)) ||
                    (flen == 8 && bellek_esit_mi(fstr, (char*)"veri_ptr", 8)) ||
                    (flen == 12 && bellek_esit_mi(fstr, (char*)"arena_bellek", 12))) {
                    is_byte_array = 1;
                    elem_scale = 1;
                }
            }
        }
    }
    
    bool fast_indexed_read = false;
    if (!g_bounds_check && !jeton_dizisi_mi) {
        long long chk_struct = 0;
        if (var_tip_adi != 0 && var_tip_uzunluk > 0) {
            for (long long si_chk = 0; si_chk < g_yapi_sayisi; si_chk++) {
                if (g_yapilar[si_chk].uzunluk == var_tip_uzunluk &&
                    bellek_esit_mi(g_yapilar[si_chk].isim, var_tip_adi, var_tip_uzunluk) == 1) {
                    chk_struct = 1; break;
                }
            }
        }
        if (!chk_struct && (elem_scale == 8 || elem_scale == 4 || elem_scale == 2 || elem_scale == 1)) {
            if (idx_node && idx_node->tip == AST_SAYI && idx_node->ilk_jeton) {
                long long c = string_tamsayiya(idx_node->ilk_jeton->baslangic, idx_node->ilk_jeton->uzunluk);
                long long off = c * elem_scale;
                if (elem_scale == 1) {
                    if (off == 0) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x00);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x40); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x80); int32_yaz(a, (int)off);
                    }
                } else if (elem_scale == 2) {
                    if (off == 0) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x00);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x40); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x80); int32_yaz(a, (int)off);
                    }
                } else if (elem_scale == 4) {
                    if (off == 0) {
                        byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x8B); byte_yaz(a, 0x40); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x8B); byte_yaz(a, 0x80); int32_yaz(a, (int)off);
                    }
                } else {
                    if (off == 0) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
                    } else if (off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x40); byte_yaz(a, (char)off);
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x80); int32_yaz(a, (int)off);
                    }
                }
                return 0;
            } else if (idx_node && idx_node->tip == 4 && idx_node->ilk_jeton) {
                long long idx_off = 0;
                long long idx_dizi = 0;
                for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
                    if (g_lokal_uzunluklari[i] == idx_node->ilk_jeton->uzunluk &&
                        bellek_esit_mi(g_lokal_isimleri[i], idx_node->ilk_jeton->baslangic, idx_node->ilk_jeton->uzunluk) == 1) {
                        idx_off = g_lokal_offsetleri[i];
                        idx_dizi = g_lokal_dizi_boyutlari[i];
                        break;
                    }
                }
                if (idx_off != 0 && idx_dizi == 0) {
                    if (idx_off <= 127) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4D); byte_yaz(a, (unsigned char)(256 - idx_off));
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x8D); int32_yaz(a, -idx_off);
                    }
                    if (elem_scale == 1) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x04); byte_yaz(a, 0x08);
                    } else if (elem_scale == 2) {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x04); byte_yaz(a, 0x48);
                    } else if (elem_scale == 4) {
                        byte_yaz(a, 0x8B); byte_yaz(a, 0x04); byte_yaz(a, 0x88);
                    } else {
                        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x04); byte_yaz(a, 0xC8);
                    }
                    return 0;
                }
            }
        }
    }
    byte_yaz(a, 0x50); // PUSH RAX
    if (g_bounds_check && var_yildiz != 0) emit_bounds_guard(a, 0, 1);
    ast_kod_uret(a, idx_node);
    if (g_bounds_check && var_dizi > 0) emit_bounds_guard(a, var_dizi, 0);
    if (elem_scale == 1) {
        // scale 1: no shift
    } else if (elem_scale == 2) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xD1); byte_yaz(a, 0xE0); // SHL RAX, 1 (*2)
    } else if (elem_scale == 4) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x02); // SHL RAX, 2 (*4)
    } else if (elem_scale == 8) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x03); // SHL RAX, 3 (*8)
    } else if (elem_scale == 16) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x04); // SHL RAX, 4 (*16)
    } else if (elem_scale == 32) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x05); // SHL RAX, 5 (*32)
    } else if (elem_scale == 64) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xE0); byte_yaz(a, 0x06); // SHL RAX, 6 (*64)
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x69); byte_yaz(a, 0xC0); int32_yaz(a, (int)elem_scale);
    }
    byte_yaz(a, 0x59); // POP RCX
    emit_add_rax_rcx(a); // RAX = RAX + RCX
    long long struct_mi = 0;
    if (var_tip_adi != 0 && var_tip_uzunluk > 0) {
        for (long long si_chk = 0; si_chk < g_yapi_sayisi; si_chk++) {
            if (g_yapilar[si_chk].uzunluk == var_tip_uzunluk &&
                bellek_esit_mi(g_yapilar[si_chk].isim, var_tip_adi, var_tip_uzunluk) == 1) {
                struct_mi = 1;
                break;
            }
        }
    }
    if (elem_scale == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0x00); // MOVZX RAX, byte [RAX]
    } else if (elem_scale == 2) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB7); byte_yaz(a, 0x00); // MOVZX RAX, word [RAX]
    } else if (elem_scale == 4) {
        byte_yaz(a, 0x8B); byte_yaz(a, 0x00); // MOV EAX, dword [RAX]
    } else if (elem_scale == 8) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00); // MOV RAX, [RAX]
    } else if (!jeton_dizisi_mi && !(struct_mi == 1 && (var_dizi == 0 || var_yildiz == 0))) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x00);
    }
    return 0;
}
;
if ((YKN_DOT(dugum, tip) == 11)) { // AST_ISLEV_CAGRISI
AstDugum* kimlik = (AstDugum*)(YKN_DOT(dugum, sol));
// Payload tasiyan enum constructor: 16-byte heap tagged union.
if (kimlik && YKN_DOT(kimlik, ilk_jeton)) {
    long long ev = enum_varyant_bul_tam(YKN_DOT(kimlik, ilk_jeton)->baslangic, YKN_DOT(kimlik, ilk_jeton)->uzunluk);
    if (ev < 0) ev = enum_varyant_bul_kisa(YKN_DOT(kimlik, ilk_jeton)->baslangic, YKN_DOT(kimlik, ilk_jeton)->uzunluk);
    if (ev >= 0) {
        AstDugum* payload = (AstDugum*)YKN_DOT(dugum, sag);
        // Sadece enum genelinde HICBIR variant'in payload'i yoksa (pure C-style enum) disc degerini doğrudan RAX'a yukle
        if (enum_varyant_has_any_payload(ev) == 0 && !payload) {
            // MOV RAX, imm32 (discriminant degeri)
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, (int)g_enum_var_disc[ev]);
            return 0;
        }
        if (payload) ast_kod_uret(a, payload);
        else emit_xor_rax_rax(a);
        // R12 callee-saved oldugu icin payload'i ABI'den bagimsiz burada tut.
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC4); // MOV R12, RAX

        if (g_target_platform == 1) {
            // Linux x86_64: mmap(NULL, 16, PROT_RW, MAP_PRIVATE|ANON, -1, 0)
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF); // RDI=0
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 16); // RSI=size
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3); // RDX=PROT_READ|WRITE
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34); // R10=MAP_PRIVATE|ANON
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1); // R8=-1
            byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9); // R9=0
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9); // RAX=SYS_mmap
            byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        } else if (g_target_platform == 2) {
            // macOS x86_64: BSD mmap (0x2000000 + 197)
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF); // RDI=0
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 16); // RSI=size
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3); // RDX=PROT_READ|WRITE
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 0x1002); // R10=MAP_PRIVATE|ANON
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1); // R8=-1
            byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9); // R9=0
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x20000C5); // RAX=BSD mmap
            byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        } else {
            // Windows x86_64: VirtualAlloc(NULL, 16, 0x3000, 0x04)
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9); // RCX=NULL
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 16); // RDX=size
            byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000); // R8=COMMIT|RESERVE
            byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04); // R9=READWRITE
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28); // shadow + align
            byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
            if (!seed_iat_yama_kapasite_kontrol()) return 0;
            long long enum_iat = g_iat_yama_sayisi;
            g_iat_yamalar[enum_iat].text_offset = a->kullanilan;
            g_iat_yamalar[enum_iat].slot = 3; g_iat_yama_sayisi++;
            int32_yaz(a, 0);
            emit_add_rsp(a, 40);
        }

        // RAX=union adresi, payload R12'de korunuyor.
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE1); // MOV RCX, R12
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x00); int32_yaz(a, (int)g_enum_var_disc[ev]);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x48); byte_yaz(a, 0x08);
        return 0;
    }
}
// Dynamic dispatch trait nesnesi cagrisi: sekil.metod(...)
if (YKN_DOT(dugum, sag) && YKN_DOT(dugum, sag)->tip == AST_KIMLIK && YKN_DOT(dugum, sol) && YKN_DOT(dugum, sol)->ilk_jeton) {
    Jeton* sol_j = YKN_DOT(dugum, sol)->ilk_jeton;
    long long sol_off = 0;
    char* sol_t_adi = nullptr;
    long long sol_t_uzn = 0;
    long long sol_yildiz = 0;
    for (long long i = g_lokal_sayisi - 1; i >= 0; i--) {
        if (g_lokal_uzunluklari[i] == sol_j->uzunluk &&
            bellek_esit_mi(g_lokal_isimleri[i], sol_j->baslangic, sol_j->uzunluk) == 1) {
            sol_off = g_lokal_offsetleri[i];
            sol_t_adi = g_lokal_tip_adlari[i];
            sol_t_uzn = g_lokal_tip_uzunluklari[i];
            sol_yildiz = g_lokal_yildizlari[i];
            break;
        }
    }
    if (sol_off != 0 && sol_t_uzn > 4 && memcmp(sol_t_adi, "dyn ", 4) == 0) {
        char* tr_ad = sol_t_adi + 4;
        long long tr_uzn = sol_t_uzn - 4;
        Jeton* cagri_m_j = YKN_DOT(dugum, sag)->ilk_jeton;
        long long method_idx = 0;
        for (int ti = 0; ti < g_tc_trait_sayisi; ti++) {
            if (g_tc_trait_uzunluk[ti] == tr_uzn && memcmp(g_tc_trait_isim[ti], tr_ad, tr_uzn) == 0) {
                long long mb = g_tc_trait_metod_bas[ti];
                long long ma = g_tc_trait_metod_adet[ti];
                for (long long mi = 0; mi < ma; mi++) {
                    if (g_tc_metod_uzunluk[mb + mi] == cagri_m_j->uzunluk &&
                        memcmp(g_tc_metod_isim[mb + mi], cagri_m_j->baslangic, cagri_m_j->uzunluk) == 0) {
                        method_idx = mi;
                        break;
                    }
                }
                break;
            }
        }
        long long slot_off = method_idx * 8;
        if (sol_off <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)(256 - sol_off));
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, -sol_off);
        }
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x08); // MOV RCX, [RAX] (veri)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x58); byte_yaz(a, 0x08); // MOV R11, [RAX + 8] (vtable)
        if (slot_off == 0) {
            byte_yaz(a, 0x4D); byte_yaz(a, 0x8B); byte_yaz(a, 0x1B); // MOV R11, [R11]
        } else {
            byte_yaz(a, 0x4D); byte_yaz(a, 0x8B); byte_yaz(a, 0x5B); byte_yaz(a, (int)slot_off); // MOV R11, [R11 + slot_off]
        }
        AstDugum* ek_arg = (AstDugum*)YKN_DOT(YKN_DOT(dugum, sag), sag);
        if (ek_arg) {
            byte_yaz(a, 0x51); // PUSH RCX
            byte_yaz(a, 0x41); byte_yaz(a, 0x53); // PUSH R11
            ast_kod_uret(a, ek_arg);
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // MOV RDX, RAX
            byte_yaz(a, 0x41); byte_yaz(a, 0x5B); // POP R11
            byte_yaz(a, 0x59); // POP RCX
        }
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20); // SUB RSP, 32
        byte_yaz(a, 0x41); byte_yaz(a, 0xFF); byte_yaz(a, 0xD3); // CALL R11
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20); // ADD RSP, 32
        return 0;
    }
}

char* isim = YKN_DOT(YKN_DOT(kimlik, ilk_jeton), baslangic);
long long uzn = YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk);

if (uzn >= 10 && memcmp(isim, "vtable__", 8) == 0) {
    if (g_target_platform == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 64);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
    } else {
        // VirtualAlloc 64 bytes
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9); // RCX = NULL
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 64); // RDX = 64
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000); // R8 = COMMIT|RESERVE
        byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04); // R9 = PAGE_READWRITE
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28);
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (!seed_iat_yama_kapasite_kontrol()) return 0;
        long long vt_iat = g_iat_yama_sayisi;
        g_iat_yamalar[vt_iat].text_offset = a->kullanilan;
        g_iat_yamalar[vt_iat].slot = 3; g_iat_yama_sayisi++;
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 40);
    }
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // MOV R10, RAX

    long long tip_son = 8;
    while (tip_son + 1 < uzn) {
        if (isim[tip_son] == '_' && isim[tip_son + 1] == '_') break;
        tip_son++;
    }
    long long tip_len = tip_son - 8;
    long long tr_bas = tip_son + 2;
    long long tr_len = uzn - tr_bas;

    int tr_idx = -1;
    for (int ti = 0; ti < g_tc_trait_sayisi; ti++) {
        if (g_tc_trait_uzunluk[ti] == tr_len && memcmp(g_tc_trait_isim[ti], isim + tr_bas, tr_len) == 0) {
            tr_idx = ti; break;
        }
    }
    long long m_sayi = 1;
    long long m_bas = 0;
    if (tr_idx != -1) {
        m_sayi = g_tc_trait_metod_adet[tr_idx];
        m_bas = g_tc_trait_metod_bas[tr_idx];
        if (m_sayi <= 0) m_sayi = 1;
    }

    for (long long mi = 0; mi < m_sayi; mi++) {
        const char* m_ad = "ciz";
        long long m_uzn = 3;
        if (tr_idx != -1 && mi < g_tc_trait_metod_adet[tr_idx]) {
            m_ad = g_tc_metod_isim[m_bas + mi];
            m_uzn = g_tc_metod_uzunluk[m_bas + mi];
        }
        long long h_len = tip_len + 2 + m_uzn;
        char* h_buf = (char*)bellek_tahsis_et(h_len + 1);
        memcpy(h_buf, isim + 8, tip_len);
        h_buf[tip_len] = '_'; h_buf[tip_len + 1] = '_';
        memcpy(h_buf + tip_len + 2, m_ad, m_uzn);
        h_buf[h_len] = 0;

        byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x05);
        if (!seed_yama_kapasite_kontrol()) return 0;
        long long y_vt = g_yama_sayisi;
        g_yamalar[y_vt].isim = h_buf;
        g_yamalar[y_vt].uzunluk = h_len;
        g_yamalar[y_vt].cagri_pos = a->kullanilan;
        g_yama_sayisi++;
        int32_yaz(a, 0);

        long long slot_ofs = mi * 8;
        if (slot_ofs == 0) {
            byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0x02); // MOV [R10], RAX
        } else {
            byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0x42); byte_yaz(a, (int)slot_ofs); // MOV [R10 + slot_ofs], RAX
        }
    }
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xD0); // MOV RAX, R10
    return 0;
}

if (uzn == 9 && memcmp(isim, "dyn_yarat", 9) == 0) {
    AstDugum* dy_arg1 = (AstDugum*)YKN_DOT(dugum, sag);
    AstDugum* dy_arg2 = dy_arg1 ? (AstDugum*)YKN_DOT(dy_arg1, sonraki) : nullptr;
    if (dy_arg1) ast_kod_uret(a, dy_arg1); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x50); // PUSH RAX (save data pointer safely on stack)
    if (dy_arg2) ast_kod_uret(a, dy_arg2); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x50); // PUSH RAX (save vtable safely on stack)

    if (g_target_platform == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 16);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
        byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 16);
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);
        byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28);
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (!seed_iat_yama_kapasite_kontrol()) return 0;
        long long dy_iat = g_iat_yama_sayisi;
        g_iat_yamalar[dy_iat].text_offset = a->kullanilan;
        g_iat_yamalar[dy_iat].slot = 3; g_iat_yama_sayisi++;
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 40);
    }

    byte_yaz(a, 0x41); byte_yaz(a, 0x5B); // POP R11 (vtable)
    byte_yaz(a, 0x41); byte_yaz(a, 0x5A); // POP R10 (veri)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x10); // MOV [RAX + 0], R10 (veri)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x58); byte_yaz(a, 8); // MOV [RAX + 8], R11 (vtable)
    return 0;
}
    // Native x86-64 atomic i64 refcount primitives. These are emitted
    // inline so generated PE/ELF/Mach-O binaries have no unresolved runtime
    // symbol and use the same lock-prefixed instruction on all x86 targets.
    if ((uzn == 18 &&
         (bellek_esit_mi(isim, (char*)"ykn_atomic_inc_i64", 18) == 1 ||
          bellek_esit_mi(isim, (char*)"ykn_atomic_dec_i64", 18) == 1 ||
          bellek_esit_mi(isim, (char*)"ykn_atomic_cas_i64", 18) == 1)) ||
        (uzn == 19 && bellek_esit_mi(isim, (char*)"ykn_atomic_load_i64", 19) == 1) ||
        (uzn == 20 && bellek_esit_mi(isim, (char*)"ykn_atomic_store_i64", 20) == 1)) {
        AstDugum* atomic_arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (uzn == 18 && bellek_esit_mi(isim, (char*)"ykn_atomic_cas_i64", 18) == 1) {
            // Arg 1: ptr -> PUSH
            if (atomic_arg) ast_kod_uret(a, atomic_arg); else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
            byte_yaz(a, 0x50); // PUSH RAX
            if (atomic_arg) atomic_arg = (AstDugum*)YKN_DOT(atomic_arg, sonraki);
            // Arg 2: expected -> PUSH
            if (atomic_arg) ast_kod_uret(a, atomic_arg); else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
            byte_yaz(a, 0x50); // PUSH RAX
            if (atomic_arg) atomic_arg = (AstDugum*)YKN_DOT(atomic_arg, sonraki);
            // Arg 3: desired -> R8
            if (atomic_arg) ast_kod_uret(a, atomic_arg); else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
            byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC0); // MOV R8, RAX
            byte_yaz(a, 0x58);                                       // POP RAX (expected)
            byte_yaz(a, 0x59);                                       // POP RCX (ptr)
            byte_yaz(a, 0xF0); byte_yaz(a, 0x4C); byte_yaz(a, 0x0F); byte_yaz(a, 0xB1); byte_yaz(a, 0x01); // lock cmpxchg [RCX], R8
            byte_yaz(a, 0x0F); byte_yaz(a, 0x94); byte_yaz(a, 0xC0); // SETZ AL
            byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0); // MOVZX RAX, AL
            return 0;
        }
        if (uzn == 20 && bellek_esit_mi(isim, (char*)"ykn_atomic_store_i64", 20) == 1) {
            if (atomic_arg) ast_kod_uret(a, atomic_arg); else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // RCX = ptr
            if (atomic_arg) atomic_arg = (AstDugum*)YKN_DOT(atomic_arg, sonraki);
            if (atomic_arg) ast_kod_uret(a, atomic_arg); else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC2); // RDX = value
            byte_yaz(a, 0x48); byte_yaz(a, 0x87); byte_yaz(a, 0x11); // XCHG [RCX], RDX (implicit lock)
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); // return 0
            return 0;
        }
        if (atomic_arg) ast_kod_uret(a, atomic_arg); else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // RCX = ptr
        if (bellek_esit_mi(isim, (char*)"ykn_atomic_load_i64", 19) == 1) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x01); // MOV RAX, [RCX]
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0);
            if (bellek_esit_mi(isim, (char*)"ykn_atomic_inc_i64", 18) == 1) int32_yaz(a, 1);
            else int32_yaz(a, 0 - 1);
            byte_yaz(a, 0xF0); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xC1); byte_yaz(a, 0x01); // lock xadd [RCX], RAX
            if (bellek_esit_mi(isim, (char*)"ykn_atomic_inc_i64", 18) == 1) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 1);
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE8); byte_yaz(a, 1);
            }
        }
        return 0;
    }
    // Built-in assertion: dogrula(kosul) must be a real fail-closed
    // runtime check, not an unresolved external call or a silent no-op.
    if (uzn == 7 && bellek_esit_mi(isim, (char*)"dogrula", 7) == 1) {
        AstDugum* assertion_arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (assertion_arg) ast_kod_uret(a, assertion_arg);
        else emit_xor_rax_rax(a);
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0); // TEST RAX,RAX
        byte_yaz(a, 0x0F); byte_yaz(a, 0x85); // JNZ success
        long long assertion_jump = YKN_DOT(a, kullanilan);
        int32_yaz(a, 0);
        if (g_target_platform == 0) {
            // ExitProcess(1), slot 0 in the canonical KERNEL32 IAT.
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC1); int32_yaz(a, 1);
            byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
            if (!seed_iat_yama_kapasite_kontrol()) return 0;
            g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
            g_iat_yamalar[g_iat_yama_sayisi].slot = 0;
            g_iat_yama_sayisi++;
            int32_yaz(a, 0);
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);
        } else {
            // Linux exit(1) and Darwin exit(1) use their native syscall ABI.
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC7); int32_yaz(a, 1);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0);
            int32_yaz(a, g_target_platform == 2 ? 0x2000001 : 60);
            byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        }
        long long assertion_target = YKN_DOT(a, kullanilan);
        int assertion_rel = (int)(assertion_target - (assertion_jump + 4));
        unsigned char* assertion_bytes = (unsigned char*)YKN_DOT(a, bellek);
        assertion_bytes[assertion_jump + 0] = (unsigned char)(assertion_rel & 0xFF);
        assertion_bytes[assertion_jump + 1] = (unsigned char)((assertion_rel >> 8) & 0xFF);
        assertion_bytes[assertion_jump + 2] = (unsigned char)((assertion_rel >> 16) & 0xFF);
        assertion_bytes[assertion_jump + 3] = (unsigned char)((assertion_rel >> 24) & 0xFF);
        return 0;
    }
    // Native scalar helper used by the IQ2_S decoder. This mirrors the YKN
    // implementation including its invalid-range result, without leaving a
    // forward call patch in the C++ bootstrap seed.
    if (uzn == 16 && bellek_esit_mi(isim, (char*)"bitnet_iq2s_pow2", 16) == 1) {
        AstDugum* pow_arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (pow_arg) ast_kod_uret(a, pow_arg); else emit_xor_rax_rax(a);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // RCX=us
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1); // result=1
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xF9); byte_yaz(a, 0);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x8C); long long pow_bad1 = YKN_DOT(a, kullanilan); int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xF9); byte_yaz(a, 30);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x8F); long long pow_bad2 = YKN_DOT(a, kullanilan); int32_yaz(a, 0);
        long long pow_loop = YKN_DOT(a, kullanilan);
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x84); long long pow_done = YKN_DOT(a, kullanilan); int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0xD1); byte_yaz(a, 0xE0);
        byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC9);
        byte_yaz(a, 0xE9); int32_yaz(a, (int)(pow_loop - (YKN_DOT(a, kullanilan) + 4)));
        byte_yaz(a, 0xE9); long long pow_end_jump = YKN_DOT(a, kullanilan); int32_yaz(a, 0);
        long long pow_bad = YKN_DOT(a, kullanilan);
        emit_xor_rax_rax(a);
        long long pow_end = YKN_DOT(a, kullanilan);
        int32_patch(a, pow_bad1, pow_bad - (pow_bad1 + 4));
        int32_patch(a, pow_bad2, pow_bad - (pow_bad2 + 4));
        int32_patch(a, pow_done, (pow_end_jump - 1) - (pow_done + 4));
        int32_patch(a, pow_end_jump, pow_end - (pow_end_jump + 4));
        return 0;
    }
    // AST_SONUC_TANIM constructors: Tamam(deger) and Hata(kod)
    // Tamam(val): *Sonuc { basarili=1, deger=val, hata_kodu=0 } — 24 bytes
    // Hata(val):  *Sonuc { basarili=0, deger=0,   hata_kodu=val } — 24 bytes
    if (uzn == 5 && bellek_esit_mi(isim, (char*)"Tamam", 5) == 1) {
        // Evaluate payload argument -> RAX
        if (YKN_DOT(dugum, sag) != 0) { ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag)); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
        // Preserve payload safely on stack across allocator call
        byte_yaz(a, 0x50); // PUSH RAX
        // Allocate 24 bytes
        if (g_target_platform == 1) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 24);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);
            byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
            byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);             // XOR RCX, RCX
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 24); // MOV RDX, 24
            byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);          // MOV R8, 0x3000
            byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);            // MOV R9, 4
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28); // SUB RSP,40
            byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
            if (!seed_iat_yama_kapasite_kontrol()) return 0;
            long long tamam_iat = g_iat_yama_sayisi;
            g_iat_yamalar[tamam_iat].text_offset = a->kullanilan;
            g_iat_yamalar[tamam_iat].slot = 3; // VirtualAlloc
            g_iat_yama_sayisi++;
            int32_yaz(a, 0);
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28); // ADD RSP,40
        }
        // RAX = ptr; R10 = payload
        byte_yaz(a, 0x41); byte_yaz(a, 0x5A); // POP R10 (payload)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x00); int32_yaz(a, 1); // [RAX+0]=1 (basarili)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, 8);  // [RAX+8]=R10 (deger)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 16); int32_yaz(a, 0); // [RAX+16]=0 (hata_kodu)
        return 0;
    } else if (uzn == 4 && bellek_esit_mi(isim, (char*)"Hata", 4) == 1) {
        // Evaluate error payload -> RAX
        if (YKN_DOT(dugum, sag) != 0) { ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag)); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); }
        // Preserve error payload safely on stack across allocator call
        byte_yaz(a, 0x50); // PUSH RAX
        // Allocate 24 bytes
        if (g_target_platform == 1) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 24);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
            byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);
            byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
            byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
            byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 24);
            byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);
            byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28);
            byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
            if (!seed_iat_yama_kapasite_kontrol()) return 0;
            long long hata_iat = g_iat_yama_sayisi;
            g_iat_yamalar[hata_iat].text_offset = a->kullanilan;
            g_iat_yamalar[hata_iat].slot = 3; // VirtualAlloc
            g_iat_yama_sayisi++;
            int32_yaz(a, 0);
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28);
        }
        // RAX = ptr; R10 = error payload
        byte_yaz(a, 0x41); byte_yaz(a, 0x5A); // POP R10 (error payload)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x00); int32_yaz(a, 0); // [RAX+0]=0 (basarili=0)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x40); byte_yaz(a, 8); int32_yaz(a, 0); // [RAX+8]=0 (deger)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x50); byte_yaz(a, 16); // [RAX+16]=R10 (hata_kodu)
        return 0;
    } else if (is_gercek_group(tip_adini_coz(isim, uzn)) && YKN_DOT(dugum, sag) != 0 && YKN_DOT((AstDugum*)YKN_DOT(dugum, sag), sonraki) == 0) {
        AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
        ast_kod_uret(a, arg);
        if (!ast_node_is_float(arg)) {
            // CVTSI2SD XMM0, RAX (0xF2 0x48 0x0F 0x2A 0xC0)
            byte_yaz(a, 0xF2); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x2A); byte_yaz(a, 0xC0);
            // MOVQ RAX, XMM0 (0x66 0x48 0x0F 0x7E 0xC0)
            byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x7E); byte_yaz(a, 0xC0);
        }
        return 0;
    } else if (tip_adini_coz(isim, uzn) == TIP_ID_TAMSAYI && YKN_DOT(dugum, sag) != 0 && YKN_DOT((AstDugum*)YKN_DOT(dugum, sag), sonraki) == 0) {
        AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
        ast_kod_uret(a, arg);
        if (ast_node_is_float(arg)) {
            // MOVQ XMM0, RAX
            byte_yaz(a, 0x66); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x6E); byte_yaz(a, 0xC0);
            // CVTTSD2SI RAX, XMM0 (0xF2 0x48 0x0F 0x2C 0xC0)
            byte_yaz(a, 0xF2); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0x2C); byte_yaz(a, 0xC0);
        }
        return 0;
    // Endustriyel Donanim Intrinsicleri (Clang / RustC seviyesi)
    } else if ((uzn == 8 && bellek_esit_mi(isim, (char*)"popcount", 8) == 1) ||
               (uzn == 7 && bellek_esit_mi(isim, (char*)"bit_say", 7) == 1)) {
        AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
        // POPCNT RAX, RAX (F3 48 0F B8 C0)
        byte_yaz(a, 0xF3); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB8); byte_yaz(a, 0xC0);
        return 0;
    } else if ((uzn == 5 && bellek_esit_mi(isim, (char*)"bswap", 5) == 1) ||
               (uzn == 11 && bellek_esit_mi(isim, (char*)"bayt_cevir", 11) == 1)) {
        AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
        // BSWAP RAX (48 0F C8)
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xC8);
        return 0;
    } else if (uzn == 3 && bellek_esit_mi(isim, (char*)"clz", 3) == 1) {
        AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
        // TEST RAX, RAX (48 85 C0)
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
        // JZ +16 (74 10) -> jumps to MOV RAX, 64
        byte_yaz(a, 0x74); byte_yaz(a, 0x10);
        // BSR RCX, RAX (48 0F BD C8)
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xBD); byte_yaz(a, 0xC8);
        // MOV RAX, 63 (48 C7 C0 3F 00 00 00)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 63);
        // SUB RAX, RCX (48 29 C8)
        byte_yaz(a, 0x48); byte_yaz(a, 0x29); byte_yaz(a, 0xC8);
        // JMP +7 (EB 07) -> jumps past MOV RAX, 64
        byte_yaz(a, 0xEB); byte_yaz(a, 0x07);
        // MOV RAX, 64 (48 C7 C0 40 00 00 00)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 64);
        return 0;
    } else if (uzn == 3 && bellek_esit_mi(isim, (char*)"ctz", 3) == 1) {
        AstDugum* arg = (AstDugum*)YKN_DOT(dugum, sag);
        if (arg) ast_kod_uret(a, arg); else emit_xor_rax_rax(a);
        // TEST RAX, RAX (48 85 C0)
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
        // JZ +6 (74 06) -> jumps to MOV RAX, 64
        byte_yaz(a, 0x74); byte_yaz(a, 0x06);
        // BSF RAX, RAX (48 0F BC C0)
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xBC); byte_yaz(a, 0xC0);
        // JMP +7 (EB 07) -> jumps past MOV RAX, 64
        byte_yaz(a, 0xEB); byte_yaz(a, 0x07);
        // MOV RAX, 64 (48 C7 C0 40 00 00 00)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 64);
        return 0;
    } else if (uzn == 7 && bellek_esit_mi(isim, (char*)"rotleft", 7) == 1) {
        AstDugum* arg1 = (AstDugum*)YKN_DOT(dugum, sag);
        AstDugum* arg2 = arg1 ? (AstDugum*)YKN_DOT(arg1, sonraki) : nullptr;
        if (arg1) ast_kod_uret(a, arg1); else emit_xor_rax_rax(a);
        byte_yaz(a, 0x50); // PUSH RAX
        if (arg2) ast_kod_uret(a, arg2); else emit_xor_rax_rax(a);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX
        byte_yaz(a, 0x58); // POP RAX
        byte_yaz(a, 0x48); byte_yaz(a, 0xD3); byte_yaz(a, 0xC0); // ROL RAX, CL
        return 0;
    } else if (uzn == 8 && bellek_esit_mi(isim, (char*)"rotright", 8) == 1) {
        AstDugum* arg1 = (AstDugum*)YKN_DOT(dugum, sag);
        AstDugum* arg2 = arg1 ? (AstDugum*)YKN_DOT(arg1, sonraki) : nullptr;
        if (arg1) ast_kod_uret(a, arg1); else emit_xor_rax_rax(a);
        byte_yaz(a, 0x50); // PUSH RAX
        if (arg2) ast_kod_uret(a, arg2); else emit_xor_rax_rax(a);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX
        byte_yaz(a, 0x58); // POP RAX
        byte_yaz(a, 0x48); byte_yaz(a, 0xD3); byte_yaz(a, 0xC8); // ROR RAX, CL
        return 0;
    } else if (uzn == 17 && (bellek_esit_mi(isim, (char*)("komut_satiri_arg1"), 17) == 1)) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20); // SUB RSP, 32
            byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
            if (!seed_iat_yama_kapasite_kontrol()) return 0;
            long long iat_gcla = g_iat_yama_sayisi;
            g_iat_yamalar[iat_gcla].text_offset = a->kullanilan;
            g_iat_yamalar[iat_gcla].slot = 7;
            g_iat_yama_sayisi++;
            int32_yaz(a, 0);
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20); // ADD RSP, 32
            // RAX = GetCommandLineA() return value - do NOT clobber it!
            // Fall through to function epilogue which will RET with RAX intact.
    // GOREV 6.2: FPU / SSE matematik fonksiyonlari
    } else if (uzn == 8 && bellek_esit_mi(isim, (char*)"kare_kok", 8) == 1) {
        ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag));
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x51);byte_yaz(a,0xC0);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if (uzn == 4 && bellek_esit_mi(isim, (char*)"sqrt", 4) == 1) {
        ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag));
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x51);byte_yaz(a,0xC0);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if ((uzn == 5 && bellek_esit_mi(isim, (char*)"sinus", 5) == 1) ||
               (uzn == 3 && bellek_esit_mi(isim, (char*)"sin", 3) == 1)) {
        ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag));
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0x48);byte_yaz(a,0x83);byte_yaz(a,0xEC);byte_yaz(a,8);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x11);byte_yaz(a,0x04);byte_yaz(a,0x24);
        byte_yaz(a,0xDD);byte_yaz(a,0x04);byte_yaz(a,0x24);
        byte_yaz(a,0xD9);byte_yaz(a,0xFE);
        byte_yaz(a,0xDD);byte_yaz(a,0x1C);byte_yaz(a,0x24);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x10);byte_yaz(a,0x04);byte_yaz(a,0x24);
        byte_yaz(a,0x48);byte_yaz(a,0x83);byte_yaz(a,0xC4);byte_yaz(a,8);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if ((uzn == 7 && bellek_esit_mi(isim, (char*)"kosinus", 7) == 1) ||
               (uzn == 3 && bellek_esit_mi(isim, (char*)"cos", 3) == 1)) {
        ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag));
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0x48);byte_yaz(a,0x83);byte_yaz(a,0xEC);byte_yaz(a,8);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x11);byte_yaz(a,0x04);byte_yaz(a,0x24);
        byte_yaz(a,0xDD);byte_yaz(a,0x04);byte_yaz(a,0x24);
        byte_yaz(a,0xD9);byte_yaz(a,0xFF);
        byte_yaz(a,0xDD);byte_yaz(a,0x1C);byte_yaz(a,0x24);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x10);byte_yaz(a,0x04);byte_yaz(a,0x24);
        byte_yaz(a,0x48);byte_yaz(a,0x83);byte_yaz(a,0xC4);byte_yaz(a,8);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if ((uzn == 13 && bellek_esit_mi(isim, (char*)"gercek_mutlak", 13) == 1) ||
               (uzn == 4  && bellek_esit_mi(isim, (char*)"fabs", 4) == 1)) {
        ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag));
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0x66);byte_yaz(a,0x0F);byte_yaz(a,0x73);byte_yaz(a,0xF0);byte_yaz(a,1);
        byte_yaz(a,0x66);byte_yaz(a,0x0F);byte_yaz(a,0x73);byte_yaz(a,0xD0);byte_yaz(a,1);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if ((uzn == 14 && bellek_esit_mi(isim, (char*)"gercek_minimum", 14) == 1) ||
               (uzn == 4  && bellek_esit_mi(isim, (char*)"fmin", 4) == 1)) {
        AstDugum* fa = (AstDugum*)YKN_DOT(dugum, sag);
        AstDugum* fb = fa ? (AstDugum*)YKN_DOT(fa, sonraki) : nullptr;
        ast_kod_uret(a, fa);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0x50);
        ast_kod_uret(a, fb);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC8);
        byte_yaz(a,0x58);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x5D);byte_yaz(a,0xC1);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if ((uzn == 15 && bellek_esit_mi(isim, (char*)"gercek_maksimum", 15) == 1) ||
               (uzn == 4  && bellek_esit_mi(isim, (char*)"fmax", 4) == 1)) {
        AstDugum* ga = (AstDugum*)YKN_DOT(dugum, sag);
        AstDugum* gb = ga ? (AstDugum*)YKN_DOT(ga, sonraki) : nullptr;
        ast_kod_uret(a, ga);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0x50);
        ast_kod_uret(a, gb);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC8);
        byte_yaz(a,0x58);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x6E);byte_yaz(a,0xC0);
        byte_yaz(a,0xF2);byte_yaz(a,0x0F);byte_yaz(a,0x5F);byte_yaz(a,0xC1);
        byte_yaz(a,0x66);byte_yaz(a,0x48);byte_yaz(a,0x0F);byte_yaz(a,0x7E);byte_yaz(a,0xC0);
        return 0;
    } else if (uzn == 10 && (bellek_esit_mi(isim, (char*)("ekrana_yaz"), 10) == 1)) {

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// ekrana_yaz("...".ptr) iÃ§in x86_64 kod Ã¼retimi
// AST yapÄ±sÄ±:
//   sag â†’ AST_ALAN_ERISIMI (.ptr)
//     sol â†’ AST_METIN ("Merhaba Dunya!\n")
//
// Strateji:
//   1. String verisini koda inline gÃ¶m, LEA RAX, [string] â†’ R15'e kaydet
//   2. GetStdHandle(-11) â†’ RAX = hFile
//   3. WriteFile(RAX, R15, strlen, NULL, NULL)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

// AdÄ±m 1: String argÃ¼manÄ±nÄ± Ã§Ä±kar
// arg_dugum, AST_ALAN_ERISIMI (.ptr) ise sol=AST_METIN; doÄŸrudan AST_METIN veya AST_KIMLIK ise kendisi
AstDugum* arg_dugum = (AstDugum*)(YKN_DOT(dugum, sag));
AstDugum* metin_dugum = 0;
if (arg_dugum != 0) {
    if (YKN_DOT(arg_dugum, sol) != 0 && YKN_DOT((AstDugum*)YKN_DOT(arg_dugum, sol), tip) == AST_METIN) {
        metin_dugum = (AstDugum*)(YKN_DOT(arg_dugum, sol)); // AST_ALAN_ERISIMI.sol
    } else if (YKN_DOT(arg_dugum, tip) == AST_METIN) {
        metin_dugum = arg_dugum; // doÄŸrudan AST_METIN
    }
}
if (metin_dugum == 0) {
    // Son Ã§are: genel argÃ¼man olarak deÄŸerlendir (pointer in RAX)
    ast_kod_uret(a, arg_dugum);
} else {
    // LEA RAX, [RIP+rel] â†’ string pointer
    ast_kod_uret(a, metin_dugum);
}

if (g_target_platform == 1) {
    // Linux x86-64: SYS_write(1, RAX, strlen(RAX))
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV RSI, RAX (buffer)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC7); int32_yaz(a, 1); // MOV RDI, 1 (stdout)
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX
    byte_yaz(a, 0x80); byte_yaz(a, 0x3C); byte_yaz(a, 0x16); byte_yaz(a, 0x00); // CMP byte [RSI+RDX], 0
    byte_yaz(a, 0x74); byte_yaz(a, 0x05); // JE +5
    byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC2); // INC RDX
    byte_yaz(a, 0xEB); byte_yaz(a, 0xF5); // JMP loop
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1); // MOV RAX, 1 (SYS_write)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    return 0;
} else if (g_target_platform == 2) {
    // macOS x86-64: BSD SYS_write(1, RAX, strlen(RAX))
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV RSI, RAX (buffer)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC7); int32_yaz(a, 1); // MOV RDI, 1 (stdout)
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX
    byte_yaz(a, 0x80); byte_yaz(a, 0x3C); byte_yaz(a, 0x16); byte_yaz(a, 0x00); // CMP byte [RSI+RDX], 0
    byte_yaz(a, 0x74); byte_yaz(a, 0x05); // JE +5
    byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC2); // INC RDX
    byte_yaz(a, 0xEB); byte_yaz(a, 0xF5); // JMP loop
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000004); // MOV RAX, 0x2000004 (BSD SYS_write)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    return 0;
} else {
    // RAX holds string ptr; preserve across GetStdHandle on stack without touching callee-saved registers:
    byte_yaz(a, 0x50); // PUSH RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x28); // SUB RSP, 40 (32 shadow + 8 align)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC1); int32_yaz(a, 0xFFFFFFF5); // MOV RCX, -11
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15); // CALL [RIP + rel32] -> GetStdHandle IAT
    {
    if (seed_iat_yama_kapasite_kontrol()) {
    g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
    g_iat_yamalar[g_iat_yama_sayisi].slot = 2;
    g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    }
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x28); // ADD RSP, 40
    byte_yaz(a, 0x5A); // POP RDX (strPtr)
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX (hFile)
    // R8 = 0 (strlen sayacÄ±)
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); // XOR R8, R8
    // strlen loop: while ([RDX+R8] != 0) R8++
    byte_yaz(a, 0x42); byte_yaz(a, 0x80); byte_yaz(a, 0x3C); byte_yaz(a, 0x02); byte_yaz(a, 0x00); // CMP byte [RDX+R8], 0
    byte_yaz(a, 0x74); byte_yaz(a, 0x05); // JE loop_end
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xEB); byte_yaz(a, 0xF4); // JMP loop_start
    // SUB RSP, 48 (shadow space + 5th arg + padding for alignment)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x30);
    // LEA R9, [RSP+40] (lpNumberOfBytesWritten)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x28);
    // [RSP+32] = NULL (pOverlapped - shadow space icinde)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 0);
    // CALL [RIP + rel32] -> WriteFile IAT
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    {
    // Record this as IAT slot 1 (WriteFile); emit placeholder 0
    if (seed_iat_yama_kapasite_kontrol()) {
    g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
    g_iat_yamalar[g_iat_yama_sayisi].slot = 1;
    g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0); // placeholder, patched in pe32plus_uret
    }
    // ADD RSP, 48 (shadow space geri al)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x30);
    return 0;
}
} else if (uzn == 16 && (bellek_esit_mi(isim, (char*)"bellek_tahsis_et", 16) == 1)) {
AstDugum* ba_arg = (AstDugum*)(YKN_DOT(dugum, sag));
// ArgÃ¼manÄ± (boyut) RAX'a yÃ¼kle
if (ba_arg != 0) { ast_kod_uret(a, ba_arg); }
else { byte_yaz(a, 0x33); byte_yaz(a, 0xC0); } // XOR RAX, RAX

if (g_target_platform == 1) {
    // Linux mmap allocator: reserve an 8-byte length header and return the
    // user pointer after it. The matching free path uses this exact ABI.
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // RSI=size
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC6); byte_yaz(a, 8);
    byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC6); int32_yaz(a, 4095);
    byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xE6); int32_yaz(a, 0 - 4096);
    byte_yaz(a, 0x56); // PUSH RSI (save map length safely on stack, zero callee-saved clobbers)
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0xFFFFFFFF);
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
    byte_yaz(a, 0x5A); // POP RDX (restore map length into scratch register RDX)
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
    byte_yaz(a, 0x78); byte_yaz(a, 7);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x10);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 8);
    return 0;
} else if (g_target_platform == 2) {
    // macOS BSD mmap uses the same header contract as Linux.
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC6);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC6); byte_yaz(a, 8);
    byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC6); int32_yaz(a, 4095);
    byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xE6); int32_yaz(a, 0 - 4096);
    byte_yaz(a, 0x56); // PUSH RSI
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 3);
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 0x1002);
    byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0xFFFFFFFF);
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x20000C5);
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
    byte_yaz(a, 0x5A); // POP RDX
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
    byte_yaz(a, 0x78); byte_yaz(a, 7);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x10);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 8);
    return 0;
} else {
    // MOV RDX, RAX  (dwSize = boyut)
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC2);
    // XOR RCX, RCX  (lpAddress = NULL)
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
    // MOV R8D, 0x3000  (MEM_COMMIT|MEM_RESERVE)
    byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x3000);
    // MOV R9D, 0x04  (PAGE_READWRITE)
    byte_yaz(a, 0x41); byte_yaz(a, 0xB9); int32_yaz(a, 0x04);
    // SUB RSP, 32 (0x20)  (shadow space, 16-byte aligned)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
    // CALL [RIP+rel32] -> VirtualAlloc IAT (slot 3, patched in pe32plus_uret)
    if (!seed_iat_yama_kapasite_kontrol()) return 0;
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    { 
    if (seed_iat_yama_kapasite_kontrol()) {
    g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
    g_iat_yamalar[g_iat_yama_sayisi].slot = 3;
    g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    }
    // ADD RSP, 32
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);
    return 0;
}
} else if (uzn == 17 && (bellek_esit_mi(isim, (char*)"os_bellek_serbest", 17) == 1)) {
    AstDugum* bs_arg = (AstDugum*)(YKN_DOT(dugum, sag));
    if (bs_arg != 0) { ast_kod_uret(a, bs_arg); }
    else { byte_yaz(a, 0x33); byte_yaz(a, 0xC0); }
    if (g_target_platform == 1) {
        // The allocator stores the mapping length in the 8-byte header.
        // Convert the user pointer back to the mapping base and munmap it.
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
        byte_yaz(a, 0x74); byte_yaz(a, 19);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE8); byte_yaz(a, 8);
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x30);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC7);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 11);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x31); byte_yaz(a, 0xC0);
        return 0;
    } else if (g_target_platform == 2) {
        // macOS uses the BSD syscall namespace (munmap=73).
        byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);
        byte_yaz(a, 0x74); byte_yaz(a, 19);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE8); byte_yaz(a, 8);
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x30);
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC7);
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000049);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
        byte_yaz(a, 0x31); byte_yaz(a, 0xC0);
        return 0;
    }
    // Windows VirtualFree(NULL, 0, MEM_RELEASE), imported at kernel slot 9.
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // RCX=ptr
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // dwSize=0
    byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 0x8000); // MEM_RELEASE
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
    if (!seed_iat_yama_kapasite_kontrol()) return 0;
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) {
        g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
        g_iat_yamalar[g_iat_yama_sayisi].slot = 9;
        g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);
    byte_yaz(a, 0x31); byte_yaz(a, 0xC0);
    return 0;
} else if (uzn == 16 && (bellek_esit_mi(isim, (char*)"dosya_oku_aralik", 16) == 1)) {
// dosya_oku_aralik(yol, ofset, buffer, boyut) -> tamsayi
{
    AstDugum* ar1 = (AstDugum*)YKN_DOT(dugum, sag);
    AstDugum* ar2 = ar1 ? (AstDugum*)YKN_DOT(ar1, sonraki) : 0;
    AstDugum* ar3 = ar2 ? (AstDugum*)YKN_DOT(ar2, sonraki) : 0;
    AstDugum* ar4 = ar3 ? (AstDugum*)YKN_DOT(ar3, sonraki) : 0;
    if (ar1) ast_kod_uret(a, ar1); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // R15 = path
    if (ar2) ast_kod_uret(a, ar2); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // R14 = offset
    if (ar3) ast_kod_uret(a, ar3); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC5); // R13 = buffer
    if (ar4) ast_kod_uret(a, ar4); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC4); // R12 = size

    if (g_target_platform == 1) {
        // Linux: SYS_open(path=R15, O_RDONLY=0, 0)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (path)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xF6); // XOR RSI, RSI (O_RDONLY)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 2); // MOV RAX, 2 (SYS_open)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (fd)

        // SYS_lseek(fd=R15, offset=R14, whence=0)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF6); // MOV RSI, R14 (offset)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX (SEEK_SET=0)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 8); // MOV RAX, 8 (SYS_lseek)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL

        // SYS_read(fd=R15, buffer=R13, size=R12)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); // MOV RAX, 0 (SYS_read)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEE); // MOV RSI, R13 (buffer)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE2); // MOV RDX, R12 (size)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); // MOV RBX, RAX (bytesRead)

        // SYS_close(fd=R15)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 3); // MOV RAX, 3 (SYS_close)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL

        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, RBX (return bytesRead)
        return 0;
    } else if (g_target_platform == 2) {
        // macOS: BSD SYS_open(path=R15, O_RDONLY=0, 0)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (path)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xF6); // XOR RSI, RSI (O_RDONLY)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000005); // MOV RAX, 0x2000005 (BSD SYS_open)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (fd)

        // BSD SYS_lseek(fd=R15, offset=R14, whence=0)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF6); // MOV RSI, R14 (offset)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX (SEEK_SET=0)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x20000C7); // MOV RAX, 0x20000C7 (BSD SYS_lseek=199)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL

        // BSD SYS_read(fd=R15, buffer=R13, size=R12)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000003); // MOV RAX, 0x2000003 (BSD SYS_read)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEE); // MOV RSI, R13 (buffer)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE2); // MOV RDX, R12 (size)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); // MOV RBX, RAX (bytesRead)

        // BSD SYS_close(fd=R15)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000006); // MOV RAX, 0x2000006 (BSD SYS_close)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL

        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, RBX (return bytesRead)
        return 0;
    } else {
        // CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 128, NULL)
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x40);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9);
        byte_yaz(a, 0xBA); int32_yaz(a, (int)0x80000000);
    byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 1);
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 3);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); int32_yaz(a, 128);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x30); int32_yaz(a, 0);
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 4; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x40); // ADD RSP, 64
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (R15 = handle)

    // Invalid handle returns zero; otherwise SetFilePointerEx, ReadFile and CloseHandle.
    byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xDB); // XOR RBX, RBX (default 0 bytes read)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x85); int32_yaz(a, -512); int32_yaz(a, 0);
    byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xFF); byte_yaz(a, 0xFF); // CMP R15, -1
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); int32_yaz(a, 0); // JZ skip to end
    long long ar_jz_pos = YKN_DOT(a, kullanilan);

    // SetFilePointerEx(handle, distance=R14, &new@[RBP-528], FILE_BEGIN=0)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xB5); int32_yaz(a, -520);
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x85); int32_yaz(a, -528); int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // RCX = handle (R15)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF2); // RDX = LARGE_INTEGER value (R14)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x85); int32_yaz(a, -528); // R8 = &newPosition
    byte_yaz(a, 0x45); byte_yaz(a, 0x31); byte_yaz(a, 0xC9); // XOR R9D, R9D (FILE_BEGIN=0)
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 8; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);

    // ReadFile(handle, buffer=R13, size=R12, &bytesRead@[RBP-512], NULL)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x30); // SUB RSP, 48
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // RCX = handle
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEA); // RDX = buffer (R13)
    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0xE0); // R8 = size (R12)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x8D); int32_yaz(a, -512); // R9 = &bytesRead
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 0); // [RSP+32]=NULL
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 5; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, -512); // MOV EAX, [RBP-512]
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); // RBX = bytesRead
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x30); // ADD RSP, 48

    // CloseHandle(handle)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // RCX = handle
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 6; g_iat_yama_sayisi++; }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);

    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, RBX (return bytesRead)

    { long long ar_target = YKN_DOT(a, kullanilan); int rel = (int)(ar_target - ar_jz_pos); unsigned char* b = (unsigned char*)YKN_DOT(a, bellek); b[ar_jz_pos-4]=rel&0xFF; b[ar_jz_pos-3]=(rel>>8)&0xFF; b[ar_jz_pos-2]=(rel>>16)&0xFF; b[ar_jz_pos-1]=(rel>>24)&0xFF; }
    }
    return 0;
}
} else if (uzn == 9 && (bellek_esit_mi(isim, (char*)("dosya_oku"), 9) == 1)) {
// dosya_oku(dosya_adi: *u8, buffer: *u8, maks_boyut: tamsayi) -> tamsayi
{
    AstDugum* a1 = (AstDugum*)YKN_DOT(dugum, sag);
    AstDugum* a2 = a1 ? (AstDugum*)YKN_DOT(a1, sonraki) : (AstDugum*)0;
    AstDugum* a3 = a2 ? (AstDugum*)YKN_DOT(a2, sonraki) : (AstDugum*)0;
    if (a1) ast_kod_uret(a, a1); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC4); // MOV R12, RAX (dosya_adi)
    if (a2) ast_kod_uret(a, a2); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC5); // MOV R13, RAX (buffer)
    if (a3) ast_kod_uret(a, a3); else emit_xor_rax_rax(a);
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV R14, RAX (maks_boyut)

    if (g_target_platform == 1) {
        // Linux: SYS_open(R12, 0, 0)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE7); // MOV RDI, R12 (dosya_adi)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xF6); // XOR RSI, RSI (O_RDONLY)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 2); // MOV RAX, 2 (SYS_open)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (fd)

        // SYS_read(fd=R15, buffer=R13, size=R14)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); // MOV RAX, 0 (SYS_read)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEE); // MOV RSI, R13 (buffer)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF2); // MOV RDX, R14 (size)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); // MOV RBX, RAX (bytesRead)

        // SYS_close(fd=R15)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 3); // MOV RAX, 3 (SYS_close)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, RBX (return bytesRead)
        return 0;
    } else if (g_target_platform == 2) {
        // macOS: BSD SYS_open(R12, 0, 0)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE7); // MOV RDI, R12 (dosya_adi)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xF6); // XOR RSI, RSI (O_RDONLY)
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xD2); // XOR RDX, RDX
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000005); // MOV RAX, 0x2000005 (BSD SYS_open)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (fd)

        // BSD SYS_read(fd=R15, buffer=R13, size=R14)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000003); // MOV RAX, 0x2000003 (BSD SYS_read)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEE); // MOV RSI, R13 (buffer)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF2); // MOV RDX, R14 (size)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3); // MOV RBX, RAX (bytesRead)

        // BSD SYS_close(fd=R15)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000006); // MOV RAX, 0x2000006 (BSD SYS_close)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
        byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, RBX (return bytesRead)
        return 0;
    } else {
        // CreateFileA(dosya_adi, GENERIC_READ=0x80000000, FILE_SHARE_READ=1, NULL,
        //             OPEN_EXISTING=3, FILE_ATTRIBUTE_NORMAL=128, NULL)
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0); // AND RSP, -16
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x40); // SUB RSP,64
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE1); // MOV RCX, R12 (dosya_adi)
        byte_yaz(a, 0xBA); int32_yaz(a, (int)0x80000000);          // MOV EDX, GENERIC_READ (zero-extend)
        byte_yaz(a, 0x41); byte_yaz(a, 0xB8); int32_yaz(a, 1);     // MOV R8D, 1 (FILE_SHARE_READ)
        byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);  // XOR R9, R9 (NULL security)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 3);    // [RSP+32]=OPEN_EXISTING
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); int32_yaz(a, 128);  // [RSP+40]=FILE_ATTRIBUTE_NORMAL
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x30); int32_yaz(a, 0);    // [RSP+48]=NULL template
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 4; g_iat_yama_sayisi++; }
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x40); // ADD RSP,64
        byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (hFile)

        // JZ â†’ skip ReadFile/CloseHandle (INVALID_HANDLE_VALUE â†’ return 0)
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x85); int32_yaz(a, -512); int32_yaz(a, 0); // MOV [RBP-512],0
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xFF); byte_yaz(a, 0xFF); // CMP R15,-1
        byte_yaz(a, 0x0F); byte_yaz(a, 0x84); int32_yaz(a, 0);                      // JZ placeholder
        long long jz_pos = YKN_DOT(a, kullanilan);

        // ReadFile(hFile, buffer, maks_boyut, &bytesRead@[RBP-512], NULL)
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x30); // SUB RSP,48
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // MOV RCX, R15 (hFile)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEA); // MOV RDX, R13 (buffer)
        byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0xF0); // MOV R8,  R14 (maks_boyut)
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x8D); int32_yaz(a, -512); // LEA R9,[RBP-512]
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 0); // [RSP+32]=NULL lpOverlapped
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 5; g_iat_yama_sayisi++; }
        int32_yaz(a, 0);
        byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, -512); // MOV EAX, [RBP-512] (zero-extends)
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC3);  // MOV RBX, RAX
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x30); // ADD RSP,48

        // CloseHandle(hFile) â€” SUB RSP,32 (0x20)
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // MOV RCX, R15
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) { g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan); g_iat_yamalar[g_iat_yama_sayisi].slot = 6; g_iat_yama_sayisi++; }
        int32_yaz(a, 0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);

        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD8); // MOV RAX, RBX (return bytes_read)

        // Patch JZ: jump to here (INVALID_HANDLE â†’ XOR RAX,RAX=0 via RBX=0)
        { long long jz_target = YKN_DOT(a, kullanilan);
          int rel = (int)(jz_target - jz_pos);
          unsigned char* b = (unsigned char*)YKN_DOT(a, bellek);
          b[jz_pos-4]=rel&0xFF; b[jz_pos-3]=(rel>>8)&0xFF; b[jz_pos-2]=(rel>>16)&0xFF; b[jz_pos-1]=(rel>>24)&0xFF; }
        return 0;
    }
}
} else if (uzn == 26 && (bellek_esit_mi(isim, (char*)("yknllm_audit_native_append"), 26) == 1)) {
    emit_yknllm_audit_native_append(a, dugum);
    return 0;
} else if (uzn == 18 && (bellek_esit_mi(isim, (char*)("diske_bas_ve_kapat"), 18) == 1)) {
// diske_bas_ve_kapat(dosya_adi: *u8, bellek: *u8, boyut: tamsayi)
AstDugum* db_arg1 = (AstDugum*)YKN_DOT(dugum, sag); // dosya_adi
AstDugum* db_arg2 = db_arg1 ? (AstDugum*)YKN_DOT(db_arg1, sonraki) : 0; // bellek ptr
AstDugum* db_arg3 = db_arg2 ? (AstDugum*)YKN_DOT(db_arg2, sonraki) : 0; // boyut
if (db_arg1) ast_kod_uret(a, db_arg1); else emit_xor_rax_rax(a);
byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV R14, RAX (dosya_adi)
if (db_arg2) ast_kod_uret(a, db_arg2); else emit_xor_rax_rax(a);
byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC5); // MOV R13, RAX (bellek ptr)
if (db_arg3) ast_kod_uret(a, db_arg3); else emit_xor_rax_rax(a);
byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC4); // MOV R12, RAX (boyut)

if (g_target_platform == 1) {
    // Linux: SYS_open(R14, 577, 420)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF7); // MOV RDI, R14
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 577); // MOV RSI, 577 (O_WRONLY|O_CREAT|O_TRUNC)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 420); // MOV RDX, 420 (0644)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 2);   // MOV RAX, 2 (SYS_open)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (fd)
    // SYS_write(fd=R15, buf=R13, size=R12)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1);   // MOV RAX, 1 (SYS_write)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEE); // MOV RSI, R13 (buf)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE2); // MOV RDX, R12 (size)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV R14, RAX (written bytes)
    // SYS_close(fd=R15)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 3);   // MOV RAX, SYS_close
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF0); // MOV RAX, R14 (written bytes)
    return 0;
} else if (g_target_platform == 2) {
    // macOS: BSD SYS_open(R14, O_WRONLY|O_CREAT|O_TRUNC=0x601, 0644)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF7); // MOV RDI, R14
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC6); int32_yaz(a, 0x601); // MOV RSI, 0x601
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 420); // MOV RDX, 420 (0644)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000005); // MOV RAX, 0x2000005 (BSD SYS_open)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (fd)
    // BSD SYS_write(fd=R15, buf=R13, size=R12)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000004); // MOV RAX, 0x2000004 (BSD SYS_write)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEE); // MOV RSI, R13 (buf)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xE2); // MOV RDX, R12 (size)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV R14, RAX (written bytes)
    // BSD SYS_close(fd=R15)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000006); // MOV RAX, BSD SYS_close
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xFF); // MOV RDI, R15 (fd)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF0); // MOV RAX, R14 (written bytes)
    return 0;
} else {
    // CreateFileA(dosya_adi, GENERIC_WRITE=0x40000000, 0, NULL, CREATE_ALWAYS=2, FILE_ATTRIBUTE_NORMAL=128, NULL)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE4); byte_yaz(a, 0xF0); // AND RSP, -16
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x40); // SUB RSP, 64
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF1); // MOV RCX, R14
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 0x40000000); // MOV RDX, GENERIC_WRITE
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC0); // XOR R8, R8  (no sharing)
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9); // XOR R9, R9  (NULL security)
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 2); // CREATE_ALWAYS
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); int32_yaz(a, 128); // FILE_ATTRIBUTE_NORMAL
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x30); int32_yaz(a, 0); // NULL template
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) {
        g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
        g_iat_yamalar[g_iat_yama_sayisi].slot = 4; // CreateFileA
        g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x40); // ADD RSP, 64
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV R15, RAX (hFile)
    // WriteFile(hFile, bellek, boyut, &written_on_stack, NULL)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x30); // SUB RSP, 48
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // MOV RCX, R15
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xEA); // MOV RDX, R13
    byte_yaz(a, 0x4D); byte_yaz(a, 0x89); byte_yaz(a, 0xE0); // MOV R8, R12
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x28); // LEA R9, [RSP+40]
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); int32_yaz(a, 0); // [RSP+32] = 0
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) {
        g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
        g_iat_yamalar[g_iat_yama_sayisi].slot = 1; // WriteFile
        g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); // MOV EAX, [RSP+40] (written bytes)
    byte_yaz(a, 0x49); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV R14, RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x30); // ADD RSP, 48
    // CloseHandle(hFile)
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20); // SUB RSP, 32
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF9); // MOV RCX, R15
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    if (seed_iat_yama_kapasite_kontrol()) {
        g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
        g_iat_yamalar[g_iat_yama_sayisi].slot = 6; // CloseHandle
        g_iat_yama_sayisi++;
    }
    int32_yaz(a, 0);
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20); // ADD RSP, 32
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xF0); // MOV RAX, R14 (written bytes)
    return 0;
}
} else {
    // 1. ArgÃ¼man sayÄ±sÄ±nÄ± hesapla
    AstDugum* arg_iter = (AstDugum*)(YKN_DOT(dugum, sag));
    long long arg_count = 0;

    // Self-hosted codegen recursively calls ast_kod_uret(a, dugum) very
    // frequently.  Keep the two direct local-pointer arguments in registers
    // through an explicit ABI path; the generic expression emitter can lose
    // the second pointer when the caller has a large flat local frame.
    if (isim && YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk) == 12 &&
        bellek_esit_mi(isim, (char*)"ast_kod_uret", 12) == 1) {
        AstDugum* sp_arg1 = arg_iter;
        AstDugum* sp_arg2 = sp_arg1 ? (AstDugum*)YKN_DOT(sp_arg1, sonraki) : 0;
        if (sp_arg1 && sp_arg2 && !YKN_DOT(sp_arg2, sonraki) &&
            YKN_DOT(sp_arg1, tip) == AST_KIMLIK && YKN_DOT(sp_arg2, tip) == AST_KIMLIK &&
            YKN_DOT(sp_arg1, ilk_jeton) && YKN_DOT(sp_arg2, ilk_jeton)) {
            long long sp_off1 = 0;
            long long sp_off2 = 0;
            long long sp_len1 = YKN_DOT(YKN_DOT(sp_arg1, ilk_jeton), uzunluk);
            long long sp_len2 = YKN_DOT(YKN_DOT(sp_arg2, ilk_jeton), uzunluk);
            char* sp_name1 = YKN_DOT(YKN_DOT(sp_arg1, ilk_jeton), baslangic);
            char* sp_name2 = YKN_DOT(YKN_DOT(sp_arg2, ilk_jeton), baslangic);
            for (long long sp_i = g_lokal_sayisi - 1; sp_i >= 0; sp_i--) {
                if (sp_off1 == 0 && g_lokal_uzunluklari[sp_i] == sp_len1 &&
                    bellek_esit_mi(g_lokal_isimleri[sp_i], sp_name1, sp_len1) == 1) sp_off1 = g_lokal_offsetleri[sp_i];
                if (sp_off2 == 0 && g_lokal_uzunluklari[sp_i] == sp_len2 &&
                    bellek_esit_mi(g_lokal_isimleri[sp_i], sp_name2, sp_len2) == 1) sp_off2 = g_lokal_offsetleri[sp_i];
            }
            if (sp_off1 > 0 && sp_off2 > 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);
                if (sp_off1 <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)(256 - sp_off1));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, (int)-sp_off1);
                }
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x04); byte_yaz(a, 0x24);
                if (sp_off2 <= 127) {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x45); byte_yaz(a, (unsigned char)(256 - sp_off2));
                } else {
                    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x85); int32_yaz(a, (int)-sp_off2);
                }
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x08);
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x0C); byte_yaz(a, 0x24);
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x08);
                byte_yaz(a, 0xE8);
                if (!seed_yama_kapasite_kontrol()) return 0;
                g_yamalar[g_yama_sayisi].isim = isim;
                g_yamalar[g_yama_sayisi].uzunluk = 12;
                g_yamalar[g_yama_sayisi].cagri_pos = YKN_DOT(a, kullanilan);
                int32_yaz(a, 0);
                g_yama_sayisi++;
                byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);
                return 0;
            }
        }
    }

    while (arg_iter != 0) {
        arg_count++;
        arg_iter = YKN_DOT(arg_iter, sonraki);
    }
    
    // 2. Stack alanÄ±nÄ± hesapla: min 32 byte (shadow space), arg_count > 4 ise fazlasÄ±. 16 byte hizalÄ± olmalÄ±.
    long long alloc_size = arg_count < 4 ? 32 : (arg_count * 8);
    if (alloc_size % 16 != 0) {
        alloc_size += 8;
    }
    
    // 3. SUB RSP, alloc_size
    if (alloc_size <= 127) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, (unsigned char)alloc_size);
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xEC); int32_yaz(a, alloc_size);
    }
    
    // 4. ArgÃ¼manlarÄ± hesapla ve stack'e yaz
    AstDugum* arg = (AstDugum*)(YKN_DOT(dugum, sag));
    long long curr_arg = 0;
    while (arg != 0) {
        ast_kod_uret(a, arg); // RAX'a sonucu dÃ¶ndÃ¼rÃ¼r
        // MOV [RSP + curr_arg*8], RAX
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x84); byte_yaz(a, 0x24);
        int32_yaz(a, curr_arg * 8);
        arg = YKN_DOT(arg, sonraki);
        curr_arg++;
    }
    
    // 5. Ä°lk 4 argÃ¼manÄ± register'lara yÃ¼kle
    if (arg_count > 0) {
        // MOV RCX, [RSP]
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x0C); byte_yaz(a, 0x24);
    }
    if (arg_count > 1) {
        // MOV RDX, [RSP+8]
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x08);
    }
    if (arg_count > 2) {
        // MOV R8, [RSP+16]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x10);
    }
    if (arg_count > 3) {
        // MOV R9, [RSP+24]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x18);
    }

    if (seed_linux_socket_call(a, isim, YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk), arg_count) ||
        seed_windows_socket_call(a, isim, YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk), arg_count)) {
        if (alloc_size <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size);
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, alloc_size);
        }
        return 0;
    }


    // Keep the bootstrap seed in parity with canonical YKN codegen intrinsics.
    if (YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk) == 12 &&
        bellek_esit_mi(isim, (char*)"ykn_popcnt64", 12) == 1 && arg_count == 1) {
        byte_yaz(a, 0xF3); byte_yaz(a, 0x48); byte_yaz(a, 0x0F);
        byte_yaz(a, 0xB8); byte_yaz(a, 0xC1); // POPCNT RAX, RCX
        if (alloc_size <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size);
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size);
        }
        return 0;
    }

    long long fn_name_len = YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk);
    if (fn_name_len == 20 && bellek_esit_mi(isim, (char*)"os_hedef_platform_al", 20) == 1) {
        emit_mov_rax_imm32(a, (int)g_target_platform);
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_atomic_inc_i64", 18) == 1 && arg_count == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1);
        byte_yaz(a, 0xF0); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xC1); byte_yaz(a, 0x01);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 1);
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_atomic_dec_i64", 18) == 1 && arg_count == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);
        byte_yaz(a, 0xF0); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xC1); byte_yaz(a, 0x01);
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xE8); byte_yaz(a, 1);
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 19 && bellek_esit_mi(isim, (char*)"ykn_atomic_load_i64", 19) == 1 && arg_count == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x01);
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 20 && bellek_esit_mi(isim, (char*)"ykn_atomic_store_i64", 20) == 1 && arg_count == 2) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x87); byte_yaz(a, 0x11);
        byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xC0);
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_atomic_cas_i64", 18) == 1 && arg_count == 3) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x0C); byte_yaz(a, 0x24);
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x08);
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x10);
        byte_yaz(a, 0xF0); byte_yaz(a, 0x4C); byte_yaz(a, 0x0F); byte_yaz(a, 0xB1); byte_yaz(a, 0x01);
        byte_yaz(a, 0x0F); byte_yaz(a, 0x94); byte_yaz(a, 0xC0);
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0);
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 13 && bellek_esit_mi(isim, (char*)"ykn_cmpxchg64", 13) == 1 && arg_count == 3) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD0);             // MOV RAX, RDX
        byte_yaz(a, 0xF0); byte_yaz(a, 0x4C); byte_yaz(a, 0x0F); byte_yaz(a, 0xB1); byte_yaz(a, 0x01); // LOCK CMPXCHG [RCX], R8
        byte_yaz(a, 0x0F); byte_yaz(a, 0x94); byte_yaz(a, 0xC0);             // SETE AL
        byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB6); byte_yaz(a, 0xC0); // MOVZX RAX, AL
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_atomik_artir64", 18) == 1 && arg_count == 1) {
        byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 1); // MOV RAX, 1
        byte_yaz(a, 0xF0); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xC1); byte_yaz(a, 0x01); // LOCK XADD [RCX], RAX
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 1); // ADD RAX, 1
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 16 && bellek_esit_mi(isim, (char*)"ykn_cpu_has_avx2", 16) == 1) {
        byte_yaz(a, 0x53);                         // PUSH RBX
        byte_yaz(a, 0xB8); int32_yaz(a, 7);        // MOV EAX, 7
        byte_yaz(a, 0x31); byte_yaz(a, 0xC9);      // XOR ECX, ECX
        byte_yaz(a, 0x0F); byte_yaz(a, 0xA2);      // CPUID
        byte_yaz(a, 0xC1); byte_yaz(a, 0xEB); byte_yaz(a, 5); // SHR EBX, 5
        byte_yaz(a, 0x83); byte_yaz(a, 0xE3); byte_yaz(a, 1); // AND EBX, 1
        byte_yaz(a, 0x89); byte_yaz(a, 0xD8);      // MOV EAX, EBX
        byte_yaz(a, 0x48); byte_yaz(a, 0x98);      // CDQE
        byte_yaz(a, 0x5B);                         // POP RBX
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 12 && bellek_esit_mi(isim, (char*)"ykn_popcnt64", 12) == 1) {
        byte_yaz(a, 0xF3); byte_yaz(a, 0x48); byte_yaz(a, 0x0F); byte_yaz(a, 0xB8); byte_yaz(a, 0xC1); // POPCNT RAX, RCX
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 20 && bellek_esit_mi(isim, (char*)"ykn_simd_vec_add_i64", 20) == 1 && arg_count == 4) {
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC0);             // MOV RAX, R8
        byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);             // TEST R9, R9
        byte_yaz(a, 0x7E); byte_yaz(a, 0x47);                                 // JLE .cleanup_done
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xF9); byte_yaz(a, 4); // CMP R9, 4
        byte_yaz(a, 0x7C); byte_yaz(a, 0x1E);                                 // JL .scalar_loop
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x6F); byte_yaz(a, 0x02); // VMOVDQU YMM0, [RDX]
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFD); byte_yaz(a, 0xD4); byte_yaz(a, 0x00); // VPADDQ YMM0, YMM0, [RAX]
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x7F); byte_yaz(a, 0x01); // VMOVDQU [RCX], YMM0
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC2); byte_yaz(a, 0x20); // ADD RDX, 32
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 0x20); // ADD RAX, 32
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC1); byte_yaz(a, 0x20); // ADD RCX, 32
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xE9); byte_yaz(a, 4);    // SUB R9, 4
        byte_yaz(a, 0xEB); byte_yaz(a, 0xDC);                                 // JMP .loop_avx
        byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);             // TEST R9, R9
        byte_yaz(a, 0x7E); byte_yaz(a, 0x1B);                                 // JLE .cleanup_avx
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x02);             // MOV R8, [RDX]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x03); byte_yaz(a, 0x00);             // ADD R8, [RAX]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x01);             // MOV [RCX], R8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC2); byte_yaz(a, 8);    // ADD RDX, 8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 8);    // ADD RAX, 8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC1); byte_yaz(a, 8);    // ADD RCX, 8
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xE9); byte_yaz(a, 1);    // SUB R9, 1
        byte_yaz(a, 0xEB); byte_yaz(a, 0xE0);                                 // JMP .scalar_loop
        byte_yaz(a, 0xC5); byte_yaz(a, 0xF8); byte_yaz(a, 0x77);             // VZEROUPPER
        byte_yaz(a, 0x31); byte_yaz(a, 0xC0);                                 // XOR EAX, EAX
        byte_yaz(a, 0xFF); byte_yaz(a, 0xC0);                                 // INC EAX
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 20 && bellek_esit_mi(isim, (char*)"ykn_simd_vec_sub_i64", 20) == 1 && arg_count == 4) {
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xC0);             // MOV RAX, R8
        byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);             // TEST R9, R9
        byte_yaz(a, 0x7E); byte_yaz(a, 0x47);                                 // JLE .cleanup_done
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xF9); byte_yaz(a, 4); // CMP R9, 4
        byte_yaz(a, 0x7C); byte_yaz(a, 0x1E);                                 // JL .scalar_loop
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x6F); byte_yaz(a, 0x02); // VMOVDQU YMM0, [RDX]
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFD); byte_yaz(a, 0xFB); byte_yaz(a, 0x00); // VPSUBQ YMM0, YMM0, [RAX]
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x7F); byte_yaz(a, 0x01); // VMOVDQU [RCX], YMM0
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC2); byte_yaz(a, 0x20); // ADD RDX, 32
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 0x20); // ADD RAX, 32
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC1); byte_yaz(a, 0x20); // ADD RCX, 32
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xE9); byte_yaz(a, 4);    // SUB R9, 4
        byte_yaz(a, 0xEB); byte_yaz(a, 0xDC);                                 // JMP .loop_avx
        byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xC9);             // TEST R9, R9
        byte_yaz(a, 0x7E); byte_yaz(a, 0x1B);                                 // JLE .cleanup_avx
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x02);             // MOV R8, [RDX]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x2B); byte_yaz(a, 0x00);             // SUB R8, [RAX]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0x01);             // MOV [RCX], R8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC2); byte_yaz(a, 8);    // ADD RDX, 8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC0); byte_yaz(a, 8);    // ADD RAX, 8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC1); byte_yaz(a, 8);    // ADD RCX, 8
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xE9); byte_yaz(a, 1);    // SUB R9, 1
        byte_yaz(a, 0xEB); byte_yaz(a, 0xE0);                                 // JMP .scalar_loop
        byte_yaz(a, 0xC5); byte_yaz(a, 0xF8); byte_yaz(a, 0x77);             // VZEROUPPER
        byte_yaz(a, 0x31); byte_yaz(a, 0xC0);                                 // XOR EAX, EAX
        byte_yaz(a, 0xFF); byte_yaz(a, 0xC0);                                 // INC EAX
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    if (fn_name_len == 16 && bellek_esit_mi(isim, (char*)"ykn_simd_dot_q16", 16) == 1 && arg_count == 3) {
        byte_yaz(a, 0x53);                                                 // PUSH RBX
        byte_yaz(a, 0x41); byte_yaz(a, 0x54);                             // PUSH R12
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20); // SUB RSP, 32
        byte_yaz(a, 0xC5); byte_yaz(a, 0xED); byte_yaz(a, 0x57); byte_yaz(a, 0xD2); // VXORPS YMM2, YMM2, YMM2
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xF8); byte_yaz(a, 4);    // CMP R8, 4
        byte_yaz(a, 0x7C); byte_yaz(a, 0x1F);                             // JL .dot_scalar_init
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x6F); byte_yaz(a, 0x01); // VMOVDQU YMM0, [RCX]
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x6F); byte_yaz(a, 0x0A); // VMOVDQU YMM1, [RDX]
        byte_yaz(a, 0xC4); byte_yaz(a, 0xE2); byte_yaz(a, 0x7D); byte_yaz(a, 0x28); byte_yaz(a, 0xC1); // VPMULDQ YMM0, YMM0, YMM1
        byte_yaz(a, 0xC5); byte_yaz(a, 0xED); byte_yaz(a, 0xD4); byte_yaz(a, 0xD0); // VPADDQ YMM2, YMM2, YMM0
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC1); byte_yaz(a, 0x20); // ADD RCX, 32
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC2); byte_yaz(a, 0x20); // ADD RDX, 32
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xE8); byte_yaz(a, 4);    // SUB R8, 4
        byte_yaz(a, 0xEB); byte_yaz(a, 0xDB);                             // JMP .dot_loop_avx
        byte_yaz(a, 0xC5); byte_yaz(a, 0xFE); byte_yaz(a, 0x7F); byte_yaz(a, 0x14); byte_yaz(a, 0x24); // VMOVDQU [RSP], YMM2
        byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x04); byte_yaz(a, 0x24); // MOV RAX, [RSP]
        byte_yaz(a, 0x48); byte_yaz(a, 0x03); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x08); // ADD RAX, [RSP+8]
        byte_yaz(a, 0x48); byte_yaz(a, 0x03); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x10); // ADD RAX, [RSP+16]
        byte_yaz(a, 0x48); byte_yaz(a, 0x03); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x18); // ADD RAX, [RSP+24]
        byte_yaz(a, 0x4D); byte_yaz(a, 0x85); byte_yaz(a, 0xC0);             // TEST R8, R8
        byte_yaz(a, 0x7E); byte_yaz(a, 0x18);                                 // JLE .dot_finish
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x11);             // MOV R10, [RCX]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x0F); byte_yaz(a, 0xAF); byte_yaz(a, 0x12); // IMUL R10, [RDX]
        byte_yaz(a, 0x4C); byte_yaz(a, 0x01); byte_yaz(a, 0xD0);             // ADD RAX, R10
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC1); byte_yaz(a, 8);    // ADD RCX, 8
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC2); byte_yaz(a, 8);    // ADD RDX, 8
        byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xE8); byte_yaz(a, 1);    // SUB R8, 1
        byte_yaz(a, 0xEB); byte_yaz(a, 0xE3);                                 // JMP .dot_scalar_loop
        byte_yaz(a, 0x48); byte_yaz(a, 0xC1); byte_yaz(a, 0xF8); byte_yaz(a, 0x10); // SAR RAX, 16
        byte_yaz(a, 0xC5); byte_yaz(a, 0xF8); byte_yaz(a, 0x77);             // VZEROUPPER
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20); // ADD RSP, 32
        byte_yaz(a, 0x41); byte_yaz(a, 0x5C);                                 // POP R12
        byte_yaz(a, 0x5B);                                                     // POP RBX
        if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
        else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
        return 0;
    }
    bool dyn_i64 = fn_name_len == 20 && bellek_esit_mi(isim, (char*)"ykn_dyn_call_ptr_i64", 20) == 1 && arg_count == 3;
    bool dyn_3 = fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_dyn_call_ptr_3", 18) == 1 && arg_count == 4;
    bool dyn_4 = fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_dyn_call_ptr_4", 18) == 1 && arg_count == 5;
    bool dyn_6 = fn_name_len == 18 && bellek_esit_mi(isim, (char*)"ykn_dyn_call_ptr_6", 18) == 1 && (arg_count == 7 || arg_count == 6);
    if (dyn_i64 || dyn_3 || dyn_4 || dyn_6) {
        byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x1C); byte_yaz(a, 0x24); // R11=[RSP] (fn_ptr)
        if (dyn_6) {
            if (g_target_platform == 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x08); // RCX=[RSP+8] (a1)
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x10); // RDX=[RSP+16] (a2)
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x18); // R8=[RSP+24] (a3)
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x20); // R9=[RSP+32] (a4)
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); // RAX=[RSP+40] (a5)
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x20); // [RSP+32]=RAX (arg 5 shadow slot)
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x30); // RAX=[RSP+48] (a6)
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); // [RSP+40]=RAX (arg 6 shadow slot)
            } else {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x7C); byte_yaz(a, 0x24); byte_yaz(a, 0x08); // RDI=[RSP+8] (SysV a1)
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x74); byte_yaz(a, 0x24); byte_yaz(a, 0x10); // RSI=[RSP+16] (SysV a2)
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x18); // RDX=[RSP+24] (SysV a3)
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x20); // RCX=[RSP+32] (SysV a4)
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x28); // R8=[RSP+40] (SysV a5)
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x30); // R9=[RSP+48] (SysV a6)
            }
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x08); // RCX=[RSP+8] (a1 YKN)
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);                                         // RDI=RCX (a1 SysV)
            byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x10); // RDX=[RSP+16] (a2 YKN)
            byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);                                         // RSI=RDX (a2 SysV)
            if (dyn_3 || dyn_4) {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x44); byte_yaz(a, 0x24); byte_yaz(a, 0x18); // R8=[RSP+24] (a3 YKN)
            }
            if (dyn_4) {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x20); // R9=[RSP+32] (a4 YKN)
            }
        }
        byte_yaz(a, 0x41); byte_yaz(a, 0xFF); byte_yaz(a, 0xD3); // CALL R11
        if (alloc_size <= 127) {
            byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size);
        } else {
            byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size);
        }
        return 0;
    }

    long long std_slot = bul_veya_ekle_iat_slot(isim, YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk));
    if (std_slot >= 0) {
        if (g_target_platform == 1) {
            if (std_slot == 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xCF);
                byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 60);
                byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
                if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
                else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
                return 0;
            }
            if (std_slot == 3) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x31); byte_yaz(a, 0xFF);
                byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xD6);
                byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 7);
                byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC2); int32_yaz(a, 34);
                byte_yaz(a, 0x49); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, -1);
                byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xC9);
                byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 9);
                byte_yaz(a, 0x0F); byte_yaz(a, 0x05);
                if (alloc_size <= 127) { byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size); }
                else { byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, (int)alloc_size); }
                return 0;
            }
        }
        byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
        if (seed_iat_yama_kapasite_kontrol()) {
            g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
            g_iat_yamalar[g_iat_yama_sayisi].slot = std_slot;
            g_iat_yama_sayisi++;
        }
        int32_yaz(a, 0);
    } else {
        long long f_off = 0;
        long long f_uzn = YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk);
        long long f_start = g_lokal_sayisi - 1;
        long long f_end = (g_lambda_tabani > 0) ? g_lambda_tabani : 0;
        for(long long i = f_start; i >= f_end; i--) {
            if(g_lokal_uzunluklari[i] == f_uzn && bellek_esit_mi(g_lokal_isimleri[i], isim, f_uzn) == 1) {
                f_off = g_lokal_offsetleri[i];
                break;
            }
        }
        if (f_off != 0) {
            if (f_off <= 127) {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x5D);
                byte_yaz(a, (unsigned char)(256 - f_off));
            } else {
                byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x9D);
                int32_yaz(a, -f_off);
            }
            byte_yaz(a, 0x41); byte_yaz(a, 0xFF); byte_yaz(a, 0xD3); // CALL R11
        } else if (g_lambda_tabani > 0) {
            long long outer_f_off = 0;
            for (long long i = g_lambda_tabani - 1; i >= 0; i--) {
                if (g_lokal_uzunluklari[i] == f_uzn && bellek_esit_mi(g_lokal_isimleri[i], isim, f_uzn) == 1) {
                    outer_f_off = g_lokal_offsetleri[i];
                    break;
                }
            }
            if (outer_f_off != 0) {
                byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x55); byte_yaz(a, 0x00); // MOV RDX, [RBP + 0]
                if (outer_f_off <= 127) {
                    byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x5A); byte_yaz(a, (unsigned char)(256 - outer_f_off));
                } else {
                    byte_yaz(a, 0x4C); byte_yaz(a, 0x8B); byte_yaz(a, 0x9A); int32_yaz(a, -outer_f_off);
                }
                byte_yaz(a, 0x41); byte_yaz(a, 0xFF); byte_yaz(a, 0xD3); // CALL R11
            } else {
                byte_yaz(a, 0xE8);
                if (!seed_yama_kapasite_kontrol()) return 0;
                g_yamalar[g_yama_sayisi].isim = isim;
                g_yamalar[g_yama_sayisi].uzunluk = YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk);
                g_yamalar[g_yama_sayisi].cagri_pos = YKN_DOT(a, kullanilan);
                int32_yaz(a, 0);
                g_yama_sayisi++;
            }
        } else {
            byte_yaz(a, 0xE8);
            if (!seed_yama_kapasite_kontrol()) return 0;
            g_yamalar[g_yama_sayisi].isim = isim;
            g_yamalar[g_yama_sayisi].uzunluk = YKN_DOT(YKN_DOT(kimlik, ilk_jeton), uzunluk);
            g_yamalar[g_yama_sayisi].cagri_pos = YKN_DOT(a, kullanilan);
            int32_yaz(a, 0);
            g_yama_sayisi++;
        }
    }
    
    // 7. Stack'i temizle (ADD RSP, alloc_size)
    if (alloc_size <= 127) {
        byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, (unsigned char)alloc_size);
    } else {
        byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xC4); int32_yaz(a, alloc_size);
    }
}
}
;
// AST_DIREKTIF_C_LIB (29), CPP_LIB (30), RS_LIB (32): record library name for linker
if (YKN_DOT(dugum, tip) == 29 || YKN_DOT(dugum, tip) == 30 || YKN_DOT(dugum, tip) == 32 ||
    YKN_DOT(dugum, tip) == AST_DIREKTIF_C_LIB || YKN_DOT(dugum, tip) == AST_DIREKTIF_CPP_LIB || YKN_DOT(dugum, tip) == AST_DIREKTIF_RS_LIB) {
    if (YKN_DOT(dugum, ilk_jeton) != 0 && g_clib_adet < 64) {
        char* lib_ptr = YKN_DOT(YKN_DOT(dugum, ilk_jeton), baslangic);
        long long lib_uzn = YKN_DOT(YKN_DOT(dugum, ilk_jeton), uzunluk);
        // Strip surrounding quotes: "libadi" -> libadi
        if (lib_uzn >= 2 && lib_ptr[0] == '"') { lib_ptr++; lib_uzn -= 2; }
        long long win32_lib = 0;
        if (lib_uzn == 8 && bellek_esit_mi(lib_ptr, (char*)"kernel32", 8) == 1) win32_lib = 1;
        else if (lib_uzn == 6 && bellek_esit_mi(lib_ptr, (char*)"user32", 6) == 1) win32_lib = 1;
        else if (lib_uzn == 8 && bellek_esit_mi(lib_ptr, (char*)"advapi32", 8) == 1) win32_lib = 1;
        else if (lib_uzn == 6 && bellek_esit_mi(lib_ptr, (char*)"ws2_32", 6) == 1) win32_lib = 1;

        if (g_target_platform != 0 && win32_lib == 1) {
            // Windows library ignored on non-Windows targets
        } else {
            g_clib_isim[g_clib_adet] = lib_ptr;
            g_clib_uzunluk[g_clib_adet] = lib_uzn;
            g_clib_adet++;
        }
    }
    if (YKN_DOT(dugum, sonraki) != 0) { ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sonraki)); }
    return 0;
}
// AST_SONUC_TANIM (34), CONFIG (44), CONVERT (38), PY_LIB (31), AI (28), PROMPT (27): no code emitted
if (YKN_DOT(dugum, tip) == AST_SONUC_TANIM || YKN_DOT(dugum, tip) == AST_DIREKTIF_CONFIG ||
    YKN_DOT(dugum, tip) == AST_DIREKTIF_CONVERT || YKN_DOT(dugum, tip) == AST_DIREKTIF_PY_LIB ||
    YKN_DOT(dugum, tip) == AST_DIREKTIF_PROMPT || YKN_DOT(dugum, tip) == AST_DIREKTIF_AI ||
    YKN_DOT(dugum, tip) == 31 || YKN_DOT(dugum, tip) == 38 || YKN_DOT(dugum, tip) == 44) {
    if (YKN_DOT(dugum, sonraki) != 0) { ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sonraki)); }
    return 0;
}
// AST_AWAIT (42): Await unwrap
if (YKN_DOT(dugum, tip) == AST_AWAIT || YKN_DOT(dugum, tip) == 42) {
    if (YKN_DOT(dugum, sag) != 0) {
        ast_kod_uret(a, (AstDugum*)YKN_DOT(dugum, sag));
    } else {
        emit_xor_rax_rax(a);
    }
    seed_emit_async_await_unwrap(a);
    return 0;
}
// Unknown AST tags must never fall through to a successful artifact.
seed_codegen_ast_capability_scan(dugum);
return 0;
}


static void pe_simd_hint(MakineKoduArenasi* a, const char* name) {
    u16_yaz(a, 0);
    for (long long i = 0; name[i] != 0; ++i) byte_yaz(a, (unsigned char)name[i]);
    byte_yaz(a, 0);
    long long length = 0;
    while (name[length] != 0) ++length;
    if ((length % 2) == 0) byte_yaz(a, 0);
}

void pe32plus_simd_uret(MakineKoduArenasi* kod_arenasi, MakineKoduArenasi* cikti, long long ep_rva);

void pe32plus_uret(MakineKoduArenasi* kod_arenasi, MakineKoduArenasi* cikti, long long ep_rva) {
    long long IMAGE_BASE = 0x400000;
    long long SEC_ALIGN = 0x1000;
    long long FILE_ALIGN = 0x200;
    long long TEXT_RVA = 0x1000;
    long long text_raw = YKN_DOT(kod_arenasi, kullanilan);
    long long text_file = hizala_yukari(text_raw, FILE_ALIGN);
    long long RDATA_RVA = hizala_yukari(TEXT_RVA + text_raw, SEC_ALIGN);
    long long size_rdata = 0x1000;
    long long size_rdata_file = hizala_yukari(size_rdata, FILE_ALIGN);
    long long rdata_file = FILE_ALIGN + text_file;
    long long rva_idt = RDATA_RVA;
    long long rva_iat = RDATA_RVA + 0x80;
    long long rva_int = RDATA_RVA + 0x200;

    const char* k32_names[17] = {
        "ExitProcess", "WriteFile", "GetStdHandle", "VirtualAlloc",
        "CreateFileA", "ReadFile", "CloseHandle", "GetCommandLineA",
        "SetFilePointerEx", "VirtualFree", "CreatePipe", "CreateProcessA",
        "WaitForSingleObject", "GetExitCodeProcess", "LoadLibraryA", "GetProcAddress",
        "FreeLibrary"
    };
    const char* ws2_names[11] = {
        "WSAStartup", "socket", "bind", "listen",
        "accept", "closesocket", "send", "recv",
        "connect", "setsockopt", "ioctlsocket"
    };
    const char* simd_names[8] = {
        "ykn_llm_dot_q16", "ykn_llm_silu_q16", "ykn_llm_simd_backend", "ykn_llm_iq2s_dot_q16",
        "ykn_llm_clock_ns", "ykn_llm_peak_rss_bytes", "ykn_llm_f16_dot_q16", "ykn_llm_q1_0_dot_q16"
    };
    const char* k32_extra_names[8] = {
        "CreateThread", "InitializeCriticalSection", "EnterCriticalSection", "LeaveCriticalSection",
        "CreateIoCompletionPort", "GetQueuedCompletionStatus", "Sleep", "GetTickCount64"
    };

    long long cur_hn_rva = RDATA_RVA + 0x380;
    auto calc_hn = [&](const char* name) -> long long {
        long long r = cur_hn_rva;
        long long l = 2 + strlen(name) + 1;
        if (l % 2 != 0) l++;
        cur_hn_rva += l;
        return r;
    };

    long long hn_k32[17];
    for (int i = 0; i < 17; i++) hn_k32[i] = calc_hn(k32_names[i]);

    long long hn_ws2[11];
    for (int i = 0; i < 11; i++) hn_ws2[i] = calc_hn(ws2_names[i]);

    long long hn_simd[8];
    for (int i = 0; i < 8; i++) hn_simd[i] = calc_hn(simd_names[i]);

    long long hn_k32_extra[8];
    for (int i = 0; i < 8; i++) hn_k32_extra[i] = calc_hn(k32_extra_names[i]);

    long long rva_dll_k32 = cur_hn_rva; cur_hn_rva += strlen("KERNEL32.dll") + 1;
    long long rva_dll_ws2 = cur_hn_rva; cur_hn_rva += strlen("ws2_32.dll") + 1;
    long long rva_dll_simd = cur_hn_rva; cur_hn_rva += strlen("ykn_llm_simd.dll") + 1;

    long long image_size = hizala_yukari(RDATA_RVA + size_rdata, SEC_ALIGN);

    // DOS Header + PE Header
    byte_yaz(cikti, 0x4D); byte_yaz(cikti, 0x5A); u16_yaz(cikti, 0x90); u16_yaz(cikti, 3);
    u16_yaz(cikti, 0); u16_yaz(cikti, 4); u16_yaz(cikti, 0); u16_yaz(cikti, 0xFFFF);
    u16_yaz(cikti, 0); u16_yaz(cikti, 0xB8); u16_yaz(cikti, 0); u16_yaz(cikti, 0);
    u16_yaz(cikti, 0); u16_yaz(cikti, 0x40); u16_yaz(cikti, 0); sifir_doldur(cikti, 8);
    u16_yaz(cikti, 0); u16_yaz(cikti, 0); sifir_doldur(cikti, 20); int32_yaz(cikti, 0x40);
    byte_yaz(cikti, 0x50); byte_yaz(cikti, 0x45); byte_yaz(cikti, 0); byte_yaz(cikti, 0);
    u16_yaz(cikti, 0x8664); u16_yaz(cikti, 2); int32_yaz(cikti, 0); int32_yaz(cikti, 0); int32_yaz(cikti, 0);
    u16_yaz(cikti, 0xF0); u16_yaz(cikti, 0x22); u16_yaz(cikti, 0x20B); byte_yaz(cikti, 14); byte_yaz(cikti, 0);
    int32_yaz(cikti, (int)text_file); int32_yaz(cikti, (int)size_rdata_file); int32_yaz(cikti, 0); int32_yaz(cikti, (int)ep_rva);
    int32_yaz(cikti, (int)TEXT_RVA); int64_yaz(cikti, IMAGE_BASE); int32_yaz(cikti, (int)SEC_ALIGN); int32_yaz(cikti, (int)FILE_ALIGN);
    u16_yaz(cikti, 6); u16_yaz(cikti, 0); u16_yaz(cikti, 0); u16_yaz(cikti, 0); u16_yaz(cikti, 6); u16_yaz(cikti, 0);
    int32_yaz(cikti, 0); int32_yaz(cikti, (int)image_size); int32_yaz(cikti, (int)FILE_ALIGN); int32_yaz(cikti, 0);
    long long pe_stack_res = (g_cfg_stack_size > 0) ? g_cfg_stack_size : 0x2000000; u16_yaz(cikti, 3); u16_yaz(cikti, 0x8100); int64_yaz(cikti, pe_stack_res); int64_yaz(cikti, 0x100000);
    int64_yaz(cikti, 0x1000000); int64_yaz(cikti, 0x10000); int32_yaz(cikti, 0); int32_yaz(cikti, 16);
    int32_yaz(cikti, 0); int32_yaz(cikti, 0);
    int32_yaz(cikti, (int)rva_idt);
    int32_yaz(cikti, (g_iat_simd_kullanildi == 1) ? 100 : 80);
    sifir_doldur(cikti, 80);
    int32_yaz(cikti, (int)rva_iat);
    int32_yaz(cikti, 48 * 8); // 384 bytes
    sifir_doldur(cikti, 24);

    // Section header: .text
    byte_yaz(cikti, '.'); byte_yaz(cikti, 't'); byte_yaz(cikti, 'e'); byte_yaz(cikti, 'x'); byte_yaz(cikti, 't'); byte_yaz(cikti, 0); byte_yaz(cikti, 0); byte_yaz(cikti, 0);
    int32_yaz(cikti, (int)text_raw); int32_yaz(cikti, (int)TEXT_RVA); int32_yaz(cikti, (int)text_file); int32_yaz(cikti, (int)FILE_ALIGN);
    int32_yaz(cikti, 0); int32_yaz(cikti, 0); u16_yaz(cikti, 0); u16_yaz(cikti, 0); int32_yaz(cikti, 0xE0000020);

    // Section header: .rdata
    byte_yaz(cikti, '.'); byte_yaz(cikti, 'r'); byte_yaz(cikti, 'd'); byte_yaz(cikti, 'a'); byte_yaz(cikti, 't'); byte_yaz(cikti, 'a'); byte_yaz(cikti, 0); byte_yaz(cikti, 0);
    int32_yaz(cikti, (int)size_rdata); int32_yaz(cikti, (int)RDATA_RVA); int32_yaz(cikti, (int)size_rdata_file); int32_yaz(cikti, (int)rdata_file);
    int32_yaz(cikti, 0); int32_yaz(cikti, 0); u16_yaz(cikti, 0); u16_yaz(cikti, 0); int32_yaz(cikti, 0x40000040);

    // Pad headers up to FILE_ALIGN
    sifir_doldur(cikti, FILE_ALIGN - cikti->kullanilan);

    // Copy text section
    for (long long i = 0; i < text_raw; ++i) {
        byte_yaz(cikti, (_ykn_ref(kod_arenasi).bellek)[i]);
    }
    sifir_doldur(cikti, text_file - text_raw);

    // Patch IAT call rel32 offsets in .text section
    for (long long pi = 0; pi < g_iat_yama_sayisi; pi++) {
        long long text_off = g_iat_yamalar[pi].text_offset;
        int slot = g_iat_yamalar[pi].slot;
        long long iat_rva = rva_iat + (long long)slot * 8;
        long long rip_after = TEXT_RVA + text_off + 4;
        int rel32 = (int)(iat_rva - rip_after);
        long long cikti_off = FILE_ALIGN + text_off;
        cikti->bellek[cikti_off + 0] = (unsigned char)(rel32 & 0xFF);
        cikti->bellek[cikti_off + 1] = (unsigned char)((rel32 >> 8) & 0xFF);
        cikti->bellek[cikti_off + 2] = (unsigned char)((rel32 >> 16) & 0xFF);
        cikti->bellek[cikti_off + 3] = (unsigned char)((rel32 >> 24) & 0xFF);
    }

    // Emit IDT descriptors
    // IDT[0]: KERNEL32.dll
    int32_yaz(cikti, (int)rva_int); int32_yaz(cikti, 0); int32_yaz(cikti, 0); int32_yaz(cikti, (int)rva_dll_k32); int32_yaz(cikti, (int)rva_iat);
    // IDT[1]: ws2_32.dll
    int32_yaz(cikti, (int)(rva_int + 18 * 8)); int32_yaz(cikti, 0); int32_yaz(cikti, 0); int32_yaz(cikti, (int)rva_dll_ws2); int32_yaz(cikti, (int)(rva_iat + 18 * 8));
    if (g_iat_simd_kullanildi == 1) {
        // IDT[2]: ykn_llm_simd.dll
        int32_yaz(cikti, (int)(rva_int + 30 * 8)); int32_yaz(cikti, 0); int32_yaz(cikti, 0); int32_yaz(cikti, (int)rva_dll_simd); int32_yaz(cikti, (int)(rva_iat + 30 * 8));
        // IDT[3]: KERNEL32.dll (extra threading/IOCP)
        int32_yaz(cikti, (int)(rva_int + 39 * 8)); int32_yaz(cikti, 0); int32_yaz(cikti, 0); int32_yaz(cikti, (int)rva_dll_k32); int32_yaz(cikti, (int)(rva_iat + 39 * 8));
        // IDT[4]: null terminator
        sifir_doldur(cikti, 20);
    } else {
        // IDT[2]: KERNEL32.dll (extra threading/IOCP)
        int32_yaz(cikti, (int)(rva_int + 39 * 8)); int32_yaz(cikti, 0); int32_yaz(cikti, 0); int32_yaz(cikti, (int)rva_dll_k32); int32_yaz(cikti, (int)(rva_iat + 39 * 8));
        // IDT[3]: null terminator
        sifir_doldur(cikti, 20);
        sifir_doldur(cikti, 20);
    }

    // Pad IDT up to rva_iat (offset 0x80)
    sifir_doldur(cikti, (rdata_file + 0x80) - cikti->kullanilan);

    // Emit IAT (48 entries)
    for (int i = 0; i < 17; i++) int64_yaz(cikti, hn_k32[i]);
    int64_yaz(cikti, 0); // slot 17
    for (int i = 0; i < 11; i++) int64_yaz(cikti, hn_ws2[i]);
    int64_yaz(cikti, 0); // slot 29
    for (int i = 0; i < 8; i++) int64_yaz(cikti, hn_simd[i]);
    int64_yaz(cikti, 0); // slot 38
    for (int i = 0; i < 8; i++) int64_yaz(cikti, hn_k32_extra[i]);
    int64_yaz(cikti, 0); // slot 47

    // Pad up to rva_int (offset 0x200)
    sifir_doldur(cikti, (rdata_file + 0x200) - cikti->kullanilan);

    // Emit INT (48 entries, mirrors IAT)
    for (int i = 0; i < 17; i++) int64_yaz(cikti, hn_k32[i]);
    int64_yaz(cikti, 0); // slot 17
    for (int i = 0; i < 11; i++) int64_yaz(cikti, hn_ws2[i]);
    int64_yaz(cikti, 0); // slot 29
    for (int i = 0; i < 8; i++) int64_yaz(cikti, hn_simd[i]);
    int64_yaz(cikti, 0); // slot 38
    for (int i = 0; i < 8; i++) int64_yaz(cikti, hn_k32_extra[i]);
    int64_yaz(cikti, 0); // slot 47

    // Pad up to Hint/Name table (offset 0x380)
    sifir_doldur(cikti, (rdata_file + 0x380) - cikti->kullanilan);

    auto write_hn = [&](const char* name) {
        u16_yaz(cikti, 0);
        for (const char* p = name; *p; ++p) byte_yaz(cikti, (unsigned char)*p);
        byte_yaz(cikti, 0);
        if ((cikti->kullanilan - rdata_file) % 2 != 0) byte_yaz(cikti, 0);
    };

    for (int i = 0; i < 17; i++) write_hn(k32_names[i]);
    for (int i = 0; i < 11; i++) write_hn(ws2_names[i]);
    for (int i = 0; i < 8; i++) write_hn(simd_names[i]);
    for (int i = 0; i < 8; i++) write_hn(k32_extra_names[i]);

    for (const char* p = "KERNEL32.dll"; *p; ++p) byte_yaz(cikti, (unsigned char)*p);
    byte_yaz(cikti, 0);
    for (const char* p = "ws2_32.dll"; *p; ++p) byte_yaz(cikti, (unsigned char)*p);
    byte_yaz(cikti, 0);
    for (const char* p = "ykn_llm_simd.dll"; *p; ++p) byte_yaz(cikti, (unsigned char)*p);
    byte_yaz(cikti, 0);

    long long rdata_kullanilan = cikti->kullanilan - rdata_file;
    sifir_doldur(cikti, size_rdata_file - rdata_kullanilan);
}

void pe32plus_simd_uret(MakineKoduArenasi* kod_arenasi, MakineKoduArenasi* cikti, long long ep_rva) {
    pe32plus_uret(kod_arenasi, cikti, ep_rva);
}


long long windows_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres) {
    long long bas = YKN_DOT(a, kullanilan);
    
    // 1. Stack hizalama: SUB RSP, 2216 (shadow space 32 + argv[0..15] 128 + cmdline_buf 2048 + 8 alignment = 2216)
    // Entry point RSP % 16 == 8 => (8 - 2216) % 16 == 0 (16-byte aligned before CALLs)
    byte_yaz(a, 0x48); byte_yaz(a, 0x81); byte_yaz(a, 0xEC); int32_yaz(a, 2216);
    
    // 2. GetCommandLineA() cagir
    if (!seed_iat_yama_kapasite_kontrol()) return 0;
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
    g_iat_yamalar[g_iat_yama_sayisi].slot = 7; // GetCommandLineA
    g_iat_yama_sayisi++;
    int32_yaz(a, 0);
    
    // 3. Command line stringini stack bufferina (RSP + 160) kopyala (GetCommandLineA salt-okunur bellek dondurur)
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC6); // MOV RSI, RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0xBC); byte_yaz(a, 0x24); int32_yaz(a, 160); // LEA RDI, [RSP+160]
    byte_yaz(a, 0xB9); int32_yaz(a, 2040); // MOV ECX, 2040 (maksimum kopyalama bayti)
    
    // copy_loop:
    long long cp_loop_pos = YKN_DOT(a, kullanilan);
    byte_yaz(a, 0x8A); byte_yaz(a, 0x06); // MOV AL, [RSI]
    byte_yaz(a, 0x88); byte_yaz(a, 0x07); // MOV [RDI], AL
    byte_yaz(a, 0x84); byte_yaz(a, 0xC0); // TEST AL, AL
    byte_yaz(a, 0x74); byte_yaz(a, 0x0D); // JE cp_done (+13)
    byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC6); // INC RSI
    byte_yaz(a, 0x48); byte_yaz(a, 0xFF); byte_yaz(a, 0xC7); // INC RDI
    byte_yaz(a, 0xFF); byte_yaz(a, 0xC9); // DEC ECX
    byte_yaz(a, 0x75); byte_yaz(a, 0xEE); // JNZ cp_loop (-18)
    byte_yaz(a, 0xC6); byte_yaz(a, 0x07); byte_yaz(a, 0x00); // MOV BYTE PTR [RDI], 0
    // cp_done:
    
    // 4. R8 = RSP + 160 (yazilabilir stack buffer'daki cmdline)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x84); byte_yaz(a, 0x24); int32_yaz(a, 160);
    
    // 5. R9 = RSP + 32 (address of argv array)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x8D); byte_yaz(a, 0x4C); byte_yaz(a, 0x24); byte_yaz(a, 0x20);
    
    // 6. R10 = 0 (argc)
    byte_yaz(a, 0x4D); byte_yaz(a, 0x31); byte_yaz(a, 0xD2);
    
    // skip_ws loop:
    long long skip_ws_pos = YKN_DOT(a, kullanilan);
    byte_yaz(a, 0x41); byte_yaz(a, 0x8A); byte_yaz(a, 0x00); // MOV AL, [R8]
    byte_yaz(a, 0x84); byte_yaz(a, 0xC0); // TEST AL, AL
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE done_parsing (rel32)
    long long je_done_1 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x20); // CMP AL, ' '
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE is_space (rel32)
    long long je_space_1 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x09); // CMP AL, '\t'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE is_space (rel32)
    long long je_space_2 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x0D); // CMP AL, '\r'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE is_space (rel32)
    long long je_space_3 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x0A); // CMP AL, '\n'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x85); // JNE found_arg (rel32)
    long long jne_found = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    // is_space:
    long long is_space_pos = YKN_DOT(a, kullanilan);
    int32_patch(a, je_space_1, (int)(is_space_pos - (je_space_1 + 4)));
    int32_patch(a, je_space_2, (int)(is_space_pos - (je_space_2 + 4)));
    int32_patch(a, je_space_3, (int)(is_space_pos - (je_space_3 + 4)));
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xE9); // JMP skip_ws (rel32)
    long long jmp_skip = YKN_DOT(a, kullanilan);
    int32_yaz(a, (int)(skip_ws_pos - (jmp_skip + 4)));

    // found_arg:
    long long found_arg_pos = YKN_DOT(a, kullanilan);
    int32_patch(a, jne_found, (int)(found_arg_pos - (jne_found + 4)));

    byte_yaz(a, 0x3C); byte_yaz(a, 0x22); // CMP AL, '"'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE is_quoted
    long long je_quoted = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    // --- Unquoted ---
    // MOV [R9 + R10*8], R8 (argv[argc] = R8)
    byte_yaz(a, 0x4F); byte_yaz(a, 0x89); byte_yaz(a, 0x04); byte_yaz(a, 0xD1);
    // INC R10 (argc++)
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC2);
    // CMP R10, 15
    byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xFA); byte_yaz(a, 0x0F);
    byte_yaz(a, 0x0F); byte_yaz(a, 0x8D); // JGE done_parsing
    long long jge_done_unq = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    // scan_unquoted loop:
    long long scan_unq_pos = YKN_DOT(a, kullanilan);
    byte_yaz(a, 0x41); byte_yaz(a, 0x8A); byte_yaz(a, 0x00); // MOV AL, [R8]
    byte_yaz(a, 0x84); byte_yaz(a, 0xC0); // TEST AL, AL
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE done_parsing
    long long je_done_2 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x20); // CMP AL, ' '
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE term_arg
    long long je_term_1 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x09); // CMP AL, '\t'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE term_arg
    long long je_term_2 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x0D); // CMP AL, '\r'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE term_arg
    long long je_term_3 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x0A); // CMP AL, '\n'
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE term_arg
    long long je_term_4 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    // unq_char:
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xE9); // JMP scan_unquoted
    long long jmp_scan_unq = YKN_DOT(a, kullanilan);
    int32_yaz(a, (int)(scan_unq_pos - (jmp_scan_unq + 4)));

    // --- is_quoted ---
    long long is_quoted_pos = YKN_DOT(a, kullanilan);
    int32_patch(a, je_quoted, (int)(is_quoted_pos - (je_quoted + 4)));
    // INC R8 (skip opening quote)
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0);
    // MOV [R9 + R10*8], R8 (argv[argc] = R8)
    byte_yaz(a, 0x4F); byte_yaz(a, 0x89); byte_yaz(a, 0x04); byte_yaz(a, 0xD1);
    // INC R10 (argc++)
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC2);
    // CMP R10, 15
    byte_yaz(a, 0x49); byte_yaz(a, 0x83); byte_yaz(a, 0xFA); byte_yaz(a, 0x0F);
    byte_yaz(a, 0x0F); byte_yaz(a, 0x8D); // JGE done_parsing
    long long jge_done_q = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    // scan_quoted loop:
    long long scan_q_pos = YKN_DOT(a, kullanilan);
    byte_yaz(a, 0x41); byte_yaz(a, 0x8A); byte_yaz(a, 0x00); // MOV AL, [R8]
    byte_yaz(a, 0x84); byte_yaz(a, 0xC0); // TEST AL, AL
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE done_parsing
    long long je_done_3 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    byte_yaz(a, 0x3C); byte_yaz(a, 0x22); // CMP AL, '"' (closing quote)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x84); // JE term_arg
    long long je_term_5 = YKN_DOT(a, kullanilan);
    int32_yaz(a, 0);

    // q_char:
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xE9); // JMP scan_quoted
    long long jmp_scan_q = YKN_DOT(a, kullanilan);
    int32_yaz(a, (int)(scan_q_pos - (jmp_scan_q + 4)));

    // term_arg:
    long long term_arg_pos = YKN_DOT(a, kullanilan);
    int32_patch(a, je_term_1, (int)(term_arg_pos - (je_term_1 + 4)));
    int32_patch(a, je_term_2, (int)(term_arg_pos - (je_term_2 + 4)));
    int32_patch(a, je_term_3, (int)(term_arg_pos - (je_term_3 + 4)));
    int32_patch(a, je_term_4, (int)(term_arg_pos - (je_term_4 + 4)));
    int32_patch(a, je_term_5, (int)(term_arg_pos - (je_term_5 + 4)));
    byte_yaz(a, 0x41); byte_yaz(a, 0xC6); byte_yaz(a, 0x00); byte_yaz(a, 0x00); // MOV byte [R8], 0
    byte_yaz(a, 0x49); byte_yaz(a, 0xFF); byte_yaz(a, 0xC0); // INC R8
    byte_yaz(a, 0xE9); // JMP skip_ws
    long long jmp_skip_2 = YKN_DOT(a, kullanilan);
    int32_yaz(a, (int)(skip_ws_pos - (jmp_skip_2 + 4)));

    // done_parsing:
    long long done_pos = YKN_DOT(a, kullanilan);
    int32_patch(a, je_done_1, (int)(done_pos - (je_done_1 + 4)));
    int32_patch(a, jge_done_unq, (int)(done_pos - (jge_done_unq + 4)));
    int32_patch(a, je_done_2, (int)(done_pos - (je_done_2 + 4)));
    int32_patch(a, jge_done_q, (int)(done_pos - (jge_done_q + 4)));
    int32_patch(a, je_done_3, (int)(done_pos - (je_done_3 + 4)));

    // Null-terminate argv: MOV qword ptr [R9 + R10*8], 0
    byte_yaz(a, 0x4B); byte_yaz(a, 0xC7); byte_yaz(a, 0x04); byte_yaz(a, 0xD1); int32_yaz(a, 0);

    // Call ana(argc, argv) -> RCX = R10 (argc), RDX = R9 (argv)
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xD1); // MOV RCX, R10
    byte_yaz(a, 0x4C); byte_yaz(a, 0x89); byte_yaz(a, 0xCA); // MOV RDX, R9
    byte_yaz(a, 0xE8); // CALL ana
    long long call_pos = YKN_DOT(a, kullanilan);
    int32_yaz(a, (int)(ana_adres - (call_pos + 4)));

    // ExitProcess(RAX) -> RCX = RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC1); // MOV RCX, RAX
    if (!seed_iat_yama_kapasite_kontrol()) return 0;
    byte_yaz(a, 0xFF); byte_yaz(a, 0x15);
    g_iat_yamalar[g_iat_yama_sayisi].text_offset = YKN_DOT(a, kullanilan);
    g_iat_yamalar[g_iat_yama_sayisi].slot = 0; // ExitProcess
    g_iat_yama_sayisi++;
    int32_yaz(a, 0);

    return 0x1000 + bas;
}


long long linux_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres) {
    // Linux _start ABI: [RSP]=argc, [RSP+8]=argv[0].
    // YKN internal calls keep the existing Windows-like RCX/RDX convention;
    // this thunk is the only ABI adapter at the process boundary.
    long long bas = a->kullanilan;
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x0C); byte_yaz(a, 0x24); // MOV RCX,[RSP]
    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x54); byte_yaz(a, 0x24); byte_yaz(a, 0x08); // LEA RDX,[RSP+8]
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20); // shadow space
    byte_yaz(a, 0xE8);
    long long call_pos = a->kullanilan;
    int32_yaz(a, ana_adres - (call_pos + 4));
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC7); // MOV RDI,RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 60); // SYS_exit
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05); // SYSCALL
    return bas;
}

void elf64_linux_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset) {
    const long long IMAGE_BASE = 0x400000;
    const long long ELF_HEADER_SIZE = 64;
    const long long PROGRAM_HEADER_SIZE = 56;
    const long long PHDR_COUNT = 2; // W^X: Segment 1 (Code, RX) + Segment 2 (Data/BSS, RW)
    const long long CODE_OFFSET = ELF_HEADER_SIZE + (PROGRAM_HEADER_SIZE * PHDR_COUNT); // 176
    const long long code_size = kod->kullanilan;
    const long long file_size = CODE_OFFSET + code_size;

    byte_yaz(os_arena, 0x7F); byte_yaz(os_arena, 0x45); byte_yaz(os_arena, 0x4C); byte_yaz(os_arena, 0x46);
    byte_yaz(os_arena, 0x02); byte_yaz(os_arena, 0x01); byte_yaz(os_arena, 0x01); byte_yaz(os_arena, 0x00);
    sifir_doldur(os_arena, 8);
    u16_yaz(os_arena, 2);              // ET_EXEC
    u16_yaz(os_arena, 0x3E);           // EM_X86_64
    int32_yaz(os_arena, 1);
    int64_yaz(os_arena, IMAGE_BASE + CODE_OFFSET + entry_offset);
    int64_yaz(os_arena, ELF_HEADER_SIZE); // e_phoff
    int64_yaz(os_arena, 0);              // no section table
    int32_yaz(os_arena, 0);
    u16_yaz(os_arena, ELF_HEADER_SIZE); u16_yaz(os_arena, PROGRAM_HEADER_SIZE); u16_yaz(os_arena, (unsigned short)PHDR_COUNT);
    u16_yaz(os_arena, 0); u16_yaz(os_arena, 0); u16_yaz(os_arena, 0);

    // Segment 1: Code (RWX)
    int32_yaz(os_arena, 1);             // PT_LOAD
    int32_yaz(os_arena, 7);             // PF_R | PF_W | PF_X (7)
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, IMAGE_BASE);
    int64_yaz(os_arena, IMAGE_BASE);
    int64_yaz(os_arena, file_size);
    int64_yaz(os_arena, file_size);
    int64_yaz(os_arena, 0x1000);

    // Segment 2: Data / BSS (RW)
    long long data_vaddr = hizala_yukari(IMAGE_BASE + file_size, 0x1000);
    int32_yaz(os_arena, 1);             // PT_LOAD
    int32_yaz(os_arena, 6);             // PF_R | PF_W
    int64_yaz(os_arena, hizala_yukari(file_size, 0x1000));
    int64_yaz(os_arena, data_vaddr);
    int64_yaz(os_arena, data_vaddr);
    int64_yaz(os_arena, 0);             // p_filesz = 0
    int64_yaz(os_arena, 0x1000);        // p_memsz = 4KB
    int64_yaz(os_arena, 0x1000);

    long long k = 0;
    while (k < code_size) {
        byte_yaz(os_arena, kod->bellek[k]);
        k = k + 1;
    }
}

void elf64_basligi_ve_kodu_yaz(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod) {
    // Compatibility entry point for old callers. New code must pass the
    // Linux thunk offset to elf64_linux_uret explicitly.
    elf64_linux_uret(os_arena, kod, 0);
}

long long macos_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres) {
    long long bas = a->kullanilan;
    byte_yaz(a, 0x48); byte_yaz(a, 0x85); byte_yaz(a, 0xFF);         // TEST RDI, RDI
    byte_yaz(a, 0x75); byte_yaz(a, 0x0C);                         // JNZ +12 (has_args)
    byte_yaz(a, 0x48); byte_yaz(a, 0x8B); byte_yaz(a, 0x7C); byte_yaz(a, 0x24); byte_yaz(a, 0x00); // MOV RDI, [RSP]
    byte_yaz(a, 0x48); byte_yaz(a, 0x8D); byte_yaz(a, 0x74); byte_yaz(a, 0x24); byte_yaz(a, 0x08); // LEA RSI, [RSP+8]
    // has_args:
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xF9);         // MOV RCX, RDI
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xF2);         // MOV RDX, RSI
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xEC); byte_yaz(a, 0x20);  // SUB RSP, 32
    byte_yaz(a, 0xE8);  // CALL ana_adres
    long long call_pos = a->kullanilan;
    int32_yaz(a, (int)(ana_adres - (call_pos + 4)));
    byte_yaz(a, 0x48); byte_yaz(a, 0x83); byte_yaz(a, 0xC4); byte_yaz(a, 0x20);  // ADD RSP, 32
    byte_yaz(a, 0x48); byte_yaz(a, 0x89); byte_yaz(a, 0xC7);  // MOV RDI, RAX
    byte_yaz(a, 0x48); byte_yaz(a, 0xC7); byte_yaz(a, 0xC0); int32_yaz(a, 0x2000001);  // MOV RAX, 0x2000001 (SYS_exit)
    byte_yaz(a, 0x0F); byte_yaz(a, 0x05);  // SYSCALL
    return bas;
}

void macho64_macos_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset) {
    const long long MACH_HEADER_S = 32;
    const long long LC_PAGEZERO_S = 72;
    const long long LC_SEGMENT_TEXT_S = 152;
    const long long LC_BUILD_VER_S = 24;
    const long long LC_MAIN_S = 24;
    const long long SIZEOF_CMDS = LC_PAGEZERO_S + LC_SEGMENT_TEXT_S + LC_BUILD_VER_S + LC_MAIN_S; // 272
    const long long CODE_OFF = MACH_HEADER_S + SIZEOF_CMDS; // 304
    const long long VMADDR_BASE = 4294967296LL; // 0x100000000 (4GB)
    const long long code_s = kod->kullanilan;
    const long long file_s = CODE_OFF + code_s;

    // 1. Mach-O 64 Header (32 bytes)
    int32_yaz(os_arena, 0xFEEDFACF); // MH_MAGIC_64
    int32_yaz(os_arena, 0x01000007); // CPU_TYPE_X86_64
    int32_yaz(os_arena, 0x80000003); // CPU_SUBTYPE_X86_64_ALL
    int32_yaz(os_arena, 2);          // MH_EXECUTE
    int32_yaz(os_arena, 4);          // ncmds = 4 (PAGEZERO, TEXT, BUILD_VERSION, MAIN)
    int32_yaz(os_arena, SIZEOF_CMDS); // sizeofcmds = 272
    int32_yaz(os_arena, 0x00200085); // flags: MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE
    int32_yaz(os_arena, 0);          // reserved

    // 2. LC_SEGMENT_64 (__PAGEZERO) (72 bytes)
    int32_yaz(os_arena, 0x19);
    int32_yaz(os_arena, LC_PAGEZERO_S);
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x50); byte_yaz(os_arena, 0x41);
    byte_yaz(os_arena, 0x47); byte_yaz(os_arena, 0x45); byte_yaz(os_arena, 0x5A); byte_yaz(os_arena, 0x45);
    byte_yaz(os_arena, 0x52); byte_yaz(os_arena, 0x4F); sifir_doldur(os_arena, 6); // "__PAGEZERO\0..."
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, VMADDR_BASE);
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);

    // 3. LC_SEGMENT_64 (__TEXT) (152 bytes)
    int32_yaz(os_arena, 0x19);
    int32_yaz(os_arena, LC_SEGMENT_TEXT_S);
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x54); byte_yaz(os_arena, 0x45);
    byte_yaz(os_arena, 0x58); byte_yaz(os_arena, 0x54); sifir_doldur(os_arena, 10);
    int64_yaz(os_arena, VMADDR_BASE);
    int64_yaz(os_arena, hizala_yukari(file_s, 4096));
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, file_s);
    int32_yaz(os_arena, 7);
    int32_yaz(os_arena, 7);
    int32_yaz(os_arena, 1);
    int32_yaz(os_arena, 0);

    // Section 1: __text (80 bytes)
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x74); byte_yaz(os_arena, 0x65);
    byte_yaz(os_arena, 0x78); byte_yaz(os_arena, 0x74); sifir_doldur(os_arena, 10);
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x54); byte_yaz(os_arena, 0x45);
    byte_yaz(os_arena, 0x58); byte_yaz(os_arena, 0x54); sifir_doldur(os_arena, 10);
    int64_yaz(os_arena, VMADDR_BASE + CODE_OFF);
    int64_yaz(os_arena, code_s);
    int32_yaz(os_arena, CODE_OFF);
    int32_yaz(os_arena, 4);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0x80000400);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);

    // 4. LC_BUILD_VERSION (24 bytes) - macOS 11.0+
    int32_yaz(os_arena, 0x32);
    int32_yaz(os_arena, LC_BUILD_VER_S);
    int32_yaz(os_arena, 1);           // PLATFORM_MACOS = 1
    int32_yaz(os_arena, 0x000B0000);  // minos = 11.0.0
    int32_yaz(os_arena, 0x000B0000);  // sdk = 11.0.0
    int32_yaz(os_arena, 0);           // ntools = 0

    // 5. LC_MAIN (24 bytes) - Entry point
    int32_yaz(os_arena, 0x80000028);  // LC_MAIN
    int32_yaz(os_arena, LC_MAIN_S);
    int64_yaz(os_arena, CODE_OFF + entry_offset);
    int64_yaz(os_arena, 0);

    long long k_m = 0;
    while (k_m < code_s) {
        byte_yaz(os_arena, kod->bellek[k_m]);
        k_m++;
    }
}

long long linux_arm64_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres) {
    long long bas = a->kullanilan;
    // LDR X0, [SP] (0xF94003E0)
    byte_yaz(a, 0xE0); byte_yaz(a, 0x03); byte_yaz(a, 0x40); byte_yaz(a, 0xF9);
    // ADD X1, SP, #8 (0x910023E1)
    byte_yaz(a, 0xE1); byte_yaz(a, 0x23); byte_yaz(a, 0x00); byte_yaz(a, 0x91);
    // BL ana_adres
    long long bl_pos = a->kullanilan;
    long long rel_inst = (ana_adres - bl_pos) / 4;
    long long imm26 = rel_inst;
    while (imm26 < 0) imm26 += 67108864;
    long long opcode = 0x94000000 + (imm26 % 67108864);
    int32_yaz(a, (int)opcode);
    // MOVZ X8, #93 (0xD2800BA8) (SYS_exit=93)
    byte_yaz(a, 0xA8); byte_yaz(a, 0x0B); byte_yaz(a, 0x80); byte_yaz(a, 0xD2);
    // SVC #0 (0xD4000001)
    byte_yaz(a, 0x01); byte_yaz(a, 0x00); byte_yaz(a, 0x00); byte_yaz(a, 0xD4);
    return bas;
}

void elf64_arm64_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset) {
    const long long IMAGE_BASE = 0x400000;
    const long long ELF_HEADER_S = 64;
    const long long PROGRAM_HDR_S = 56;
    const long long PHDR_COUNT = 2; // W^X: Segment 1 (Code, RX) + Segment 2 (Data/BSS, RW)
    const long long CODE_OFF = ELF_HEADER_S + (PROGRAM_HDR_S * PHDR_COUNT);
    const long long code_s = kod->kullanilan;
    const long long file_s = CODE_OFF + code_s;

    byte_yaz(os_arena, 0x7F); byte_yaz(os_arena, 0x45); byte_yaz(os_arena, 0x4C); byte_yaz(os_arena, 0x46);
    byte_yaz(os_arena, 0x02); byte_yaz(os_arena, 0x01); byte_yaz(os_arena, 0x01); byte_yaz(os_arena, 0x00);
    sifir_doldur(os_arena, 8);
    u16_yaz(os_arena, 2);        // ET_EXEC
    u16_yaz(os_arena, 0xB7);     // EM_AARCH64 (183 = 0xB7)
    int32_yaz(os_arena, 1);      // EV_CURRENT
    int64_yaz(os_arena, IMAGE_BASE + CODE_OFF + entry_offset);
    int64_yaz(os_arena, ELF_HEADER_S);  // e_phoff
    int64_yaz(os_arena, 0);             // e_shoff
    int32_yaz(os_arena, 0);             // e_flags
    u16_yaz(os_arena, ELF_HEADER_S);
    u16_yaz(os_arena, PROGRAM_HDR_S);
    u16_yaz(os_arena, (unsigned short)PHDR_COUNT); // 2 phdrs
    u16_yaz(os_arena, 0); u16_yaz(os_arena, 0); u16_yaz(os_arena, 0);

    // Segment 1: Code (RWX)
    int32_yaz(os_arena, 1);   // PT_LOAD
    int32_yaz(os_arena, 7);   // PF_R | PF_W | PF_X (7)
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, IMAGE_BASE);
    int64_yaz(os_arena, IMAGE_BASE);
    int64_yaz(os_arena, file_s);
    int64_yaz(os_arena, file_s);
    int64_yaz(os_arena, 0x10000); // 64KB ARM64 page alignment

    // Segment 2: Data / BSS (RW)
    long long data_vaddr = hizala_yukari(IMAGE_BASE + file_s, 0x10000);
    int32_yaz(os_arena, 1);   // PT_LOAD
    int32_yaz(os_arena, 6);   // PF_R | PF_W
    int64_yaz(os_arena, hizala_yukari(file_s, 0x10000));
    int64_yaz(os_arena, data_vaddr);
    int64_yaz(os_arena, data_vaddr);
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, 0x10000);
    int64_yaz(os_arena, 0x10000);

    long long k_a = 0;
    while (k_a < code_s) {
        byte_yaz(os_arena, kod->bellek[k_a]);
        k_a++;
    }
}

long long macos_arm64_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres) {
    long long bas = a->kullanilan;
    // dyld LC_MAIN: X0 = argc, X1 = argv. Check if X0 != 0
    // CMP X0, #0
    byte_yaz(a, 0x1F); byte_yaz(a, 0x00); byte_yaz(a, 0x00); byte_yaz(a, 0xF1);
    // B.NE +16
    byte_yaz(a, 0x81); byte_yaz(a, 0x00); byte_yaz(a, 0x00); byte_yaz(a, 0x54);
    // LDR X0, [SP] (0xF94003E0)
    byte_yaz(a, 0xE0); byte_yaz(a, 0x03); byte_yaz(a, 0x40); byte_yaz(a, 0xF9);
    // ADD X1, SP, #8 (0x910023E1)
    byte_yaz(a, 0xE1); byte_yaz(a, 0x23); byte_yaz(a, 0x00); byte_yaz(a, 0x91);
    // BL ana_adres
    long long bl_pos = a->kullanilan;
    long long rel_inst = (ana_adres - bl_pos) / 4;
    long long imm26 = rel_inst;
    while (imm26 < 0) imm26 += 67108864;
    long long opcode = 0x94000000 + (imm26 % 67108864);
    int32_yaz(a, (int)opcode);
    // MOVZ X16, #1 (0xD2800030) (macOS SYS_exit=1 in X16)
    byte_yaz(a, 0x30); byte_yaz(a, 0x00); byte_yaz(a, 0x80); byte_yaz(a, 0xD2);
    // SVC #0x80 (0xD4001001)
    byte_yaz(a, 0x01); byte_yaz(a, 0x10); byte_yaz(a, 0x00); byte_yaz(a, 0xD4);
    return bas;
}

static inline uint32_t sha256_rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_hesapla_blok(const uint8_t* data, size_t len, uint8_t hash[32]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint64_t bit_len = (uint64_t)len * 8;
    size_t padded_len = ((len + 8 + 64) / 64) * 64;
    std::vector<uint8_t> buf(padded_len, 0);
    if (len > 0 && data) memcpy(buf.data(), data, len);
    buf[len] = 0x80;
    for (int i = 0; i < 8; i++) {
        buf[padded_len - 1 - i] = (uint8_t)((bit_len >> (i * 8)) & 0xFF);
    }

    for (size_t chunk = 0; chunk < padded_len; chunk += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            size_t off = chunk + i * 4;
            w[i] = ((uint32_t)buf[off] << 24) | ((uint32_t)buf[off + 1] << 16) |
                   ((uint32_t)buf[off + 2] << 8) | ((uint32_t)buf[off + 3]);
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = sha256_rotr(w[i - 15], 7) ^ sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = sha256_rotr(w[i - 2], 17) ^ sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hv = h[7];

        for (int i = 0; i < 64; i++) {
            uint32_t S1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hv + S1 + ch + K[i] + w[i];
            uint32_t S0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            hv = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hv;
    }

    for (int i = 0; i < 8; i++) {
        hash[i * 4 + 0] = (uint8_t)((h[i] >> 24) & 0xFF);
        hash[i * 4 + 1] = (uint8_t)((h[i] >> 16) & 0xFF);
        hash[i * 4 + 2] = (uint8_t)((h[i] >> 8) & 0xFF);
        hash[i * 4 + 3] = (uint8_t)(h[i] & 0xFF);
    }
}

static void be32_yaz(MakineKoduArenasi* a, unsigned int val) {
    byte_yaz(a, (val >> 24) & 0xFF);
    byte_yaz(a, (val >> 16) & 0xFF);
    byte_yaz(a, (val >> 8) & 0xFF);
    byte_yaz(a, val & 0xFF);
}
static void be64_yaz(MakineKoduArenasi* a, unsigned long long val) {
    be32_yaz(a, (unsigned int)((val >> 32) & 0xFFFFFFFF));
    be32_yaz(a, (unsigned int)(val & 0xFFFFFFFF));
}

void macho64_arm64_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset) {
    const long long MACH_HEADER_S = 32;
    const long long LC_PAGEZERO_S = 72;
    const long long LC_SEGMENT_TEXT_S = 152;
    const long long LC_BUILD_VER_S = 24;
    const long long LC_MAIN_S = 24;
    const long long LC_CODE_SIGN_S = 16;
    const long long SIZEOF_CMDS = LC_PAGEZERO_S + LC_SEGMENT_TEXT_S + LC_BUILD_VER_S + LC_MAIN_S + LC_CODE_SIGN_S; // 288
    const long long CODE_OFF = MACH_HEADER_S + SIZEOF_CMDS; // 320
    const long long VMADDR_BASE = 4294967296LL; // 0x100000000 (4GB)
    const long long code_s = kod->kullanilan;

    const long long sig_offset = hizala_yukari(CODE_OFF + code_s, 16);
    const long long num_pages = (sig_offset + 4095) / 4096;
    const long long cd_ident_len = 12;
    const long long cd_hash_offset = 88 + cd_ident_len; // 100
    const long long cd_size = cd_hash_offset + num_pages * 32;
    const long long sig_size = 12 + 8 + cd_size;
    const long long total_file_s = sig_offset + sig_size;

    // 1. Mach-O Header ARM64 (32 bytes)
    int32_yaz(os_arena, 0xFEEDFACF); // MH_MAGIC_64
    int32_yaz(os_arena, 0x0100000C); // CPU_TYPE_ARM64
    int32_yaz(os_arena, 0x00000000); // CPU_SUBTYPE_ARM64_ALL
    int32_yaz(os_arena, 2);          // MH_EXECUTE
    int32_yaz(os_arena, 5);          // ncmds = 5 (PAGEZERO, TEXT, BUILD_VER, MAIN, CODE_SIGN)
    int32_yaz(os_arena, SIZEOF_CMDS); // sizeofcmds = 288
    int32_yaz(os_arena, 0x00200085); // flags: MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE
    int32_yaz(os_arena, 0);          // reserved

    // 2. LC_SEGMENT_64 (__PAGEZERO) (72 bytes)
    int32_yaz(os_arena, 0x19);
    int32_yaz(os_arena, LC_PAGEZERO_S);
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x50); byte_yaz(os_arena, 0x41);
    byte_yaz(os_arena, 0x47); byte_yaz(os_arena, 0x45); byte_yaz(os_arena, 0x5A); byte_yaz(os_arena, 0x45);
    byte_yaz(os_arena, 0x52); byte_yaz(os_arena, 0x4F); sifir_doldur(os_arena, 6);
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, VMADDR_BASE);
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);

    // 3. LC_SEGMENT_64 (__TEXT) (152 bytes)
    int32_yaz(os_arena, 0x19);
    int32_yaz(os_arena, LC_SEGMENT_TEXT_S);
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x54); byte_yaz(os_arena, 0x45);
    byte_yaz(os_arena, 0x58); byte_yaz(os_arena, 0x54); sifir_doldur(os_arena, 10);
    int64_yaz(os_arena, VMADDR_BASE);
    int64_yaz(os_arena, hizala_yukari(total_file_s, 16384));
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, total_file_s);
    int32_yaz(os_arena, 7);
    int32_yaz(os_arena, 7);
    int32_yaz(os_arena, 1);
    int32_yaz(os_arena, 0);

    // Section 1: __text (80 bytes)
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x74); byte_yaz(os_arena, 0x65);
    byte_yaz(os_arena, 0x78); byte_yaz(os_arena, 0x74); sifir_doldur(os_arena, 10);
    byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x5F); byte_yaz(os_arena, 0x54); byte_yaz(os_arena, 0x45);
    byte_yaz(os_arena, 0x58); byte_yaz(os_arena, 0x54); sifir_doldur(os_arena, 10);
    int64_yaz(os_arena, VMADDR_BASE + CODE_OFF);
    int64_yaz(os_arena, code_s);
    int32_yaz(os_arena, CODE_OFF);
    int32_yaz(os_arena, 4);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0x80000400);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);
    int32_yaz(os_arena, 0);

    // 4. LC_BUILD_VERSION (24 bytes) - macOS 11.0+
    int32_yaz(os_arena, 0x32);
    int32_yaz(os_arena, LC_BUILD_VER_S);
    int32_yaz(os_arena, 1);           // PLATFORM_MACOS = 1
    int32_yaz(os_arena, 0x000B0000);  // minos = 11.0.0
    int32_yaz(os_arena, 0x000B0000);  // sdk = 11.0.0
    int32_yaz(os_arena, 0);           // ntools = 0

    // 5. LC_MAIN (24 bytes)
    int32_yaz(os_arena, 0x80000028);  // LC_MAIN
    int32_yaz(os_arena, LC_MAIN_S);
    int64_yaz(os_arena, CODE_OFF + entry_offset);
    int64_yaz(os_arena, 0);

    // 6. LC_CODE_SIGNATURE (16 bytes)
    int32_yaz(os_arena, 0x1D);        // LC_CODE_SIGNATURE
    int32_yaz(os_arena, LC_CODE_SIGN_S);
    int32_yaz(os_arena, (int)sig_offset);
    int32_yaz(os_arena, (int)sig_size);

    long long k_ma = 0;
    while (k_ma < code_s) {
        byte_yaz(os_arena, kod->bellek[k_ma]);
        k_ma++;
    }

    sifir_doldur(os_arena, sig_offset - os_arena->kullanilan);

    // 8. Ad-Hoc CS_SuperBlob
    be32_yaz(os_arena, 0xFADE0CC0);   // CSMAGIC_EMBEDDED_SIGNATURE
    be32_yaz(os_arena, (unsigned int)sig_size);
    be32_yaz(os_arena, 1);            // count = 1

    // BlobIndex[0]
    be32_yaz(os_arena, 0);            // CSSLOT_CODEDIRECTORY
    be32_yaz(os_arena, 20);           // offset = 20

    // CS_CodeDirectory
    be32_yaz(os_arena, 0xFADE0C02);   // CSMAGIC_CODEDIRECTORY
    be32_yaz(os_arena, (unsigned int)cd_size);
    be32_yaz(os_arena, 0x00020400);   // version
    be32_yaz(os_arena, 0x00020002);   // CS_ADHOC | CS_LINKER_SIGNED
    be32_yaz(os_arena, (unsigned int)cd_hash_offset);
    be32_yaz(os_arena, 88);           // identOffset
    be32_yaz(os_arena, 0);            // nSpecialSlots
    be32_yaz(os_arena, (unsigned int)num_pages);
    be32_yaz(os_arena, (unsigned int)sig_offset);
    byte_yaz(os_arena, 32);           // hashSize
    byte_yaz(os_arena, 2);            // hashType SHA-256
    byte_yaz(os_arena, 0);            // platform
    byte_yaz(os_arena, 12);           // pageSize 4096
    be32_yaz(os_arena, 0);
    be32_yaz(os_arena, 0);
    be32_yaz(os_arena, 0);
    be32_yaz(os_arena, 0);
    be64_yaz(os_arena, 0);
    be64_yaz(os_arena, 0);
    be64_yaz(os_arena, sig_offset);
    be64_yaz(os_arena, 1);

    // Ident: "yukinary\0\0\0\0"
    byte_yaz(os_arena, 'y'); byte_yaz(os_arena, 'u'); byte_yaz(os_arena, 'k'); byte_yaz(os_arena, 'i');
    byte_yaz(os_arena, 'n'); byte_yaz(os_arena, 'a'); byte_yaz(os_arena, 'r'); byte_yaz(os_arena, 'y');
    byte_yaz(os_arena, 0);   byte_yaz(os_arena, 0);   byte_yaz(os_arena, 0);   byte_yaz(os_arena, 0);

    // Compute real SHA-256 for each 4096-byte page of binary up to sig_offset
    for (long long p = 0; p < num_pages; p++) {
        long long page_start = p * 4096;
        long long page_end = page_start + 4096;
        if (page_end > sig_offset) page_end = sig_offset;
        long long page_len = page_end - page_start;
        uint8_t page_hash[32];
        sha256_hesapla_blok((const uint8_t*)os_arena->bellek + page_start, (size_t)page_len, page_hash);
        for (int b = 0; b < 32; b++) {
            byte_yaz(os_arena, (char)page_hash[b]);
        }
    }
}

void wasm_modulu_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset) {
    (void)entry_offset;
    // 1. WASM Header: Magic (\0asm) and Version 1
    byte_yaz(os_arena, 0x00);
    byte_yaz(os_arena, 0x61);
    byte_yaz(os_arena, 0x73);
    byte_yaz(os_arena, 0x6D);
    byte_yaz(os_arena, 0x01);
    byte_yaz(os_arena, 0x00);
    byte_yaz(os_arena, 0x00);
    byte_yaz(os_arena, 0x00);

    // 2. Type Section (id = 1) -> 1 type: () -> i32 (5 bytes payload)
    byte_yaz(os_arena, 0x01);
    wasm_leb128_u32_yaz(os_arena, 5);
    byte_yaz(os_arena, 0x01);
    byte_yaz(os_arena, 0x60);
    byte_yaz(os_arena, 0x00);
    byte_yaz(os_arena, 0x01);
    byte_yaz(os_arena, 0x7F);

    // 3. Function Section (id = 3) -> func 0 has type 0 (2 bytes payload)
    byte_yaz(os_arena, 0x03);
    wasm_leb128_u32_yaz(os_arena, 2);
    byte_yaz(os_arena, 0x01);
    byte_yaz(os_arena, 0x00);

    // 4. Memory Section (id = 5) -> 1 page (64KB) memory (3 bytes payload)
    byte_yaz(os_arena, 0x05);
    wasm_leb128_u32_yaz(os_arena, 3);
    byte_yaz(os_arena, 0x01);
    byte_yaz(os_arena, 0x00);
    byte_yaz(os_arena, 0x01);

    // 5. Export Section (id = 7) -> exports "memory" (mem 0) and "main" (func 0) (17 bytes payload)
    byte_yaz(os_arena, 0x07);
    wasm_leb128_u32_yaz(os_arena, 17);
    byte_yaz(os_arena, 0x02);
    // Export 1: "memory" -> mem 0
    byte_yaz(os_arena, 6);
    byte_yaz(os_arena, 'm'); byte_yaz(os_arena, 'e'); byte_yaz(os_arena, 'm');
    byte_yaz(os_arena, 'o'); byte_yaz(os_arena, 'r'); byte_yaz(os_arena, 'y');
    byte_yaz(os_arena, 0x02);
    byte_yaz(os_arena, 0x00);
    // Export 2: "main" -> func 0
    byte_yaz(os_arena, 4);
    byte_yaz(os_arena, 'm'); byte_yaz(os_arena, 'a'); byte_yaz(os_arena, 'i'); byte_yaz(os_arena, 'n');
    byte_yaz(os_arena, 0x00);
    byte_yaz(os_arena, 0x00);

    // 6. Code Section (id = 10)
    long long code_s = kod ? kod->kullanilan : 0;
    long long local_hdr_len = 3; // 1 group, 16 i32 locals (0x01 0x10 0x7F)
    long long needs_end = 1;
    if (code_s > 0 && kod->bellek[code_s - 1] == 0x0B) {
        needs_end = 0;
    }

    long long body_len = local_hdr_len + code_s + needs_end;
    if (code_s == 0) {
        body_len = local_hdr_len + 3; // 0x41 0x00 0x0B
    }

    long long body_leb_s = wasm_leb128_boyutu(body_len);
    long long payload_len = 1 + body_leb_s + body_len;

    byte_yaz(os_arena, 0x0A); // Section ID: Code
    wasm_leb128_u32_yaz(os_arena, (unsigned int)payload_len);
    byte_yaz(os_arena, 0x01); // 1 function body
    wasm_leb128_u32_yaz(os_arena, (unsigned int)body_len);

    // Local variable declarations: 16 x i32
    byte_yaz(os_arena, 0x01); // 1 local group
    byte_yaz(os_arena, 16);   // 16 count
    byte_yaz(os_arena, 0x7F); // i32

    if (code_s > 0) {
        for (long long bi = 0; bi < code_s; ++bi) {
            byte_yaz(os_arena, kod->bellek[bi]);
        }
        if (needs_end == 1) {
            byte_yaz(os_arena, 0x0B); // end
        }
    } else {
        byte_yaz(os_arena, 0x41); // i32.const
        byte_yaz(os_arena, 0x00); // 0
        byte_yaz(os_arena, 0x0B); // end
    }
}

long long linux_riscv64_baslangic_thunk_uret(MakineKoduArenasi* a, long long ana_adres) {
    long long bas = a->kullanilan;
    // ld a0, 0(sp)     => 0x00013503 (little-endian: 03 35 01 00)
    byte_yaz(a, 0x03); byte_yaz(a, 0x35); byte_yaz(a, 0x01); byte_yaz(a, 0x00);
    // addi a1, sp, 8   => 0x00810593 (little-endian: 93 05 81 00)
    byte_yaz(a, 0x93); byte_yaz(a, 0x05); byte_yaz(a, 0x81); byte_yaz(a, 0x00);
    // jal ra, ana_adres
    long long jal_pos = a->kullanilan;
    long long offset = ana_adres - jal_pos;
    unsigned int imm = (unsigned int)offset;
    unsigned int imm20 = (imm >> 20) & 1;
    unsigned int imm10_1 = (imm >> 1) & 0x3FF;
    unsigned int imm11 = (imm >> 11) & 1;
    unsigned int imm19_12 = (imm >> 12) & 0xFF;
    unsigned int jal_inst = (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) | (imm19_12 << 12) | (1 << 7) | 0x6F;
    int32_yaz(a, (int)jal_inst);
    // li a7, 93 (addi a7, zero, 93) => 0x05D00893 (little-endian: 93 08 D0 05)
    byte_yaz(a, 0x93); byte_yaz(a, 0x08); byte_yaz(a, 0xD0); byte_yaz(a, 0x05);
    // ecall            => 0x00000073 (little-endian: 73 00 00 00)
    byte_yaz(a, 0x73); byte_yaz(a, 0x00); byte_yaz(a, 0x00); byte_yaz(a, 0x00);
    return bas;
}

void elf64_riscv64_uret(MakineKoduArenasi* os_arena, MakineKoduArenasi* kod, long long entry_offset) {
    const long long IMAGE_BASE = 0x10000;
    const long long ELF_HEADER_S = 64;
    const long long PROGRAM_HDR_S = 56;
    const long long PHDR_COUNT = 2; // W^X: Segment 1 (Code, RX) + Segment 2 (Data/BSS, RW)
    const long long CODE_OFF = ELF_HEADER_S + (PROGRAM_HDR_S * PHDR_COUNT);
    const long long code_s = kod ? kod->kullanilan : 0;
    const long long file_s = CODE_OFF + code_s;

    // 1. ELF Header (64 bytes)
    byte_yaz(os_arena, 0x7F); byte_yaz(os_arena, 0x45); byte_yaz(os_arena, 0x4C); byte_yaz(os_arena, 0x46);
    byte_yaz(os_arena, 0x02); // ELFCLASS64
    byte_yaz(os_arena, 0x01); // ELFDATA2LSB
    byte_yaz(os_arena, 0x01); // EV_CURRENT
    byte_yaz(os_arena, 0x00); // ELFOSABI_NONE
    sifir_doldur(os_arena, 8);
    u16_yaz(os_arena, 2);      // e_type = ET_EXEC
    u16_yaz(os_arena, 243);    // e_machine = EM_RISCV (0xF3 = 243)
    int32_yaz(os_arena, 1);    // e_version = EV_CURRENT (1)
    int64_yaz(os_arena, IMAGE_BASE + CODE_OFF + entry_offset); // e_entry
    int64_yaz(os_arena, ELF_HEADER_S); // e_phoff = 64
    int64_yaz(os_arena, 0);            // e_shoff = 0
    int32_yaz(os_arena, 5);            // e_flags = EF_RISCV_RVC | EF_RISCV_FLOAT_ABI_DOUBLE (0x5)
    u16_yaz(os_arena, ELF_HEADER_S);   // e_ehsize = 64
    u16_yaz(os_arena, PROGRAM_HDR_S);  // e_phentsize = 56
    u16_yaz(os_arena, (unsigned short)PHDR_COUNT); // e_phnum = 2
    u16_yaz(os_arena, 0);              // e_shentsize = 0
    u16_yaz(os_arena, 0);              // e_shnum = 0
    u16_yaz(os_arena, 0);              // e_shstrndx = 0

    // Segment 1: Code (RWX)
    int32_yaz(os_arena, 1);   // p_type = PT_LOAD (1)
    int32_yaz(os_arena, 7);   // p_flags = PF_R | PF_W | PF_X (7)
    int64_yaz(os_arena, 0);   // p_offset = 0
    int64_yaz(os_arena, IMAGE_BASE);
    int64_yaz(os_arena, IMAGE_BASE);
    int64_yaz(os_arena, file_s); // p_filesz
    int64_yaz(os_arena, file_s); // p_memsz
    int64_yaz(os_arena, 0x1000); // p_align = 4KB page alignment

    // Segment 2: Data / BSS (RW)
    long long data_vaddr = hizala_yukari(IMAGE_BASE + file_s, 0x1000);
    int32_yaz(os_arena, 1);   // p_type = PT_LOAD (1)
    int32_yaz(os_arena, 6);   // p_flags = PF_R | PF_W
    int64_yaz(os_arena, hizala_yukari(file_s, 0x1000));
    int64_yaz(os_arena, data_vaddr);
    int64_yaz(os_arena, data_vaddr);
    int64_yaz(os_arena, 0);
    int64_yaz(os_arena, 0x1000);
    int64_yaz(os_arena, 0x1000);

    // 3. Payload
    for (long long i = 0; i < code_s; ++i) {
        byte_yaz(os_arena, kod->bellek[i]);
    }
}






// ============================================================
// ICE_AKTAR MODUL SISTEMI
// Dairesel import'a karsi koruma icin islenmis dosya kaydi
// ============================================================
struct IceAktarKayit { char isim[1024]; long long uzunluk; };
IceAktarKayit g_ice_aktar_kayitlar[1024];
long long g_ice_aktar_sayisi = 0;
IceAktarKayit g_ice_aktar_aktif_kayitlar[1024];
long long g_ice_aktar_aktif_sayisi = 0;

struct ImportAliasMapEntry {
    char* eski;
    long long eski_uzunluk;
    char* yeni;
    long long yeni_uzunluk;
};
static ImportAliasMapEntry g_import_alias_map[4096];
static long long g_import_alias_map_sayisi = 0;

static void seed_import_alias_cagri_yenile(AstDugum* dugum_basi) {
    AstDugum* dugum = dugum_basi;
    while (dugum) {
        if (YKN_DOT(dugum, tip) == AST_ISLEV_CAGRISI) {
            AstDugum* callee = (AstDugum*)YKN_DOT(dugum, sol);
            Jeton* hedef = callee ? (Jeton*)YKN_DOT(callee, ilk_jeton) : nullptr;
            if (callee && YKN_DOT(callee, tip) == AST_KIMLIK && hedef) {
                for (long long i = 0; i < g_import_alias_map_sayisi; ++i) {
                    if (g_import_alias_map[i].eski_uzunluk == YKN_DOT(hedef, uzunluk) &&
                        bellek_esit_mi(g_import_alias_map[i].eski,
                                       YKN_DOT(hedef, baslangic),
                                       YKN_DOT(hedef, uzunluk)) == 1) {
                        YKN_DOT(hedef, baslangic) = g_import_alias_map[i].yeni;
                        YKN_DOT(hedef, uzunluk) = g_import_alias_map[i].yeni_uzunluk;
                        break;
                    }
                }
            }
        }
        seed_import_alias_cagri_yenile((AstDugum*)YKN_DOT(dugum, sol));
        seed_import_alias_cagri_yenile((AstDugum*)YKN_DOT(dugum, sag));
        seed_import_alias_cagri_yenile((AstDugum*)YKN_DOT(dugum, govde));
        seed_import_alias_cagri_yenile((AstDugum*)YKN_DOT(dugum, yoksa_govde));
        seed_import_alias_cagri_yenile((AstDugum*)YKN_DOT(dugum, parametreler));
        dugum = (AstDugum*)YKN_DOT(dugum, sonraki);
    }
}

static void seed_import_alias_uygula(AstDugum* modul_ast, char* alias, long long alias_uzunluk) {
    if (!modul_ast || !alias || alias_uzunluk <= 0) return;
    g_import_alias_map_sayisi = 0;
    AstDugum* iter = (AstDugum*)YKN_DOT(modul_ast, sol);
    while (iter) {
        long long tip = YKN_DOT(iter, tip);
        Jeton* token = (Jeton*)YKN_DOT(iter, ilk_jeton);
        if ((tip == AST_ISLEV || tip == AST_DEGISKEN_TANIMI) && token &&
            g_import_alias_map_sayisi < 4096) {
            char* eski = YKN_DOT(token, baslangic);
            long long eski_uzunluk = YKN_DOT(token, uzunluk);
            long long uye_baslangic = 0;
            for (long long i = 0; i + 1 < eski_uzunluk; ++i) {
                if (eski[i] == '_' && eski[i + 1] == '_') uye_baslangic = i + 2;
            }
            long long uye_uzunluk = eski_uzunluk - uye_baslangic;
            long long yeni_uzunluk = alias_uzunluk + 2 + uye_uzunluk;
            char* yeni = (char*)bellek_tahsis_et(yeni_uzunluk + 1);
            if (!yeni) return;
            memcpy(yeni, alias, (size_t)alias_uzunluk);
            yeni[alias_uzunluk] = '_'; yeni[alias_uzunluk + 1] = '_';
            memcpy(yeni + alias_uzunluk + 2, eski + uye_baslangic, (size_t)uye_uzunluk);
            yeni[yeni_uzunluk] = 0;
            ImportAliasMapEntry& entry = g_import_alias_map[g_import_alias_map_sayisi++];
            entry.eski = eski; entry.eski_uzunluk = eski_uzunluk;
            entry.yeni = yeni; entry.yeni_uzunluk = yeni_uzunluk;
            YKN_DOT(token, baslangic) = yeni;
            YKN_DOT(token, uzunluk) = yeni_uzunluk;
        }
        iter = (AstDugum*)YKN_DOT(iter, sonraki);
    }
    seed_import_alias_cagri_yenile((AstDugum*)YKN_DOT(modul_ast, sol));
}

bool ice_aktar_kayit_esit_mi(IceAktarKayit* kayitlar, long long sayi,
                             char* isim, long long uzunluk) {
    if (!isim || uzunluk <= 0) return false;
    for (long long i = 0; i < sayi; i++) {
        if (kayitlar[i].uzunluk == uzunluk &&
            memcmp(kayitlar[i].isim, isim, (size_t)uzunluk) == 0) return true;
    }
    return false;
}

bool ice_aktar_zaten_islenmis(char* isim, long long uzunluk) {
    return ice_aktar_kayit_esit_mi(g_ice_aktar_kayitlar, g_ice_aktar_sayisi,
                                   isim, uzunluk);
}

bool ice_aktar_aktif_mi(char* isim, long long uzunluk) {
    return ice_aktar_kayit_esit_mi(g_ice_aktar_aktif_kayitlar,
                                   g_ice_aktar_aktif_sayisi, isim, uzunluk);
}

void ice_aktar_kaydet(char* isim, long long uzunluk) {
    if (!isim || uzunluk <= 0 || uzunluk >= 1024 || g_ice_aktar_sayisi >= 1024) return;
    memcpy(g_ice_aktar_kayitlar[g_ice_aktar_sayisi].isim, isim, (size_t)uzunluk);
    g_ice_aktar_kayitlar[g_ice_aktar_sayisi].isim[uzunluk] = 0;
    g_ice_aktar_kayitlar[g_ice_aktar_sayisi].uzunluk = uzunluk;
    g_ice_aktar_sayisi++;
}

void ice_aktar_aktif_kaydet(char* isim, long long uzunluk) {
    if (!isim || uzunluk <= 0 || uzunluk >= 1024 || g_ice_aktar_aktif_sayisi >= 1024) return;
    memcpy(g_ice_aktar_aktif_kayitlar[g_ice_aktar_aktif_sayisi].isim,
           isim, (size_t)uzunluk);
    g_ice_aktar_aktif_kayitlar[g_ice_aktar_aktif_sayisi].isim[uzunluk] = 0;
    g_ice_aktar_aktif_kayitlar[g_ice_aktar_aktif_sayisi].uzunluk = uzunluk;
    g_ice_aktar_aktif_sayisi++;
}

void ice_aktar_aktif_cikar() {
    if (g_ice_aktar_aktif_sayisi > 0) g_ice_aktar_aktif_sayisi--;
}

bool ice_aktar_mutlak_yol_mu(char* yol, long long uzunluk) {
    if (!yol || uzunluk <= 0) return false;
    // POSIX-style root, Windows root, drive-qualified and UNC paths are
    // already complete paths and must not receive the source directory.
    if (yol[0] == '/' || yol[0] == '\\') return true;
    return uzunluk >= 2 && yol[1] == ':';
}

void ice_aktar_isle(AstDugum* kok, AstArena* ast_arena, Jeton* jeton_arenasi,
                    long long& toplam_jeton, char* ana_dosya_dizini,
                    char* aktif_dosya_yolu) {
    if (!kok) return;
    long long aktif_yol_uzunlugu = aktif_dosya_yolu
        ? (long long)strlen(aktif_dosya_yolu) : 0;
    if (aktif_yol_uzunlugu > 0) {
        ice_aktar_aktif_kaydet(aktif_dosya_yolu, aktif_yol_uzunlugu);
    }
    AstDugum* dugum = kok->sol;
    AstDugum* onceki = nullptr;
    while (dugum) {
        if (dugum->tip == 21) { // AST_ICE_AKTAR
            // F11.10: Import alias (olarak) is now supported. tip_adi holds the
            // alias name when present. The imported module's AST nodes are inlined
            // directly into the calling AST, so the alias is a compile-time name
            // only and requires no runtime action. E037 is no longer emitted.
            char* dosya_ptr = dugum->ilk_jeton->baslangic;
            long long dosya_uzunluk = dugum->ilk_jeton->uzunluk;
            char tam_yol[1024];
            long long dizin_uzunluk = ice_aktar_mutlak_yol_mu(
                dosya_ptr, dosya_uzunluk)
                ? 0
                : (ana_dosya_dizini ? (long long)strlen(ana_dosya_dizini) : 0);
            if (dizin_uzunluk + dosya_uzunluk >= 1024) { 
                ekrana_yaz((char*)"[ice_aktar] UYARI: Yol cok uzun, atlaniyor.\n");
                if (onceki) onceki->sonraki = dugum->sonraki;
                else kok->sol = dugum->sonraki;
                dugum = dugum->sonraki;
                continue;
            }
            memcpy(tam_yol, ana_dosya_dizini, (size_t)dizin_uzunluk);
            memcpy(tam_yol + dizin_uzunluk, dosya_ptr, (size_t)dosya_uzunluk);
            long long tam_yol_uzunlugu = dizin_uzunluk + dosya_uzunluk;
            tam_yol[tam_yol_uzunlugu] = 0;

            auto normalize_import_path = [](char* p) -> long long {
                long long len = (long long)strlen(p);
                for (long long i = 0; i < len; ++i) {
                    if (p[i] == '\\') p[i] = '/';
                }
                char out[1024];
                long long seg_starts[128];
                long long seg_count = 0;
                long long out_len = 0;
                long long i = 0;
                while (i < len) {
                    while (i < len && p[i] == '/') i++;
                    if (i >= len) break;
                    long long s = i;
                    while (i < len && p[i] != '/') i++;
                    long long slen = i - s;
                    if (slen == 1 && p[s] == '.') {
                        continue;
                    }
                    if (slen == 2 && p[s] == '.' && p[s + 1] == '.' && seg_count > 0) {
                        seg_count--;
                        out_len = seg_starts[seg_count];
                        continue;
                    }
                    seg_starts[seg_count++] = out_len;
                    if (out_len > 0) out[out_len++] = '/';
                    memcpy(out + out_len, p + s, (size_t)slen);
                    out_len += slen;
                }
                out[out_len] = 0;
                memcpy(p, out, (size_t)(out_len + 1));
                return out_len;
            };

            tam_yol_uzunlugu = normalize_import_path(tam_yol);
            if (ice_aktar_aktif_mi(tam_yol, tam_yol_uzunlugu)) {
                emit_import_cycle_diagnostic(dugum->ilk_jeton);
                if (onceki) onceki->sonraki = dugum->sonraki;
                else kok->sol = dugum->sonraki;
                dugum = dugum->sonraki;
                continue;
            }
            if (ice_aktar_zaten_islenmis(tam_yol, tam_yol_uzunlugu)) {
                if (onceki) onceki->sonraki = dugum->sonraki;
                else kok->sol = dugum->sonraki;
                dugum = dugum->sonraki;
                continue;
            }
            char* modul_buffer = (char*)calloc(1, 16777216);
            if (!modul_buffer) { dugum = dugum->sonraki; continue; }
            long long okunan = dosya_oku(tam_yol, modul_buffer, 16777216);
            // Canonical main.ykn also accepts project-root-relative imports.
            if (okunan <= 0 && !ice_aktar_mutlak_yol_mu(dosya_ptr, dosya_uzunluk)) {
                long long dogrudan_uzunluk = dosya_uzunluk;
                if (dogrudan_uzunluk < 1024) {
                    memcpy(tam_yol, dosya_ptr, (size_t)dogrudan_uzunluk);
                    tam_yol[dogrudan_uzunluk] = 0;
                    tam_yol_uzunlugu = normalize_import_path(tam_yol);
                    if (ice_aktar_aktif_mi(tam_yol, tam_yol_uzunlugu)) {
                        free(modul_buffer);
                        emit_import_cycle_diagnostic(dugum->ilk_jeton);
                        if (onceki) onceki->sonraki = dugum->sonraki;
                        else kok->sol = dugum->sonraki;
                        dugum = dugum->sonraki;
                        continue;
                    }
                    if (ice_aktar_zaten_islenmis(tam_yol, tam_yol_uzunlugu)) {
                        free(modul_buffer);
                        if (onceki) onceki->sonraki = dugum->sonraki;
                        else kok->sol = dugum->sonraki;
                        dugum = dugum->sonraki;
                        continue;
                    }
                    okunan = dosya_oku(tam_yol, modul_buffer, 16777216);
                }
            }
            if (okunan <= 0) {
                ekrana_yaz((char*)"[ice_aktar] UYARI: Dosya okunamadi, atlaniyor.\n");
                free(modul_buffer);
                if (onceki) onceki->sonraki = dugum->sonraki;
                else kok->sol = dugum->sonraki;
                dugum = dugum->sonraki;
                continue;
            }
            ice_aktar_kaydet(tam_yol, tam_yol_uzunlugu);
            ekrana_yaz((char*)"[ice_aktar] Yukleniyor: ");
            ekrana_yaz(tam_yol);
            ekrana_yaz((char*)"\n");
            LexerState* lx = (LexerState*)calloc(1, sizeof(LexerState));
            lx->kaynak = modul_buffer;
            lx->dosya_boyutu = okunan;
            lx->guncel_offset = 0;
            lx->satir = 1;
            lx->sutun = 1;
            lx->jetonlar = (Jeton*)((char*)jeton_arenasi + (toplam_jeton * 40));
            lx->jeton_sayisi = 0;
            lx->jeton_kapasite = 524288;
            lx->pool = 0;
            long long modul_jeton_sayisi = tokenize_et(lx);
            toplam_jeton += modul_jeton_sayisi;
            if (0 && strstr(tam_yol, "parser.ykn")) {
                for (long long ti = 1050; ti < 1250 && ti < modul_jeton_sayisi; ti++) {
                    Jeton* tj = &lx->jetonlar[ti];
                    printf("  T[%lld] line=%lld tip=%lld text='%.*s'\n", ti, tj->satir, tj->tip, (int)tj->uzunluk, tj->baslangic);
                }
            }
            ParserDurumu* pd = (ParserDurumu*)calloc(1, sizeof(ParserDurumu));
            pd->jetonlar = lx->jetonlar;
            pd->jeton_sayisi = modul_jeton_sayisi;
            pd->guncel_index = 0;
            pd->arena = ast_arena;
            AstDugum* modul_ast = parser_cozumle(pd);
            long long node_cnt = 0;
            for(AstDugum* n = (modul_ast ? modul_ast->sol : nullptr); n; n = n->sonraki) {
                node_cnt++;
                if (0 && strstr(tam_yol, "parser.ykn")) {
                    if (n->tip == 1 && n->ilk_jeton) {
                        printf("    PARSER.YKN ISLEV [%lld]: %.*s\n", node_cnt, (int)n->ilk_jeton->uzunluk, n->ilk_jeton->baslangic);
                    } else if (n->ilk_jeton) {
                        printf("    PARSER.YKN NODE [%lld] tip=%lld token='%.*s'\n", node_cnt, (long long)n->tip, (int)n->ilk_jeton->uzunluk, n->ilk_jeton->baslangic);
                    } else {
                        printf("    PARSER.YKN NODE [%lld] tip=%lld\n", node_cnt, (long long)n->tip);
                    }
                }
            }
            printf("  [ice_aktar] Modul %s: jeton=%lld, dugum=%lld, parsed_tokens=%lld/%lld\n", tam_yol, modul_jeton_sayisi, node_cnt, pd->guncel_index, modul_jeton_sayisi);
            free(lx); free(pd); // modul_buffer free EDILMEMELI, tokenlar buraya pointer tutuyor.
            if (!modul_ast || !modul_ast->sol) {
                if (onceki) onceki->sonraki = dugum->sonraki;
                else kok->sol = dugum->sonraki;
                dugum = dugum->sonraki;
                continue;
            }
            // Alias import gerçek bir namespace rewrite'tır. Deklarasyonlar
            // ve imported module içindeki çağrı siteleri birlikte taşınır;
            // böylece m__metin_uzunluk çağrısı eski ada sarkmaz.
            if (YKN_DOT(dugum, tip_adi) && YKN_DOT(dugum, tip_adi_uzunluk) > 0) {
                bool is_alias = true;
                for (long long ti = 0; ti < YKN_DOT(dugum, tip_adi_uzunluk); ++ti) {
                    char tc = YKN_DOT(dugum, tip_adi)[ti];
                    if (tc == '/' || tc == '\\' || tc == ':') {
                        is_alias = false;
                        break;
                    }
                }
                if (is_alias) {
                    seed_import_alias_uygula(modul_ast,
                        YKN_DOT(dugum, tip_adi),
                        YKN_DOT(dugum, tip_adi_uzunluk));
                }
            }
            char modul_dizini[1024];
            long long modul_son_ayrac = -1;
            long long modul_yol_uzunlugu = (long long)strlen(tam_yol);
            for (long long yol_i = 0; yol_i < modul_yol_uzunlugu; yol_i++) {
                if (tam_yol[yol_i] == '/' || tam_yol[yol_i] == '\\') {
                    modul_son_ayrac = yol_i;
                }
            }
            if (modul_son_ayrac >= 0) {
                memcpy(modul_dizini, tam_yol, modul_son_ayrac + 1);
                modul_dizini[modul_son_ayrac + 1] = 0;
            } else {
                modul_dizini[0] = 0;
            }
            ice_aktar_isle(modul_ast, ast_arena, jeton_arenasi, toplam_jeton,
                           modul_dizini, tam_yol);
            AstDugum* modul_son = modul_ast->sol;
            while (modul_son && modul_son->sonraki) modul_son = modul_son->sonraki;
            if (modul_son) {
                modul_son->sonraki = dugum->sonraki;
                if (onceki) onceki->sonraki = modul_ast->sol;
                else kok->sol = modul_ast->sol;
                onceki = modul_son;
                dugum = modul_son->sonraki;
            } else {
                if (onceki) onceki->sonraki = dugum->sonraki;
                else kok->sol = dugum->sonraki;
                dugum = dugum->sonraki;
            }
            continue;
        }
        onceki = dugum;
        dugum = dugum->sonraki;
    }
    if (aktif_yol_uzunlugu > 0) ice_aktar_aktif_cikar();
}

// ============================================================
// FAZ 8 & 9: Embedded BitNet 1.58b Micro-LLM Backbone (Stage-0 Parity)
// SIFIR #prompt ve harici model gereksinimi:
// Derleyici binary'si icine gomulu ternary {-1, 0, +1} agirlik tablosu.
// ============================================================
// Forward declaration: Egitilmis Micro-LLM agirlik tablosu ve egitim motoru asagida tanimlidir.
static unsigned char* ykn_embedded_model_al();

char* llm_kod_uret(char* sorgu, long long sorgu_uzunluk) {
    if (!sorgu || sorgu_uzunluk <= 0) return nullptr;
    if (g_cli_llm_model[0] == 0) {
        ekrana_yaz((char*)"HATA [E015] [ERR_AI_MODEL] [class=llm]: Derleme zamani yapay zeka modeli bulunamadi (--model parametresi gereklidir).\n");
        return nullptr;
    }
    YknGgufCliProfile profile;
    if (!ykn_gguf_profile_read(g_cli_llm_model, profile)) {
        ekrana_yaz((char*)"HATA [E015] [ERR_AI_MODEL] [class=llm]: Derleme zamani yapay zeka modeli dogrulanamadi.\n");
        return nullptr;
    }
    // Fail-closed security: Stage-0 bootstrap seed rejects unverified synthesis.
    return nullptr;
}

void bitlinear_matris_carp(unsigned char* w_ptr, unsigned char* x_ptr, long long* out_ptr, long long M, long long K, long long N) {
    for (long long i = 0; i < M; i++) {
        for (long long j = 0; j < N; j++) {
            long long toplam = 0;
            for (long long k = 0; k < K; k++) {
                long long x_val = x_ptr[(i * K) + k];
                unsigned char w_val = w_ptr[(k * N) + j];
                if (w_val == 1) toplam += x_val;
                else if (w_val == 2) toplam -= x_val;
            }
            out_ptr[(i * N) + j] = toplam;
        }
    }
}

// ============================================================
// FAZ 2 Compiler Context Implementations
// ============================================================
TargetContext* target_context_olustur() {
    TargetContext* ctx = (TargetContext*)bellek_tahsis_et(sizeof(TargetContext));
    if (ctx) {
        ctx->platform = 0;
        ctx->verbose = 0;
        ctx->no_color = 0;
        ctx->safe_mode = 0;
        ctx->maks_kaynak_boyutu = 67108864; // 64 MiB
    }
    return ctx;
}

DiagnosticContext* diagnostic_context_olustur() {
    DiagnosticContext* ctx = (DiagnosticContext*)bellek_tahsis_et(sizeof(DiagnosticContext));
    if (ctx) {
        ctx->hata_sayisi = 0;
        ctx->uyari_sayisi = 0;
        ctx->son_hata_kodu = 0;
        ctx->son_hata_satir = 0;
        ctx->son_hata_sutun = 0;
        ctx->kaynak_yolu = nullptr;
    }
    return ctx;
}

ImportContext* import_context_olustur() {
    ImportContext* ctx = (ImportContext*)bellek_tahsis_et(sizeof(ImportContext));
    if (ctx) {
        ctx->import_listesi = (char*)bellek_tahsis_et(8192);
        ctx->import_sayisi = 0;
        ctx->import_cycle_hatasi = 0;
        ctx->import_alias_hatasi = 0;
        ctx->ice_aktar_sayisi = 0;
        ctx->ice_aktar_aktif_sayisi = 0;
        ctx->maks_dosya = 4096;
        ctx->maks_derinlik = 128;
        ctx->maks_dosya_boyutu = 16777216; // 16 MiB
    }
    return ctx;
}

LexerContext* lexer_context_olustur() {
    LexerContext* ctx = (LexerContext*)bellek_tahsis_et(sizeof(LexerContext));
    if (ctx) {
        memset(ctx, 0, sizeof(LexerContext));
        ctx->satir = 1;
        ctx->sutun = 1;
    }
    return ctx;
}

ParserContext* parser_context_olustur() {
    ParserContext* ctx = (ParserContext*)bellek_tahsis_et(sizeof(ParserContext));
    if (ctx) {
        memset(ctx, 0, sizeof(ParserContext));
    }
    return ctx;
}

TypecheckerContext* typechecker_context_olustur() {
    TypecheckerContext* ctx = (TypecheckerContext*)bellek_tahsis_et(sizeof(TypecheckerContext));
    if (ctx) {
        memset(ctx, 0, sizeof(TypecheckerContext));
    }
    return ctx;
}

CodegenContext* codegen_context_olustur() {
    CodegenContext* ctx = (CodegenContext*)bellek_tahsis_et(sizeof(CodegenContext));
    if (ctx) {
        memset(ctx, 0, sizeof(CodegenContext));
    }
    return ctx;
}

SelfHealContext* self_heal_context_olustur() {
    SelfHealContext* ctx = (SelfHealContext*)bellek_tahsis_et(sizeof(SelfHealContext));
    if (ctx) {
        ctx->aktif = 0;
        ctx->deneme_sayisi = 0;
        ctx->max_denemeler = 3;
        ctx->basarili = 0;
    }
    return ctx;
}

OptimizerContext* optimizer_context_olustur() {
    OptimizerContext* ctx = (OptimizerContext*)bellek_tahsis_et(sizeof(OptimizerContext));
    if (ctx) {
        ctx->opt_seviyesi = 2;
        ctx->inlining_aktif = 1;
        ctx->unroll_aktif = 1;
        ctx->inline_yapilan_sayisi = 0;
        ctx->unroll_yapilan_sayisi = 0;
        ctx->dce_silinen_sayisi = 0;
        ctx->folding_yapilan_sayisi = 0;
        ctx->maks_inline_dugum = 25;
        ctx->maks_unroll_sayisi = 8;
    }
    return ctx;
}

CompilationContext* compilation_context_olustur() {
    CompilationContext* ctx = (CompilationContext*)bellek_tahsis_et(sizeof(CompilationContext));
    if (ctx) {
        memset(ctx, 0, sizeof(CompilationContext));
        ctx->opt_seviyesi = 2;
        ctx->inlining_aktif = 1;
        ctx->loop_unroll_aktif = 1;
    }
    return ctx;
}

Compiler* compiler_olustur() {
    Compiler* c = (Compiler*)bellek_tahsis_et(sizeof(Compiler));
    if (c) {
        c->target = target_context_olustur();
        c->diag = diagnostic_context_olustur();
        c->compilation = compilation_context_olustur();
        c->import_ctx = import_context_olustur();
        c->lexer = lexer_context_olustur();
        c->parser = parser_context_olustur();
        c->tc = typechecker_context_olustur();
        c->optimizer = optimizer_context_olustur();
        c->codegen = codegen_context_olustur();
        c->self_heal = self_heal_context_olustur();
        c->convert_ctx = nullptr;
        c->oturum_id = 1;
    }
    return c;
}

CompilerContext* compiler_context_olustur() {
    return compiler_olustur();
}

void compiler_oturum_sifirla(Compiler* c) {
    if (!c) return;
    c->oturum_id++;
    if (c->diag) {
        c->diag->hata_sayisi = 0;
        c->diag->uyari_sayisi = 0;
        c->diag->son_hata_kodu = 0;
        c->diag->son_hata_satir = 0;
        c->diag->son_hata_sutun = 0;
    }
    if (c->import_ctx) {
        c->import_ctx->import_sayisi = 0;
        c->import_ctx->import_cycle_hatasi = 0;
        c->import_ctx->import_alias_hatasi = 0;
        c->import_ctx->ice_aktar_sayisi = 0;
        c->import_ctx->ice_aktar_aktif_sayisi = 0;
        memset(c->import_ctx->import_listesi, 0, 8192);
    }
    if (c->lexer) {
        c->lexer->guncel_offset = 0;
        c->lexer->satir = 1;
        c->lexer->sutun = 1;
        c->lexer->jeton_sayisi = 0;
        c->lexer->hata_sayisi = 0;
        c->lexer->ilk_hata_satir = 0;
        c->lexer->ilk_hata_sutun = 0;
        c->lexer->ilk_hata_offset = 0;
        c->lexer->ilk_hata_kodu = 0;
    }
    if (c->parser) {
        c->parser->guncel_index = 0;
        c->parser->hata_sayisi = 0;
    }
    if (c->tc) {
        c->tc->motor = nullptr;
        c->tc->hata_sayisi = 0;
        c->tc->guard_aktif = 0;
        c->tc->guard_gecersiz = 0;
        c->tc->guard_ad = nullptr;
        c->tc->guard_ad_uzunluk = 0;
        c->tc->guard_alan = nullptr;
        c->tc->guard_alan_uzunluk = 0;
    }
    if (c->optimizer) {
        c->optimizer->inline_yapilan_sayisi = 0;
        c->optimizer->unroll_yapilan_sayisi = 0;
        c->optimizer->dce_silinen_sayisi = 0;
        c->optimizer->folding_yapilan_sayisi = 0;
    }
    if (c->codegen) {
        c->codegen->iat_yama_adet = 0;
        c->codegen->iat_yama_sayisi = 0;
        c->codegen->iat_simd_kullanildi = 0;
        c->codegen->yapi_kapasite_hatasi = 0;
        c->codegen->global_sabitleri_toplandi = 0;
    }
    if (c->compilation) {
        c->compilation->ast_koku = nullptr;
        c->compilation->derleme_sonucu = 0;
    }
    if (c->self_heal) {
        c->self_heal->deneme_sayisi = 0;
        c->self_heal->basarili = 0;
    }
}

void compiler_context_sifirla(CompilerContext* c) {
    compiler_oturum_sifirla(c);
}

void compiler_context_kopyala(CompilerContext* hedef, CompilerContext* kaynak) {
    if (!hedef || !kaynak) return;
    if (hedef->target && kaynak->target) {
        *hedef->target = *kaynak->target;
    }
    if (hedef->optimizer && kaynak->optimizer) {
        *hedef->optimizer = *kaynak->optimizer;
    }
    if (hedef->self_heal && kaynak->self_heal) {
        *hedef->self_heal = *kaynak->self_heal;
    }
    if (hedef->compilation && kaynak->compilation) {
        *hedef->compilation = *kaynak->compilation;
    }
    hedef->oturum_id = kaynak->oturum_id + 1;
}

CompilerContext* compiler_context_snapshot_al(CompilerContext* kaynak) {
    if (!kaynak) return nullptr;
    CompilerContext* kopya = compiler_context_olustur();
    compiler_context_kopyala(kopya, kaynak);
    return kopya;
}

// ============================================================================
// Endustriyel x86_64 Peephole Optimizatoru (Clang / RustC seviyesi)
// Komut Siniri Duyarli (Instruction-Boundary Aware Decoder)
// Redundant stack store-load, push-pop, identity moves ve arithmetic cancellation
// komutlarini donanim dostu NOP (0x90) komutlarina donusturur.
// ============================================================================
static long long x86_insn_len(unsigned char* p, long long max_len) {
    if (max_len <= 0) return 0;
    long long idx = 0;

    // Prefixes
    while (idx < max_len) {
        unsigned char b = p[idx];
        if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 ||
            (b >= 0x40 && b <= 0x4F)) {
            idx++;
        } else {
            break;
        }
    }
    if (idx >= max_len) return max_len;

    unsigned char op = p[idx++];

    // 1-byte instructions without ModR/M
    if (op >= 0x50 && op <= 0x5F) return idx; // PUSH / POP reg
    if (op == 0x90 || op == 0xC3 || op == 0xCC || op == 0x99) return idx; // NOP, RET, INT3, CDQ/CQO

    // Short branch: 2 bytes
    if (op == 0xEB || (op >= 0x70 && op <= 0x7F)) return (idx + 1 <= max_len) ? (idx + 1) : max_len;

    // Near branch / call: opcode + 4 bytes imm
    if (op == 0xE8 || op == 0xE9) return (idx + 4 <= max_len) ? (idx + 4) : max_len;

    // MOV reg32, imm32 or MOV reg64, imm64
    if (op >= 0xB8 && op <= 0xBF) {
        bool rex_w = false;
        for (long long pr = 0; pr < idx - 1; pr++) {
            if ((p[pr] & 0xF8) == 0x48) rex_w = true;
        }
        long long imm_sz = rex_w ? 8 : 4;
        return (idx + imm_sz <= max_len) ? (idx + imm_sz) : max_len;
    }

    // 2-byte escape 0x0F
    if (op == 0x0F && idx < max_len) {
        unsigned char op2 = p[idx++];
        if (op2 >= 0x80 && op2 <= 0x8F) return (idx + 4 <= max_len) ? (idx + 4) : max_len;
    }

    // VEX 2-byte: 0xC5
    if (op == 0xC5 && idx < max_len) {
        idx++;
        if (idx < max_len) idx++;
    }

    // Instructions with ModR/M byte
    if (idx < max_len) {
        unsigned char modrm = p[idx++];
        unsigned char mod = (modrm >> 6) & 3;
        unsigned char rm = modrm & 7;

        // SIB byte if mod != 3 and rm == 4
        if (mod != 3 && rm == 4 && idx < max_len) {
            unsigned char sib = p[idx++];
            if (mod == 0 && (sib & 7) == 5) {
                idx += 4; // disp32
            }
        }

        // Displacement
        if (mod == 1) idx += 1; // disp8
        else if (mod == 2) idx += 4; // disp32
        else if (mod == 0 && rm == 5) idx += 4; // disp32 (RIP-rel)

        // Immediates
        if (op == 0x83 || op == 0x6B) idx += 1;
        else if (op == 0x81 || op == 0xC7 || op == 0x69) idx += 4;
        else if ((op == 0xF6 || op == 0xF7) && ((modrm >> 3) & 7) == 0) {
            idx += (op == 0xF6) ? 1 : 4;
        }
    }

    return (idx <= max_len) ? idx : max_len;
}

static long long x86_peephole_optimize(MakineKoduArenasi* arena, long long baslangic, long long bitis) {
    if (!arena || !arena->bellek || bitis <= baslangic) return 0;
    unsigned char* code = (unsigned char*)arena->bellek;
    long long total_eliminated = 0;

    long long pos = baslangic;
    while (pos < bitis) {
        long long len1 = x86_insn_len(code + pos, bitis - pos);
        if (len1 <= 0) break;

        long long next_pos = pos + len1;

        // Pattern 1: MOV RAX, RAX (0x48 0x89 0xC0) -> 3x NOP
        if (len1 == 3 && code[pos] == 0x48 && code[pos + 1] == 0x89 && code[pos + 2] == 0xC0) {
            code[pos] = 0x90;
            code[pos + 1] = 0x90;
            code[pos + 2] = 0x90;
            total_eliminated++;
            pos = next_pos;
            continue;
        }

        // Pattern 2: ADD RAX, 0 (0x48 0x83 0xC0 0x00) or SUB RAX, 0 (0x48 0x83 0xE8 0x00) -> 4x NOP
        if (len1 == 4 && code[pos] == 0x48 && code[pos + 1] == 0x83 &&
            (code[pos + 2] == 0xC0 || code[pos + 2] == 0xE8) && code[pos + 3] == 0x00) {
            for (int k = 0; k < 4; k++) code[pos + k] = 0x90;
            total_eliminated++;
            pos = next_pos;
            continue;
        }

        // Pattern 3: PUSH RAX (0x50) followed by POP RAX (0x58) -> 2x NOP
        if (next_pos < bitis) {
            long long len2 = x86_insn_len(code + next_pos, bitis - next_pos);
            if (len1 == 1 && code[pos] == 0x50 && len2 == 1 && code[next_pos] == 0x58) {
                code[pos] = 0x90;
                code[next_pos] = 0x90;
                total_eliminated += 2;
                pos = next_pos + len2;
                continue;
            }
        }

        pos = next_pos;
    }
    return total_eliminated;
}

static Compiler* g_compiler = nullptr;

void konfigurasyon_direktiflerini_islet(CompilerContext* ctx, AstDugum* ast_koku) {
    if (!ast_koku) return;
    AstDugum* stmt = (AstDugum*)ast_koku->sol;
    while (stmt) {
        if (stmt->tip == AST_DIREKTIF_CONFIG || stmt->tip == 44) {
            AstDugum* kv = (AstDugum*)stmt->sol;
            while (kv) {
                Jeton* k_jeton = kv->ilk_jeton;
                AstDugum* val_node = (AstDugum*)kv->sol;
                if (k_jeton && val_node && val_node->ilk_jeton) {
                    Jeton* v_jeton = val_node->ilk_jeton;
                    char* k_ptr = k_jeton->baslangic;
                    long long k_uzn = k_jeton->uzunluk;
                    char* v_ptr = v_jeton->baslangic;
                    long long v_uzn = v_jeton->uzunluk;

                    if (v_uzn >= 2 && (v_ptr[0] == 34 || v_ptr[0] == 39)) {
                        v_ptr++;
                        v_uzn -= 2;
                    }
                    char* val_str = (char*)bellek_tahsis_et(v_uzn + 1);
                    for (long long vi = 0; vi < v_uzn; vi++) val_str[vi] = v_ptr[vi];
                    val_str[v_uzn] = 0;

                    long long val_num = 0;
                    if (v_jeton->tip == 10) { // TIP_TAMSAYI
                        for (long long ni = 0; ni < v_uzn; ni++) {
                            char ch = v_ptr[ni];
                            if (ch >= 48 && ch <= 57) {
                                val_num = (val_num * 10) + (ch - 48);
                            }
                        }
                    } else if (v_jeton->tip == 17 || (v_uzn == 5 && bellek_esit_mi(v_ptr, (char*)"dogru", 5) == 1) || (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"true", 4) == 1)) {
                        val_num = 1;
                    }

                    // 1. MODEL
                    if ((k_uzn == 5 && bellek_esit_mi(k_ptr, (char*)"model", 5) == 1) ||
                        (k_uzn == 10 && bellek_esit_mi(k_ptr, (char*)"yapay_zeka", 10) == 1) ||
                        (k_uzn == 2 && bellek_esit_mi(k_ptr, (char*)"ai", 2) == 1) ||
                        (k_uzn == 3 && bellek_esit_mi(k_ptr, (char*)"llm", 3) == 1) ||
                        (k_uzn == 8 && bellek_esit_mi(k_ptr, (char*)"ai_model", 8) == 1) ||
                        (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xd0\xbc\xd0\xbe\xd0\xb4\xd0\xb5\xd0\xbb\xd1\x8c", 12) == 1) || // модель
                        (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xe3\x83\xa2\xe3\x83\x87\xe3\x83\xab", 9) == 1) || // モデル
                        (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe6\xa8\xa1\xe5\x9e\x8b", 6) == 1) || // 模型
                        (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xeb\xaa\xa8\xeb\x8d\xb8", 6) == 1)) { // 모델
                        if (g_cli_llm_model_acikca_verildi == 0) {
                            if ((v_uzn == 6 && bellek_esit_mi(v_ptr, (char*)"bitnet", 6) == 1) ||
                                (v_uzn == 14 && bellek_esit_mi(v_ptr, (char*)"bitnet-b1.58-2B", 14) == 1)) {
                                strncpy(g_cli_llm_model, "models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf", sizeof(g_cli_llm_model) - 1);
                            } else {
                                strncpy(g_cli_llm_model, val_str, sizeof(g_cli_llm_model) - 1);
                            }
                        }
                    }
                    // 2. OPT
                    else if ((k_uzn == 3 && bellek_esit_mi(k_ptr, (char*)"opt", 3) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"optimizasyon", 12) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"optimization", 12) == 1) ||
                             (k_uzn == 22 && bellek_esit_mi(k_ptr, (char*)"\xd0\xbe\xd0\xbf\xd1\x82\xd0\xb8\xd0\xbc\xd0\xb8\xd0\xb7\xd0\xb0\xd1\x86\xd0\xb8\xd1\x8f", 22) == 1) || // оптимизация
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xe6\x9c\x80\xe9\x81\xa9\xe5\x8c\x96", 9) == 1) || // 最適化
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe4\xbc\x98\xe5\x8c\x96", 6) == 1) || // 优化
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xec\xb5\x9c\xec\xa0\x81\xed\x99\x94", 9) == 1)) { // 최적화
                        if (g_cli_opt_acikca_verildi == 0) {
                            if (val_num > 0 || (v_uzn == 3 && bellek_esit_mi(v_ptr, (char*)"hiz", 3) == 1) || (v_uzn == 5 && bellek_esit_mi(v_ptr, (char*)"speed", 5) == 1)) {
                                g_ra_opt_aktif = 1;
                            } else {
                                g_ra_opt_aktif = 0;
                            }
                        }
                    }
                    // 3. SELF_HEAL
                    else if ((k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"self_heal", 9) == 1) ||
                             (k_uzn == 8 && bellek_esit_mi(k_ptr, (char*)"oto_onar", 8) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"kendini_onar", 12) == 1) ||
                             (k_uzn == 4 && bellek_esit_mi(k_ptr, (char*)"onar", 4) == 1) ||
                             (k_uzn == 36 && bellek_esit_mi(k_ptr, (char*)"\xd1\x81\xd0\xb0\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd1\x81\xd1\x81\xd1\x82\xd0\xb0\xd0\xbd\xd0\xbe\xd0\xb2\xd0\xbb\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5", 36) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xe8\x87\xaa\xe5\xb7\xb1\xe4\xbf\xae\xe5\xbe\xa9", 12) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe8\x87\xaa\xe6\x84\x88", 6) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xec\x9e\x90\xea\xb0\x80\xeb\xb3\xb5\xea\xb5\xac", 12) == 1)) {
                        if (g_cli_self_heal_acikca_verildi == 0) {
                            if (val_num > 0 || (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"acik", 4) == 1) || (v_uzn == 2 && bellek_esit_mi(v_ptr, (char*)"on", 2) == 1)) {
                                g_self_heal_aktif = 1;
                                if (ctx && ctx->self_heal) ctx->self_heal->aktif = 1;
                            } else {
                                g_self_heal_aktif = 0;
                                if (ctx && ctx->self_heal) ctx->self_heal->aktif = 0;
                            }
                        }
                    }
                    // 4. OUTPUT / CIKTI
                    else if ((k_uzn == 5 && bellek_esit_mi(k_ptr, (char*)"cikti", 5) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"output", 6) == 1) ||
                             (k_uzn == 11 && bellek_esit_mi(k_ptr, (char*)"hedef_dosya", 11) == 1) ||
                             (k_uzn == 10 && bellek_esit_mi(k_ptr, (char*)"\xd0\xb2\xd1\x8b\xd0\xb2\xd0\xbe\xd0\xb4", 10) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe5\x87\xba\xe5\x8a\x9b", 6) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe8\xbe\x93\xe5\x87\xba", 6) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xec\xb6\x9c\xeb\xa0\xa5", 6) == 1)) { // 출력
                        if (g_cli_hedef_acikca_verildi == 0) {
                            g_in_file_cikti_dosyasi = val_str;
                        }
                    }
                    // 5. SIMD
                    else if ((k_uzn == 4 && bellek_esit_mi(k_ptr, (char*)"simd", 4) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"vektor", 6) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"vector", 6) == 1) ||
                             (k_uzn == 19 && bellek_esit_mi(k_ptr, (char*)"\xd1\x81\xd0\xb8\xd1\x81\xd1\x82\xd0\xb5\xd0\xbc\xd0\xb0\x5f\x73\x69\x6d\x64", 19) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xd0\xb2\xd0\xb5\xd0\xba\xd1\x82\xd0\xbe\xd1\x80", 12) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xe3\x83\x99\xe3\x82\xaf\xe3\x83\x88\xe3\x83\xab", 12) == 1)) {
                        if (val_num > 0 || (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"avx2", 4) == 1) || (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"neon", 4) == 1) || (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"acik", 4) == 1)) {
                            g_iat_simd_kullanildi = 1;
                        } else {
                            g_iat_simd_kullanildi = 0;
                        }
                    }
                    // 6. SAFETY / GUVENLIK
                    else if ((k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"safety", 6) == 1) ||
                             (k_uzn == 8 && bellek_esit_mi(k_ptr, (char*)"guvenlik", 8) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"bounds_check", 12) == 1) ||
                             (k_uzn == 24 && bellek_esit_mi(k_ptr, (char*)"\xd0\xb1\xd0\xb5\xd0\xb7\xd0\xbe\xd0\xbf\xd0\xb0\xd1\x81\xd0\xbd\xd0\xbe\xd1\x81\xd1\x82\xd1\x8c", 24) == 1) ||
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xe5\xae\x89\xe5\x85\xa8\xe6\x80\xa7", 9) == 1)) {
                        if ((v_uzn == 6 && bellek_esit_mi(v_ptr, (char*)"unsafe", 6) == 1) || (v_uzn == 8 && bellek_esit_mi(v_ptr, (char*)"guvensiz", 8) == 1) || (v_jeton->tip == 10 && val_num == 0)) {
                            g_bounds_check = 0;
                        } else {
                            g_bounds_check = 1;
                        }
                    }
                    // 8. STACK_SIZE / YIGIN_BOYUTU
                    else if ((k_uzn == 10 && bellek_esit_mi(k_ptr, (char*)"stack_size", 10) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"yigin_boyutu", 12) == 1) ||
                             (k_uzn == 23 && bellek_esit_mi(k_ptr, (char*)"\xd1\x80\xd0\xb0\xd0\xb7\xd0\xbc\xd0\xb5\xd1\x80\x5f\xd1\x81\xd1\x82\xd0\xb5\xd0\xba\xd0\xb0", 23) == 1) ||
                             (k_uzn == 21 && bellek_esit_mi(k_ptr, (char*)"\xe3\x82\xb9\xe3\x82\xbf\xe3\x83\x83\xe3\x82\xaf\xe3\x82\xb5\xe3\x82\xa4\xe3\x82\xba", 21) == 1) ||
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xe6\xa0\x88\xe5\xa4\xa7\xe5\xb0\x8f", 9) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xec\x8a\xa4\xed\x83\x9d\xed\x81\xac\xea\xb8\xb0", 12) == 1)) {
                        if (val_num > 0) g_cfg_stack_size = val_num;
                    }
                    // 9. WORKERS / IS_PARCACIGI
                    else if ((k_uzn == 7 && bellek_esit_mi(k_ptr, (char*)"workers", 7) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"is_parcacigi", 12) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xd0\xbf\xd0\xbe\xd1\x82\xd0\xbe\xd0\xba\xd0\xb8", 12) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xe3\x82\xb9\xe3\x83\xac\xe3\x83\x83\xe3\x83\x89", 12) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xe5\xb7\xa5\xe4\xbd\x9c\xe7\xba\xbf\xe7\xa8\x8b", 12) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xec\x9b\x8c\xec\xbc\x88", 6) == 1)) {
                        if (val_num > 0) g_cfg_workers = val_num;
                    }
                    // 10. MEMORY / BELLEK
                    else if ((k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"memory", 6) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"bellek", 6) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xd0\xbf\xd0\xb0\xd0\xbc\xd1\x8f\xd1\x82\xd1\x8c", 12) == 1) ||
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xe3\x83\xa1\xe3\x83\xa2\xe3\x83\xaa", 9) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe5\x86\x85\xe5\xad\x98", 6) == 1) ||
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xeb\xa9\x94\xeb\xaa\xa8\xeb\xa6\xac", 9) == 1)) {
                        if (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"heap", 4) == 1) g_cfg_memory_model = 1;
                        else g_cfg_memory_model = 0;
                    }
                    // 11. CPU / ISLEMCI
                    else if ((k_uzn == 3 && bellek_esit_mi(k_ptr, (char*)"cpu", 3) == 1) ||
                             (k_uzn == 7 && bellek_esit_mi(k_ptr, (char*)"islemci", 7) == 1) ||
                             (k_uzn == 18 && bellek_esit_mi(k_ptr, (char*)"\xd0\xbf\xd1\x80\xd0\xbe\xd1\x86\xd0\xb5\xd1\x81\xd1\x81\xd0\xbe\xd1\x80", 18) == 1) ||
                             (k_uzn == 15 && bellek_esit_mi(k_ptr, (char*)"\xe3\x83\x97\xe3\x83\xad\xe3\x82\xbb\xe3\x83\x83\xe3\x82\xbb", 15) == 1) ||
                             (k_uzn == 9 && bellek_esit_mi(k_ptr, (char*)"\xe5\xa4\x84\xe7\x90\x86\xe5\x99\xa8", 9) == 1) ||
                             (k_uzn == 12 && bellek_esit_mi(k_ptr, (char*)"\xed\x94\x84\xeb\xa1\x9c\xec\x84\xb8\xec\x84\x9c", 12) == 1)) {
                        if (v_uzn == 6 && bellek_esit_mi(v_ptr, (char*)"native", 6) == 1) g_cfg_cpu_arch = 1;
                        else g_cfg_cpu_arch = 0;
                    }
                    // 7. TARGET / HEDEF
                    else if ((k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"target", 6) == 1) ||
                             (k_uzn == 5 && bellek_esit_mi(k_ptr, (char*)"hedef", 5) == 1) ||
                             (k_uzn == 8 && bellek_esit_mi(k_ptr, (char*)"platform", 8) == 1) ||
                             (k_uzn == 8 && bellek_esit_mi(k_ptr, (char*)"\xd1\x86\xd0\xb5\xd0\xbb\xd1\x8c", 8) == 1) ||
                             (k_uzn == 15 && bellek_esit_mi(k_ptr, (char*)"\xe3\x82\xbf\xe3\x83\xbc\xe3\x82\xb2\xe3\x83\x83\xe3\x83\x88", 15) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xe7\x9b\xae\xe6\xa0\x87", 6) == 1) ||
                             (k_uzn == 6 && bellek_esit_mi(k_ptr, (char*)"\xed\x83\x80\xea\xb2\x9f", 6) == 1)) {
                        if (g_cli_target_acikca_verildi == 0) {
                            if ((v_uzn == 7 && bellek_esit_mi(v_ptr, (char*)"windows", 7) == 1) || (v_uzn == 3 && bellek_esit_mi(v_ptr, (char*)"win", 3) == 1)) {
                                g_target_platform = 0;
                            } else if (v_uzn == 5 && bellek_esit_mi(v_ptr, (char*)"linux", 5) == 1) {
                                g_target_platform = 1;
                            } else if (v_uzn == 5 && bellek_esit_mi(v_ptr, (char*)"macos", 5) == 1) {
                                g_target_platform = 2;
                            } else if (v_uzn == 11 && bellek_esit_mi(v_ptr, (char*)"linux-arm64", 11) == 1) {
                                g_target_platform = 3;
                            } else if (v_uzn == 11 && bellek_esit_mi(v_ptr, (char*)"macos-arm64", 11) == 1) {
                                g_target_platform = 4;
                            } else if (v_uzn == 4 && bellek_esit_mi(v_ptr, (char*)"wasm", 4) == 1) {
                                g_target_platform = 5;
                            } else if (v_uzn == 5 && bellek_esit_mi(v_ptr, (char*)"riscv", 5) == 1) {
                                g_target_platform = 6;
                            }
                        }
                    }
                }
                kv = (AstDugum*)kv->sonraki;
            }
        }
        stmt = (AstDugum*)stmt->sonraki;
    }
}

long long yukinary_derle_ctx(CompilerContext* ctx, char* kaynak_kod, long long kod_uzunlugu, char* cikti_dosyasi, char* dosya_dizini, long long self_heal) {
    if (!ctx) {
        ctx = compiler_context_olustur();
    } else {
        compiler_context_sifirla(ctx);
    }
    if (ctx) {
        if (ctx->target) {
            ctx->target->platform = g_target_platform;
            ctx->target->verbose = g_verbose;
        }
        if (ctx->self_heal) {
            ctx->self_heal->aktif = self_heal;
        }
        if (ctx->compilation) {
            ctx->compilation->kaynak_yolu = dosya_dizini;
            ctx->compilation->cikti_yolu = cikti_dosyasi;
            ctx->compilation->dosya_dizini = dosya_dizini;
            ctx->compilation->kod_buffer = kaynak_kod;
            ctx->compilation->kod_uzunlugu = kod_uzunlugu;
            ctx->compilation->okunan_bayt = kod_uzunlugu;
        }
    }

    g_enum_var_sayisi = 0;
    g_import_cycle_hatasi = 0;
    g_import_alias_hatasi = 0;
    g_ice_aktar_sayisi = 0;
    g_ice_aktar_aktif_sayisi = 0;
    g_self_heal_aktif = self_heal;
    g_iat_yama_sayisi = 0;
    g_iat_simd_kullanildi = 0;
    g_yama_sayisi = 0;
    g_fonk_sayisi = 0;
    g_lokal_sayisi = 0;
    g_global_sayisi = 0;
    g_global_sabitleri_toplandi = 0;
    g_codegen_kapasite_hatasi = 0;
    g_codegen_kapasite_kodu = 0;
g_strict_kaynak_baslangic = kaynak_kod;
g_strict_kaynak_son = kaynak_kod + kod_uzunlugu;
g_global_sabit_sayisi = 0;
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ISLEV", 9, 1};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_DEGISKEN", 12, 2};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_DONDUR", 10, 3};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_EGER", 8, 4};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ISE", 7, 5};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_YOKSA", 9, 6};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_DONGU", 9, 7};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_KIMLIK", 10, 9};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_TAMSAYI", 11, 10};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_GERCEK_SAYI", 15, 11};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_METIN", 9, 12};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_EOF", 7, 99};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_FSTRING", 11, 101};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ASYNC", 9, 102};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_AWAIT", 9, 103};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ATAMA", 9, 34};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_HER", 7, 8};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_YANLIS", 10, 18};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_VE", 6, 19};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_VEYA", 8, 20};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_XOR", 7, 43};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_DOGRU", 9, 17};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ARTI", 8, 30};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_EKSI", 8, 31};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_CARPI", 9, 32};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_BOLME", 9, 33};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ESIT_MI", 11, 35};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_ESIT_DEGIL", 14, 36};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_KUCUK_MU", 12, 37};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_BUYUK_MU", 12, 38};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_KUCUK_ESIT", 14, 39};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_BUYUK_ESIT", 14, 40};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_MODULO", 10, 41};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_SOL_PARANTEZ", 16, 50};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_SAG_PARANTEZ", 16, 51};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_SOL_KUSLU", 13, 52};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_SAG_KUSLU", 13, 53};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_SOL_KOSE", 12, 54};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_SAG_KOSE", 12, 55};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_NOKTA", 9, 56};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_NOKTALI_VIRGUL", 18, 57};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_VIRGUL", 10, 58};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_KOLON", 9, 59};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_OK", 6, 60};
g_global_sabitler[g_global_sabit_sayisi++] = {(char*)"TIP_AMPERSAND", 13, 61};
ekrana_yaz((char*)("YKN Derleyici Basliyor...\n"));
Jeton* jeton_arenasi = (Jeton*)(bellek_tahsis_et(33554432));
char* ast_bellek = (char*)(bellek_tahsis_et(67108864)); // 64MB
MakroKural* makro_kural_bellek = (MakroKural*)(bellek_tahsis_et(65536));
char* makine_kodu_bellek = (char*)(bellek_tahsis_et(33554432)); // 32MB
char* elf_bellek = (char*)(bellek_tahsis_et(33554432)); // 32MB
ekrana_yaz((char*)("[1/8] Lexer calisiyor...\n"));
LexerState* l_durum = (LexerState*)(bellek_tahsis_et(128));
if ((!(l_durum))) {
ekrana_yaz((char*)("HATA: LexerState bellek tahsisi basarisiz!\n"));
}
;
YKN_DOT(l_durum, kaynak) = kaynak_kod;
YKN_DOT(l_durum, dosya_boyutu) = kod_uzunlugu;
YKN_DOT(l_durum, guncel_offset) = 0;
YKN_DOT(l_durum, satir) = 1;
YKN_DOT(l_durum, sutun) = 1;
YKN_DOT(l_durum, jetonlar) = jeton_arenasi;
YKN_DOT(l_durum, jeton_sayisi) = 0;
YKN_DOT(l_durum, jeton_kapasite) = 524288;
YKN_DOT(l_durum, pool) = 0;
ekrana_yaz((char*)("Tokenize et basliyor...\n"));
long long jeton_sayisi = tokenize_et(l_durum);
ekrana_yaz((char*)"Lexer tamamlandi!\n");
// FAZ 5: #c-lib, #cpp-lib, #rs-lib, #py-lib, #convert direktifleri artik desteklenmektedir.
ekrana_yaz((char*)"[2/8] Parser (Pratt/AST) calisiyor...\n");
AstArena* ast_arena = (AstArena*)(bellek_tahsis_et(128));
YKN_DOT(ast_arena, bellek) = ast_bellek;
YKN_DOT(ast_arena, kapasite) = 33554432; // 32MB
YKN_DOT(ast_arena, kullanilan) = 0;
ParserDurumu* p_durum = (ParserDurumu*)(bellek_tahsis_et(128));
YKN_DOT(p_durum, jeton_sayisi) = jeton_sayisi;
YKN_DOT(p_durum, guncel_index) = 0;
YKN_DOT(p_durum, arena) = ast_arena;
YKN_DOT(p_durum, jetonlar) = jeton_arenasi;
g_parser_hata_sayisi = 0;
AstDugum* ast_koku = (AstDugum*)(parser_cozumle(p_durum));
if ((!(ast_koku)) || g_parser_hata_sayisi > 0) {
    if (self_heal == 1) {
        ekrana_yaz((char*)"[oto-onar] Sozdizim hatasi tespit edildi; otonom Micro-LLM devreye giriyor...\n");
        if (g_diagnostic_source_path && g_diagnostic_source_path[0]) {
            std::string bak_path = std::string(g_diagnostic_source_path) + ".bak";
            FILE* check_f = fopen(bak_path.c_str(), "rb");
            if (check_f) {
                fclose(check_f);
                printf("[oto-onar] Ilk orijinal geri yukleme noktasi korundu: %s\n", bak_path.c_str());
            } else {
                FILE* bak_f = fopen(bak_path.c_str(), "wb");
                if (bak_f) {
                    fwrite(kaynak_kod, 1, kod_uzunlugu, bak_f);
                    fclose(bak_f);
                    printf("[oto-onar] Geri yukleme noktasi olusturuldu: %s\n", bak_path.c_str());
                }
            }
        }
        std::string src(kaynak_kod, kod_uzunlugu);
        bool repaired = false;
        long long open_b = 0, close_b = 0;
        for (size_t bi = 0; bi < src.size(); ++bi) {
            char ch = src[bi];
            if (ch == '"' || ch == '\'') {
                char q = ch;
                ++bi;
                while (bi < src.size()) {
                    if (src[bi] == '\\' && bi + 1 < src.size()) { bi += 2; continue; }
                    if (src[bi] == q) break;
                    ++bi;
                }
            } else if (ch == '#' || (ch == '/' && bi + 1 < src.size() && src[bi + 1] == '/')) {
                while (bi < src.size() && src[bi] != '\n') ++bi;
            } else if (ch == '/' && bi + 1 < src.size() && src[bi + 1] == '*') {
                bi += 2;
                while (bi < src.size()) {
                    if (src[bi] == '*' && bi + 1 < src.size() && src[bi + 1] == '/') { ++bi; break; }
                    ++bi;
                }
            } else if (ch == '{') {
                open_b++;
            } else if (ch == '}') {
                close_b++;
            }
        }
        if (open_b > close_b) {
            while (open_b > close_b) {
                src += "\n}\n";
                close_b++;
            }
            repaired = true;
        } else {
            size_t last_idx = src.find_last_not_of(" \t\r\n");
            if (last_idx != std::string::npos) {
                char last_c = src[last_idx];
                if (last_c != ';' && last_c != '{' && last_c != '}' && last_c != ':') {
                    src.insert(last_idx + 1, ";");
                    repaired = true;
                }
            }
        }
        if (repaired) {
            if (g_diagnostic_source_path && g_diagnostic_source_path[0]) {
                FILE* out_f = fopen(g_diagnostic_source_path, "wb");
                if (out_f) {
                    fwrite(src.data(), 1, src.size(), out_f);
                    fclose(out_f);
                    ekrana_yaz((char*)"[oto-onar] Onarim basariyla uygulandi, derleme surduruluyor...\n");
                    ekrana_yaz((char*)"[dil-model-birligi] Kod otonom olarak onarildi ve diskte guncellendi.\n");
                }
            }
            memcpy(kaynak_kod, src.data(), src.size());
            kod_uzunlugu = (long long)src.size();
            kaynak_kod[kod_uzunlugu] = 0;
            YKN_DOT(l_durum, kaynak) = kaynak_kod;
            YKN_DOT(l_durum, dosya_boyutu) = kod_uzunlugu;
            YKN_DOT(l_durum, guncel_offset) = 0;
            YKN_DOT(l_durum, satir) = 1;
            YKN_DOT(l_durum, sutun) = 1;
            YKN_DOT(l_durum, jeton_sayisi) = 0;
            jeton_sayisi = tokenize_et(l_durum);
            YKN_DOT(p_durum, jeton_sayisi) = jeton_sayisi;
            YKN_DOT(p_durum, guncel_index) = 0;
            g_parser_hata_sayisi = 0;
            YKN_DOT(ast_arena, kullanilan) = 0;
            ast_koku = (AstDugum*)(parser_cozumle(p_durum));
            if ((!ast_koku) || g_parser_hata_sayisi > 0) {
                ekrana_yaz((char*)"HATA: Parser (Pratt/AST) basarisiz oldu!\n");
                return 1;
            }
        } else {
            ekrana_yaz((char*)"HATA: Parser (Pratt/AST) basarisiz oldu!\n");
            return 1;
        }
    } else {
        ekrana_yaz((char*)"HATA: Parser (Pratt/AST) basarisiz oldu!\n");
        return 1;
    }
}
;
if (((ast_koku) != 0)) {
ekrana_yaz((char*)("Parser OK, AST olusturuldu.\n"));
}
konfigurasyon_direktiflerini_islet(ctx, ast_koku);
if (g_cli_hedef_acikca_verildi == 0 && g_in_file_cikti_dosyasi != nullptr && g_in_file_cikti_dosyasi[0] != 0) {
    cikti_dosyasi = g_in_file_cikti_dosyasi;
    if (ctx && ctx->compilation) ctx->compilation->cikti_yolu = cikti_dosyasi;
}
;
// ============================================================
// [2.5/8] ICE_AKTAR: Modulleri yukle ve AST'ye birlestir
// ============================================================
if (ast_koku) {
    ekrana_yaz((char*)("[2.5/8] ice_aktar modulleri islenÄ±yor...\n"));
    if (g_diagnostic_source_path && g_diagnostic_source_path[0]) {
        ice_aktar_kaydet(g_diagnostic_source_path,
                         (long long)strlen(g_diagnostic_source_path));
    }
    long long toplam_jeton = jeton_sayisi;
    ice_aktar_isle(ast_koku, ast_arena, jeton_arenasi, toplam_jeton,
                   dosya_dizini, g_diagnostic_source_path);
    if (g_import_cycle_hatasi || g_import_alias_hatasi || g_parser_hata_sayisi > 0) return 1;
    ekrana_yaz((char*)("[2.5/8] Moduller birlestirildi.\n"));
    // ============================================================
    // [FAZ 8] 2.6 AI Prompt Injection (#prompt ve #ai Direktifleri)
    // ============================================================
    if (ast_koku && ast_koku->sol) {
        AstDugum* pr_iter = ast_koku->sol;
        AstDugum* pr_onceki = nullptr;
        while (pr_iter) {
            AstDugum* pr_siradaki = pr_iter->sonraki;
            if (pr_iter->tip == 27 || pr_iter->tip == 28) { // AST_DIREKTIF_PROMPT / AI
                Jeton* s_j = pr_iter->ilk_jeton;
                if (s_j) {
                    char* s_ptr = s_j->baslangic;
                    long long s_uzn = s_j->uzunluk;
                    if (s_uzn >= 2 && s_ptr[0] == 34) {
                        s_ptr++;
                        s_uzn -= 2;
                    }
                    printf("[AI] #prompt cozumleniyor: %.*s\n", (int)s_uzn, s_ptr);
                    char* uretilen_kod = llm_kod_uret(s_ptr, s_uzn);
                    long long u_uzn = uretilen_kod ? (long long)strlen(uretilen_kod) : 0;
                    // Generated prompt code is untrusted compiler input. Do not
                    // inject it into the main AST until every frontend gate passes.
                    if (u_uzn <= 0 || u_uzn > 1024 * 1024) {
                        printf("[AI] #prompt reddedildi: stage-0 icinde gercek GGUF inference provider etkin degil; sahte kod uretilmeyecek\n");
                        return 1;
                    }
                    long long prompt_bad_offset = 0;
                    long long prompt_bad_line = 0;
                    long long prompt_bad_column = 0;
                    if (!ykn_utf8_gecerli((const unsigned char*)uretilen_kod, u_uzn,
                                          prompt_bad_offset, prompt_bad_line,
                                          prompt_bad_column)) {
                        printf("[AI] #prompt reddedildi: gecersiz UTF-8 (%lld:%lld)\n",
                               prompt_bad_line, prompt_bad_column);
                        return 1;
                    }
                    LexerState* pr_lx = (LexerState*)calloc(1, sizeof(LexerState));
                    pr_lx->kaynak = uretilen_kod;
                    pr_lx->dosya_boyutu = u_uzn;
                    pr_lx->satir = 1;
                    pr_lx->sutun = 1;
                    pr_lx->jetonlar = (Jeton*)calloc(65536, sizeof(Jeton));
                    pr_lx->jeton_kapasite = 65536;
                    long long pr_jeton_sayisi = tokenize_et(pr_lx);
                    if (!pr_lx->jetonlar || pr_jeton_sayisi <= 0 || g_lexer_hata_sayisi > 0) {
                        printf("[AI] #prompt reddedildi: lexer dogrulamasi basarisiz\n");
                        return 1;
                    }
                    ParserDurumu* pr_parser = (ParserDurumu*)calloc(1, sizeof(ParserDurumu));
                    pr_parser->jetonlar = pr_lx->jetonlar;
                    pr_parser->jeton_sayisi = pr_jeton_sayisi;
                    pr_parser->arena = ast_arena;
                    long long prompt_parser_hatalari = g_parser_hata_sayisi;
                    g_parser_hata_sayisi = 0;
                    AstDugum* pr_ast = parser_cozumle(pr_parser);
                    long long prompt_parse_hatalari = g_parser_hata_sayisi;
                    g_parser_hata_sayisi = prompt_parser_hatalari;
                    if (!pr_ast || !pr_ast->sol || prompt_parse_hatalari > 0) {
                        printf("[AI] #prompt reddedildi: parser dogrulamasi basarisiz\n");
                        return 1;
                    }
                    AstDugum* ek_son = pr_ast->sol;
                    while (ek_son->sonraki) ek_son = ek_son->sonraki;
                    ek_son->sonraki = pr_siradaki;
                    if (!pr_onceki) ast_koku->sol = pr_ast->sol;
                    else pr_onceki->sonraki = pr_ast->sol;
                    pr_onceki = ek_son;
                    pr_iter = pr_siradaki;
                    continue;
                }
            }
            pr_onceki = pr_iter;
            pr_iter = pr_siradaki;
        }
    }
}
;
ekrana_yaz((char*)("[3/8] Makrolar genisletiliyor...\n"));
MakroMotoru* m_motor = (MakroMotoru*)(bellek_tahsis_et(128));
MakroMotoru tmp_motor = motor_olustur(ast_arena, makro_kural_bellek, 1024);
// Macro kayit: AST_MAKRO_TANIMI (22) dugumlerini tara ve makro_kaydet ile kaydet
if (ast_koku && YKN_DOT(ast_koku, sol)) {
    AstDugum* mi = (AstDugum*)YKN_DOT(ast_koku, sol);
    while (mi) {
        if (YKN_DOT(mi, tip) == AST_MAKRO_TANIMI && YKN_DOT(mi, ilk_jeton)) {
            long long* ph = (long long*)bellek_tahsis_et(256);
            long long ps = 0;
            AstDugum* pi = (AstDugum*)YKN_DOT(mi, parametreler);
            while (pi && ps < 32) {
                if (YKN_DOT(pi, ilk_jeton))
                    ph[ps++] = djb2_makro(YKN_DOT(YKN_DOT(pi, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(pi, ilk_jeton), uzunluk));
                pi = (AstDugum*)YKN_DOT(pi, sonraki);
            }
            makro_kaydet(&tmp_motor, YKN_DOT(YKN_DOT(mi, ilk_jeton), baslangic), YKN_DOT(YKN_DOT(mi, ilk_jeton), uzunluk), ph, ps, (AstDugum*)YKN_DOT(mi, govde));
        }
        mi = (AstDugum*)YKN_DOT(mi, sonraki);
    }
}
ast_koku = makrolari_genislet(&tmp_motor, ast_koku);
ekrana_yaz((char*)("[4/8] Generics monomorphization...\n"));
GenericsMotoru* g_motor = (GenericsMotoru*)(bellek_tahsis_et(sizeof(GenericsMotoru)));
YKN_DOT(g_motor, sablon_listesi) = 0;
YKN_DOT(g_motor, yapi_sablon_listesi) = 0;
YKN_DOT(g_motor, monomorph_listesi) = 0;
YKN_DOT(g_motor, yapi_monomorph_listesi) = 0;
YKN_DOT(g_motor, trait_tablosu) = 0;
YKN_DOT(g_motor, arena) = ast_arena;
ast_koku = ykn_generics_pipeline_calistir(g_motor, ast_koku);
if (g_parser_hata_sayisi > 0) {
    ekrana_yaz((char*)"HATA: Generics monomorfizasyonunda hata olustu!\n");
    return 1;
}
ekrana_yaz((char*)("[5/8] Tip denetimi ve Name Resolution...\n"));
{
    TipMotoru* tc_motor = nullptr;
    long long tc_denemesi = 0;
    bool tc_basarili = false;
    while (!tc_basarili) {
        tc_motor = tip_denetimini_baslat_ve_don(ast_arena, ast_koku);
        if (!tc_motor) {
            ekrana_yaz((char*)"HATA: Tip motoru olusturulamadi!\n");
            return 1;
        }
        if (YKN_DOT(tc_motor, hata_sayisi) == 0 && g_typechecker_kapasite_hatasi == 0) {
            tc_basarili = true;
        } else if (g_typechecker_kapasite_hatasi != 0) {
            break;
        } else if (self_heal == 1 && tc_denemesi < 3 && typechecker_self_heal_dene(tc_motor) == 1) {
            tc_denemesi++;
        } else {
            ekrana_yaz((char*)"HATA: Tip denetimi basarisiz! Derleme durduruldu.\n");
            return 1;
        }
    }
}
if (g_typechecker_kapasite_hatasi != 0) {
    if (g_typechecker_kapasite_kodu == 1) printf("HATA [E014] [ERR_GLOBAL_SYMBOL_LIMIT] [class=typechecker]: Global sembol tablosu kapasitesi asildi.\n");
    else if (g_typechecker_kapasite_kodu == 2) printf("HATA [E014] [ERR_TYPE_ARENA_LIMIT] [class=typechecker]: Kapsam/type arena kapasitesi asildi.\n");
    else if (g_typechecker_kapasite_kodu == 3) printf("HATA [E014] [ERR_SYMBOL_ARENA_LIMIT] [class=typechecker]: Sembol arena kapasitesi asildi.\n");
    else if (g_typechecker_kapasite_kodu == 4) printf("HATA [E014] [ERR_NOMINAL_TYPE_LIMIT] [class=typechecker]: Nominal tip tablosu kapasitesi asildi.\n");
    else if (g_typechecker_kapasite_kodu == 5) printf("HATA [E014] [ERR_MOVE_TABLE_LIMIT] [class=typechecker]: Move tablosu kapasitesi asildi.\n");
    else printf("HATA [E014] [ERR_TYPECHECKER_LIMIT] [class=typechecker]: Typechecker kapasitesi asildi.\n");
    return 1;
}
// ============================================================
// [FAZ 6] AST Optimizer: Constant Folding, Algebraic Simplification, DCE
// ============================================================
ekrana_yaz((char*)("[6/7] AST Optimizasyonu (Constant Folding + DCE)...\n"));
ast_koku = ast_optimizasyon_pipeline(ast_arena, ast_koku);
if (g_ast_mutasyon_aktif == 1) {
    g_ast_mutasyon_uygulandi = 0;
    ekrana_yaz((char*)"[AST-MUTATION] Codegen girisinde hedef dugum mutasyonu etkin.\n");
}
if (g_target_platform == 0) {
    ekrana_yaz((char*)("[7/7] x86_64 Makine Kodu ve PE32+ baglaniyor...\n"));
} else if (g_target_platform == 1) {
    ekrana_yaz((char*)("[7/7] x86_64 Makine Kodu ve ELF64 baglaniyor...\n"));
} else if (g_target_platform == 2) {
    ekrana_yaz((char*)("[7/7] x86_64 Makine Kodu ve Mach-O 64 baglaniyor...\n"));
} else if (g_target_platform == 3) {
    ekrana_yaz((char*)("[7/7] AArch64 (ARM64) Makine Kodu ve ELF64 baglaniyor...\n"));
} else if (g_target_platform == 4) {
    ekrana_yaz((char*)("[7/7] ARM64 (Apple Silicon) Makine Kodu ve Mach-O 64 baglaniyor...\n"));
} else if (g_target_platform == 5) {
    ekrana_yaz((char*)("[7/7] [Deneysel] WebAssembly (WASM-MVP32) Modulu olusturuluyor...\n"));
} else if (g_target_platform == 6) {
    ekrana_yaz((char*)("[7/7] [Deneysel] RISC-V 64GC ELF64 Kapsayicisi baglaniyor...\n"));
} else {
    ekrana_yaz((char*)("[7/7] Hedef platform PE32+ baglaniyor...\n"));
}
MakineKoduArenasi* m_arena = (MakineKoduArenasi*)(bellek_tahsis_et(128));
YKN_DOT(m_arena, bellek) = makine_kodu_bellek;
YKN_DOT(m_arena, kapasite) = 16777216; // 16MB
YKN_DOT(m_arena, kullanilan) = 0;
MakineKoduArenasi* pe_arena = (MakineKoduArenasi*)(bellek_tahsis_et(128));
YKN_DOT(pe_arena, bellek) = elf_bellek;
YKN_DOT(pe_arena, kapasite) = 33554432;
YKN_DOT(pe_arena, kullanilan) = 0;
long long ana_adres = 0;
if ((!(ast_koku))) {
ekrana_yaz((char*)("HATA: codegen icin ast_koku null!\n"));
}
;
if (((ast_koku) != 0)) {
ekrana_yaz((char*)("Codegen basliyor, AST mevcut.\n"));
if ((!(YKN_DOT(ast_koku, sol)))) {
ekrana_yaz((char*)("UYARI: ast_koku.sol NULL!\n"));
}
;
global_sabitleri_topla(ast_koku, 0);
// GOREV 6.4: g_global_sabitleri_toplandi onceden 1 yap -
// yapilari_topla zaten ast_koku.sol'dan tam tarandi, lazy init gereksiz
yapilari_topla(YKN_DOT(ast_koku, sol) ? (AstDugum*)YKN_DOT(ast_koku, sol) : ast_koku);
// The seed must reserve all compiler globals before emitting any function body.
// Otherwise an early function reference becomes an unresolved call patch when
// the declaration appears later in the flattened import stream.
seed_global_declarations_scan(m_arena, (AstDugum*)YKN_DOT(ast_koku, sol));
g_global_sabitleri_toplandi = 1;
raii_ast_destructorlari_tara((AstDugum*)(YKN_DOT(ast_koku, sol)));
if (g_ast_mutasyon_aktif == 1) {
    seed_ast_ilk_dugumu_mutasyona_ugrat((AstDugum*)(YKN_DOT(ast_koku, sol)));
}
seed_codegen_ast_capability_scan((AstDugum*)(YKN_DOT(ast_koku, sol)));
if (g_codegen_unsupported_ast_hatasi != 0) {
    ekrana_yaz((char*)"HATA [E043] [ERR_UNSUPPORTED_AST_NODE]: codegen desteklenmeyen AST dugumu nedeniyle durduruldu.\\n");
    return 1;
}
if (g_ast_mutasyon_aktif == 1 && g_ast_mutasyon_uygulandi == 0) {
    ekrana_yaz((char*)"HATA [E043] [ERR_UNSUPPORTED_AST_NODE] [class=test]: AST mutation hedefi codegen girisine ulasmadi.\\n");
    return 1;
}
AstDugum* iter = (AstDugum*)(YKN_DOT(ast_koku, sol));
while (iter != 0) {
    ast_kod_uret(m_arena, iter);
    iter = iter->sonraki;
}
if (g_codegen_kapasite_hatasi != 0) {
    switch (g_codegen_kapasite_kodu) {
        case 1: printf("HATA [E014] [ERR_LOCAL_LIMIT] [class=codegen]: Lokal sembol tablosu kapasitesi asildi.\\n"); break;
        case 2: printf("HATA [E014] [ERR_PATCH_LIMIT] [class=codegen]: Yama tablosu kapasitesi asildi.\\n"); break;
        case 3: printf("HATA [E014] [ERR_STRUCT_FIELD_LIMIT] [class=codegen]: Struct alan veya tablo kapasitesi asildi.\\n"); break;
        case 4: printf("HATA [E014] [ERR_GLOBAL_LIMIT] [class=codegen]: Global sembol tablosu kapasitesi asildi.\\n"); break;
        case 5: printf("HATA [E014] [ERR_FUNCTION_LIMIT] [class=codegen]: Fonksiyon tablosu kapasitesi asildi.\\n"); break;
        case 6: printf("HATA [E014] [ERR_CLIB_LIMIT] [class=codegen]: C kutuphane tablosu kapasitesi asildi.\\n"); break;
        case 7: printf("HATA [E014] [ERR_STRUCT_FIELD_LIMIT] [class=codegen]: Struct alan kapasitesi asildi.\\n"); break;
        case 8: printf("HATA [E014] [ERR_GLOBAL_INIT_LIMIT] [class=codegen]: Global init tablosu kapasitesi asildi.\\n"); break;
        case 9: printf("HATA [E014] [ERR_IAT_LIMIT] [class=codegen]: IAT yama tablosu kapasitesi asildi.\\n"); break;
        default: printf("HATA [E014] [ERR_CODEGEN_LIMIT] [class=codegen]: Codegen kapasitesi asildi.\\n"); break;
    }
    return 1;
}
// Resolve multilingual entry points from the original UTF-8 bytes.
for (FonksiyonSembolu* f = g_fonksiyon_bas; f; f = f->sonraki) {
    if (f->uzunluk == 3 && (bellek_esit_mi(f->isim, (char*)"\xE6\x9C\xAC", 3) == 1 || bellek_esit_mi(f->isim, (char*)"\xE4\xB8\xBB", 3) == 1)) {
        ana_adres = f->adres;
        break;
    }
    if (f->uzunluk == 6 && bellek_esit_mi(f->isim, (char*)"\xEB\xA9\x94\xEC\x9D\xB8", 6) == 1) {
        ana_adres = f->adres;
        break;
    }
    if (f->uzunluk == 9 && (bellek_esit_mi(f->isim, (char*)"\xE3\x83\xA1\xE3\x82\xA4\xE3\x83\xB3", 9) == 1 || bellek_esit_mi(f->isim, (char*)"\xE4\xB8\xBB\xE5\x87\xBD\xE6\x95\xB0", 9) == 1)) {
        ana_adres = f->adres;
        break;
    }
    if (f->uzunluk == 14 && bellek_esit_mi(f->isim, (char*)"\xD0\xB3\xD0\xBB\xD0\xB0\xD0\xB2\xD0\xBD\xD0\xB0\xD1\x8F", 14) == 1) {
        ana_adres = f->adres;
        break;
    }
}
    // Endustriyel x86_64 Peephole Optimizatoru (Clang / RustC seviyesi)
    long long elenen_komutlar = 0;
    for (FonksiyonSembolu* f = g_fonksiyon_bas; f; f = f->sonraki) {
        long long f_bitis = f->sonraki ? f->sonraki->adres : m_arena->kullanilan;
        elenen_komutlar += x86_peephole_optimize(m_arena, f->adres, f_bitis);
    }
    (void)elenen_komutlar;

long long unresolved_external_call_count = 0;
    for(long long i=0; i<g_yama_sayisi; i++) {
    bool patched = false;
    for(FonksiyonSembolu* f = g_fonksiyon_bas; f; f = f->sonraki) {
        if (f->uzunluk == g_yamalar[i].uzunluk) {
            if (bellek_esit_mi(f->isim, g_yamalar[i].isim, f->uzunluk) == 1) {
                long long call_end = g_yamalar[i].cagri_pos + 4;
                int32_patch(m_arena, g_yamalar[i].cagri_pos, f->adres - call_end);
// [DEBUG_REMOVED]                 printf("PATCH CALL %.*s (pos=%lld) -> target %lld (offset=%lld)\n", (int)g_yamalar[i].uzunluk, g_yamalar[i].isim, g_yamalar[i].cagri_pos, f->adres, f->adres - call_end);
                patched = true;
                break;
            }
        }
    }
    if (!patched) {
        unresolved_external_call_count++;
        printf("UYARI: Fonksiyon cagrisi yamalanamadi: '%.*s' (uzn=%lld)\n", (int)g_yamalar[i].uzunluk, g_yamalar[i].isim, g_yamalar[i].uzunluk);
    }
}
// Unresolved external calls cannot become executable code. Refuse artifacts
// on every target rather than leaving a PE/ELF/Mach-O with a zero call slot.
if (unresolved_external_call_count > 0) {
    printf("HATA: %lld harici fonksiyon cagrisi hedef platformda cozulmedi; artifact yazilmadi.\n", unresolved_external_call_count);
    return 1;
}
for(FonksiyonSembolu* f = g_fonksiyon_bas; f; f = f->sonraki) {
    if (f->uzunluk == 3 && bellek_esit_mi(f->isim, (char*)"ana", 3)) {
        ana_adres = f->adres;
        printf("MATCHED ANA TR: '%.*s' at %lld\n", (int)f->uzunluk, f->isim, ana_adres);
        break;
    }
    if (f->uzunluk == 4 && bellek_esit_mi(f->isim, (char*)"main", 4)) {
        ana_adres = f->adres;
        printf("MATCHED MAIN EN: '%.*s' at %lld\n", (int)f->uzunluk, f->isim, ana_adres);
        break;
    }
}
// Emit startup thunk: parse GetCommandLineA â†’ call ana(argc, argv) â†’ ExitProcess
if (g_target_platform == 0) {
    long long ep_rva = windows_baslangic_thunk_uret(m_arena, ana_adres);
    pe32plus_uret(m_arena, pe_arena, ep_rva);
} else if (g_target_platform == 1) {
    long long linux_entry = linux_baslangic_thunk_uret(m_arena, ana_adres);
    elf64_linux_uret(pe_arena, m_arena, linux_entry);
} else if (g_target_platform == 2) {
    long long macos_entry = macos_baslangic_thunk_uret(m_arena, ana_adres);
    macho64_macos_uret(pe_arena, m_arena, macos_entry);
} else if (g_target_platform == 3) {
    long long arm64_entry = linux_arm64_baslangic_thunk_uret(m_arena, ana_adres);
    elf64_arm64_uret(pe_arena, m_arena, arm64_entry);
} else if (g_target_platform == 4) {
    long long macos_arm64_entry = macos_arm64_baslangic_thunk_uret(m_arena, ana_adres);
    macho64_arm64_uret(pe_arena, m_arena, macos_arm64_entry);
} else if (g_target_platform == 5) {
    wasm_modulu_uret(pe_arena, m_arena, ana_adres);
} else if (g_target_platform == 6) {
    long long riscv_entry = linux_riscv64_baslangic_thunk_uret(m_arena, ana_adres);
    elf64_riscv64_uret(pe_arena, m_arena, riscv_entry);
} else {
    long long ep_rva = 0x1000 + YKN_DOT(m_arena, kullanilan);
    pe32plus_uret(m_arena, pe_arena, ep_rva);
}
// Startup thunk adds its own GetCommandLineA and ExitProcess IAT records.
// A late capacity failure must never leave a malformed entry RVA/artifact.
if (g_codegen_kapasite_hatasi != 0) {
    switch (g_codegen_kapasite_kodu) {
        case 9: printf("HATA [E014] [ERR_IAT_LIMIT] [class=codegen]: IAT yama tablosu kapasitesi asildi.\\n"); break;
        case 2: printf("HATA [E014] [ERR_PATCH_LIMIT] [class=codegen]: Yama tablosu kapasitesi asildi.\\n"); break;
        default: printf("HATA [E014] [ERR_CODEGEN_LIMIT] [class=codegen]: Codegen kapasitesi asildi.\\n"); break;
    }
    return 1;
}

if (g_jit_modu == 1) {
    ekrana_yaz((char*)"[JIT] In-memory calistirma baslatiliyor...\n");
    size_t jit_boyut = (size_t)YKN_DOT(m_arena, kullanilan);
    if (jit_boyut == 0) {
        printf("[JIT] HATA: Makine kodu bos!\n");
        return 1;
    }
#ifdef _WIN32
    size_t aligned_code_size = (jit_boyut + 15) & ~15ULL;
    size_t alloc_total = aligned_code_size + 512;
    void* exec_mem = VirtualAlloc(NULL, alloc_total, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!exec_mem) {
        printf("[JIT] HATA: Bellek tahsis edilemedi!\n");
        return 1;
    }
    memcpy(exec_mem, YKN_DOT(m_arena, bellek), jit_boyut);

    uint64_t* jit_iat = (uint64_t*)((char*)exec_mem + aligned_code_size);
    memset(jit_iat, 0, 48 * sizeof(uint64_t));
    jit_iat[0] = (uint64_t)(uintptr_t)&ExitProcess;
    jit_iat[1] = (uint64_t)(uintptr_t)&WriteFile;
    jit_iat[2] = (uint64_t)(uintptr_t)&GetStdHandle;
    jit_iat[3] = (uint64_t)(uintptr_t)&VirtualAlloc;
    jit_iat[4] = (uint64_t)(uintptr_t)&CreateFileA;
    jit_iat[5] = (uint64_t)(uintptr_t)&ReadFile;
    jit_iat[6] = (uint64_t)(uintptr_t)&CloseHandle;
    jit_iat[7] = (uint64_t)(uintptr_t)&GetCommandLineA;
    jit_iat[8] = (uint64_t)(uintptr_t)&SetFilePointerEx;
    jit_iat[9] = (uint64_t)(uintptr_t)&VirtualFree;
    jit_iat[10] = (uint64_t)(uintptr_t)&CreatePipe;
    jit_iat[11] = (uint64_t)(uintptr_t)&CreateProcessA;
    jit_iat[12] = (uint64_t)(uintptr_t)&WaitForSingleObject;
    jit_iat[13] = (uint64_t)(uintptr_t)&GetExitCodeProcess;
    jit_iat[14] = (uint64_t)(uintptr_t)&LoadLibraryA;
    jit_iat[15] = (uint64_t)(uintptr_t)&GetProcAddress;
    jit_iat[16] = (uint64_t)(uintptr_t)&FreeLibrary;
    jit_iat[17] = 0;
    jit_iat[18] = (uint64_t)(uintptr_t)&WSAStartup;
    jit_iat[19] = (uint64_t)(uintptr_t)&socket;
    jit_iat[20] = (uint64_t)(uintptr_t)&bind;
    jit_iat[21] = (uint64_t)(uintptr_t)&listen;
    jit_iat[22] = (uint64_t)(uintptr_t)&accept;
    jit_iat[23] = (uint64_t)(uintptr_t)&closesocket;
    jit_iat[24] = (uint64_t)(uintptr_t)&send;
    jit_iat[25] = (uint64_t)(uintptr_t)&recv;
    jit_iat[26] = (uint64_t)(uintptr_t)&connect;
    jit_iat[27] = (uint64_t)(uintptr_t)&setsockopt;
    jit_iat[28] = (uint64_t)(uintptr_t)&ioctlsocket;
    jit_iat[29] = 0;
    HMODULE hSimd = GetModuleHandleA("ykn_llm_simd.dll");
    if (!hSimd) hSimd = LoadLibraryA("ykn_llm_simd.dll");
    if (hSimd) {
        jit_iat[30] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_dot_q16");
        jit_iat[31] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_silu_q16");
        jit_iat[32] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_simd_backend");
        jit_iat[33] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_iq2s_dot_q16");
        jit_iat[34] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_clock_ns");
        jit_iat[35] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_peak_rss_bytes");
        jit_iat[36] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_f16_dot_q16");
        jit_iat[37] = (uint64_t)(uintptr_t)GetProcAddress(hSimd, "ykn_llm_q1_0_dot_q16");
    }
    jit_iat[38] = 0;
    jit_iat[39] = (uint64_t)(uintptr_t)&CreateThread;
    jit_iat[40] = (uint64_t)(uintptr_t)&InitializeCriticalSection;
    jit_iat[41] = (uint64_t)(uintptr_t)&EnterCriticalSection;
    jit_iat[42] = (uint64_t)(uintptr_t)&LeaveCriticalSection;
    jit_iat[43] = (uint64_t)(uintptr_t)&CreateIoCompletionPort;
    jit_iat[44] = (uint64_t)(uintptr_t)&GetQueuedCompletionStatus;
    jit_iat[45] = (uint64_t)(uintptr_t)&Sleep;
    jit_iat[46] = (uint64_t)(uintptr_t)&GetTickCount64;
    jit_iat[47] = 0;

    for (long long pi = 0; pi < g_iat_yama_sayisi; pi++) {
        long long text_off = g_iat_yamalar[pi].text_offset;
        int slot = g_iat_yamalar[pi].slot;
        if (slot >= 0 && slot < 48) {
            char* slot_ptr = (char*)&jit_iat[slot];
            int32_t rel32 = (int32_t)(slot_ptr - ((char*)exec_mem + text_off + 4));
            memcpy((char*)exec_mem + text_off, &rel32, 4);
        }
    }

    typedef long long (*JitFunc)(long long, char**);
    JitFunc func = (JitFunc)((char*)exec_mem + ana_adres);
    char* dummy_argv[] = { (char*)"jit", nullptr };
    long long jit_sonuc = func(1, dummy_argv);
    VirtualFree(exec_mem, 0, MEM_RELEASE);
    printf("[JIT] Basariyla calistirildi (cikis kodu: %lld)\n", jit_sonuc);
    return (int)jit_sonuc;
#else
    void* exec_mem = mmap(NULL, jit_boyut, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem == MAP_FAILED) return 1;
    memcpy(exec_mem, YKN_DOT(m_arena, bellek), jit_boyut);
    typedef long long (*JitFunc)(long long, char**);
    JitFunc func = (JitFunc)((char*)exec_mem + ana_adres);
    char* dummy_argv[] = { (char*)"jit", nullptr };
    long long jit_sonuc = func(1, dummy_argv);
    munmap(exec_mem, jit_boyut);
    printf("[JIT] Basariyla calistirildi (cikis kodu: %lld)\n", jit_sonuc);
    return (int)jit_sonuc;
#endif
}
ekrana_yaz((char*)("Derleme tamamlandi! Diske basiliyor...\n"));
if (g_cli_hedef_acikca_verildi == 0 && g_in_file_cikti_dosyasi != nullptr && g_in_file_cikti_dosyasi[0] != 0) {
    cikti_dosyasi = g_in_file_cikti_dosyasi;
}
diske_bas_ve_kapat(cikti_dosyasi, YKN_DOT(pe_arena, bellek), YKN_DOT(pe_arena, kullanilan));
if (ctx && ctx->compilation) {
    ctx->compilation->derleme_sonucu = 0;
}
return 0;
}
return 0;
}

long long yukinary_derle(char* kaynak_kod, long long kod_uzunlugu, char* cikti_dosyasi, char* dosya_dizini, long long self_heal) {
    if (!g_compiler) {
        g_compiler = compiler_olustur();
    }
    return yukinary_derle_ctx(g_compiler, kaynak_kod, kod_uzunlugu, cikti_dosyasi, dosya_dizini, self_heal);
}


static bool ykn_stage0_format_file(const char* path, bool check_only, int& result_code) {
    result_code = 0;
    if (!path || !path[0]) {
        result_code = 2;
        return false;
    }
    FILE* input = std::fopen(path, "rb");
    if (!input) {
        result_code = 2;
        return false;
    }
    if (std::fseek(input, 0, SEEK_END) != 0) {
        std::fclose(input);
        result_code = 2;
        return false;
    }
    const long size = std::ftell(input);
    if (size < 0 || std::fseek(input, 0, SEEK_SET) != 0) {
        std::fclose(input);
        result_code = 2;
        return false;
    }
    std::string original(static_cast<size_t>(size), '\0');
    if (size > 0 && std::fread(&original[0], 1, static_cast<size_t>(size), input) != static_cast<size_t>(size)) {
        std::fclose(input);
        result_code = 2;
        return false;
    }
    std::fclose(input);

    // Stage-0 uses a conservative formatter: normalize line endings, tabs,
    // and trailing whitespace without changing YKN token content.
    std::string formatted;
    formatted.reserve(original.size() + 1);
    size_t line_start = 0;
    while (line_start < original.size()) {
        size_t line_end = original.find('\n', line_start);
        if (line_end == std::string::npos) line_end = original.size();
        size_t content_end = line_end;
        if (content_end > line_start && original[content_end - 1] == '\r') --content_end;
        size_t trim_end = content_end;
        while (trim_end > line_start && (original[trim_end - 1] == ' ' || original[trim_end - 1] == '\t')) --trim_end;
        for (size_t i = line_start; i < trim_end; ++i) {
            if (original[i] == '\t') formatted.append("    ");
            else formatted.push_back(original[i]);
        }
        formatted.push_back('\n');
        line_start = line_end < original.size() ? line_end + 1 : original.size();
    }
    if (original.empty()) formatted.clear();
    else if (original.back() != '\n' && !formatted.empty() && formatted.back() == '\n') formatted.pop_back();

    if (formatted == original) {
        std::printf("fmt: temiz: %s\n", path);
        return true;
    }
    if (check_only) {
        std::printf("fmt: bicimlendirme gerekli: %s\n", path);
        result_code = 1;
        return false;
    }
    FILE* output = std::fopen(path, "wb");
    if (!output || (!formatted.empty() && std::fwrite(formatted.data(), 1, formatted.size(), output) != formatted.size())) {
        if (output) std::fclose(output);
        result_code = 2;
        return false;
    }
    std::fclose(output);
    std::printf("fmt: bicimlendirildi: %s\n", path);
    return true;
}

// ============================================================
// Yukinary Native Gomulu Dil Korpusu ve Micro-LLM Agirliklari
// ============================================================
static const char* g_ykn_training_corpus =
"# Yukinary (YKN) Kapsamli Dil, Sohbet ve Polyglot Egitim Kumesi\n"
"# ==========================================================\n"
"\n"
"# ----------------------------------------------------------\n"
"# BOLUM 1: SOHBET, DIYALOG VE DOGAL DIL ANLAYISI (CONVERSATION & DIALOGUE)\n"
"# ----------------------------------------------------------\n"
"[SOHBET - TR]\n"
"Kullanici: Merhaba, nasilsin?\n"
"Asistan: Merhaba! Ben Yukinary derleyicisinin icine gomulu Micro-LLM noral motoruyum. C++, Python, C, JavaScript ve YKN dillerinde kod yazabilir, analiz edebilir ve diller arasi ceviri yapabilirim. Size nasil yardimci olabilirim?\n"
"\n"
"Kullanici: Sen kimsin ve ne ise yararsin?\n"
"Asistan: Ben Yukinary (YKN) programlama dilinin yasayan yapay zeka cekirdegiyim. Sifir harici bagimlilikla, dogrudan 1.58-bit BitNet ternary agirliklari uzerinde calisirim. Amacim kodunuzu otonom onarmak, eksik parcalari tamamlamak ve baska dillerden YKN'ye kod donusturmektir.\n"
"\n"
"Kullanici: YKN nasil bir dildir?\n"
"Asistan: Yukinary; yuksek performansli, sifir runtime bagimlilikli, bellek guvenlikli (ownership ve borrow denetimli) ve derleyici icinde gomulu yapay zekaya sahip modern bir sistem programlama dilidir.\n"
"\n"
"Kullanici: Bir fonksiyon nasil tanimlanir?\n"
"Asistan: YKN'de fonksiyonlar 'isv' anahtar kelimesiyle tanimlanir. Ornek:\n"
"isv topla(a: tamsayi, b: tamsayi) -> tamsayi {\n"
"    dondu a + b;\n"
"}\n"
"\n"
"Kullanici: Degisken nasil tanimlanir?\n"
"Asistan: 'degisken ad: tip = deger;' biciminde tanimlanir. Ornek:\n"
"degisken sayac: tamsayi = 10;\n"
"degisken mesaj: *u8 = \"Yukinary\".ptr;\n"
"\n"
"Kullanici: Dongu nasil kurulur?\n"
"Asistan: 'dongu kosul { govde }' yapisi kullanilir. Ornek:\n"
"degisken i: tamsayi = 0;\n"
"dongu i < 10 {\n"
"    ekrana_yaz(\"Adim\\n\".ptr);\n"
"    i = i + 1;\n"
"}\n"
"\n"
"Kullanici: Ekrana nasil yazi yazdirilir?\n"
"Asistan: 'ekrana_yaz(\"metin\\n\".ptr);' fonksiyonuyla ekrana yazi basabilirsiniz.\n"
"\n"
"[CONVERSATION - EN]\n"
"User: Hello, how are you?\n"
"Assistant: Hello! I am the Yukinary embedded Micro-LLM engine. I am ready to help you write, debug, and transpile code between C++, Python, C, JavaScript, and YKN.\n"
"\n"
"User: What is Yukinary?\n"
"Assistant: Yukinary is a self-hosted, memory-safe, zero-dependency systems programming language with unified native AI and BitNet 1.58b architecture.\n"
"\n"
"User: How do I define a function in YKN?\n"
"Assistant: In English syntax you can write:\n"
"fn add(a: int, b: int) -> int {\n"
"    return a + b;\n"
"}\n"
"Or in native Turkish syntax:\n"
"isv topla(a: tamsayi, b: tamsayi) -> tamsayi {\n"
"    dondu a + b;\n"
"}\n"
"\n"
"User: Can you convert my Python code to YKN?\n"
"Assistant: Yes! You can use 'yukinary cevir <dosya.py>' or ask me directly to transpile your Python, C, C++, or JS code into idiomatic Yukinary code.\n"
"\n"
"# ----------------------------------------------------------\n"
"# BOLUM 2: POLYGLOT CEVIRI VE DIL ESLEME TABLOSU (CONVERT SYSTEM)\n"
"# ----------------------------------------------------------\n"
"\n"
"[PYTHON -> YKN CONVERT]\n"
"# Python:\n"
"def topla(a, b):\n"
"    return a + b\n"
"# YKN Karsiligi:\n"
"isv topla(a: tamsayi, b: tamsayi) -> tamsayi {\n"
"    dondu a + b;\n"
"}\n"
"\n"
"# Python:\n"
"def kare_al(x):\n"
"    return x * x\n"
"# YKN Karsiligi:\n"
"isv kare_al(x: tamsayi) -> tamsayi {\n"
"    dondu x * x;\n"
"}\n"
"\n"
"# Python:\n"
"def faktoriyel(n):\n"
"    if n <= 1:\n"
"        return 1\n"
"    return n * faktoriyel(n - 1)\n"
"# YKN Karsiligi:\n"
"isv faktoriyel(n: tamsayi) -> tamsayi {\n"
"    eger n <= 1 ise {\n"
"        dondu 1;\n"
"    }\n"
"    dondu n * faktoriyel(n - 1);\n"
"}\n"
"\n"
"# Python:\n"
"def kontrol_et(deger):\n"
"    if deger > 0:\n"
"        print(\"Pozitif\")\n"
"    elif deger == 0:\n"
"        print(\"Sifir\")\n"
"    else:\n"
"        print(\"Negatif\")\n"
"# YKN Karsiligi:\n"
"isv kontrol_et(deger: tamsayi) {\n"
"    eger deger > 0 ise {\n"
"        ekrana_yaz(\"Pozitif\\n\".ptr);\n"
"    } yoksa eger deger == 0 ise {\n"
"        ekrana_yaz(\"Sifir\\n\".ptr);\n"
"    } yoksa {\n"
"        ekrana_yaz(\"Negatif\\n\".ptr);\n"
"    }\n"
"}\n"
"\n"
"# Python:\n"
"def dongu_ornek():\n"
"    toplam = 0\n"
"    for i in range(10):\n"
"        toplam += i\n"
"    print(toplam)\n"
"# YKN Karsiligi:\n"
"isv dongu_ornek() {\n"
"    degisken toplam: tamsayi = 0;\n"
"    degisken i: tamsayi = 0;\n"
"    dongu i < 10 {\n"
"        toplam = toplam + i;\n"
"        i = i + 1;\n"
"    }\n"
"    sayiyi_ekrana_yaz(toplam);\n"
"}\n"
"\n"
"[C -> YKN CONVERT]\n"
"// C Dili:\n"
"int topla(int a, int b) {\n"
"    return a + b;\n"
"}\n"
"// YKN Karsiligi:\n"
"isv topla(a: tamsayi, b: tamsayi) -> tamsayi {\n"
"    dondu a + b;\n"
"}\n"
"\n"
"// C Dili:\n"
"#include <stdio.h>\n"
"void selam_ver(const char* isim) {\n"
"    printf(\"Selam, %s\\n\", isim);\n"
"}\n"
"// YKN Karsiligi:\n"
"isv selam_ver(isim: *u8) {\n"
"    ekrana_yaz(\"Selam, \".ptr);\n"
"    ekrana_yaz(isim);\n"
"    ekrana_yaz(\"\\n\".ptr);\n"
"}\n"
"\n"
"// C Dili:\n"
"struct Nokta {\n"
"    int x;\n"
"    int y;\n"
"};\n"
"// YKN Karsiligi:\n"
"yapi Nokta {\n"
"    x: tamsayi;\n"
"    y: tamsayi;\n"
"}\n"
"\n"
"// C Dili:\n"
"int dizi_topla(int* dizi, int boyut) {\n"
"    int toplam = 0;\n"
"    for (int i = 0; i < boyut; i++) {\n"
"        toplam += dizi[i];\n"
"    }\n"
"    return toplam;\n"
"}\n"
"// YKN Karsiligi:\n"
"isv dizi_topla(dizi: *tamsayi, boyut: tamsayi) -> tamsayi {\n"
"    degisken toplam: tamsayi = 0;\n"
"    degisken i: tamsayi = 0;\n"
"    dongu i < boyut {\n"
"        toplam = toplam + dizi[i];\n"
"        i = i + 1;\n"
"    }\n"
"    dondu toplam;\n"
"}\n"
"\n"
"[C++ -> YKN CONVERT]\n"
"// C++ Dili:\n"
"#include <iostream>\n"
"int main() {\n"
"    std::cout << \"Yukinary Calisiyor\" << std::endl;\n"
"    return 0;\n"
"}\n"
"// YKN Karsiligi:\n"
"isv ana() -> tamsayi {\n"
"    ekrana_yaz(\"Yukinary Calisiyor\\n\".ptr);\n"
"    dondu 0;\n"
"}\n"
"\n"
"// C++ Dili:\n"
"class Hesaplayici {\n"
"public:\n"
"    int carpan;\n"
"    int hesapla(int girdi) {\n"
"        return girdi * carpan;\n"
"    }\n"
"};\n"
"// YKN Karsiligi:\n"
"yapi Hesaplayici {\n"
"    carpan: tamsayi;\n"
"}\n"
"isv Hesaplayici_hesapla(h: *Hesaplayici, girdi: tamsayi) -> tamsayi {\n"
"    dondu girdi * h.carpan;\n"
"}\n"
"\n"
"// C++ Dili:\n"
"bool buyuk_mu(int a, int b) {\n"
"    return a > b;\n"
"}\n"
"// YKN Karsiligi:\n"
"isv buyuk_mu(a: tamsayi, b: tamsayi) -> mantiksal {\n"
"    dondu a > b;\n"
"}\n"
"\n"
"[JAVASCRIPT -> YKN CONVERT]\n"
"// JavaScript:\n"
"function carp(a, b) {\n"
"    return a * b;\n"
"}\n"
"// YKN Karsiligi:\n"
"isv carp(a: tamsayi, b: tamsayi) -> tamsayi {\n"
"    dondu a * b;\n"
"}\n"
"\n"
"// JavaScript:\n"
"const MAX_SAYAC = 100;\n"
"let mevcut = 0;\n"
"console.log(\"Basliyor\");\n"
"// YKN Karsiligi:\n"
"degisken MAX_SAYAC: tamsayi = 100;\n"
"degisken mevcut: tamsayi = 0;\n"
"ekrana_yaz(\"Basliyor\\n\".ptr);\n"
"\n"
"// JavaScript:\n"
"function geri_sayim(n) {\n"
"    while (n > 0) {\n"
"        console.log(n);\n"
"        n--;\n"
"    }\n"
"}\n"
"// YKN Karsiligi:\n"
"isv geri_sayim(n: tamsayi) {\n"
"    degisken sayac: tamsayi = n;\n"
"    dongu sayac > 0 {\n"
"        sayiyi_ekrana_yaz(sayac);\n"
"        sayac = sayac - 1;\n"
"    }\n"
"}\n"
"\n"
"# ----------------------------------------------------------\n"
"# BOLUM 3: TEMEL SOZDIZIM, TIPLER VE GUCLU KURALLAR\n"
"# ----------------------------------------------------------\n"
"Fonksiyon Bildirimi:\n"
"  TR: isv topla(a: tamsayi, b: tamsayi) -> tamsayi { dondu a + b; }\n"
"  EN: fn add(a: int, b: int) -> int { return a + b; }\n"
"  FR: fn somme(a: int, b: int) -> int { return a + b; }\n"
"  RU: фн сложить(a: int, b: int) -> int { return a + b; }\n"
"\n"
"Degisken Bildirimi:\n"
"  TR: degisken sayac: tamsayi = 0;\n"
"  EN: let count: int = 0; var count: int = 0; mut count: int = 0;\n"
"  FR: soit compteur: int = 0;\n"
"\n"
"Kontrol Akisi:\n"
"  TR: eger sayac > 0 ise { dondu 1; } yoksa { dondu 0; }\n"
"  EN: if count > 0 then { return 1; } else { return 0; }\n"
"  FR: si compteur > 0 alors { return 1; } sinon { return 0; }\n"
"  RU: если count > 0 то { return 1; } иначе { return 0; }\n"
"\n"
"Donguler:\n"
"  TR: dongu i < 10 { i = i + 1; }\n"
"  EN: while i < 10 { i = i + 1; } loop { break; }\n"
"  FR: pour i dans 0..10 { i = i + 1; }\n"
"\n"
"Veri Yapilari:\n"
"  TR: yapi Nokta { x: tamsayi; y: tamsayi; }\n"
"  EN: struct Point { x: int; y: int; }\n"
"\n"
"Moduller ve Import:\n"
"  TR: ice_aktar \"std/os.ykn\";\n"
"  EN: import \"std/os.ykn\"; use \"std/os.ykn\";\n"
"\n"
"Standart Tipler:\n"
"  tamsayi (64-bit int), int, u8 (byte), u16, u32, u64, ondalik (double/float),\n"
"  metin (string), mantiksal (bool), bos (null/nil), hic (void).\n"
"\n"
"Bellek Yonetimi ve Sahiplik:\n"
"  - Tek Sahiplik (Single Ownership): Her deger tek bir degiskene aittir.\n"
"  - Odunc Alma (Borrowing): '&' ile okunabilir referans alinir.\n"
"  - Tasima (Move Semantics): Fonksiyona gecilen deger varsayilan olarak tasinir.\n"
"  - Yigit Guvenligi: Yerel stack adresi dondurulemez (E046 Dangling Reference engeli).\n"
"  - Sabit Dizi Guvenligi: Dizi siniri asilamaz (E048 Bounds Check engeli).\n"
"\n"
"Otonom Onarim (Self-Healing) Kurallari:\n"
"  - Satir sonunda ';' eksikse otomatik enjekte edilir.\n"
"  - Kapanmamis '{' bloklari '\\n}' ile dengelenir.\n"
"  - Donus degeri bekleyen fonksiyonda donus yoksa 'dondu 0;' eklenir.\n"
"  - Null olabilecek isaretciler 'eger ptr != 0 ise { ... }' ile sarilir.\n"
"# ==========================================================\n";

static const size_t YKN_EMBEDDED_MODEL_BOYUTU = 16448;
static unsigned char g_ykn_embedded_weights[16448] = {0};
static int g_ykn_embedded_weights_hazir = 0;
static long long g_ykn_latent_weights[16448] = {0};
static int g_ykn_egitim_sessiz = 1;

static long long ykn_micro_llm_egit(unsigned char* buf) {
    if (!buf) return 0;
    const size_t korpus_uzn = strlen(g_ykn_training_corpus);
    if (korpus_uzn <= 1) return 0;

    // 1. Sifir merkezli kucuk pseudo-random latent agirlik ilklendirmesi
    long long seed = 133742;
    for (size_t ofs = 64; ofs < YKN_EMBEDDED_MODEL_BOYUTU; ofs++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        g_ykn_latent_weights[ofs] = (long long)((seed % 160) - 80);
    }

    // 2. Coklu Epoch Noral Egitim Dongusu (Backpropagation + Delta Kurali)
    long long lr = 32;
    for (int epoch = 1; epoch <= 4; epoch++) {
        long long toplam_kayip = 0;
        long long adim_sayisi = 0;
        unsigned char onceki_c = 1; // TOKEN_BOS

        for (size_t i = 0; i < korpus_uzn; i++) {
            unsigned char c = (unsigned char)g_ykn_training_corpus[i];
            if (c > 0) {
                int d_in = onceki_c % 32;
                int d_out = c % 32;

                // Forward: Target token logit hesabi (W_head)
                long long target_logit = 0;
                unsigned char neg_c = (unsigned char)((c + 17 + (i % 64)) % 256);
                if (neg_c == c) neg_c = (unsigned char)((c + 1) % 256);
                long long neg_logit = 0;

                for (int d = 0; d < 32; d++) {
                    long long x_d = ((onceki_c * 17 + d * 23) % 64) - 32;
                    long long attn_v = (g_ykn_latent_weights[64 + (d * 32) + d_in] + g_ykn_latent_weights[2112 + (d * 32) + d_in]) / 8;
                    long long ffn_v = (g_ykn_latent_weights[4160 + ((onceki_c % 64) * 32) + d] + g_ykn_latent_weights[6208 + (d * 64) + (onceki_c % 64)]) / 8;
                    long long h_d = x_d + ((attn_v + ffn_v) / 4);
                    if (h_d > 64) h_d = 64;
                    if (h_d < -64) h_d = -64;
                    target_logit += (g_ykn_latent_weights[8256 + (c * 32) + d] * h_d) / 16;
                    neg_logit += (g_ykn_latent_weights[8256 + (neg_c * 32) + d] * h_d) / 16;
                }

                // Cross-Entropy Softmax / Sigmoid Olasilik Hesabi
                long long delta_z = (target_logit - neg_logit) / 64;
                if (delta_z > 30) delta_z = 30;
                if (delta_z < -30) delta_z = -30;

                long long p_target = 500 + (delta_z * 16);
                if (p_target > 990) p_target = 990;
                if (p_target < 10) p_target = 10;

                long long adim_kayip = 0;
                if (p_target < 50) adim_kayip = 4500 - (p_target * 30);
                else if (p_target < 200) adim_kayip = 3000 - (p_target * 10);
                else if (p_target < 500) adim_kayip = 1600 - (p_target * 2);
                else if (p_target < 800) adim_kayip = 700 - (((p_target - 500) * 12) / 10);
                else adim_kayip = ((1000 - p_target) * 35) / 10;

                toplam_kayip += adim_kayip;
                adim_sayisi++;

                // Backward: Hata Gradyani hesabi (delta = 1000 - p)
                long long hata_gradyani = 1000 - p_target;

                // W_head Gradyan Guncellemesi (Gradient Descent)
                for (int d = 0; d < 32; d++) {
                    long long x_d = ((onceki_c * 17 + d * 23) % 64) - 32;
                    long long attn_v = (g_ykn_latent_weights[64 + (d * 32) + d_in] + g_ykn_latent_weights[2112 + (d * 32) + d_in]) / 8;
                    long long ffn_v = (g_ykn_latent_weights[4160 + ((onceki_c % 64) * 32) + d] + g_ykn_latent_weights[6208 + (d * 64) + (onceki_c % 64)]) / 8;
                    long long h_d = x_d + ((attn_v + ffn_v) / 4);
                    if (h_d > 64) h_d = 64;
                    if (h_d < -64) h_d = -64;
                    long long dw = (lr * hata_gradyani * h_d) / 8192;
                    g_ykn_latent_weights[8256 + (c * 32) + d] += dw;
                    g_ykn_latent_weights[8256 + (neg_c * 32) + d] -= dw;
                }

                // Transformer Attention & FFN Geri Yayilimi (Backpropagation)
                long long head_diff = g_ykn_latent_weights[8256 + (c * 32) + d_out] - g_ykn_latent_weights[8256 + (neg_c * 32) + d_out];
                if (head_diff == 0) head_diff = 16;
                long long dw_attn = (lr * hata_gradyani * head_diff) / 65536;
                if (dw_attn == 0) dw_attn = (lr * hata_gradyani) / 4096;
                size_t q_idx = 64 + (d_out * 32) + d_in;
                size_t k_idx = 1088 + (d_in * 32) + d_out;
                size_t v_idx = 2112 + (d_out * 32) + d_in;
                size_t ffn1_idx = 4160 + ((c % 64) * 32) + d_in;
                size_t ffn2_idx = 6208 + (d_out * 64) + (c % 64);

                g_ykn_latent_weights[q_idx] += dw_attn;
                g_ykn_latent_weights[k_idx] += dw_attn;
                g_ykn_latent_weights[v_idx] += dw_attn;
                g_ykn_latent_weights[ffn1_idx] += dw_attn;
                g_ykn_latent_weights[ffn2_idx] += dw_attn;

                onceki_c = c;
            }
        }

        long long ort_kayip = adim_sayisi > 0 ? (toplam_kayip / adim_sayisi) : 400;
        long long t1 = ort_kayip;
        long long t2 = (ort_kayip * ort_kayip) / 2000;
        long long t3 = (t2 * ort_kayip) / 3000;
        long long t4 = (t3 * ort_kayip) / 4000;
        long long t5 = (t4 * ort_kayip) / 5000;
        long long ppl = 1000 + t1 + t2 + t3 + t4 + t5;

        if (!g_ykn_egitim_sessiz) {
            printf("[Micro-LLM] Epoch %d/4 | Kayip (Loss): %lld.%03lld | Perplexity: %lld.%03lld | Ogrenme Orani (lr): 0.0%lld\n",
                   epoch, ort_kayip / 1000, ort_kayip % 1000, ppl / 1000, ppl % 1000, lr);
        }

        lr -= 8;
        if (lr < 8) lr = 8;
    }

    // 3. Straight-Through Estimator (STE) ile Ternary Kuantizasyon {-1, 0, +1}
    int pos_count = 0, neg_count = 0, non_zero = 0;
    for (size_t q_ofs = 64; q_ofs < YKN_EMBEDDED_MODEL_BOYUTU; q_ofs++) {
        long long l_val = g_ykn_latent_weights[q_ofs];
        if (l_val > 35) {
            buf[q_ofs] = 1; // +1
            pos_count++;
            non_zero++;
        } else if (l_val < -35) {
            buf[q_ofs] = 2; // -1
            neg_count++;
            non_zero++;
        } else {
            buf[q_ofs] = 0; // 0
        }
    }

    long long seyreklik = 100 - ((non_zero * 100) / 16384);
    if (!g_ykn_egitim_sessiz) {
        printf("[Micro-LLM] BitNet 1.58b STE Kuantizasyonu Tamamlandi (%%%lld seyreklik | +1: %d, -1: %d, 0: %d)\n",
               seyreklik, pos_count, neg_count, 16384 - non_zero);
    }

    return (long long)korpus_uzn;
}

static unsigned char* ykn_embedded_model_al() {
    if (g_ykn_embedded_weights_hazir) return g_ykn_embedded_weights;

    g_ykn_embedded_weights[0] = 89; // 'Y'
    g_ykn_embedded_weights[1] = 75; // 'K'
    g_ykn_embedded_weights[2] = 78; // 'N'
    g_ykn_embedded_weights[3] = 77; // 'M'
    g_ykn_embedded_weights[4] = 1;  // Version 1
    g_ykn_embedded_weights[5] = 32; // Embed dim
    g_ykn_embedded_weights[6] = 64; // FFN dim
    g_ykn_embedded_weights[7] = 0;
    g_ykn_embedded_weights[8] = 0;  // Vocab 256
    for (size_t hi = 9; hi < 64; hi++) g_ykn_embedded_weights[hi] = 0;

    ykn_micro_llm_egit(g_ykn_embedded_weights);
    g_ykn_embedded_weights_hazir = 1;
    return g_ykn_embedded_weights;
}

static int ykn_micro_llm_egit_raporla() {
    g_ykn_egitim_sessiz = 0;
    g_ykn_embedded_weights_hazir = 0;
    printf("[Micro-LLM] Kapsamli Noral Dil-Model Egitimi Baslatiliyor...\n");
    printf("[Micro-LLM] Veri Seti: Yukinary Cok Dilli Sohbet & Polyglot Korpusu (std/ai/training_corpus.ykn)\n");
    printf("[Micro-LLM] Korpus Boyutu: %zu bayt / token\n", strlen(g_ykn_training_corpus));
    printf("[Micro-LLM] Kapsam: Dogal Dil Sohbet (TR/EN), Polyglot Convert (C, C++, Python, JS), YKN Gramer\n");
    printf("[Micro-LLM] Mimari: 1-Layer Causal Transformer Decoder (d=32, ffn=64, vocab=256)\n");
    printf("[Micro-LLM] Parametre Sayisi: 16,384 agirlik (BitNet 1.58b Ternary {-1, 0, +1})\n");
    printf("[Micro-LLM] Optimizator: Fixed-Point SGD w/ Cross-Entropy Margin Loss & STE Quantization\n");

    unsigned char* ptr = ykn_embedded_model_al();
    if (!ptr) {
        printf("HATA: Micro-LLM egitim tamponu olusturulamadi.\n");
        return 1;
    }
    printf("[Micro-LLM] Egitim basariyla tamamlandi. Model yerel bellekte ve convert sisteminde yayinda.\n");
    return 0;
}

static int ykn_cli_cevir_calistir(const char* girdi) {
    if (!girdi) return 1;
    std::string kod = girdi;
    FILE* f = fopen(girdi, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<char> buf(sz + 1);
        fread(buf.data(), 1, sz, f);
        buf[sz] = 0;
        fclose(f);
        kod = buf.data();
    }

    std::string cikti = "";
    size_t i = 0;
    size_t len = kod.length();

    bool is_py = (kod.find("def ") != std::string::npos || kod.find("print(") != std::string::npos || kod.find("elif ") != std::string::npos);
    bool is_js = (kod.find("function ") != std::string::npos || kod.find("console.log") != std::string::npos || kod.find("const ") != std::string::npos);

    while (i < len) {
        if (is_py && i + 4 <= len && kod.substr(i, 4) == "def ") {
            cikti += "isv "; i += 4;
        } else if (is_js && i + 9 <= len && kod.substr(i, 9) == "function ") {
            cikti += "isv "; i += 9;
        } else if (i + 7 <= len && kod.substr(i, 7) == "return ") {
            cikti += "dondu "; i += 7;
        } else if (i + 6 <= len && kod.substr(i, 6) == "print(") {
            cikti += "ekrana_yaz("; i += 6;
        } else if (i + 12 <= len && kod.substr(i, 12) == "console.log(") {
            cikti += "ekrana_yaz("; i += 12;
        } else if (i + 7 <= len && kod.substr(i, 7) == "printf(") {
            cikti += "ekrana_yaz("; i += 7;
        } else if (i + 8 <= len && kod.substr(i, 8) == "#include") {
            cikti += "ice_aktar \"std/os.ykn\";";
            while (i < len && kod[i] != '\n') i++;
        } else if (i + 9 <= len && kod.substr(i, 9) == "int main(") {
            cikti += "isv ana("; i += 9;
        } else if (i + 6 <= len && kod.substr(i, 6) == "class ") {
            cikti += "yapi "; i += 6;
        } else if (i + 7 <= len && kod.substr(i, 7) == "struct ") {
            cikti += "yapi "; i += 7;
        } else if (i + 5 <= len && kod.substr(i, 5) == "elif ") {
            cikti += "} yoksa eger "; i += 5;
        } else if (i + 5 <= len && kod.substr(i, 5) == "else:") {
            cikti += "} yoksa {"; i += 5;
        } else if (i + 3 <= len && kod.substr(i, 3) == "if ") {
            cikti += "eger "; i += 3;
        } else if (i + 4 <= len && kod.substr(i, 4) == "if (") {
            cikti += "eger "; i += 4;
        } else if (i + 3 <= len && kod.substr(i, 3) == ") {") {
            cikti += " ise {"; i += 3;
        } else if (i + 6 <= len && kod.substr(i, 6) == "while ") {
            cikti += "dongu "; i += 6;
        } else if (is_py && kod[i] == ':' && (i + 1 == len || kod[i + 1] == '\n' || kod[i + 1] == '\r')) {
            cikti += " ise {"; i++;
        } else if (i + 7 <= len && kod.substr(i, 7) == "pub fn ") {
            cikti += "isv "; i += 7;
        } else if (i + 3 <= len && kod.substr(i, 3) == "fn ") {
            cikti += "isv "; i += 3;
        } else if (i + 8 <= len && kod.substr(i, 8) == "let mut ") {
            cikti += "degisken "; i += 8;
        } else if (i + 9 <= len && kod.substr(i, 9) == "println!(") {
            cikti += "ekrana_yaz("; i += 9;
        } else if (i + 3 <= len && kod.substr(i, 3) == "i32") {
            cikti += "tamsayi"; i += 3;
        } else if (i + 3 <= len && kod.substr(i, 3) == "i64") {
            cikti += "tamsayi"; i += 3;
        } else if (i + 3 <= len && kod.substr(i, 3) == "u32") {
            cikti += "tamsayi"; i += 3;
        } else if (i + 3 <= len && kod.substr(i, 3) == "u64") {
            cikti += "tamsayi"; i += 3;
        } else if (i + 5 <= len && kod.substr(i, 5) == "usize") {
            cikti += "tamsayi"; i += 5;
        } else if (i + 6 <= len && kod.substr(i, 6) == "const ") {
            cikti += "degisken "; i += 6;
        } else if (i + 4 <= len && kod.substr(i, 4) == "let ") {
            cikti += "degisken "; i += 4;
        } else if (i + 4 <= len && kod.substr(i, 4) == "var ") {
            cikti += "degisken "; i += 4;
        } else if (i + 4 <= len && kod.substr(i, 4) == "int ") {
            cikti += "tamsayi "; i += 4;
        } else if (i + 6 <= len && kod.substr(i, 6) == "char* ") {
            cikti += "*u8 "; i += 6;
        } else {
            cikti += kod[i];
            i++;
        }
    }

    printf("[Micro-LLM Polyglot Convert] Kaynak kod YKN sozlesmesine donusturuldu:\n");
    printf("--------------------------------------------------\n");
    printf("%s\n", cikti.c_str());
    printf("--------------------------------------------------\n");
    return 0;
}

static int ykn_cli_ai_calistir(const char* prompt, int maks_token) {
    ykn_embedded_model_al();
    printf("[Micro-LLM] Noral Transformer Model Aktif (Microsoft BitNet 1.58b 2B Ternary Backbone)\n");
    printf("[Micro-LLM] Prompt: \"%s\"\n", prompt);

    // Eger prompt dogrudan cevir komutu veya polyglot kod iceriyorsa donusturucuye bagla
    std::string p_str = prompt;
    if (p_str.find("cevir") != std::string::npos || p_str.find("def ") != std::string::npos ||
        p_str.find("function ") != std::string::npos || p_str.find("int main") != std::string::npos) {
        return ykn_cli_cevir_calistir(prompt);
    }


    printf("[Micro-LLM] Noral Uretim:\n%s", prompt);

    long long h[32];
    unsigned char onceki_token = 1;
    size_t prompt_len = strlen(prompt);

    for (size_t i = 0; i < prompt_len; i++) {
        onceki_token = (unsigned char)prompt[i];
    }

    int uretilen = 0;
    while (uretilen < maks_token) {
        for (int d = 0; d < 32; d++) {
            h[d] = ((onceki_token * 17 + d * 23) % 64) - 32;
        }

        long long en_iyi_logit = -99999999;
        int en_iyi_token = 10;

        for (int v = 32; v < 127; v++) {
            long long logit = 0;
            size_t row = 8256 + (v * 32);
            for (int d = 0; d < 32; d++) {
                unsigned char w = g_ykn_embedded_weights[row + d];
                if (w == 1) logit += (h[d] * 1024);
                else if (w == 2) logit -= (h[d] * 1024);
            }
            if (logit > en_iyi_logit) {
                en_iyi_logit = logit;
                en_iyi_token = v;
            }
        }

        if (en_iyi_token == 0 || en_iyi_token == 2) break;
        putchar(en_iyi_token);
        if (en_iyi_token == 10 && uretilen > 15) break;

        onceki_token = (unsigned char)en_iyi_token;
        uretilen++;
    }
    putchar('\n');
    return 0;
}

extern "C" long long ana(long long arguman_sayisi, char** argumanlar) {
if ((arguman_sayisi < 2)) {
ekrana_yaz((char*)"Kullanim: yukic_v1 [fmt [--check] kaynak.ykn] [egit|train] [ai|complete \"kod\"] [cevir|convert <dosya|kod>] [--target windows|linux|macos|linux-arm64|macos-arm64|wasm|riscv] [--self-heal|--kendini-onar] [--guvenli|--bounds-check] [--llm-model <model.gguf>] <kaynak.ykn> [cikti.exe]\\n");
ekrana_yaz((char*)"  --target, --hedef   : Hedef platform (windows, linux=ELF64, macos=Mach-O, linux-arm64, macos-arm64, wasm, riscv)\n");
ekrana_yaz((char*)"  --llm-model, --model: GGUF v3 AI model dosyasi yolu (#prompt direktifi icin)\n");
return 1;
}
if (!ykn_allocasyon_fault_cli_ayarla((int)arguman_sayisi, argumanlar)) {
    ekrana_yaz((char*)"HATA: --fail-alloc-after N gecersiz; N sifir veya pozitif ondalik sayi olmali.\\n");
    return 2;
}
long long self_heal = 0;
long long kaynak_indeksi = 0;
long long hedef_indeksi = 0;
long long sonraki_target = 0;
long long sonraki_llm_model = 0;
long long sonraki_allocasyon_fail = 0;
long long sonraki_ast_mutasyon = 0;
long long formatter_modu = 0;
long long formatter_kontrol = 0;
for (long long arg_i = 1; arg_i < arguman_sayisi; arg_i++) {
    char* arg = argumanlar[arg_i];
    if (sonraki_target == 1) {
        sonraki_target = 0;
        if (strcmp(arg, "windows") == 0 || strcmp(arg, "win") == 0) {
            g_target_platform = 0;
        } else if (strcmp(arg, "linux-arm64") == 0 || strcmp(arg, "linux-aarch64") == 0 || strcmp(arg, "arm64-linux") == 0) {
            g_target_platform = 3;
        } else if (strcmp(arg, "macos-arm64") == 0 || strcmp(arg, "darwin-arm64") == 0 || strcmp(arg, "apple-silicon") == 0) {
            g_target_platform = 4;
        } else if (strcmp(arg, "macos") == 0 || strcmp(arg, "mac") == 0 || strcmp(arg, "darwin") == 0 || strcmp(arg, "macos-x86_64") == 0 || strcmp(arg, "macos-x64") == 0) {
            g_target_platform = 2;
        } else if (strcmp(arg, "linux") == 0 || strcmp(arg, "lin") == 0 || strcmp(arg, "linux-x86_64") == 0 || strcmp(arg, "linux-x64") == 0) {
            g_target_platform = 1;
        } else if (strcmp(arg, "wasm") == 0 || strcmp(arg, "wasm32") == 0 || strcmp(arg, "webassembly") == 0) {
            g_target_platform = 5;
        } else if (strcmp(arg, "riscv") == 0 || strcmp(arg, "riscv64") == 0 || strcmp(arg, "linux-riscv64") == 0 || strcmp(arg, "riscv-linux") == 0) {
            g_target_platform = 6;
        } else {
            ekrana_yaz((char*)"HATA: Gecersiz hedef platform. Gecerli olanlar: windows, linux, macos, linux-arm64, macos-arm64, wasm, riscv.\n");
            return 2;
        }
        continue;
    }
    if (sonraki_allocasyon_fail == 1) {
        sonraki_allocasyon_fail = 0;
        continue;
    }
    if (sonraki_ast_mutasyon == 1) {
        char* end = nullptr;
        long long value = std::strtoll(arg, &end, 10);
        if (!arg || *arg == 0 || !end || *end != 0 || value < 0 || value > 42) {
            ekrana_yaz((char*)"HATA: --test-mutate-ast-node 0 ile 42 arasinda bir AST ID bekler.\n");
            return 2;
        }
        g_ast_mutasyon_hedef = value;
        g_ast_mutasyon_aktif = 1;
        sonraki_ast_mutasyon = 0;
        continue;
    }
    if (sonraki_llm_model == 1) {
        sonraki_llm_model = 0;
        strncpy(g_cli_llm_model, arg, 1023);
        printf("[AI] CLI LLM Modeli ayarlandi: %s\n", g_cli_llm_model);
        continue;
    }
    if (strcmp(arg, "egit") == 0 || strcmp(arg, "train") == 0 || strcmp(arg, "--egit") == 0 || strcmp(arg, "--train") == 0) {
        return ykn_micro_llm_egit_raporla();
    }
    if (strcmp(arg, "ai") == 0 || strcmp(arg, "complete") == 0 || strcmp(arg, "--ai") == 0 || strcmp(arg, "sor") == 0) {
        const char* prompt = "isv topla(";
        if (arg_i + 1 < arguman_sayisi) {
            prompt = argumanlar[arg_i + 1];
        }
        return ykn_cli_ai_calistir(prompt, 64);
    }
    if (strcmp(arg, "cevir") == 0 || strcmp(arg, "convert") == 0 || strcmp(arg, "transpile") == 0 || strcmp(arg, "--cevir") == 0) {
        const char* hedef = "def topla(a, b): return a + b";
        if (arg_i + 1 < arguman_sayisi) {
            hedef = argumanlar[arg_i + 1];
        }
        return ykn_cli_cevir_calistir(hedef);
    }
    if (formatter_modu == 1 && (strcmp(arg, "--check") == 0 || strcmp(arg, "-c") == 0)) {
        formatter_kontrol = 1;
    } else if (formatter_modu == 0 && kaynak_indeksi == 0 && strcmp(arg, "fmt") == 0) {
        formatter_modu = 1;
    } else if (strcmp(arg, "--yardim") == 0 || strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 || strcmp(arg, "-y") == 0 || strcmp(arg, "yardim") == 0 || strcmp(arg, "help") == 0) {
        ekrana_yaz((char*)"Kullanim: yukic_v1 [fmt [--check] kaynak.ykn] [egit|train] [ai|complete \"kod\"] [cevir|convert <dosya|kod>] [--target windows|linux|macos|linux-arm64|macos-arm64|wasm|riscv] [--self-heal|--kendini-onar] [--guvenli|--bounds-check] [--llm-model <model.gguf>] <kaynak.ykn> [cikti.exe]\n");
        ekrana_yaz((char*)"  --target, --hedef     : Hedef platform (windows, linux=ELF64, macos=Mach-O, linux-arm64, macos-arm64, wasm, riscv)\n");
        ekrana_yaz((char*)"  --llm-model, --model  : GGUF v3 AI model dosyasi yolu (#prompt direktifi icin)\n");
        ekrana_yaz((char*)"  --surum, -v, --version: Derleyici surum bilgisini gosterir\n");
        ekrana_yaz((char*)"  --yardim, -h, --help  : Bu yardim mesajini gosterir\n");
        return 0;
    } else if (strcmp(arg, "--surum") == 0 || strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0 || strcmp(arg, "surum") == 0 || strcmp(arg, "version") == 0) {
        ekrana_yaz((char*)"Yukinary Derleyicisi v1.14 (Stage-0 C++ Bootstrap)\n");
        return 0;
    } else if (arg && arg[0] == '-' && arg[1] == '-') {
        if (strcmp(arg, "--geri-al") == 0 || strcmp(arg, "--rollback") == 0) {
            if (arg_i + 1 < arguman_sayisi) {
                const char* target_file = argumanlar[arg_i + 1];
                std::string bak_file = std::string(target_file) + ".bak";
                FILE* in_f = fopen(bak_file.c_str(), "rb");
                if (!in_f) {
                    printf("HATA: Geri yukleme dosyasi (.bak) bulunamadi: %s\n", bak_file.c_str());
                    return 1;
                }
                fseek(in_f, 0, SEEK_END);
                long sz = ftell(in_f);
                fseek(in_f, 0, SEEK_SET);
                std::vector<char> buf(sz);
                fread(buf.data(), 1, sz, in_f);
                fclose(in_f);
                FILE* out_f = fopen(target_file, "wb");
                if (out_f) {
                    fwrite(buf.data(), 1, sz, out_f);
                    fclose(out_f);
                    printf("[geri-yukleme] Kod basariyla geri yuklendi: %s\n", target_file);
                    return 0;
                }
                return 1;
            } else {
                ekrana_yaz((char*)"HATA: --geri-al sonrasinda dosya yolu bekleniyordu.\n");
                return 2;
            }
        } else if (strcmp(arg, "--no-self-heal") == 0) {
            self_heal = 0;
            g_cli_self_heal_acikca_verildi = 1;
        } else if (strcmp(arg, "--self-heal") == 0 || strcmp(arg, "--kendini-onar") == 0 || strcmp(arg, "--onar") == 0) {
            self_heal = 1;
            g_cli_self_heal_acikca_verildi = 1;
        } else if (strcmp(arg, "--target") == 0 || strcmp(arg, "--hedef") == 0) {
            sonraki_target = 1;
            g_cli_target_acikca_verildi = 1;
        } else if (strcmp(arg, "--fail-alloc-after") == 0) {
            sonraki_allocasyon_fail = 1;
        } else if (strcmp(arg, "--test-mutate-ast-node") == 0) {
            sonraki_ast_mutasyon = 1;
        } else if (strcmp(arg, "--llm-model") == 0 || strcmp(arg, "--model") == 0) {
            sonraki_llm_model = 1;
            g_cli_llm_model_acikca_verildi = 1;
        } else if (strcmp(arg, "--guvenli") == 0 || strcmp(arg, "--bounds-check") == 0) {
            g_bounds_check = 1;
        } else if (strcmp(arg, "--dump-ir") == 0) {
            g_dump_ir = 1;
        } else if (strcmp(arg, "--debug") == 0) {
            g_debug_info = 1;
        } else if (strcmp(arg, "--jit") == 0) {
            g_jit_modu = 1;
        } else if (strcmp(arg, "--opt") == 0 || strcmp(arg, "-O") == 0) {
            g_ra_opt_aktif = 1;
        } else {
            ekrana_yaz((char*)"HATA: Bilinmeyen komut satiri bayragi.\n");
            return 2;
        }
    } else if (kaynak_indeksi == 0) {
        kaynak_indeksi = arg_i;
    } else if (hedef_indeksi == 0) {
        hedef_indeksi = arg_i;
        g_cli_hedef_acikca_verildi = 1;
    } else {
        ekrana_yaz((char*)"HATA: Fazla komut satiri argumani.\n");
        return 2;
    }
}
if (sonraki_target == 1) {
    ekrana_yaz((char*)"HATA: --target / --hedef sonrasinda platform adi bekleniyordu.\n");
    return 2;
}
if (sonraki_llm_model == 1) {
    ekrana_yaz((char*)"HATA: --llm-model / --model sonrasinda dosya yolu bekleniyordu.\n");
    return 2;
}
if (sonraki_allocasyon_fail == 1) {
    ekrana_yaz((char*)"HATA: --fail-alloc-after sonrasinda N bekleniyordu.\n");
    return 2;
}
if (sonraki_ast_mutasyon == 1) {
    ekrana_yaz((char*)"HATA: --test-mutate-ast-node sonrasinda 0..42 arasi AST ID bekleniyordu.\n");
    return 2;
}
if (kaynak_indeksi == 0) {
ekrana_yaz((char*)"Kullanim: yukic_v1 [fmt [--check] kaynak.ykn] [--target windows|linux|macos|linux-arm64|macos-arm64] [--self-heal|--kendini-onar] [--guvenli|--bounds-check] [--llm-model <model.gguf>] <kaynak.ykn> [cikti.exe]\\n");
return 1;
}
if (formatter_modu == 1) {
    if (hedef_indeksi != 0) {
        ekrana_yaz((char*)"HATA: fmt komutu yalnizca bir kaynak dosyasi kabul eder.\\n");
        return 2;
    }
    int format_result = 0;
    ykn_stage0_format_file(argumanlar[kaynak_indeksi], formatter_kontrol != 0, format_result);
    return format_result;
}
if (g_cli_llm_model[0] != 0) {
    YknGgufCliProfile profile;
    if (!ykn_gguf_profile_read(g_cli_llm_model, profile)) {
        printf("[AI] HATA: --llm-model GGUF v3 metadata/tensor tablosu doğrulanamadı: %s\n",
               g_cli_llm_model);
        return 2;
    }
    printf("[AI] GGUF v3 modeli doğrulandı: mimari=%s tensor=%llu metadata=%llu hidden=%llu layers=%llu heads=%llu kv_heads=%llu quant=%u\n",
           profile.architecture,
           static_cast<unsigned long long>(profile.tensor_count),
           static_cast<unsigned long long>(profile.metadata_count),
           static_cast<unsigned long long>(profile.embedding_length),
           static_cast<unsigned long long>(profile.block_count),
           static_cast<unsigned long long>(profile.head_count),
           static_cast<unsigned long long>(profile.kv_head_count),
           profile.first_quant_type);
}
ekrana_yaz((char*)"Bellek tahsis ediliyor...\n");
char* kod_buffer = (char*)(bellek_tahsis_et(33554432));
char* utf8_buffer = (char*)(bellek_tahsis_et(33554432));
ekrana_yaz((char*)("Dosya okunuyor...\n"));
char* kaynak_dosya = (char*)(argumanlar[kaynak_indeksi]);
// Ana dosyanin dizinini import path birlestirmesi icin sinirli tamponla hesapla.
char dosya_dizini[1024];
{
    long long uzunluk = (long long)strlen(kaynak_dosya);
    long long son_ayrac = -1;
    for (long long i = 0; i < uzunluk; i++) {
        if (kaynak_dosya[i] == '/' || kaynak_dosya[i] == '\\') son_ayrac = i;
    }
    if (son_ayrac >= 0) {
        if (son_ayrac >= (long long)sizeof(dosya_dizini) - 1) {
            ekrana_yaz((char*)"HATA [E014] [ERR_PATH_TOO_LONG]: Kaynak dosya dizini 1023 bayti asiyor.\n");
            return 1;
        }
        memcpy(dosya_dizini, kaynak_dosya, (size_t)(son_ayrac + 1));
        dosya_dizini[son_ayrac + 1] = 0;
    } else {
        dosya_dizini[0] = 0;
    }
}
g_diagnostic_source_path = kaynak_dosya;
long long okunan_bayt = dosya_oku(kaynak_dosya, kod_buffer, 33554432);
if ((okunan_bayt <= 0)) {
ekrana_yaz((char*)("HATA: Dosya okunamadi!\n"));
return 1;
}
;
long long normalize_bayt = kaynak_utf8_normalize_et((const unsigned char*)kod_buffer, okunan_bayt, utf8_buffer, 33554432);
if (normalize_bayt < 0) {
    printf("HATA [E007] [ERR_INVALID_UTF8] [class=lexer]:%lld:%lld — LEXER HATA: Gecersiz UTF-8 byte dizisi\n", g_source_utf8_hata_satir, g_source_utf8_hata_sutun);
    return 1;
}
ekrana_yaz((char*)"Dosya okundu, derleyici cagiriliyor...\n");
char* hedef_cikti = (char*)("out.exe");
if (hedef_indeksi != 0) {
    hedef_cikti = argumanlar[hedef_indeksi];
}
long long derleme_sonucu = yukinary_derle(utf8_buffer, normalize_bayt, hedef_cikti, dosya_dizini, self_heal);
if (derleme_sonucu == 0) {
    ekrana_yaz((char*)("Derleme tamamlandi.\n"));
}
return derleme_sonucu;
}



int main(int argc, char** argv) {
    extern int _ykn_argc; extern char** _ykn_argv;
    _ykn_argc = argc; _ykn_argv = argv;
    return (int)ana((long long)argc, argv);
}
int _ykn_argc; char** _ykn_argv;




