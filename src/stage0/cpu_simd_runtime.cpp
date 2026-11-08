// ============================================================
// Yukinary CPU SIMD (AVX2/AVX-512) Fallback Runtime
// ============================================================

#include <immintrin.h>
#include <stdint.h>

extern "C" {

// 1. AVX2 MatMul Fallback
void yukinary_simd_matmul(float* A, float* B, float* C, int M, int N, int K) {
    // Simple block AVX2 multiplication
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; j += 8) { // 8 floats per AVX2 register
            __m256 sum = _mm256_setzero_ps();
            for (int k = 0; k < K; ++k) {
                __m256 a_val = _mm256_broadcast_ss(&A[i * K + k]);
                __m256 b_val = _mm256_loadu_ps(&B[k * N + j]);
                sum = _mm256_fmadd_ps(a_val, b_val, sum); // Fused Multiply-Add
            }
            _mm256_storeu_ps(&C[i * N + j], sum);
        }
    }
}

// 2. AVX2 1.58-bit BitNet MatMul
void yukinary_simd_bitnet_matmul(float* A, int8_t* B_ternary, float* C, int M, int N, int K) {
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                int8_t weight = B_ternary[k * N + j];
                // BitNet logic: only add or sub
                if (weight == 1) sum += A[i * K + k];
                else if (weight == -1) sum -= A[i * K + k];
            }
            C[i * N + j] = sum;
        }
    }
}

} // extern "C"
