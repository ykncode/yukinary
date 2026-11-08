// =============================================================================
// arm_runtime.cpp  -  Yukinary ARM NEON Backend Runtime Kütüphanesi
// generator_arm_hir.py tarafından üretilen .cpp dosyalarına #include edilir.
// =============================================================================
// Bu dosya; ykn_neon_* ön ekli fonksiyonları tanımlar — generator_arm_hir.py
// tarafından üretilen çağrılarla birebir eşleşir.
// =============================================================================
#define YKN_ARM_BACKEND
#include "libykn_runtime.h"
#include <cmath>
#include <algorithm>

// ARM NEON: mevcut platforma göre koşullu olarak etkinleştirilir.
// Windows/x86'da arm_neon.h yoktur; scalar float32x4_t shim kullanılır.
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#  include <arm_neon.h>
#  define YKN_HAS_NEON 1
#else
#  define YKN_HAS_NEON 0
// GCC/Clang/MSVC taşınabilir scalar shim — brace-init / vector_size yok.
// (Eski __attribute__((vector_size(16))) + return {v,v,v,v} GCC/Clang'da kırılıyordu.)
struct float32x4_t {
    float v[4];
    float& operator[](int i) { return v[i]; }
    float operator[](int i) const { return v[i]; }
};
inline float32x4_t vdupq_n_f32(float x) {
    float32x4_t r; r.v[0] = r.v[1] = r.v[2] = r.v[3] = x; return r;
}
inline float32x4_t vld1q_f32(const float* p) {
    float32x4_t r; r.v[0]=p[0]; r.v[1]=p[1]; r.v[2]=p[2]; r.v[3]=p[3]; return r;
}
inline void vst1q_f32(float* p, float32x4_t a) {
    p[0]=a.v[0]; p[1]=a.v[1]; p[2]=a.v[2]; p[3]=a.v[3];
}
inline float32x4_t vmaxq_f32(float32x4_t a, float32x4_t b) {
    float32x4_t r;
    r.v[0]=std::max(a.v[0],b.v[0]); r.v[1]=std::max(a.v[1],b.v[1]);
    r.v[2]=std::max(a.v[2],b.v[2]); r.v[3]=std::max(a.v[3],b.v[3]);
    return r;
}
inline float32x4_t vaddq_f32(float32x4_t a, float32x4_t b) {
    float32x4_t r;
    r.v[0]=a.v[0]+b.v[0]; r.v[1]=a.v[1]+b.v[1];
    r.v[2]=a.v[2]+b.v[2]; r.v[3]=a.v[3]+b.v[3];
    return r;
}
inline float32x4_t vmulq_f32(float32x4_t a, float32x4_t b) {
    float32x4_t r;
    r.v[0]=a.v[0]*b.v[0]; r.v[1]=a.v[1]*b.v[1];
    r.v[2]=a.v[2]*b.v[2]; r.v[3]=a.v[3]*b.v[3];
    return r;
}
inline float32x4_t vmlaq_f32(float32x4_t c, float32x4_t a, float32x4_t b) {
    float32x4_t r;
    r.v[0]=c.v[0]+a.v[0]*b.v[0]; r.v[1]=c.v[1]+a.v[1]*b.v[1];
    r.v[2]=c.v[2]+a.v[2]*b.v[2]; r.v[3]=c.v[3]+a.v[3]*b.v[3];
    return r;
}
inline float vaddvq_f32(float32x4_t a) {
    return a.v[0]+a.v[1]+a.v[2]+a.v[3];
}
#endif

// =============================================================================
// --- BELLEK ARENA (CPU/ARM tarafı — malloc tabanlı) ---
// libykn_runtime.h'de extern olarak bildirilen arena fonksiyonları burada
// gerçeklenir. CUDA sürümünden farklı olarak cudaMalloc yerine malloc kullanır.
// =============================================================================

YknArena g_gpu_arena = {nullptr, 0, 0};  // "GPU" arena — ARM'da RAM kullanır

void ykn_arena_init(size_t size_in_bytes) {
    if (g_gpu_arena.base_ptr) {
        // Zaten başlatılmış — yeniden başlatma
        free(g_gpu_arena.base_ptr);
    }
    g_gpu_arena.base_ptr = static_cast<uint8_t*>(malloc(size_in_bytes));
    if (!g_gpu_arena.base_ptr) {
        fprintf(stderr, "[YKN ARM] Arena bellek tahsisi basarisiz: %zu bayt\n", size_in_bytes);
        exit(EXIT_FAILURE);
    }
    g_gpu_arena.total_size    = size_in_bytes;
    g_gpu_arena.current_offset = 0;
}

void* ykn_arena_alloc(size_t size_in_bytes) {
    // 16-byte hizalama (NEON SIMD için)
    size_t aligned = (size_in_bytes + 15) & ~(size_t)15;
    if (g_gpu_arena.current_offset + aligned > g_gpu_arena.total_size) {
        fprintf(stderr, "[YKN ARM] Arena doldu! Istenen: %zu, Kalan: %zu\n",
                aligned, g_gpu_arena.total_size - g_gpu_arena.current_offset);
        exit(EXIT_FAILURE);
    }
    void* ptr = g_gpu_arena.base_ptr + g_gpu_arena.current_offset;
    g_gpu_arena.current_offset += aligned;
    return ptr;
}

void ykn_arena_reset() {
    g_gpu_arena.current_offset = 0;
}

void ykn_arena_free() {
    if (g_gpu_arena.base_ptr) {
        free(g_gpu_arena.base_ptr);
        g_gpu_arena.base_ptr = nullptr;
    }
    g_gpu_arena.total_size    = 0;
    g_gpu_arena.current_offset = 0;
}

// =============================================================================
// --- MATRIS / VEKTÖR OLUŞTURUCULAR ---
// =============================================================================

template <typename T = float>
inline
Matris<T> create_matris(int w, int h) {
    Matris<float> m; m.genislik = w; m.yukseklik = h;
    m.data = std::shared_ptr<float>(new float[(size_t)w * h](), [](float* p){ delete[] p; });
    return m;
}

template <typename T = float>
inline
Vektor<T> create_vektor(int len) {
    Vektor<float> v; v.uzunluk = len;
    v.data = std::shared_ptr<float>(new float[len](), [](float* p){ delete[] p; });
    return v;
}

// =============================================================================
// --- ARM NEON OPERATÖR AŞIRI YÜKLEMELERİ ---
// =============================================================================

template <typename T>
inline
Matris<T> operator*(const Matris<T>& A, const Matris<T>& B) {
    Matris<T> C = create_matris<T>(B.genislik, A.yukseklik);
    for (int i = 0; i < A.yukseklik; ++i) {
        for (int j = 0; j < B.genislik; ++j) {
            float32x4_t acc = vdupq_n_f32(0.0f);
            int k = 0;
            for (; k <= A.genislik - 4; k += 4) {
                float32x4_t a = vld1q_f32(&A.data.get()[i * A.genislik + k]);
                float32x4_t b = vld1q_f32(&B.data.get()[k * B.genislik + j]);
                acc = vmlaq_f32(acc, a, b);
            }
            float sum = vaddvq_f32(acc);
            for (; k < A.genislik; ++k)
                sum += A.data.get()[i * A.genislik + k] * B.data.get()[k * B.genislik + j];
            C.data.get()[i * C.genislik + j] = sum;
        }
    }
    return C;
}

template <typename T>
inline
Vektor<T> operator*(const Matris<T>& A, const Vektor<T>& B) {
    Vektor<T> C = create_vektor<T>(A.yukseklik);
    for (int i = 0; i < A.yukseklik; ++i) {
        float32x4_t acc = vdupq_n_f32(0.0f);
        int k = 0;
        for (; k <= A.genislik - 4; k += 4) {
            float32x4_t a = vld1q_f32(&A.data.get()[i * A.genislik + k]);
            float32x4_t b = vld1q_f32(&B.data.get()[k]);
            acc = vmlaq_f32(acc, a, b);
        }
        float sum = vaddvq_f32(acc);
        for (; k < A.genislik; ++k)
            sum += A.data.get()[i * A.genislik + k] * B.data.get()[k];
        C.data.get()[i] = sum;
    }
    return C;
}

template <typename T>
inline
Matris<T> operator+(const Matris<T>& A, const Matris<T>& B) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    int i = 0;
    for (; i <= total - 4; i += 4) {
        float32x4_t a = vld1q_f32(&A.data.get()[i]);
        float32x4_t b = vld1q_f32(&B.data.get()[i]);
        vst1q_f32(&C.data.get()[i], vaddq_f32(a, b));
    }
    for (; i < total; ++i)
        C.data.get()[i] = A.data.get()[i] + B.data.get()[i];
    return C;
}

// =============================================================================
// --- YKN_NEON_* SARMALAYICI FONKSİYONLARI (generator_arm_hir.py ile senkron) ---
// Bu fonksiyon isimleri generator_arm_hir.py'daki _NEON_ACTIVATION tablosuyla
// birebir eşleşmelidir.
// =============================================================================

// Matris<float> çarpımı (ykn_neon_matmul)
inline Matris<float> ykn_neon_matmul(const Matris<float>& A, const Matris<float>& B) {
    return A * B;  // NEON operator* kullanır
}

// BitNet: bitnet_matmul(A, B, scale) — ternary matmul + dequantize
inline Matris<float> ykn_neon_bitnet_matmul(const Matris<float>& A, const Matris<float>& B, float scale) {
    Matris<float> C = A * B;
    int total = C.genislik * C.yukseklik;
    float32x4_t v_scale = vdupq_n_f32(1.0f / scale);
    int i = 0;
    for (; i + 4 <= total; i += 4) {
        float32x4_t v = vld1q_f32(&C.data.get()[i]);
        vst1q_f32(&C.data.get()[i], vmulq_f32(v, v_scale));
    }
    for (; i < total; ++i)
        C.data.get()[i] /= scale;
    return C;
}

// BitNet: popcount + center + dequantize (scalar int)
inline int ykn_neon_bitnet_popcount(int a, int b, int shift, float scale) {
    int xor_val = a ^ b;
    int pc = 0;
    while (xor_val) { pc += (xor_val & 1); xor_val >>= 1; }
    return (int)((pc - shift) * scale);
}

// Fused matmul + bias + aktivasyon (ykn_neon_matmul_fused)
inline Matris<float> ykn_neon_matmul_fused(const Matris<float>& A, const Matris<float>& B,
                                     const Matris<float>* bias, const char* activation) {
    Matris<float> C = A * B;
    int total = C.genislik * C.yukseklik;
    // Bias toplama
    if (bias && bias->data) {
        for (int i = 0; i < total; ++i)
            C.data.get()[i] += bias->data.get()[i % (bias->genislik * bias->yukseklik)];
    }
    // Aktivasyon uygulama
    std::string act(activation ? activation : "none");
    if (act == "relu") {
        float32x4_t zero = vdupq_n_f32(0.0f);
        int i = 0;
        for (; i <= total - 4; i += 4)
            vst1q_f32(&C.data.get()[i], vmaxq_f32(vld1q_f32(&C.data.get()[i]), zero));
        for (; i < total; ++i) C.data.get()[i] = std::max(0.0f, C.data.get()[i]);
    } else if (act == "gelu") {
        for (int i = 0; i < total; ++i) {
            float x = C.data.get()[i];
            C.data.get()[i] = 0.5f * x * (1.0f + std::tanh(0.7978845608f * (x + 0.044715f * x*x*x)));
        }
    } else if (act == "sigmoid") {
        for (int i = 0; i < total; ++i)
            C.data.get()[i] = 1.0f / (1.0f + std::exp(-C.data.get()[i]));
    } else if (act == "tanh") {
        for (int i = 0; i < total; ++i)
            C.data.get()[i] = std::tanh(C.data.get()[i]);
    }
    return C;
}

// Aktivasyon fonksiyonları
template <typename T = float>
inline
Matris<T> ykn_neon_relu_f32(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    float32x4_t zero = vdupq_n_f32(0.0f);
    int i = 0;
    for (; i <= total - 4; i += 4)
        vst1q_f32(&C.data.get()[i], vmaxq_f32(vld1q_f32(&A.data.get()[i]), zero));
    for (; i < total; ++i)
        C.data.get()[i] = std::max(0.0f, A.data.get()[i]);
    return C;
}

template <typename T = float>
inline
Matris<T> ykn_neon_gelu_f32(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    for (int i = 0; i < total; ++i) {
        float x = A.data.get()[i];
        C.data.get()[i] = 0.5f * x * (1.0f + std::tanh(0.7978845608f * (x + 0.044715f * x*x*x)));
    }
    return C;
}

template <typename T = float>
inline
Matris<T> ykn_neon_sigmoid_f32(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    for (int i = 0; i < total; ++i)
        C.data.get()[i] = 1.0f / (1.0f + std::exp(-A.data.get()[i]));
    return C;
}

template <typename T = float>
inline
Matris<T> ykn_neon_tanh_f32(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    for (int i = 0; i < total; ++i)
        C.data.get()[i] = std::tanh(A.data.get()[i]);
    return C;
}

// RMSNorm: sqrt(mean(x^2)) ile normalleştirme
template <typename T = float>
inline
Matris<T> ykn_neon_rmsnorm(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    // Ortalama kare hesapla
    float sum_sq = 0.0f;
    for (int i = 0; i < total; ++i)
        sum_sq += A.data.get()[i] * A.data.get()[i];
    float rms = std::sqrt(sum_sq / total + 1e-6f);
    for (int i = 0; i < total; ++i)
        C.data.get()[i] = A.data.get()[i] / rms;
    return C;
}

// Softmax (numerically stable)
template <typename T = float>
inline
Matris<T> ykn_neon_softmax(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    // Satır bazlı softmax
    for (int row = 0; row < A.yukseklik; ++row) {
        float* src = A.data.get() + row * A.genislik;
        float* dst = C.data.get() + row * C.genislik;
        float maxv = *std::max_element(src, src + A.genislik);
        float sumexp = 0.0f;
        for (int j = 0; j < A.genislik; ++j) {
            dst[j] = std::exp(src[j] - maxv);
            sumexp += dst[j];
        }
        for (int j = 0; j < A.genislik; ++j)
            dst[j] /= sumexp;
    }
    return C;
}

// Eski isimlerle uyumluluk katmanı (generator_arm.py hala relu() çağırabilir)
template <typename T = float>
inline
Matris<T> relu(const Matris<T>& A)    { return ykn_neon_relu_f32(A); }
template <typename T = float>
inline
Matris<T> sigmoid(const Matris<T>& A) { return ykn_neon_sigmoid_f32(A); }
template <typename T = float>
inline
Matris<T> gelu(const Matris<T>& A)    { return ykn_neon_gelu_f32(A); }
template <typename T = float>
inline
Matris<T> rmsnorm(const Matris<T>& A) { return ykn_neon_rmsnorm(A); }
template <typename T = float>
inline
Matris<T> softmax(const Matris<T>& A) { return ykn_neon_softmax(A); }
