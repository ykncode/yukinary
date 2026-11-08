// ============================================================
// Yukinary JIT (Just-In-Time) Execution Engine
// ============================================================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

extern "C" {

struct YukinaryJitOrtami {
    void* calistirilabilir_bellek;
    int toplam_kapasite;
    int kullanilan_bayt;
    int son_cikis_kodu;
};

// JIT modunun platformda desteklenip desteklenmedigini bildirir
int yukinary_jit_destekleniyor_mu() {
    return 1;
}

// Stateful JIT oturumu olusturur (REPL oturumlari ve --jit kosusu icin)
YukinaryJitOrtami* yukinary_jit_ortami_olustur(int varsayilan_kapasite) {
    if (varsayilan_kapasite <= 0) varsayilan_kapasite = 1048576; // 1 MB

    YukinaryJitOrtami* ort = (YukinaryJitOrtami*)malloc(sizeof(YukinaryJitOrtami));
    if (!ort) return NULL;

#ifdef _WIN32
    void* mem = VirtualAlloc(NULL, varsayilan_kapasite, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        free(ort);
        return NULL;
    }
#else
    void* mem = mmap(NULL, varsayilan_kapasite, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        free(ort);
        return NULL;
    }
#endif

    ort->calistirilabilir_bellek = mem;
    ort->toplam_kapasite = varsayilan_kapasite;
    ort->kullanilan_bayt = 0;
    ort->son_cikis_kodu = 0;
    return ort;
}

// Makine kodunu JIT bellegine yukler ve dogrudan RAM uzerinden calistirir
int yukinary_jit_kodu_calistir(YukinaryJitOrtami* ort, const unsigned char* makine_kodu, int boyut) {
    if (!ort || !ort->calistirilabilir_bellek || !makine_kodu || boyut <= 0) return -1;
    if (ort->kullanilan_bayt + boyut > ort->toplam_kapasite) {
        printf("[JIT Hata] Calistirilabilir bellek sayfasi doldu!\n");
        return -2;
    }

    void* hedef_ptr = (char*)ort->calistirilabilir_bellek + ort->kullanilan_bayt;
    memcpy(hedef_ptr, makine_kodu, boyut);

    typedef int (*JitFunc)();
    JitFunc fn = (JitFunc)hedef_ptr;

    int sonuc = fn();
    ort->kullanilan_bayt += boyut;
    ort->son_cikis_kodu = sonuc;
    return sonuc;
}

// Takes generated machine code (bytes) and executes it directly from RAM (One-shot JIT)
int yukinary_jit_calistir(const unsigned char* makine_kodu, int boyut) {
    if (!makine_kodu || boyut <= 0) return -1;

#ifdef _WIN32
    // 1. Allocate Executable Memory (PAGE_EXECUTE_READWRITE)
    void* exec_mem = VirtualAlloc(NULL, boyut, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!exec_mem) {
        printf("[JIT Hata] Bellek ayrilamadi!\n");
        return -1;
    }

    // 2. Copy machine code into executable memory
    memcpy(exec_mem, makine_kodu, boyut);

    // 3. Cast memory pointer to function pointer and CALL it
    typedef int (*JitFunc)();
    JitFunc func = (JitFunc)exec_mem;
    
    int sonuc = func(); // Jump to the code!

    // 4. Free the memory
    VirtualFree(exec_mem, 0, MEM_RELEASE);
    return sonuc;
#else
    // POSIX mmap implementation
    void* exec_mem = mmap(NULL, boyut, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem == MAP_FAILED) return -1;
    
    memcpy(exec_mem, makine_kodu, boyut);
    
    typedef int (*JitFunc)();
    JitFunc func = (JitFunc)exec_mem;
    int sonuc = func();
    
    munmap(exec_mem, boyut);
    return sonuc;
#endif
}

// JIT ortamini kapatir ve bellek sayfalarini sisteme iade eder
void yukinary_jit_ortami_kapat(YukinaryJitOrtami* ort) {
    if (!ort) return;
    if (ort->calistirilabilir_bellek) {
#ifdef _WIN32
        VirtualFree(ort->calistirilabilir_bellek, 0, MEM_RELEASE);
#else
        munmap(ort->calistirilabilir_bellek, ort->toplam_kapasite);
#endif
        ort->calistirilabilir_bellek = NULL;
    }
    free(ort);
}

} // extern "C"
