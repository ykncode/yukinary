#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    char method[8];
    char path[64];
    char host[64];
    int content_length;
    int route_id; // 1: orders, 2: users, 3: health
} HttpRequest;

int parse_http_request(const char *raw, HttpRequest *req) {
    const char *p = raw;
    req->route_id = 0;
    req->content_length = 0;

    // Metod
    int m_len = 0;
    while (*p && *p != ' ' && m_len < 7) {
        req->method[m_len++] = *p++;
    }
    req->method[m_len] = '\0';
    if (*p == ' ') p++;

    // Path
    int p_len = 0;
    while (*p && *p != ' ' && p_len < 63) {
        req->path[p_len++] = *p++;
    }
    req->path[p_len] = '\0';

    // Rotalama
    if (strncmp(req->path, "/api/v1/orders", 14) == 0) req->route_id = 1;
    else if (strncmp(req->path, "/api/v1/users", 13) == 0) req->route_id = 2;
    else if (strncmp(req->path, "/healthz", 8) == 0) req->route_id = 3;

    // Headers
    const char *cl = strstr(raw, "Content-Length:");
    if (cl) {
        cl += 15;
        while (*cl == ' ') cl++;
        req->content_length = atoi(cl);
    }
    return 1;
}

int main() {
    int count = 100000;
    const char *sample_requests[3] = {
        "POST /api/v1/orders/create HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 42\r\n\r\n{\"item_id\": 9942, \"qty\": 4, \"price\": 120}",
        "GET /api/v1/users/profile?id=512 HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 0\r\n\r\n",
        "GET /healthz HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 12\r\n\r\n{\"status\":\"ok\"}"
    };

    char **req_pool = (char **)malloc(count * sizeof(char *));
    for (int i = 0; i < count; ++i) {
        req_pool[i] = (char *)sample_requests[i % 3];
    }

    LARGE_INTEGER freq, t1, t2;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);

    long long total_bytes = 0;
    int orders_count = 0;
    int users_count = 0;
    int health_count = 0;
    HttpRequest parsed;

    for (int i = 0; i < count; ++i) {
        parse_http_request(req_pool[i], &parsed);
        total_bytes += parsed.content_length;
        if (parsed.route_id == 1) orders_count++;
        else if (parsed.route_id == 2) users_count++;
        else if (parsed.route_id == 3) health_count++;
    }

    QueryPerformanceCounter(&t2);
    double elapsed_ms = (double)(t2.QuadPart - t1.QuadPart) * 1000.0 / (double)freq.QuadPart;
    double req_per_sec = (double)count / (elapsed_ms / 1000.0);

    printf("HTTP C (-O3) Parser & Router Sonucu:\n");
    printf("100,000 Istek | Toplam CL: %lld | Orders: %d | Users: %d | Health: %d\n", total_bytes, orders_count, users_count, health_count);
    printf("Sure: %.2f ms | Throughput: %.0f istek/saniye\n", elapsed_ms, req_per_sec);

    free(req_pool);
    return 0;
}

