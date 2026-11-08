// ============================================================
// Yukinary Native IO Wrapper
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

extern "C" {

void* yukinary_sys_file_open(const char* path, int mode) {
#ifdef _WIN32
    DWORD access = (mode == 0) ? GENERIC_READ : GENERIC_WRITE;
    DWORD create_disp = (mode == 0) ? OPEN_EXISTING : CREATE_ALWAYS;
    HANDLE h = CreateFileA(path, access, 0, NULL, create_disp, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    return (void*)h;
#else
    int flags = (mode == 0) ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);
    int fd = open(path, flags, 0666);
    if (fd < 0) return NULL;
    return (void*)(intptr_t)fd;
#endif
}

int yukinary_sys_file_read(void* handle, char* buffer, int size) {
#ifdef _WIN32
    DWORD bytesRead;
    if (ReadFile((HANDLE)handle, buffer, size, &bytesRead, NULL)) {
        return (int)bytesRead;
    }
    return -1;
#else
    return read((int)(intptr_t)handle, buffer, size);
#endif
}

void yukinary_sys_file_close(void* handle) {
#ifdef _WIN32
    CloseHandle((HANDLE)handle);
#else
    close((int)(intptr_t)handle);
#endif
}

} // extern "C"
