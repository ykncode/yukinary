#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>

using tamsayi = long long;
using metin = std::string;

extern "C" void matris_carp();

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
inline char* bellek_tahsis_et(long long boyut) { return (char*)calloc(1, boyut); }
inline long long ekrana_yaz(const char* m) { std::cout << m; return 0; }
inline long long dosya_oku(const char* yol, char* buf, long long n) { FILE* f = fopen(yol, "rb"); if(!f) return 0; long long r = fread(buf, 1, n, f); fclose(f); return r; }
inline long long diske_bas_ve_kapat(const char* yol, char* buf, long long n) { FILE* f = fopen(yol, "wb"); if(!f) return 0; long long w = fwrite(buf, 1, n, f); fclose(f); return w; }

void konsol_yaz(char* metin, long long uzunluk);
long long kucuk_fonk(long long x, long long y);
long long derin_ozyineleme(long long n);
extern "C" long long ana();
harici;
ExitProcess(uExitCode, , tamsayi);
GetStdHandle(nStdHandle, , tamsayi);
tamsayi;
WriteFile(hFile, , tamsayi, lpBuffer, ( * u8), nNumberOfBytesToWrite, , tamsayi, lpNumberOfBytesWritten, ( * tamsayi), lpOverlapped, , tamsayi);
void konsol_yaz(char* metin, long long uzunluk) {
long long stdout_handle = GetStdHandle((0 - 11));
long long yazilan = 0;
WriteFile(stdout_handle, metin, uzunluk, &yazilan, 0);
}


long long kucuk_fonk(long long x, long long y) {
long long a = (x + 10);
long long b = (y * 2);
return (a + b);
}


long long derin_ozyineleme(long long n) {
if ((n <= 0)) {
return 0;
}
;
return (1 + derin_ozyineleme((n - 1)));
}


extern "C" long long ana() {
konsol_yaz((char*)("=== TEST DINAMIK STACK FRAME ===\n"), 33);
long long r = kucuk_fonk(5, 7);
if ((r == 29)) {
(char*)("[OK] Kucuk fonksiyon calisti: 29\n");
;
;
}
 else {
konsol_yaz((char*)("[HATA] Kucuk fonksiyon hatali!\n"), 31);
ExitProcess(1);
}
;
long long ozy = derin_ozyineleme(5000);
if ((ozy == 5000)) {
(char*)("[OK] Derin ozyineleme (5000 cagri) dinamik frame ile basarili\n");
;
;
}
 else {
konsol_yaz((char*)("[HATA] Derin ozyineleme hatali!\n"), 31);
ExitProcess(2);
}
;
konsol_yaz((char*)("TEST DINAMIK FRAME: BASARILI\n"), 29);
ExitProcess(0);
return 0;
}



int main(int argc, char** argv) {
    extern int _ykn_argc; extern char** _ykn_argv;
    _ykn_argc = argc; _ykn_argv = argv;
    return (int)ana((long long)argc, argv);
}
int _ykn_argc; char** _ykn_argv;
