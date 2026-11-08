// Native host kernels for the YKN LLM runtime.
// This file is deliberately independent from the YKN source compiler so the
// generated executable can choose a scalar fallback when it is not linked.
#include "llm_simd.h"
#include "llm_iq2s_grid.inc"
#include <array>
#include <chrono>
#include <cstdint>
#if defined(_WIN32)
#  include <windows.h>
#  include <psapi.h>
#else
#  include <sys/resource.h>
#endif

#if defined(__AVX512F__)
#  include <immintrin.h>
#  define YKN_LLM_AVX512 1
#elif defined(__AVX2__)
#  include <immintrin.h>
#  define YKN_LLM_AVX2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#  include <arm_neon.h>
#  define YKN_LLM_NEON 1
#endif

extern "C" uint64_t ykn_llm_clock_ns() {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

extern "C" uint64_t ykn_llm_peak_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<uint64_t>(counters.PeakWorkingSetSize);
    }
    return 0;
#else
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#  if defined(__APPLE__)
    return static_cast<uint64_t>(usage.ru_maxrss);
#  else
    return static_cast<uint64_t>(usage.ru_maxrss) * 1024ULL;
#  endif
#endif
}

static inline int64_t ykn_f16_q16_decode(uint16_t bits) {
    int64_t sign = static_cast<int64_t>(bits) / 32768;
    int64_t exponent = (static_cast<int64_t>(bits) / 1024) % 32;
    int64_t mantissa = static_cast<int64_t>(bits) % 1024;
    if (exponent == 0) {
        int64_t value = mantissa / 256;
        return sign ? -value : value;
    }
    if (exponent == 31) return 0;
    int64_t value = (1024 + mantissa) * 64;
    int64_t shift = exponent - 15;
    if (shift > 0) {
        for (int64_t i = 0; i < shift; ++i) value *= 2;
    } else {
        for (int64_t i = 0; i < -shift; ++i) value /= 2;
    }
    return sign ? -value : value;
}

extern "C" int64_t ykn_llm_f16_dot_q16(const int64_t * activation,
                                        int64_t * qact,
                                        std::size_t K) {
    if (!activation || !qact || K == 0) return 0;
    int64_t amax = 0;
    for (std::size_t i = 0; i < K; ++i) {
        int64_t x = activation[i];
        if (x < 0) x = -x;
        if (x > amax) amax = x;
    }
    if (amax == 0) {
        for (std::size_t i = 0; i < K; ++i) qact[i] = 0;
        return 0;
    }
    int64_t amax_half = amax / 2;
    for (std::size_t i = 0; i < K; ++i) {
        int64_t x = activation[i];
        int64_t x_abs = x < 0 ? -x : x;
        int64_t sign = x < 0 ? -1 : 1;
        int64_t q = (x_abs * 127 + amax_half) / amax;
        if (q > 127) q = 127;
        qact[i] = q * sign;
    }
    return amax;
}

static inline int64_t ykn_q16_isqrt(int64_t value) {
    if (value <= 0) return 0;
    int64_t low = 0;
    int64_t high = 4294967295LL;
    if (value < high) high = value + 1;
    while (low + 1 < high) {
        int64_t mid = (low + high) / 2;
        if (mid <= value / mid) low = mid;
        else high = mid;
    }
    return low;
}

static inline int64_t ykn_rmsnorm_impl(const int64_t * girdi,
                                       const int64_t * agirlik,
                                       int64_t * cikti,
                                       int64_t eleman_sayisi,
                                       int64_t epsilon_q16) {
    if (!girdi || !cikti || eleman_sayisi <= 0) return 0;
    int64_t maksimum = 0;
    for (int64_t i = 0; i < eleman_sayisi; ++i) {
        int64_t mutlak = girdi[i] < 0 ? -girdi[i] : girdi[i];
        if (mutlak > maksimum) maksimum = mutlak;
    }
    if (maksimum == 0) {
        for (int64_t i = 0; i < eleman_sayisi; ++i) cikti[i] = 0;
        return 1;
    }
    int64_t olcek_boleni = 1;
    if (maksimum > 1000000000LL) {
        olcek_boleni = (maksimum + 65535) / 65536;
    }
    int64_t epsilon_normalize_q16 = 0;
    if (maksimum <= 3037000499LL) {
        int64_t eps_num = epsilon_q16 * 65536LL * 65536LL;
        epsilon_normalize_q16 = eps_num / (maksimum * maksimum);
    }
    int64_t kare_toplami = 0;
    for (int64_t i = 0; i < eleman_sayisi; ++i) {
        int64_t normalize = (olcek_boleni == 1) ? (girdi[i] * 65536LL) / maksimum : (girdi[i] / olcek_boleni);
        cikti[i] = normalize;
        kare_toplami += (normalize * normalize) / 65536LL;
    }
    int64_t ortalama = kare_toplami / eleman_sayisi;
    int64_t rms = ykn_q16_isqrt((ortalama + epsilon_normalize_q16) * 65536LL);
    if (rms <= 0) return 0;
    for (int64_t i = 0; i < eleman_sayisi; ++i) {
        int64_t normalize2 = cikti[i];
        int64_t w = agirlik ? agirlik[i] : 65536LL;
        cikti[i] = (normalize2 * w) / rms;
    }
    return 1;
}

extern "C" int64_t ykn_llm_q1_0_dot_q16(const int64_t * arg1,
                                        const int64_t * arg2,
                                        int64_t arg3) {
    if (!arg1 || !arg2) return 0;
    if (arg3 > 10000000LL) {
        const int64_t * pack = reinterpret_cast<const int64_t *>(arg3);
        const int64_t * agirlik = reinterpret_cast<const int64_t *>(pack[0]);
        int64_t eleman_sayisi = pack[1];
        int64_t epsilon_q16 = pack[2];
        int64_t * cikti = const_cast<int64_t *>(arg2);
        return ykn_rmsnorm_impl(arg1, agirlik, cikti, eleman_sayisi, epsilon_q16);
    }
    const uint8_t * packed = reinterpret_cast<const uint8_t *>(arg1);
    const int64_t * activation = arg2;
    std::size_t k = static_cast<std::size_t>(arg3);
    if (k == 0 || (k % 128) != 0) return 0;
    __int128 total = 0;
    for (std::size_t block = 0; block < k; block += 128) {
        const uint8_t * row = packed + (block / 128) * 18;
        uint16_t scale_bits = static_cast<uint16_t>(row[0]) |
                              (static_cast<uint16_t>(row[1]) << 8);
        int64_t scale = ykn_f16_q16_decode(scale_bits);
        for (std::size_t i = 0; i < 128; ++i) {
            uint8_t sign_byte = row[2 + i / 8];
            int64_t weight = ((sign_byte / (1ULL << (i % 8))) % 2) ? scale : -scale;
            total += (static_cast<__int128>(activation[block + i]) * weight) / 65536;
        }
    }
    if (total > static_cast<__int128>(INT64_MAX)) return INT64_MAX;
    if (total < static_cast<__int128>(INT64_MIN)) return INT64_MIN;
    return static_cast<int64_t>(total);
}

extern "C" int ykn_llm_simd_backend() {
#if defined(YKN_LLM_AVX512)
    return 3;
#elif defined(YKN_LLM_AVX2)
    return 2;
#elif defined(YKN_LLM_NEON)
    return 1;
#else
    return 0;
#endif
}

static inline int64_t ykn_clamp_i64(__int128 value) {
    if (value > static_cast<__int128>(INT64_MAX)) return INT64_MAX;
    if (value < static_cast<__int128>(INT64_MIN)) return INT64_MIN;
    return static_cast<int64_t>(value);
}

static inline int64_t ykn_trunc_div_i128(__int128 value, __int128 divisor) {
    if (value >= 0) return ykn_clamp_i64(value / divisor);
    return ykn_clamp_i64(-((-value) / divisor));
}

extern "C" int64_t ykn_llm_dot_q16(const int64_t * a, const int64_t * b, std::size_t n) {
    if (!a || !b) return 0;
#if defined(YKN_LLM_AVX512) || defined(YKN_LLM_AVX2)
    __m256i acc0 = _mm256_setzero_si256();
    __m256i acc1 = _mm256_setzero_si256();
    std::size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256i av0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + i));
        __m256i bv0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b + i));
        __m256i av1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + i + 4));
        __m256i bv1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b + i + 4));
        acc0 = _mm256_add_epi64(acc0, _mm256_mul_epi32(av0, bv0));
        acc1 = _mm256_add_epi64(acc1, _mm256_mul_epi32(av1, bv1));
    }
    for (; i + 4 <= n; i += 4) {
        __m256i av = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + i));
        __m256i bv = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b + i));
        acc0 = _mm256_add_epi64(acc0, _mm256_mul_epi32(av, bv));
    }
    acc0 = _mm256_add_epi64(acc0, acc1);
    alignas(32) int64_t res[4];
    _mm256_store_si256(reinterpret_cast<__m256i *>(res), acc0);
    int64_t sum = res[0] + res[1] + res[2] + res[3];
    for (; i < n; ++i) sum += a[i] * b[i];
    return sum / 65536;
#else
    __int128 sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += static_cast<__int128>(a[i]) * static_cast<__int128>(b[i]);
    }
    return ykn_trunc_div_i128(sum, 65536);
#endif
}

static inline int64_t ykn_exp_neg_q16(int64_t x) {
    if (x >= 0) return 65536;
    if (x < -8 * 65536) return 0;
    const int64_t z = x / 8;
    const int64_t z2 = (z * z) / 65536;
    const int64_t z3 = (z2 * z) / 65536;
    const int64_t z4 = (z3 * z) / 65536;
    const int64_t z5 = (z4 * z) / 65536;
    int64_t term = 65536 + z + z2 / 2 + z3 / 6 + z4 / 24 + z5 / 120;
    if (term < 0) term = 0;
    int64_t result = term;
    for (int i = 0; i < 7; ++i) result = (result * term) / 65536;
    return result;
}

static inline int64_t ykn_silu_scalar(int64_t x) {
    if (x >= 8 * 65536) return x;
    if (x <= -8 * 65536) return 0;
    const int64_t exp_term = ykn_exp_neg_q16(x >= 0 ? -x : x);
    const int64_t sigmoid = x >= 0
        ? (65536LL * 65536LL) / (65536 + exp_term)
        : (exp_term * 65536) / (65536 + exp_term);
    return ykn_trunc_div_i128(static_cast<__int128>(x) * sigmoid, 65536);
}

static int32_t g_silu_lut[1048576];
static bool g_silu_lut_ready = false;

static void init_silu_lut() {
    for (int32_t i = -524288; i < 524288; ++i) {
        g_silu_lut[i + 524288] = static_cast<int32_t>(ykn_silu_scalar(i));
    }
    g_silu_lut_ready = true;
}

static inline int64_t ykn_silu_fast(int64_t x) {
    if (x >= 524288) return x;
    if (x <= -524288) return 0;
    return g_silu_lut[x + 524288];
}

extern "C" void ykn_llm_silu_q16(const int64_t * gate, const int64_t * up, int64_t * output, std::size_t n) {
    if (!gate || !output || n == 0) return;
    if (!g_silu_lut_ready) init_silu_lut();
    if (up) {
        for (std::size_t i = 0; i < n; ++i) {
            int64_t silu_val = ykn_silu_fast(gate[i]);
            __int128 prod = static_cast<__int128>(silu_val) * up[i];
            output[i] = prod >= 0 ? static_cast<int64_t>(prod >> 16) : static_cast<int64_t>(-((-prod) >> 16));
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) output[i] = ykn_silu_fast(gate[i]);
    }
}

namespace {

static std::array<uint8_t, 1024 * 8> g_iq2s_grid{};
static std::array<uint8_t, 1024 * 2> g_iq2s_grid_packed{};
static bool g_iq2s_grid_ready = false;

static int base64_value(char value) {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

static void init_iq2s_grid() {
    std::size_t output = 0;
    int accumulator = 0;
    int bits = 0;
    for (const char *cursor = YKN_IQ2S_GRID_BASE64; *cursor && output < 2048; ++cursor) {
        if (*cursor == '=') break;
        const int value = base64_value(*cursor);
        if (value < 0) continue;
        accumulator = (accumulator << 6) | value;
        bits += 6;
        while (bits >= 8 && output < 2048) {
            bits -= 8;
            const uint8_t byte = static_cast<uint8_t>((accumulator >> bits) & 0xFF);
            g_iq2s_grid_packed[output++] = byte;
        }
    }
    // The packed table stores two bits per lane: 0->8, 1->25, 2->43.
    for (std::size_t index = 0; index < 1024; ++index) {
        const uint16_t packed = static_cast<uint16_t>(g_iq2s_grid_packed[index * 2]) |
            (static_cast<uint16_t>(g_iq2s_grid_packed[index * 2 + 1]) << 8);
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const uint8_t code = static_cast<uint8_t>((packed >> (lane * 2)) & 3);
            g_iq2s_grid[index * 8 + lane] = code == 0 ? 8 : (code == 1 ? 25 : 43);
        }
    }
}

static int64_t f16_q16(uint8_t lo, uint8_t hi) {
    const int bits = static_cast<int>(lo) + static_cast<int>(hi) * 256;
    const int sign = bits / 32768;
    const int exponent = (bits / 1024) % 32;
    const int mantissa = bits % 1024;
    if (exponent == 0 || exponent == 31) return 0;
    int64_t result = static_cast<int64_t>(1024 + mantissa) * 64;
    const int difference = exponent - 15;
    if (difference > 0) result *= (int64_t{1} << difference);
    else if (difference < 0) result /= (int64_t{1} << (-difference));
    return sign ? -result : result;
}

static int64_t iq2s_value_q16(const uint8_t * block, std::size_t element) {
    const std::size_t group = element / 32;
    const std::size_t inner = element % 32;
    const std::size_t lane_group = inner / 8;
    const std::size_t lane = inner % 8;
    const int q = block[2 + group * 4 + lane_group];
    const int high = block[66 + group];
    const int grid_high = (high >> (2 * lane_group)) & 3;
    const int grid_index = q + grid_high * 256;
    const int signs = block[34 + group * 4 + lane_group];
    const int nibble = (block[74 + group] >> ((inner / 16) * 4)) & 15;
    const int64_t scale = (f16_q16(block[0], block[1]) * (1 + 2 * nibble)) / 8;
    // IQ2_S grid entries 8/25/43 are Q5.0 codes; GGML's real
    // dequantization applies the missing /32 normalization here.
    int64_t result = (scale * g_iq2s_grid[static_cast<std::size_t>(grid_index) * 8 + lane]) / 32;
    if (((signs >> lane) & 1) != 0) result = -result;
    return result;
}

} // namespace

static inline int64_t ykn_attention_v_accum_impl(const int64_t * skorlar,
                                                  const int64_t * deger_cache,
                                                  int64_t * cikti,
                                                  const int64_t * params) {
    if (!skorlar || !deger_cache || !cikti || !params) return 0;
    int64_t window_count = params[0];
    int64_t start_token = params[1];
    int64_t end_token = params[2];
    int64_t kv_kafa_sayisi = params[3];
    int64_t kv_kafa = params[4];
    int64_t kafa_boyutu = params[5];
    if (window_count <= 0 || kafa_boyutu <= 0) return 0;

    int64_t en_buyuk = skorlar[0];
    for (int64_t i = 1; i < window_count; ++i) {
        if (skorlar[i] > en_buyuk) en_buyuk = skorlar[i];
    }
    int64_t agirliklar[512];
    int64_t toplam = 0;
    for (int64_t i = 0; i < window_count; ++i) {
        agirliklar[i] = ykn_exp_neg_q16(skorlar[i] - en_buyuk);
        toplam += agirliklar[i];
    }
    if (toplam <= 0) return 0;
    for (int64_t i = 0; i < window_count; ++i) {
        agirliklar[i] = (agirliklar[i] * 65536LL) / toplam;
    }

    for (int64_t i = 0; i < kafa_boyutu; ++i) cikti[i] = 0;

    int64_t slot = 0;
    for (int64_t tok = start_token; tok <= end_token; ++tok, ++slot) {
        int64_t taban = (tok * kv_kafa_sayisi + kv_kafa) * kafa_boyutu;
        int64_t w_slot = agirliklar[slot];
        const int64_t * v_ptr = deger_cache + taban;
        for (int64_t i = 0; i < kafa_boyutu; ++i) {
            cikti[i] += w_slot * v_ptr[i];
        }
    }
    for (int64_t i = 0; i < kafa_boyutu; ++i) {
        cikti[i] /= 65536LL;
    }
    return 1;
}

extern "C" int64_t ykn_llm_iq2s_dot_q16(const int64_t * arg1,
                                        const int64_t * arg2,
                                        int64_t arg3,
                                        int64_t arg4) {
    if (arg4 > 10000000LL) {
        const int64_t * params = reinterpret_cast<const int64_t *>(arg4);
        int64_t * cikti = reinterpret_cast<int64_t *>(arg3);
        return ykn_attention_v_accum_impl(arg1, arg2, cikti, params);
    }
    if (!arg1 || !arg2) return 0;
    if (!g_iq2s_grid_ready) {
        init_iq2s_grid();
        g_iq2s_grid_ready = true;
    }
    const uint8_t * packed = reinterpret_cast<const uint8_t *>(arg1);
    const int64_t * activation = arg2;
    std::size_t k = static_cast<std::size_t>(arg3);
    std::size_t row_offset = static_cast<std::size_t>(arg4);
    if (k == 0) return 0;
    const uint8_t * row = packed + row_offset;
    __int128 total = 0;
    for (std::size_t element = 0; element < k; ++element) {
        const uint8_t * block = row + (element / 256) * 82;
        const int64_t weight = iq2s_value_q16(block, element % 256);
        total += (static_cast<__int128>(activation[element]) * weight) / 65536;
    }
    return ykn_clamp_i64(total);
}
