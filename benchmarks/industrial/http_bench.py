import time

count = 100000
samples = [
    "POST /api/v1/orders/create HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 42\r\n\r\n{\"item_id\": 9942, \"qty\": 4, \"price\": 120}",
    "GET /api/v1/users/profile?id=512 HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 0\r\n\r\n",
    "GET /healthz HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 12\r\n\r\n{\"status\":\"ok\"}"
]

req_pool = [samples[i % 3] for i in range(count)]

t1 = time.perf_counter()
total_bytes = 0
orders_count = 0
users_count = 0
health_count = 0

for raw in req_pool:
    parts = raw.split(" ", 2)
    path = parts[1]
    if path.startswith("/api/v1/orders"):
        orders_count += 1
    elif path.startswith("/api/v1/users"):
        users_count += 1
    elif path.startswith("/healthz"):
        health_count += 1

    pos = raw.find("Content-Length:")
    if pos != -1:
        end = raw.find("\r\n", pos)
        cl_str = raw[pos + 15:end].strip()
        total_bytes += int(cl_str)

t2 = time.perf_counter()
elapsed_ms = (t2 - t1) * 1000.0
req_per_sec = count / (elapsed_ms / 1000.0)

print("HTTP Python 3.13 Parser & Router Sonucu:")
print(f"100,000 Istek | Toplam CL: {total_bytes} | Orders: {orders_count} | Users: {users_count} | Health: {health_count}")
print(f"Sure: {elapsed_ms:.2f} ms | Throughput: {req_per_sec:.0f} istek/saniye")

