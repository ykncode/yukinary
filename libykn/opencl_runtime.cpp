// =============================================================================
// opencl_runtime.cpp  -  Yukinary OpenCL Backend Runtime Kütüphanesi
// generator_opencl_hir.py tarafından üretilen .cpp dosyalarına #include edilir.
// =============================================================================
// Bu dosya; ykn_opencl_* ön ekli fonksiyonları tanımlar —
// generator_opencl_hir.py tarafından üretilen çağrılarla birebir eşleşir.
// =============================================================================
#define YKN_OPENCL_BACKEND
#include "libykn_runtime.h"

#ifdef __APPLE__
#  include <OpenCL/opencl.h>
#else
#  include <CL/cl.h>
#endif

#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>

// =============================================================================
// --- BELLEK ARENA (OpenCL/CPU tarafı — malloc tabanlı) ---
// libykn_runtime.h'de extern olarak bildirilen arena fonksiyonları burada
// gerçeklenir. OpenCL host tarafı RAM kullanır; GPU tamponu ayrıca yönetilir.
// =============================================================================

YknArena g_gpu_arena = {nullptr, 0, 0};

void ykn_arena_init(size_t size_in_bytes) {
    if (g_gpu_arena.base_ptr) {
        free(g_gpu_arena.base_ptr);
    }
    g_gpu_arena.base_ptr = static_cast<uint8_t*>(malloc(size_in_bytes));
    if (!g_gpu_arena.base_ptr) {
        fprintf(stderr, "[YKN OpenCL] Arena bellek tahsisi basarisiz: %zu bayt\n", size_in_bytes);
        exit(EXIT_FAILURE);
    }
    g_gpu_arena.total_size    = size_in_bytes;
    g_gpu_arena.current_offset = 0;
}

void* ykn_arena_alloc(size_t size_in_bytes) {
    size_t aligned = (size_in_bytes + 15) & ~(size_t)15;
    if (g_gpu_arena.current_offset + aligned > g_gpu_arena.total_size) {
        fprintf(stderr, "[YKN OpenCL] Arena doldu! Istenen: %zu, Kalan: %zu\n",
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
// --- OPENCL DURUM YÖNETİMİ ---
// =============================================================================

// Global OpenCL nesneleri — ykn_opencl_init() ile başlatılır.
static cl_platform_id   ykn_ocl_platform  = nullptr;
static cl_device_id     ykn_ocl_device    = nullptr;
cl_context              ykn_cl_context    = nullptr;  // extern: extern bildirenlere görünür
cl_command_queue        ykn_cl_queue      = nullptr;  // extern: extern bildirenlere görünür
static cl_program       ykn_ocl_program   = nullptr;

// generator_opencl_hir.py'daki kernel adları
static cl_kernel        ykn_k_matmul      = nullptr;
static cl_kernel        ykn_k_relu        = nullptr;
static cl_kernel        ykn_k_gelu        = nullptr;
static cl_kernel        ykn_k_sigmoid     = nullptr;
static cl_kernel        ykn_k_softmax     = nullptr;
static cl_kernel        ykn_k_rmsnorm     = nullptr;

// =============================================================================
// --- BELLEK YÖNETİMİ ---
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
// --- OPENCL PLATFORM BAŞLATMA (ykn_opencl_init) ---
// =============================================================================

// Yerleşik OpenCL kernel kaynak dizesi
// (generator_opencl_hir.py kendi kernel'larını ayrıca üretir;
//  bu kaynaklar runtime'ın iç kullanımı içindir.)
static const char* YKN_INTERNAL_KERNELS = R"opencl(
__kernel void ykn_matmul(
    __global const float* A,
    __global const float* B,
    __global float* C,
    const int M, const int N, const int K)
{
    int row = get_global_id(0);
    int col = get_global_id(1);
    if (row >= M || col >= N) return;
    float sum = 0.0f;
    for (int k = 0; k < K; k++)
        sum += A[row * K + k] * B[k * N + col];
    C[row * N + col] = sum;
}

__kernel void ykn_relu(__global float* X, const int N) {
    int i = get_global_id(0);
    if (i < N) X[i] = X[i] > 0.0f ? X[i] : 0.0f;
}

__kernel void ykn_gelu(__global float* X, const int N) {
    int i = get_global_id(0);
    if (i < N) {
        float x = X[i];
        X[i] = 0.5f * x * (1.0f + tanh(0.7978845608f * (x + 0.044715f * x * x * x)));
    }
}

__kernel void ykn_sigmoid(__global float* X, const int N) {
    int i = get_global_id(0);
    if (i < N) X[i] = 1.0f / (1.0f + exp(-X[i]));
}

__kernel void ykn_rmsnorm(__global const float* A, __global float* C,
                           const int N, const float eps) {
    int row = get_global_id(0);
    float sum_sq = 0.0f;
    for (int i = 0; i < N; i++)
        sum_sq += A[row * N + i] * A[row * N + i];
    float rms = sqrt(sum_sq / N + eps);
    for (int i = 0; i < N; i++)
        C[row * N + i] = A[row * N + i] / rms;
}

__kernel void ykn_softmax(__global float* X, const int rows, const int cols) {
    int row = get_global_id(0);
    if (row >= rows) return;
    float maxv = X[row * cols];
    for (int j = 1; j < cols; j++) maxv = max(maxv, X[row * cols + j]);
    float sumexp = 0.0f;
    for (int j = 0; j < cols; j++) { X[row * cols + j] = exp(X[row * cols + j] - maxv); sumexp += X[row * cols + j]; }
    for (int j = 0; j < cols; j++) X[row * cols + j] /= sumexp;
}
)opencl";

static void _ykn_check(cl_int err, const char* op) {
    if (err != CL_SUCCESS)
        fprintf(stderr, "[OpenCL] Hata %d: %s\n", (int)err, op);
}

// =============================================================================
// --- BITNET WRAPPER FONKSIYONLARI (OpenCL) ---
// =============================================================================

// BitNet matmul: basit matmul + dequantize (scale ile bol)
inline Matris<float> ykn_opencl_bitnet_matmul(const Matris<float>& A, const Matris<float>& B, float scale) {
    Matris<float> C = ykn_opencl_matmul(A, B);
    int total = C.genislik * C.yukseklik;
    float32x4_t v_scale = vdupq_n_f32(1.0f / scale);
    for (int i = 0; i + 4 <= total; i += 4) {
        float32x4_t v = vld1q_f32(&C.data.get()[i]);
        vst1q_f32(&C.data.get()[i], vmulq_f32(v, v_scale));
    }
    for (int i = (total / 4) * 4; i < total; ++i)
        C.data.get()[i] /= scale;
    return C;
}

// BitNet popcount + center + dequantize (scalar int)
inline int ykn_opencl_bitnet_popcount(int a, int b, int shift, float scale) {
    int xor_val = a ^ b;
    int pc = 0;
    while (xor_val) { pc += (xor_val & 1); xor_val >>= 1; }
    return (int)((pc - shift) * scale);
}

// ykn_opencl_init — generator_opencl_hir.py tarafından main()'de çağrılır.
// 'user_kernels' parametresi: generator'ın ürettiği ek kernel kaynak dizesi (nullptr ise sadece dahili kerneller kullanılır).
inline void ykn_opencl_init(const char* user_kernels = nullptr) {
    cl_int err;
    cl_platform_id platforms[4]; cl_uint np;
    err = clGetPlatformIDs(4, platforms, &np);
    if (err != CL_SUCCESS || np == 0) {
        fprintf(stderr, "[OpenCL] Platform bulunamadi — CPU fallback modunda calisiliyor.\n");
        return;
    }
    ykn_ocl_platform = platforms[0];

    err = clGetDeviceIDs(ykn_ocl_platform, CL_DEVICE_TYPE_GPU, 1, &ykn_ocl_device, nullptr);
    if (err != CL_SUCCESS)
        err = clGetDeviceIDs(ykn_ocl_platform, CL_DEVICE_TYPE_CPU, 1, &ykn_ocl_device, nullptr);
    if (err != CL_SUCCESS) { fprintf(stderr, "[OpenCL] Cihaz bulunamadi.\n"); return; }

    ykn_cl_context = clCreateContext(nullptr, 1, &ykn_ocl_device, nullptr, nullptr, &err);
    ykn_cl_queue   = clCreateCommandQueue(ykn_cl_context, ykn_ocl_device, 0, &err);

    // Kaynak kodu birleştir: dahili + kullanıcı
    std::string combined = YKN_INTERNAL_KERNELS;
    if (user_kernels) combined += "\n"; combined += (user_kernels ? user_kernels : "");
    const char* src_ptr = combined.c_str();
    size_t src_len = combined.size();
    ykn_ocl_program = clCreateProgramWithSource(ykn_cl_context, 1, &src_ptr, &src_len, &err);
    err = clBuildProgram(ykn_ocl_program, 1, &ykn_ocl_device, "-cl-fast-relaxed-math", nullptr, nullptr);
    if (err != CL_SUCCESS) {
        char log[4096]; size_t loglen;
        clGetProgramBuildInfo(ykn_ocl_program, ykn_ocl_device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, &loglen);
        fprintf(stderr, "[OpenCL] Build hatasi:\n%.*s\n", (int)loglen, log);
        return;
    }

    // Kernel nesnelerini oluştur
    ykn_k_matmul  = clCreateKernel(ykn_ocl_program, "ykn_matmul",  &err);
    ykn_k_relu    = clCreateKernel(ykn_ocl_program, "ykn_relu",    &err);
    ykn_k_gelu    = clCreateKernel(ykn_ocl_program, "ykn_gelu",    &err);
    ykn_k_sigmoid = clCreateKernel(ykn_ocl_program, "ykn_sigmoid", &err);
    ykn_k_rmsnorm = clCreateKernel(ykn_ocl_program, "ykn_rmsnorm", &err);
    ykn_k_softmax = clCreateKernel(ykn_ocl_program, "ykn_softmax", &err);
}

// ykn_opencl_cleanup — generator_opencl_hir.py tarafından program sonunda çağrılır.
inline void ykn_opencl_cleanup() {
    if (ykn_k_matmul)  clReleaseKernel(ykn_k_matmul);
    if (ykn_k_relu)    clReleaseKernel(ykn_k_relu);
    if (ykn_k_gelu)    clReleaseKernel(ykn_k_gelu);
    if (ykn_k_sigmoid) clReleaseKernel(ykn_k_sigmoid);
    if (ykn_k_rmsnorm) clReleaseKernel(ykn_k_rmsnorm);
    if (ykn_k_softmax) clReleaseKernel(ykn_k_softmax);
    if (ykn_ocl_program) clReleaseProgram(ykn_ocl_program);
    if (ykn_cl_queue)    clReleaseCommandQueue(ykn_cl_queue);
    if (ykn_cl_context)  clReleaseContext(ykn_cl_context);
    ykn_k_matmul = ykn_k_relu = ykn_k_gelu = ykn_k_sigmoid = nullptr;
    ykn_k_rmsnorm = ykn_k_softmax = nullptr;
    ykn_cl_queue = nullptr; ykn_cl_context = nullptr;
}

// =============================================================================
// --- YKN_OPENCL_* FONKSİYONLARI (generator_opencl_hir.py ile senkron) ---
// =============================================================================

// Yardımcı: tek kanallı GPU buffer tahsis + upload
static inline cl_mem _ocl_buf_r(const float* data, size_t bytes) {
    cl_int err;
    cl_mem buf = clCreateBuffer(ykn_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, (void*)data, &err);
    _ykn_check(err, "clCreateBuffer READ");
    return buf;
}
static inline cl_mem _ocl_buf_rw(float* data, size_t bytes) {
    cl_int err;
    cl_mem buf = clCreateBuffer(ykn_cl_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, data, &err);
    _ykn_check(err, "clCreateBuffer READ_WRITE");
    return buf;
}
static inline cl_mem _ocl_buf_w(size_t bytes) {
    cl_int err;
    cl_mem buf = clCreateBuffer(ykn_cl_context, CL_MEM_WRITE_ONLY, bytes, nullptr, &err);
    _ykn_check(err, "clCreateBuffer WRITE");
    return buf;
}

// CPU fallback: OpenCL başlatılamadığında scalar matmul
static inline Matris<float> _cpu_matmul(const Matris<float>& A, const Matris<float>& B) {
    Matris<T> C = create_matris<T>(B.genislik, A.yukseklik);
    for (int i = 0; i < A.yukseklik; ++i)
        for (int j = 0; j < B.genislik; ++j) {
            float s = 0.0f;
            for (int k = 0; k < A.genislik; ++k)
                s += A.data.get()[i * A.genislik + k] * B.data.get()[k * B.genislik + j];
            C.data.get()[i * C.genislik + j] = s;
        }
    return C;
}

// ykn_opencl_matmul — Matris<float> çarpımı (GPU veya CPU fallback)
inline Matris<float> ykn_opencl_matmul(const Matris<float>& A, const Matris<float>& B) {
    if (!ykn_cl_context || !ykn_k_matmul) return _cpu_matmul(A, B);

    Matris<T> C = create_matris<T>(B.genislik, A.yukseklik);
    size_t sA = (size_t)A.genislik * A.yukseklik * sizeof(float);
    size_t sB = (size_t)B.genislik * B.yukseklik * sizeof(float);
    size_t sC = (size_t)C.genislik * C.yukseklik * sizeof(float);

    cl_mem dA = _ocl_buf_r(A.data.get(), sA);
    cl_mem dB = _ocl_buf_r(B.data.get(), sB);
    cl_mem dC = _ocl_buf_w(sC);
    int M = A.yukseklik, N = B.genislik, K = A.genislik;

    clSetKernelArg(ykn_k_matmul, 0, sizeof(cl_mem), &dA);
    clSetKernelArg(ykn_k_matmul, 1, sizeof(cl_mem), &dB);
    clSetKernelArg(ykn_k_matmul, 2, sizeof(cl_mem), &dC);
    clSetKernelArg(ykn_k_matmul, 3, sizeof(int),    &M);
    clSetKernelArg(ykn_k_matmul, 4, sizeof(int),    &N);
    clSetKernelArg(ykn_k_matmul, 5, sizeof(int),    &K);

    size_t global[2] = { (size_t)M, (size_t)N };
    clEnqueueNDRangeKernel(ykn_cl_queue, ykn_k_matmul, 2, nullptr, global, nullptr, 0, nullptr, nullptr);
    clEnqueueReadBuffer(ykn_cl_queue, dC, CL_TRUE, 0, sC, C.data.get(), 0, nullptr, nullptr);
    clReleaseMemObject(dA); clReleaseMemObject(dB); clReleaseMemObject(dC);
    return C;
}

// ykn_opencl_matmul_fused — Fused matmul + bias + aktivasyon
inline Matris<float> ykn_opencl_matmul_fused(const Matris<float>& A, const Matris<float>& B,
                                       const Matris<float>* bias, const char* activation) {
    Matris<float> C = ykn_opencl_matmul(A, B);
    int total = C.genislik * C.yukseklik;
    if (bias && bias->data) {
        int bsz = bias->genislik * bias->yukseklik;
        for (int i = 0; i < total; ++i)
            C.data.get()[i] += bias->data.get()[i % bsz];
    }
    std::string act(activation ? activation : "none");
    if (act == "relu") {
        for (int i = 0; i < total; ++i) C.data.get()[i] = std::max(0.0f, C.data.get()[i]);
    } else if (act == "gelu") {
        for (int i = 0; i < total; ++i) {
            float x = C.data.get()[i];
            C.data.get()[i] = 0.5f*x*(1.0f+std::tanh(0.7978845608f*(x+0.044715f*x*x*x)));
        }
    } else if (act == "sigmoid") {
        for (int i = 0; i < total; ++i)
            C.data.get()[i] = 1.0f / (1.0f + std::exp(-C.data.get()[i]));
    }
    return C;
}

// Aktivasyon fonksiyonları (GPU dispatch; küçük matrislerde CPU fallback)
template <typename T = float>
inline
Matris<T> ykn_opencl_relu(const Matris<T>& A) {
    if (!ykn_cl_context || !ykn_k_relu) {
        Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
        int n = A.genislik * A.yukseklik;
        for (int i = 0; i < n; ++i) C.data.get()[i] = std::max(0.0f, A.data.get()[i]);
        return C;
    }
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int N = A.genislik * A.yukseklik;
    size_t sz = (size_t)N * sizeof(float);
    cl_mem dX = _ocl_buf_rw(A.data.get(), sz);
    clSetKernelArg(ykn_k_relu, 0, sizeof(cl_mem), &dX);
    clSetKernelArg(ykn_k_relu, 1, sizeof(int),    &N);
    size_t g = N;
    clEnqueueNDRangeKernel(ykn_cl_queue, ykn_k_relu, 1, nullptr, &g, nullptr, 0, nullptr, nullptr);
    clEnqueueReadBuffer(ykn_cl_queue, dX, CL_TRUE, 0, sz, C.data.get(), 0, nullptr, nullptr);
    clReleaseMemObject(dX);
    return C;
}

template <typename T = float>
inline
Matris<T> ykn_opencl_gelu(const Matris<T>& A) {
    // CPU fallback: GELU GPU kernel'i ykn_k_gelu ile aynı pattern
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int N = A.genislik * A.yukseklik;
    if (!ykn_cl_context || !ykn_k_gelu) {
        for (int i = 0; i < N; ++i) {
            float x = A.data.get()[i];
            C.data.get()[i] = 0.5f*x*(1.0f+std::tanh(0.7978845608f*(x+0.044715f*x*x*x)));
        }
        return C;
    }
    size_t sz = (size_t)N * sizeof(float);
    cl_mem dX = _ocl_buf_rw(A.data.get(), sz);
    clSetKernelArg(ykn_k_gelu, 0, sizeof(cl_mem), &dX);
    clSetKernelArg(ykn_k_gelu, 1, sizeof(int),    &N);
    size_t g = N;
    clEnqueueNDRangeKernel(ykn_cl_queue, ykn_k_gelu, 1, nullptr, &g, nullptr, 0, nullptr, nullptr);
    clEnqueueReadBuffer(ykn_cl_queue, dX, CL_TRUE, 0, sz, C.data.get(), 0, nullptr, nullptr);
    clReleaseMemObject(dX);
    return C;
}

template <typename T = float>
inline
Matris<T> ykn_opencl_sigmoid(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int N = A.genislik * A.yukseklik;
    for (int i = 0; i < N; ++i) C.data.get()[i] = 1.0f/(1.0f+std::exp(-A.data.get()[i]));
    return C;
}

// RMSNorm: satır bazlı GPU dispatch
template <typename T = float>
inline
Matris<T> ykn_opencl_rmsnorm(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    if (!ykn_cl_context || !ykn_k_rmsnorm) {
        int total = A.genislik * A.yukseklik;
        float sum_sq = 0.0f;
        for (int i = 0; i < total; ++i) sum_sq += A.data.get()[i] * A.data.get()[i];
        float rms = std::sqrt(sum_sq / total + 1e-6f);
        for (int i = 0; i < total; ++i) C.data.get()[i] = A.data.get()[i] / rms;
        return C;
    }
    size_t sA = (size_t)A.genislik * A.yukseklik * sizeof(float);
    cl_mem dA = _ocl_buf_r(A.data.get(), sA);
    cl_mem dC = _ocl_buf_w(sA);
    int N = A.genislik; float eps = 1e-6f;
    clSetKernelArg(ykn_k_rmsnorm, 0, sizeof(cl_mem), &dA);
    clSetKernelArg(ykn_k_rmsnorm, 1, sizeof(cl_mem), &dC);
    clSetKernelArg(ykn_k_rmsnorm, 2, sizeof(int),    &N);
    clSetKernelArg(ykn_k_rmsnorm, 3, sizeof(float),  &eps);
    size_t g = A.yukseklik;
    clEnqueueNDRangeKernel(ykn_cl_queue, ykn_k_rmsnorm, 1, nullptr, &g, nullptr, 0, nullptr, nullptr);
    clEnqueueReadBuffer(ykn_cl_queue, dC, CL_TRUE, 0, sA, C.data.get(), 0, nullptr, nullptr);
    clReleaseMemObject(dA); clReleaseMemObject(dC);
    return C;
}

// Softmax: satır bazlı GPU dispatch
template <typename T = float>
inline
Matris<T> ykn_opencl_softmax(const Matris<T>& A) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    if (!ykn_cl_context || !ykn_k_softmax) {
        // CPU numerically stable softmax
        for (int row = 0; row < A.yukseklik; ++row) {
            float* src = A.data.get() + row * A.genislik;
            float* dst = C.data.get() + row * C.genislik;
            float maxv = *std::max_element(src, src + A.genislik);
            float sumexp = 0.0f;
            for (int j = 0; j < A.genislik; ++j) { dst[j] = std::exp(src[j]-maxv); sumexp += dst[j]; }
            for (int j = 0; j < A.genislik; ++j) dst[j] /= sumexp;
        }
        return C;
    }
    size_t sA = (size_t)A.genislik * A.yukseklik * sizeof(float);
    cl_mem dX = _ocl_buf_rw(A.data.get(), sA);
    int rows = A.yukseklik, cols = A.genislik;
    clSetKernelArg(ykn_k_softmax, 0, sizeof(cl_mem), &dX);
    clSetKernelArg(ykn_k_softmax, 1, sizeof(int),    &rows);
    clSetKernelArg(ykn_k_softmax, 2, sizeof(int),    &cols);
    size_t g = rows;
    clEnqueueNDRangeKernel(ykn_cl_queue, ykn_k_softmax, 1, nullptr, &g, nullptr, 0, nullptr, nullptr);
    clEnqueueReadBuffer(ykn_cl_queue, dX, CL_TRUE, 0, sA, C.data.get(), 0, nullptr, nullptr);
    clReleaseMemObject(dX);
    return C;
}

// =============================================================================
// --- OPERATÖR AŞIRI YÜKLEMELERİ (CPU Fallback) ---
// =============================================================================

template <typename T>
inline
Matris<T> operator*(const Matris<T>& A, const Matris<T>& B) { return ykn_opencl_matmul(A, B); }

template <typename T>
inline
Matris<T> operator+(const Matris<T>& A, const Matris<T>& B) {
    Matris<T> C = create_matris<T>(A.genislik, A.yukseklik);
    int total = A.genislik * A.yukseklik;
    for (int i = 0; i < total; ++i)
        C.data.get()[i] = A.data.get()[i] + B.data.get()[i];
    return C;
}

// Uyumluluk katmanı (eski generator_opencl.py adları)
template <typename T = float>
inline
Matris<T> relu(const Matris<T>& A)    { return ykn_opencl_relu(A); }
template <typename T = float>
inline
Matris<T> sigmoid(const Matris<T>& A) { return ykn_opencl_sigmoid(A); }
template <typename T = float>
inline
Matris<T> softmax(const Matris<T>& A) { return ykn_opencl_softmax(A); }
template <typename T = float>
inline
Matris<T> rmsnorm(const Matris<T>& A) { return ykn_opencl_rmsnorm(A); }
