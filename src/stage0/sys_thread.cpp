// ============================================================
// Yukinary Native Threading Wrapper
// ============================================================

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

extern "C" {

void* yukinary_sys_mutex_create() {
#ifdef _WIN32
    CRITICAL_SECTION* cs = new CRITICAL_SECTION;
    InitializeCriticalSection(cs);
    return (void*)cs;
#else
    pthread_mutex_t* mutex = new pthread_mutex_t;
    pthread_mutex_init(mutex, NULL);
    return (void*)mutex;
#endif
}

void yukinary_sys_mutex_lock(void* mutex) {
#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION*)mutex);
#else
    pthread_mutex_lock((pthread_mutex_t*)mutex);
#endif
}

void yukinary_sys_mutex_unlock(void* mutex) {
#ifdef _WIN32
    LeaveCriticalSection((CRITICAL_SECTION*)mutex);
#else
    pthread_mutex_unlock((pthread_mutex_t*)mutex);
#endif
}

} // extern "C"
