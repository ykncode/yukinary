#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#define M 64
#define K 512
#define N 512
#define ITERATIONS 100

static inline void rmsnorm(int32_t* x, int size) {
    int64_t sum_sq = 0;
    for (int i = 0; i < size; ++i) {
        int64_t v = x[i] / 256;
        sum_sq += v * v;
    }
    int64_t mean = sum_sq / size;
    if (mean <= 0) mean = 1;
    int32_t scale = (int32_t)(65536 / sqrt((double)mean));
    for (int i = 0; i < size; ++i) {
        x[i] = (int32_t)(((int64_t)x[i] * scale) / 65536);
    }
}

static inline void bitlinear_matmul(const uint8_t* W, const int32_t* X, int32_t* Out) {
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            int64_t sum = 0;
            const int x_row = i * K;
            for (int k = 0; k < K; ++k) {
                int32_t x_val = X[x_row + k];
                uint8_t w_val = W[k * N + j]; // 0: 0, 1: +1, 2: -1
                if (w_val == 1) sum += x_val;
                else if (w_val == 2) sum -= x_val;
            }
            Out[i * N + j] = (int32_t)sum;
        }
    }
}

int main() {
    uint8_t* W = (uint8_t*)malloc(K * N);
    int32_t* X = (int32_t*)malloc(M * K * sizeof(int32_t));
    int32_t* Out = (int32_t*)malloc(M * N * sizeof(int32_t));

    // Deterministic initialization
    for (int i = 0; i < K * N; ++i) W[i] = (uint8_t)((i * 7 + 3) % 3);
    for (int i = 0; i < M * K; ++i) X[i] = (int32_t)((i * 13 + 5) % 256 - 128);

    clock_t start = clock();
    for (int iter = 0; iter < ITERATIONS; ++iter) {
        for (int i = 0; i < M; ++i) {
            rmsnorm(&X[i * K], K);
        }
        bitlinear_matmul(W, X, Out);
        // Feed back small slice to avoid dead-code elimination
        X[0] = Out[0] % 256;
    }
    clock_t end = clock();

    double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
    double gops = (double)(ITERATIONS * (int64_t)M * K * N * 2) / (elapsed_sec * 1e9);

    printf("[C-GCC-O3] Elapsed: %.4f s | Throughput: %.2f GOP/s | Checksum: %d\n",
           elapsed_sec, gops, Out[0] + Out[M * N - 1]);

    free(W); free(X); free(Out);
    return 0;
}

