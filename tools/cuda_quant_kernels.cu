#include <cuda_fp16.h>
#include <stdint.h>

__device__ __forceinline__ int64_t q32_from_half(const unsigned char *p) {
    const uint16_t bits = static_cast<uint16_t>(p[0]) |
                          (static_cast<uint16_t>(p[1]) << 8);
    const __half value = __ushort_as_half(bits);
    return static_cast<int64_t>(__half2float(value) * 4294967296.0f);
}

__device__ __forceinline__ int64_t q4_k_weight(const unsigned char *block,
                                                int element) {
    const int group = element / 32;
    const int lane = element % 32;
    const int packed_index = (group % 4) * 32 + lane;
    const int packed = block[16 + packed_index];
    const int q = (group >= 4) ? packed >> 4 : packed & 15;
    int scale;
    int minimum;
    if (group < 4) {
        scale = block[4 + group] & 63;
        minimum = block[8 + group] & 63;
    } else {
        const int scale_hi = block[4 + group - 4];
        const int scale_min_hi = block[4 + group];
        const int scale_min_pair = block[4 + group + 4];
        scale = (scale_min_pair & 15) + ((scale_hi >> 6) * 16);
        minimum = (scale_min_pair >> 4) + ((scale_min_hi >> 6) * 16);
    }
    const int64_t d_q32 = q32_from_half(block);
    const int64_t dmin_q32 = q32_from_half(block + 2);
    return (d_q32 * (scale * q) - dmin_q32 * minimum) / 65536;
}

__device__ __forceinline__ int64_t q6_k_weight(const unsigned char *block,
                                                int element) {
    const int half = element / 128;
    const int local = element % 128;
    const int group = local / 32;
    const int lane = local % 32;
    const int ql_offset = half * 64 + (group % 2) * 32 + lane;
    const int qh_offset = 128 + half * 32 + lane;
    const int ql_byte = block[ql_offset];
    const int ql = group >= 2 ? ql_byte >> 4 : ql_byte & 15;
    const int qh_byte = block[qh_offset];
    const int shift = group * 2;
    const int q = ql + ((qh_byte >> shift) & 3) * 16 - 32;
    int scale = block[192 + half * 8 + group * 2 + lane / 16];
    if (scale >= 128) scale -= 256;
    const int64_t d_q32 = q32_from_half(block + 208);
    return (d_q32 * scale * q) / 65536;
}

extern "C" __global__ void ykn_q4_k_matvec(const unsigned char *weights,
                                const int64_t *activation,
                                int64_t *output, int K, int N) {
    const int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= N) return;
    const int block_count = K / 256;
    const unsigned char *row_weights = weights +
        static_cast<int64_t>(row) * block_count * 144;
    float sum = 0.0f;
    for (int block_index = 0; block_index < block_count; ++block_index) {
        const int64_t *x = activation + block_index * 256;
        int64_t max_signed = x[0];
        int64_t max_abs = max_signed < 0 ? -max_signed : max_signed;
        for (int i = 1; i < 256; ++i) {
            const int64_t ax = x[i] < 0 ? -x[i] : x[i];
            if (ax > max_abs) {
                max_abs = ax;
                max_signed = x[i];
            }
        }
        if (max_abs == 0) continue;
        const unsigned char *w = row_weights + block_index * 144;
        int32_t dot_scale = 0;
        int32_t dot_min = 0;
        for (int i = 0; i < 256; ++i) {
            const int group = i / 32;
            const int lane = i % 32;
            const int packed = w[16 + (group % 4) * 32 + lane];
            const int q = (group >= 4) ? packed >> 4 : packed & 15;
            int scale;
            int minimum;
            if (group < 4) {
                scale = w[4 + group] & 63;
                minimum = w[8 + group] & 63;
            } else {
                const int scale_hi = w[group];
                const int scale_min_hi = w[4 + group];
                const int pair = w[8 + group];
                scale = (pair & 15) + ((scale_hi >> 6) << 4);
                minimum = (pair >> 4) + ((scale_min_hi >> 6) << 4);
            }
            const float ratio = -127.0f * (static_cast<float>(x[i]) /
                                            static_cast<float>(max_signed));
            int q8 = __float2int_rn(ratio);
            q8 = q8 < -127 ? -127 : (q8 > 127 ? 127 : q8);
            dot_scale += scale * q * q8;
            dot_min += minimum * q8;
        }
        const __half d_bits = __ushort_as_half(static_cast<uint16_t>(w[0]) |
                                                (static_cast<uint16_t>(w[1]) << 8));
        const __half dm_bits = __ushort_as_half(static_cast<uint16_t>(w[2]) |
                                                 (static_cast<uint16_t>(w[3]) << 8));
        const float d = __half2float(d_bits);
        const float dmin = __half2float(dm_bits);
        const float d8 = -static_cast<float>(max_signed) / (127.0f * 65536.0f);
        sum += d8 * (d * static_cast<float>(dot_scale) -
                     dmin * static_cast<float>(dot_min));
    }
    output[row] = static_cast<int64_t>(sum * 65536.0f);
}

extern "C" __global__ void ykn_q6_k_matvec(const unsigned char *weights,
                                const int64_t *activation,
                                int64_t *output, int K, int N) {
    const int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= N) return;
    const int block_count = K / 256;
    const unsigned char *row_weights = weights +
        static_cast<int64_t>(row) * block_count * 210;
    float sum = 0.0f;
    for (int block_index = 0; block_index < block_count; ++block_index) {
        const int64_t *x = activation + block_index * 256;
        int64_t max_signed = x[0];
        int64_t max_abs = max_signed < 0 ? -max_signed : max_signed;
        for (int i = 1; i < 256; ++i) {
            const int64_t ax = x[i] < 0 ? -x[i] : x[i];
            if (ax > max_abs) {
                max_abs = ax;
                max_signed = x[i];
            }
        }
        if (max_abs == 0) continue;
        int32_t dot = 0;
        const unsigned char *w = row_weights + block_index * 210;
        for (int i = 0; i < 256; ++i) {
            const int half = i / 128;
            const int local = i % 128;
            const int group = local / 32;
            const int lane = local % 32;
            const int ql_offset = half * 64 + (group % 2) * 32 + lane;
            const int qh_offset = 128 + half * 32 + lane;
            const int ql_byte = w[ql_offset];
            const int ql = group >= 2 ? ql_byte >> 4 : ql_byte & 15;
            const int q = ql + (((w[qh_offset] >> (group * 2)) & 3) << 4) - 32;
            int scale = w[192 + half * 8 + group * 2 + lane / 16];
            if (scale >= 128) scale -= 256;
            const float ratio = -127.0f * (static_cast<float>(x[i]) /
                                            static_cast<float>(max_signed));
            int q8 = __float2int_rn(ratio);
            q8 = q8 < -127 ? -127 : (q8 > 127 ? 127 : q8);
            dot += scale * q * q8;
        }
        const __half d_bits = __ushort_as_half(static_cast<uint16_t>(w[208]) |
                                                (static_cast<uint16_t>(w[209]) << 8));
        const float d6 = __half2float(d_bits);
        const float d8 = -static_cast<float>(max_signed) / (127.0f * 65536.0f);
        sum += d6 * d8 * static_cast<float>(dot);
    }
    output[row] = static_cast<int64_t>(sum * 65536.0f);
}
