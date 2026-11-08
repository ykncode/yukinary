use std::time::Instant;

struct HttpRequest<'a> {
    method: &'a str,
    path: &'a str,
    content_length: usize,
    route_id: u8,
}

fn parse_http_request<'a>(raw: &'a str) -> HttpRequest<'a> {
    let mut parts = raw.split_whitespace();
    let method = parts.next().unwrap_or("");
    let path = parts.next().unwrap_or("");

    let route_id = if path.starts_with("/api/v1/orders") {
        1
    } else if path.starts_with("/api/v1/users") {
        2
    } else if path.starts_with("/healthz") {
        3
    } else {
        0
    };

    let mut content_length = 0;
    if let Some(pos) = raw.find("Content-Length:") {
        let rest = &raw[pos + 15..];
        let end = rest.find("\r\n").unwrap_or(rest.len());
        content_length = rest[..end].trim().parse().unwrap_or(0);
    }

    HttpRequest {
        method,
        path,
        content_length,
        route_id,
    }
}

fn main() {
    let count = 100000;
    let samples = [
        "POST /api/v1/orders/create HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 42\r\n\r\n{\"item_id\": 9942, \"qty\": 4, \"price\": 120}",
        "GET /api/v1/users/profile?id=512 HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 0\r\n\r\n",
        "GET /healthz HTTP/1.1\r\nHost: api.yukinary.internal\r\nUser-Agent: BenchAgent/2.0\r\nContent-Length: 12\r\n\r\n{\"status\":\"ok\"}",
    ];

    let mut req_pool = Vec::with_capacity(count);
    for i in 0..count {
        req_pool.push(samples[i % 3]);
    }

    let t1 = Instant::now();
    let mut total_bytes = 0usize;
    let mut orders_count = 0;
    let mut users_count = 0;
    let mut health_count = 0;

    for &raw in &req_pool {
        let req = parse_http_request(raw);
        total_bytes += req.content_length;
        match req.route_id {
            1 => orders_count += 1,
            2 => users_count += 1,
            3 => health_count += 1,
            _ => {}
        }
    }

    let elapsed = t1.elapsed();
    let elapsed_ms = elapsed.as_secs_f64() * 1000.0;
    let req_per_sec = (count as f64) / (elapsed_ms / 1000.0);

    println!("HTTP Rust (opt-level=3) Parser & Router Sonucu:");
    println!(
        "100,000 Istek | Toplam CL: {} | Orders: {} | Users: {} | Health: {}",
        total_bytes, orders_count, users_count, health_count
    );
    println!(
        "Sure: {:.2} ms | Throughput: {:.0} istek/saniye",
        elapsed_ms, req_per_sec
    );
}

