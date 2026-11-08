// ============================================================
// Yukinary Native Event Loop (epoll / IOCP)
// ============================================================

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/epoll.h>
#include <unistd.h>
#endif

extern "C" {

void* yukinary_sys_event_loop_create() {
#ifdef _WIN32
    // Create IOCP
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    return (void*)iocp;
#else
    // Create epoll
    int epfd = epoll_create1(0);
    return (void*)(intptr_t)epfd;
#endif
}

int yukinary_sys_event_add(void* loop, int fd, void* user_data) {
#ifdef _WIN32
    // Attach socket to IOCP
    HANDLE iocp = (HANDLE)loop;
    HANDLE hFile = (HANDLE)(intptr_t)fd;
    CreateIoCompletionPort(hFile, iocp, (ULONG_PTR)user_data, 0);
    return 0;
#else
    int epfd = (int)(intptr_t)loop;
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET; // Edge-triggered
    ev.data.ptr = user_data;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
#endif
}

// Block and wait for events
int yukinary_sys_event_wait(void* loop, void** out_user_data) {
#ifdef _WIN32
    HANDLE iocp = (HANDLE)loop;
    DWORD bytes;
    ULONG_PTR key;
    LPOVERLAPPED overlapped;
    if (GetQueuedCompletionStatus(iocp, &bytes, &key, &overlapped, INFINITE)) {
        *out_user_data = (void*)key;
        return 1;
    }
    return 0;
#else
    int epfd = (int)(intptr_t)loop;
    struct epoll_event events[1];
    int n = epoll_wait(epfd, events, 1, -1);
    if (n > 0) {
        *out_user_data = events[0].data.ptr;
        return 1;
    }
    return 0;
#endif
}

} // extern "C"
