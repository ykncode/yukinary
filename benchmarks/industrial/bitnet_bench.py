import time
import math
import array

M = 64
K = 512
N = 512
ITERATIONS = 5  # Python is ~50-100x slower in pure loops; run 5 iterations and extrapolate to 100 for fairness

def rmsnorm(x, offset, size):
    sum_sq = 0
    for i in range(size):
        v = x[offset + i] // 256
        sum_sq += v * v
    mean = sum_sq // size
    if mean <= 0:
        mean = 1
    scale = int(65536 / math.sqrt(mean))
    for i in range(size):
        x[offset + i] = (x[offset + i] * scale) // 65536

def bitlinear_matmul(W, X, Out):
    for i in range(M):
        x_row = i * K
        out_row = i * N
        for j in range(N):
            total = 0
            for k in range(K):
                w_val = W[k * N + j]
                if w_val == 1:
                    total += X[x_row + k]
                elif w_val == 2:
                    total -= X[x_row + k]
            Out[out_row + j] = total

def main():
    W = bytearray(((i * 7 + 3) % 3) for i in range(K * N))
    X = array.array('i', [(((i * 13 + 5) % 256) - 128) for i in range(M * K)])
    Out = array.array('i', [0] * (M * N))

    t0 = time.perf_counter()
    for _iter in range(ITERATIONS):
        for i in range(M):
            rmsnorm(X, i * K, K)
        bitlinear_matmul(W, X, Out)
        X[0] = Out[0] % 256
    t1 = time.perf_counter()

    elapsed_actual = t1 - t0
    # Normalize to 100 iterations
    elapsed_sec = elapsed_actual * (100 / ITERATIONS)
    gops = (100 * M * K * N * 2) / (elapsed_sec * 1e9)

    print(f"[PYTHON-3.13] Extrapolated 100-iter Elapsed: {elapsed_sec:.4f} s (Actual 5-iter: {elapsed_actual:.4f} s) | Throughput: {gops:.4f} GOP/s | Checksum: {Out[0] + Out[M * N - 1]}")

if __name__ == "__main__":
    main()
