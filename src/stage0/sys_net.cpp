// ============================================================
// Yukinary Native Network (Socket) Wrapper
// ============================================================

#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

extern "C" {

long long yukinary_sys_net_init() {
#ifdef _WIN32
    WSADATA wsaData{};
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

long long yukinary_sys_socket_create(long long family, long long type, long long protocol) {
#ifdef _WIN32
    SOCKET s = ::socket(static_cast<int>(family), static_cast<int>(type), static_cast<int>(protocol));
    return s == INVALID_SOCKET ? -1 : static_cast<long long>(s);
#else
    int s = ::socket(static_cast<int>(family), static_cast<int>(type), static_cast<int>(protocol));
    return s < 0 ? -1 : static_cast<long long>(s);
#endif
}

long long yukinary_sys_socket_bind(long long sock, long long address, long long port) {
    if (sock < 0 || address < 0 || port < 0 || port > 65535) return -1;
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = static_cast<uint32_t>(address);
    server.sin_port = htons(static_cast<uint16_t>(port));
    return ::bind(
#ifdef _WIN32
        static_cast<SOCKET>(sock),
#else
        static_cast<int>(sock),
#endif
        reinterpret_cast<sockaddr*>(&server), sizeof(server)) == 0 ? 0 : -1;
}

long long yukinary_sys_socket_listen(long long sock, long long backlog) {
    if (sock < 0 || backlog < 0 || backlog > 2147483647LL) return -1;
    return ::listen(
#ifdef _WIN32
        static_cast<SOCKET>(sock),
#else
        static_cast<int>(sock),
#endif
        static_cast<int>(backlog)) == 0 ? 0 : -1;
}

long long yukinary_sys_socket_accept(long long sock) {
    if (sock < 0) return -1;
#ifdef _WIN32
    SOCKET client = ::accept(static_cast<SOCKET>(sock), nullptr, nullptr);
    return client == INVALID_SOCKET ? -1 : static_cast<long long>(client);
#else
    int client = ::accept(static_cast<int>(sock), nullptr, nullptr);
    return client < 0 ? -1 : static_cast<long long>(client);
#endif
}

long long yukinary_sys_socket_close(long long sock) {
    if (sock < 0) return -1;
#ifdef _WIN32
    return ::closesocket(static_cast<SOCKET>(sock)) == 0 ? 0 : -1;
#else
    return ::close(static_cast<int>(sock)) == 0 ? 0 : -1;
#endif
}

long long yukinary_sys_socket_send(long long sock, char* buffer, long long length) {
    if (sock < 0 || !buffer || length < 0 || length > 2147483647LL) return -1;
#ifdef _WIN32
    return ::send(static_cast<SOCKET>(sock), buffer, static_cast<int>(length), 0);
#else
    return ::send(static_cast<int>(sock), buffer, static_cast<size_t>(length), 0);
#endif
}

long long yukinary_sys_socket_recv(long long sock, char* buffer, long long length) {
    if (sock < 0 || !buffer || length < 0 || length > 2147483647LL) return -1;
#ifdef _WIN32
    return ::recv(static_cast<SOCKET>(sock), buffer, static_cast<int>(length), 0);
#else
    return ::recv(static_cast<int>(sock), buffer, static_cast<size_t>(length), 0);
#endif
}

long long yukinary_sys_socket_connect(long long sock, long long address, long long port) {
    if (sock < 0 || address < 0 || port < 0 || port > 65535) return -1;
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = static_cast<uint32_t>(address);
    endpoint.sin_port = htons(static_cast<uint16_t>(port));
    return ::connect(
#ifdef _WIN32
        static_cast<SOCKET>(sock),
#else
        static_cast<int>(sock),
#endif
        reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) == 0 ? 0 : -1;
}

long long yukinary_sys_socket_set_timeout(long long sock, long long millis) {
    if (sock < 0 || millis < 0) return -1;
#ifdef _WIN32
    DWORD timeout = millis > 0xFFFFFFFFLL ? 0xFFFFFFFFu : static_cast<DWORD>(millis);
    int size = static_cast<int>(sizeof(timeout));
    int a = ::setsockopt(static_cast<SOCKET>(sock), SOL_SOCKET, SO_RCVTIMEO,
                         reinterpret_cast<const char*>(&timeout), size);
    int b = ::setsockopt(static_cast<SOCKET>(sock), SOL_SOCKET, SO_SNDTIMEO,
                         reinterpret_cast<const char*>(&timeout), size);
#else
    timeval timeout{};
    timeout.tv_sec = static_cast<time_t>(millis / 1000);
    timeout.tv_usec = static_cast<suseconds_t>((millis % 1000) * 1000);
    socklen_t size = static_cast<socklen_t>(sizeof(timeout));
    int a = ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_RCVTIMEO, &timeout, size);
    int b = ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_SNDTIMEO, &timeout, size);
#endif
    return (a == 0 && b == 0) ? 0 : -1;
}

long long yukinary_sys_socket_set_reuseaddr(long long sock) {
    if (sock < 0) return -1;
    int enabled = 1;
#ifdef _WIN32
    return ::setsockopt(static_cast<SOCKET>(sock), SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == 0 ? 0 : -1;
#else
    return ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_REUSEADDR,
                        &enabled, sizeof(enabled)) == 0 ? 0 : -1;
#endif
}

long long yukinary_sys_dns_resolve(char* host, char* address, long long address_capacity) {
    if (!host || !host[0] || !address || address_capacity < 4) return -1;
    in_addr parsed{};
    if (::inet_pton(AF_INET, host, &parsed) == 1) {
        memcpy(address, &parsed.s_addr, 4);
        return 0;
    }
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

// Make socket non-blocking
long long yukinary_sys_socket_set_nonblocking(long long sock) {
#ifdef _WIN32
    if (sock < 0) return -1;
    u_long mode = 1;
    return ioctlsocket(static_cast<SOCKET>(sock), FIONBIO, &mode) == 0 ? 0 : -1;
#else
    if (sock < 0) return -1;
    int flags = fcntl(static_cast<int>(sock), F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(static_cast<int>(sock), F_SETFL, flags | O_NONBLOCK) == 0 ? 0 : -1;
#endif
}

long long yukinary_sys_net_sendfile(long long out_sock, long long in_file, long long count) {
    if (out_sock < 0 || in_file < 0 || count < 0) return -1;
#ifdef _WIN32
    if (count > 0xFFFFFFFFLL) return -1;
    HANDLE file = reinterpret_cast<HANDLE>(static_cast<intptr_t>(in_file));
    return TransmitFile(static_cast<SOCKET>(out_sock), file, static_cast<DWORD>(count), 0, nullptr, nullptr, 0)
        ? count : -1;
#else
    off_t offset = 0;
    ssize_t sent = ::sendfile(static_cast<int>(out_sock), static_cast<int>(in_file), &offset,
                              static_cast<size_t>(count));
    return sent < 0 ? -1 : static_cast<long long>(sent);
#endif
}
} // extern "C"
