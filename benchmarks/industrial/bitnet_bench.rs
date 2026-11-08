use std::time::Instant;

const M: usize = 64;
const K: usize = 512;
const N: usize = 512;
const ITERATIONS: usize = 100;

#[inline(always)]
fn rmsnorm(x: &mut [i32]) {
    let mut sum_sq: i64 = 0;
    for &val in x.iter() {
        let v = (val / 256) as i64;
        sum_sq += v * v;
    }
    let mean = sum_sq / (x.len() as i64);
    let mean = if mean <= 0 { 1 } else { mean };
    let scale = (65536.0 / (mean as f64).sqrt()) as i32;
    for val in x.iter_mut() {
        *val = (((*val as i64) * (scale as i64)) / 65536) as i32;
    }
}

#[inline(always)]
fn bitlinear_matmul(w: &[u8], x: &[i32], out: &mut [i32]) {
    for i in 0..M {
        let x_row = i * K;
        for j in 0..N {
            let mut sum: i64 = 0;
            for k in 0..K {
                let x_val = x[x_row + k];
                let w_val = w[k * N + j];
                if w_val == 1 {
                    sum += x_val as i64;
                } else if w_val == 2 {
                    sum -= x_val as i64;
                }
            }
            out[i * N + j] = sum as i32;
        }
    }
}

fn main() {
    let mut w = vec![0u8; K * N];
    let mut x = vec![0i32; M * K];
    let mut out = vec![0i32; M * N];

    for i in 0..K * N {
        w[i] = ((i * 7 + 3) % 3) as u8;
    }
    for i in 0..M * K {
        x[i] = ((i * 13 + 5) % 256) as i32 - 128;
    }

    let start = Instant::now();
    for _iter in 0..ITERATIONS {
        for i in 0..M {
            rmsnorm(&mut x[i * K..(i + 1) * K]);
        }
        bitlinear_matmul(&w, &x, &mut out);
        x[0] = out[0] % 256;
    }
    let elapsed = start.elapsed();
    let elapsed_sec = elapsed.as_secs_f64();
    let gops = (ITERATIONS as f64 * (M * K * N * 2) as f64) / (elapsed_sec * 1e9);

    println!(
        "[RUST-OPT3] Elapsed: {:.4} s | Throughput: {:.2} GOP/s | Checksum: {}",
        elapsed_sec,
        gops,
        out[0] + out[M * N - 1]
    );
}

