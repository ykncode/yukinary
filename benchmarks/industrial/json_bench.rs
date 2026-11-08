use std::time::Instant;

struct UserRecord {
    id: i64,
    score: i64,
    active: bool,
}

fn parse_user_json(json: &str) -> UserRecord {
    let mut id = 0i64;
    let mut score = 0i64;
    let mut active = false;

    if let Some(pos) = json.find("\"id\":") {
        let rest = &json[pos + 5..];
        let end = rest.find(',').unwrap_or(rest.len());
        id = rest[..end].trim().parse().unwrap_or(0);
    }
    if let Some(pos) = json.find("\"score\":") {
        let rest = &json[pos + 8..];
        let end = rest.find(',').unwrap_or(rest.find('}').unwrap_or(rest.len()));
        score = rest[..end].trim().parse().unwrap_or(0);
    }
    if let Some(pos) = json.find("\"active\":") {
        let rest = &json[pos + 9..];
        if rest.starts_with("true") {
            active = true;
        }
    }

    UserRecord { id, score, active }
}

fn main() {
    let count = 50000;
    let mut json_strings = Vec::with_capacity(count);
    for i in 0..count {
        let id = (i * 17) % 100000;
        let score = (i * 23) % 1000;
        let active_str = if i % 2 == 0 { "true" } else { "false" };
        json_strings.push(format!(
            "{{\"id\":{},\"user\":\"usr_{}\",\"score\":{},\"active\":{}}}",
            id,
            i % 500,
            score,
            active_str
        ));
    }

    let t1 = Instant::now();
    let mut sum_score = 0i64;
    let mut active_count = 0;

    for s in &json_strings {
        let rec = parse_user_json(s);
        sum_score += rec.score;
        if rec.active {
            active_count += 1;
        }
    }
    let elapsed = t1.elapsed();
    let elapsed_ms = elapsed.as_secs_f64() * 1000.0;
    let msgs_per_sec = (count as f64) / (elapsed_ms / 1000.0);

    println!("JSON Rust (opt-level=3) Sonucu:");
    println!(
        "50,000 JSON Kaydi | Toplam Skor: {} | Aktif Sayisi: {}",
        sum_score, active_count
    );
    println!(
        "Sure: {:.2} ms | Throughput: {:.0} json/saniye",
        elapsed_ms, msgs_per_sec
    );
}

