#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#if defined(_WIN32)
__declspec(dllexport)
#endif
long long dyn_add(long long first, long long second) {
    printf("DYN_DLL_CALLED\\n");
    fflush(stdout);
    return first + second;
}

#if defined(_WIN32)
__declspec(dllexport)
#endif
long long dyn_mul(long long first, long long second) {
    return first * second;
}

#ifdef __cplusplus
}
#endif
