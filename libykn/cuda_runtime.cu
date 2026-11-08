// =============================================================================
// cuda_runtime.cu  -  Yukinary CUDA Backend Runtime Kütüphanesi
// generator_cuda.py tarafından üretilen .cu dosyalarına #include edilir.
// Bu dosyayı doğrudan derlemeyin; her zaman ana .cu dosyası aracılığıyla derleyin.
// =============================================================================
#define YKN_CUDA_BACKEND
#include "libykn_runtime.h"
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <mma.h>

using namespace nvcuda;

// =============================================================================
// --- VRAM MEMORY ARENA İMPLEMENTASYONU ---
// =============================================================================

YknArena g_gpu_arena = {nullptr, 0, 0};

void ykn_arena_init(size_t size_in_bytes) {
    if (g_gpu_arena.base_ptr != nullptr) return; // Zaten baslatildi
    cudaError_t err = cudaMallocManaged(&g_gpu_arena.base_ptr, size_in_bytes);
    if (err != cudaSuccess) {
        fprintf(stderr, "[YKN HATA] CUDA Arena baslatilamadi (%zu bayt): %s\n", size_in_bytes, cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    g_gpu_arena.total_size = size_in_bytes;
    g_gpu_arena.current_offset = 0;
}

void* ykn_arena_alloc(size_t size_in_bytes) {
    // Hizalama (Alignment) - 256 bayt sınırına yuvarla
    size_t align = 256;
    size_t aligned_size = (size_in_bytes + align - 1) & ~(align - 1);
    
    if (g_gpu_arena.current_offset + aligned_size > g_gpu_arena.total_size) {
        fprintf(stderr, "[YKN HATA] CUDA Arena tasmastasi! Istenen: %zu, Kalan: %zu\n", 
                aligned_size, g_gpu_arena.total_size - g_gpu_arena.current_offset);
        exit(EXIT_FAILURE);
    }
    
    void* ptr = g_gpu_arena.base_ptr + g_gpu_arena.current_offset;
    g_gpu_arena.current_offset += aligned_size;
    return ptr;
}

void ykn_arena_reset() {
    g_gpu_arena.current_offset = 0;
}

void ykn_arena_free() {
    if (g_gpu_arena.base_ptr) {
        cudaFree(g_gpu_arena.base_ptr);
        g_gpu_arena.base_ptr = nullptr;
        g_gpu_arena.total_size = 0;
        g_gpu_arena.current_offset = 0;
    }
}

// =============================================================================
// --- CUDA RAII BELLEK YÖNETİMİ ---
// =============================================================================

// CudaCachingAllocator kaldirildi, yerine YknArena (Linear Memory Arena) eklendi.

// =============================================================================
// --- CUDA STREAM HAVUZU (Asenkron Boru Hattı / Pipeline) ---
// =============================================================================

#ifndef YKN_STREAM_POOL_SIZE
#define YKN_STREAM_POOL_SIZE 4
#endif

class YknStreamPool {
private:
    cudaStream_t streams[YKN_STREAM_POOL_SIZE];
    int next_stream;
    std::mutex mtx;

    YknStreamPool() : next_stream(0) {
        for (int i = 0; i < YKN_STREAM_POOL_SIZE; ++i)
            cudaStreamCreate(&streams[i]);
    }
    ~YknStreamPool() {
        for (int i = 0; i < YKN_STREAM_POOL_SIZE; ++i)
            cudaStreamDestroy(streams[i]);
    }

public:
    static YknStreamPool& get() {
        static YknStreamPool instance;
        return instance;
    }

    // Round-robin stream dağıtımı
    cudaStream_t acquire() {
        std::lock_guard<std::mutex> lock(mtx);
        cudaStream_t s = streams[next_stream];
        next_stream = (next_stream + 1) % YKN_STREAM_POOL_SIZE;
        return s;
    }

    void sync(cudaStream_t s) { cudaStreamSynchronize(s); }

    void sync_all() {
        for (int i = 0; i < YKN_STREAM_POOL_SIZE; ++i)
            cudaStreamSynchronize(streams[i]);
    }
};

// Yukinary dil tarafından çağrılabilecek senkronizasyon fonksiyonları
inline void senkronize_et() { YknStreamPool::get().sync_all(); }
inline void boru_hatti_baslat() { /* no-op: stream havuzu hazır */ }
inline void boru_hatti_bitir() { YknStreamPool::get().sync_all(); }

template <typename T = float>
inline Matris<T> create_matris(int w, int h) {
    Matris<T> m; m.genislik = w; m.yukseklik = h;
    size_t bytes = (size_t)w * h * sizeof(T);
    T* ptr = (T*)ykn_arena_alloc(bytes);
    // Linear Arena'da tekil matris free isleme alinmaz (Arena reset yapilir)
    m.data = std::shared_ptr<T>(ptr, [](T*) {});
    return m;
}

template <typename T = float>
inline Vektor<T> create_vektor(int len) {
    Vektor<T> v; v.uzunluk = len;
    size_t bytes = (size_t)len * sizeof(T);
    T* ptr = (T*)ykn_arena_alloc(bytes);
    v.data = std::shared_ptr<T>(ptr, [](T*) {});
    return v;
}

// =============================================================================
// --- DİNAMİK CUDA GRID HESAPLAMASI ---
// =============================================================================

inline void get_grid_dims(int width, int height, dim3& threads, dim3& blocks) {
    int device;
    cudaGetDevice(&device);
    cudaDeviceProp props;
    cudaGetDeviceProperties(&props, device);
    int t = (props.maxThreadsPerBlock >= 1024) ? 32 : 16;
    threads = dim3(t, t);
    blocks  = dim3((width  + t - 1) / t,
                   (height + t - 1) / t);
}

// =============================================================================
// --- CUDA ÇEKIRDEK (KERNEL) TANIMLARI ---
// =============================================================================

// Matris<float> x Matris<float> çarpımı (C = A * B)
template <typename T>
__global__ void matris_carp_kernel(T* A, T* B, T* C, int m, int n, int k) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < m && col < k) {
        T sum = (T)0.0f;
        for (int i = 0; i < n; ++i)
            sum += A[row * n + i] * B[i * k + col];
        C[row * k + col] = sum;
    }
}

// Tensor Cores (WMMA) tabanlı Matris<float> x Matris<float> çarpımı (Yalnızca __half için)
__global__ void wmma_matris_carp_kernel(__half* A, __half* B, __half* C, int m, int n, int k) {
#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 700
    // 16x16x16 tile'lar kullanılarak optimize matmul
    int warpM = (blockIdx.x * blockDim.x + threadIdx.x) / warpSize;
    int warpN = (blockIdx.y * blockDim.y + threadIdx.y);
    
    // WMMA fragments
    wmma::fragment<wmma::matrix_a, 16, 16, 16, __half, wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b, 16, 16, 16, __half, wmma::row_major> b_frag;
    wmma::fragment<wmma::accumulator, 16, 16, 16, __half> c_frag;
    
    wmma::fill_fragment(c_frag, __float2half(0.0f));
    
    for (int i = 0; i < n; i += 16) {
        // Sınır kontrolleri basitleştirilmiştir. Gerçek prod'da m,n,k'ye göre bounds checking yapılmalıdır.
        wmma::load_matrix_sync(a_frag, A + warpM * 16 * n + i, n);
        wmma::load_matrix_sync(b_frag, B + i * k + warpN * 16, k);
        wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);
    }
    wmma::store_matrix_sync(C + warpM * 16 * k + warpN * 16, c_frag, k, wmma::mem_row_major);
#else
    // Fallback: Eger cihaz Tensor Core desteklemiyorsa, bos doner (veya standart kernel cagrilabilir)
#endif
}

// Matris<float> x Vektör çarpımı (y = W * x)
template <typename T>
__global__ void matris_vektor_kernel(T* A, T* B, T* C, int m, int n) {
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < m) {
        T sum = (T)0.0f;
        for (int i = 0; i < n; ++i)
            sum += A[row * n + i] * B[i];
        C[row] = sum;
    }
}

// Eleman bazlı matris toplama (C = A + B)
template <typename T>
__global__ void matris_topla_kernel(T* A, T* B, T* C, int total) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < total) C[idx] = A[idx] + B[idx];
}

// 1-Bit XNOR-POPCOUNT çekirdek (BitNet)
__global__ void bit1matris_carp_kernel(uint32_t* A, uint32_t* B, int* C,
                                       int m, int n_words, int k) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < m && col < k) {
        int xnor_sum = 0;
        for (int w = 0; w < n_words; w++)
            xnor_sum += __popc(~(A[row * n_words + w] ^ B[col * n_words + w]));
        C[row * k + col] = 2 * xnor_sum - n_words * 32;
    }
}

// =============================================================================
// --- WARP REDUCTION (INDIRGEME) FONKSIYONLARI ---
// =============================================================================

__inline__ __device__ float warpReduceSum(float val) {
    for (int offset = warpSize/2; offset > 0; offset /= 2)
        val += __shfl_down_sync(0xffffffff, val, offset);
    return val;
}

__inline__ __device__ float warpReduceMax(float val) {
    for (int offset = warpSize/2; offset > 0; offset /= 2)
        val = max(val, __shfl_down_sync(0xffffffff, val, offset));
    return val;
}

// LLM: RMSNorm Çekirdeği
__global__ void rmsnorm_kernel(float* in, float* out, int hidden_size, float eps) {
    int row = blockIdx.x;
    int tid = threadIdx.x;
    float* in_row = in + row * hidden_size;
    float* out_row = out + row * hidden_size;
    
    float sum_sq = 0.0f;
    for (int i = tid; i < hidden_size; i += blockDim.x) {
        float val = in_row[i];
        sum_sq += val * val;
    }
    
    // Warp-level reduce
    sum_sq = warpReduceSum(sum_sq);
    
    // Sadece thread 0 (warp 0) shared memory veya global register'a atabilir
    // Ancak blockDim.x genelde warpSize'dan büyük olabilir (Örn: 256).
    // Basitlik için blockDim.x <= 32 (1 warp) varsayımı yapıyoruz.
    __shared__ float s_inv_rms;
    if (tid == 0) {
        s_inv_rms = rsqrtf((sum_sq / hidden_size) + eps);
    }
    __syncthreads();
    
    for (int i = tid; i < hidden_size; i += blockDim.x) {
        out_row[i] = in_row[i] * s_inv_rms;
    }
}

// LLM: Softmax Çekirdeği
__global__ void softmax_kernel(float* in, float* out, int hidden_size) {
    int row = blockIdx.x;
    int tid = threadIdx.x;
    float* in_row = in + row * hidden_size;
    float* out_row = out + row * hidden_size;
    
    float max_val = -1e20f;
    for (int i = tid; i < hidden_size; i += blockDim.x) {
        max_val = max(max_val, in_row[i]);
    }
    max_val = warpReduceMax(max_val);
    
    __shared__ float s_max;
    if (tid == 0) s_max = max_val;
    __syncthreads();
    
    float sum = 0.0f;
    for (int i = tid; i < hidden_size; i += blockDim.x) {
        float exp_val = expf(in_row[i] - s_max);
        out_row[i] = exp_val;
        sum += exp_val;
    }
    sum = warpReduceSum(sum);
    
    __shared__ float s_sum;
    if (tid == 0) s_sum = sum;
    __syncthreads();
    
    for (int i = tid; i < hidden_size; i += blockDim.x) {
        out_row[i] = out_row[i] / s_sum;
    }
}

// =============================================================================
// --- AKTİVASYON FONKSİYONLARI (Device Inline) ---
// =============================================================================

__device__ __forceinline__ float relu(float x)    { return x > 0.0f ? x : 0.0f; }
__device__ __forceinline__ float sigmoid(float x)  { return 1.0f / (1.0f + expf(-x)); }
__device__ __forceinline__ float gelu(float x) {
    return 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
}

// Aktivasyon kernel'ları
__global__ void matris_relu_kernel   (float* A, float* C, int total) { int i = blockIdx.x * blockDim.x + threadIdx.x; if(i<total) C[i]=relu(A[i]); }
__global__ void matris_sigmoid_kernel(float* A, float* C, int total) { int i = blockIdx.x * blockDim.x + threadIdx.x; if(i<total) C[i]=sigmoid(A[i]); }
__global__ void matris_gelu_kernel   (float* A, float* C, int total) { int i = blockIdx.x * blockDim.x + threadIdx.x; if(i<total) C[i]=gelu(A[i]); }

// =============================================================================
// --- C++ OPERATÖR AŞIRI YÜKLEMELERİ (GPU Dispatch) ---
// =============================================================================

template <typename T>
inline Matris<T> operator*(const Matris<T>& A, const Matris<T>& B) {
    auto s = YknStreamPool::get().acquire();
    Matris<T> C = create_matris<T>(B.genislik, A.yukseklik);
    dim3 threads, blocks;
    get_grid_dims(C.genislik, C.yukseklik, threads, blocks);
    
    if constexpr (std::is_same_v<T, __half>) {
        wmma_matris_carp_kernel<<<blocks, threads, 0, s>>>(
            A.data.get(), B.data.get(), C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    } else {
        matris_carp_kernel<<<blocks, threads, 0, s>>>(
            A.data.get(), B.data.get(), C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    }

    YknStreamPool::get().sync(s);
    return C;
}

template <typename T>
inline Matris<T> async_matmul(const Matris<T>& A, const Matris<T>& B) {
    auto s = YknStreamPool::get().acquire();
    Matris<T> C = create_matris<T>(B.genislik, A.yukseklik);
    dim3 threads, blocks;
    get_grid_dims(C.genislik, C.yukseklik, threads, blocks);
    
    if constexpr (std::is_same_v<T, __half>) {
        wmma_matris_carp_kernel<<<blocks, threads, 0, s>>>(
            A.data.get(), B.data.get(), C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    } else {
        matris_carp_kernel<<<blocks, threads, 0, s>>>(
            A.data.get(), B.data.get(), C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    }

    return C; // Pipeline modu: senkronizasyon yok
}

template <typename T>
inline Vektor<T> operator*(const Matris<float>& A, const Vektor<float>& B) {
    auto s = YknStreamPool::get().acquire();
    Vektor<float> C = create_vektor(A.yukseklik);
    int threads = 256, blocks = (C.uzunluk + 255) / 256;
    matris_vektor_kernel<<<blocks, threads, 0, s>>>(
        A.data.get(), B.data.get(), C.data.get(),
        A.yukseklik, A.genislik);
    YknStreamPool::get().sync(s);
    return C;
}

// Senkron (blocking)
template <typename T>
inline Matris<T> operator+(const Matris<T>& A, const Matris<T>& B) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    int threads = 256, blocks = (total + 255) / 256;
    matris_topla_kernel<<<blocks, threads, 0, s>>>(
        A.data.get(), B.data.get(), C.data.get(), total);
    YknStreamPool::get().sync(s);
    return C;
}

inline Bit1Matris operator*(const Bit1Matris& A, const Bit1Matris& B) {
    int* ptr = nullptr;
    cudaMallocManaged(&ptr, (size_t)A.yukseklik * B.genislik * sizeof(int));
    dim3 threads, blocks;
    get_grid_dims(B.genislik, A.yukseklik, threads, blocks);
    bit1matris_carp_kernel<<<blocks, threads>>>(
        A.data.get(), B.data.get(), ptr,
        A.yukseklik, A.kelime_sayisi, B.genislik);
    cudaDeviceSynchronize();
    Bit1Matris C;
    C.genislik = B.genislik; C.yukseklik = A.yukseklik;
    C.kelime_sayisi = (B.genislik + 31) / 32;
    C.data = std::shared_ptr<uint32_t>((uint32_t*)ptr, [](uint32_t* p){ cudaFree(p); });
    return C;
}

// =============================================================================
// --- BITNET WRAPPER FONKSIYONLARI ---
// generator_cuda.py HirFusedOp 'bitnet_matmul'/'xnor+popcount' desenleri için
// aşağıdaki üst-düzey fonksiyonları çağırır.
// =============================================================================

// Desen 1: bitnet_matmul(A, B, scale) — ternary matmul + dequantize
template <typename T = float>
inline Matris<T> bitnet_matmul(const Matris<T>& A, const Matris<T>& B, T scale = (T)1.0) {
    // Basit matmul + scale ile simüle: gerçek BitNet'te bit1matris_carp kullanılır.
    // Şimdilik float matris ile yaklaşık simülasyon.
    auto s = YknStreamPool::get().acquire();
    Matris<T> C = create_matris(B.genislik, A.yukseklik);
    int m = A.yukseklik, n = B.genislik, k = A.genislik;
    dim3 threads, blocks;
    get_grid_dims(n, m, threads, blocks);
    if (m > 0 && n > 0 && k > 0)
        matmul_kernel<<<blocks, threads, 0, s>>>(A.data.get(), B.data.get(), C.data.get(), m, n, k);
    else
        C = create_matris<T>(0, 0);
    YknStreamPool::get().sync(s);
    // Dequantize: scale ile bol
    if (scale != (T)1.0) {
        int total = C.genislik * C.yukseklik;
        matris_eleman_carp_kernel<<<(total+255)/256, 256, 0, s>>>(
            C.data.get(), scale, C.data.get(), total);
        YknStreamPool::get().sync(s);
    }
    return C;
}

// Desen 2: bitnet_popcount(A, B, shift, scale) — XNOR + popcount + center + dequantize
inline int bitnet_popcount(int a, int b, int shift = 0, int scale = 1) {
    int xor_val = a ^ b;
    int pc = 0;
    while (xor_val) { pc += (xor_val & 1); xor_val >>= 1; }
    return (pc - shift) * scale;
}

// =============================================================================
// --- AKTİVASYON & LLM SARMALAYICILARI: SENKRON + ASENKRON ÇIFTLER ---
// Senkron versiyon: kernel'ı kendi stream'inde kuyruğa alır, sonra sync eder.
// Asenkron (async_) versiyon: sadece kuyruğa alır, sync ETMEz → pipeline için
// =============================================================================

// --- relu ---
template <typename T = float>
inline Matris<T> relu(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    matris_relu_kernel<<<(total+255)/256, 256, 0, s>>>(A.data.get(), C.data.get(), total);
    YknStreamPool::get().sync(s); return C;
}
template <typename T = float>
inline Matris<T> async_relu(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    matris_relu_kernel<<<(total+255)/256, 256, 0, s>>>(A.data.get(), C.data.get(), total);
    return C; // Senkronizasyon yok — boru hattı devam eder
}

// --- sigmoid ---
template <typename T = float>
inline Matris<T> sigmoid(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    matris_sigmoid_kernel<<<(total+255)/256, 256, 0, s>>>(A.data.get(), C.data.get(), total);
    YknStreamPool::get().sync(s); return C;
}
template <typename T = float>
inline Matris<T> async_sigmoid(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    matris_sigmoid_kernel<<<(total+255)/256, 256, 0, s>>>(A.data.get(), C.data.get(), total);
    return C;
}

// --- gelu ---
template <typename T = float>
inline Matris<T> gelu(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    matris_gelu_kernel<<<(total+255)/256, 256, 0, s>>>(A.data.get(), C.data.get(), total);
    YknStreamPool::get().sync(s); return C;
}
template <typename T = float>
inline Matris<T> async_gelu(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int total = C.genislik * C.yukseklik;
    matris_gelu_kernel<<<(total+255)/256, 256, 0, s>>>(A.data.get(), C.data.get(), total);
    return C;
}

// --- rmsnorm (LLM) ---
template <typename T = float>
inline Matris<T> rmsnorm(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int blocks = A.yukseklik;
    int threads = ((min(A.genislik, 256) + 31) / 32) * 32;
    rmsnorm_kernel<<<blocks, threads, 0, s>>>(A.data.get(), C.data.get(), A.genislik, 1e-6f);
    YknStreamPool::get().sync(s); return C;
}
template <typename T = float>
inline Matris<T> async_rmsnorm(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int blocks = A.yukseklik;
    int threads = ((min(A.genislik, 256) + 31) / 32) * 32;
    rmsnorm_kernel<<<blocks, threads, 0, s>>>(A.data.get(), C.data.get(), A.genislik, 1e-6f);
    return C; // Pipeline: sonraki kernel bu stream'i beklemiyor
}

// --- softmax (LLM) ---
template <typename T = float>
inline Matris<T> softmax(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int blocks = A.yukseklik;
    int threads = ((min(A.genislik, 256) + 31) / 32) * 32;
    softmax_kernel<<<blocks, threads, 0, s>>>(A.data.get(), C.data.get(), A.genislik);
    YknStreamPool::get().sync(s); return C;
}
template <typename T = float>
inline Matris<T> async_softmax(const Matris<T>& A) {
    auto s = YknStreamPool::get().acquire();
    Matris<float> C = create_matris(A.genislik, A.yukseklik);
    int blocks = A.yukseklik;
    int threads = ((min(A.genislik, 256) + 31) / 32) * 32;
    softmax_kernel<<<blocks, threads, 0, s>>>(A.data.get(), C.data.get(), A.genislik);
    return C;
}

// =============================================================================
// --- FUSED KERNEL'LAR (MatMul + Bias + Activation tek geçişte) ---
// Optimizer'da HirFusedOp tespiti yapıldığında bu kernel'lar çağrılır.
// Kazanım: Kernel başlatma gecikmesi ve VRAM trafiği ~2-3x azalır.
// =============================================================================

#define TILE_SIZE 32

// MatMul + Bias Toplama + ReLU (Fused)
__global__ void matmul_bias_relu_fused_kernel(
    const float* __restrict__ A,
    const float* __restrict__ B,
    const float* __restrict__ bias,
    float* __restrict__ C,
    int M, int N, int K)
{
    __shared__ float As[TILE_SIZE][TILE_SIZE];
    __shared__ float Bs[TILE_SIZE][TILE_SIZE];

    int bx = blockIdx.x, by = blockIdx.y;
    int tx = threadIdx.x, ty = threadIdx.y;
    
    int row = by * TILE_SIZE + ty;
    int col = bx * TILE_SIZE + tx;

    float sum = 0.0f;
    for (int p = 0; p < (N + TILE_SIZE - 1) / TILE_SIZE; ++p) {
        if (row < M && (p * TILE_SIZE + tx) < N)
            As[ty][tx] = A[row * N + p * TILE_SIZE + tx];
        else
            As[ty][tx] = 0.0f;

        if ((p * TILE_SIZE + ty) < N && col < K)
            Bs[ty][tx] = B[(p * TILE_SIZE + ty) * K + col];
        else
            Bs[ty][tx] = 0.0f;

        __syncthreads();

        for (int i = 0; i < TILE_SIZE; ++i)
            sum += As[ty][i] * Bs[i][tx];

        __syncthreads();
    }

    if (row < M && col < K) {
        sum += bias ? bias[col] : 0.0f;
        C[row * K + col] = sum > 0.0f ? sum : 0.0f;
    }
}

// MatMul + Bias Toplama + GELU (Fused)
__global__ void matmul_bias_gelu_fused_kernel(
    const float* __restrict__ A,
    const float* __restrict__ B,
    const float* __restrict__ bias,
    float* __restrict__ C,
    int M, int N, int K)
{
    __shared__ float As[TILE_SIZE][TILE_SIZE];
    __shared__ float Bs[TILE_SIZE][TILE_SIZE];

    int bx = blockIdx.x, by = blockIdx.y;
    int tx = threadIdx.x, ty = threadIdx.y;
    
    int row = by * TILE_SIZE + ty;
    int col = bx * TILE_SIZE + tx;

    float sum = 0.0f;
    for (int p = 0; p < (N + TILE_SIZE - 1) / TILE_SIZE; ++p) {
        if (row < M && (p * TILE_SIZE + tx) < N)
            As[ty][tx] = A[row * N + p * TILE_SIZE + tx];
        else
            As[ty][tx] = 0.0f;

        if ((p * TILE_SIZE + ty) < N && col < K)
            Bs[ty][tx] = B[(p * TILE_SIZE + ty) * K + col];
        else
            Bs[ty][tx] = 0.0f;

        __syncthreads();

        for (int i = 0; i < TILE_SIZE; ++i)
            sum += As[ty][i] * Bs[i][tx];

        __syncthreads();
    }

    if (row < M && col < K) {
        sum += bias ? bias[col] : 0.0f;
        C[row * K + col] = gelu(sum);
    }
}

// MatMul + Bias Toplama + Sigmoid (Fused)
__global__ void matmul_bias_sigmoid_fused_kernel(
    const float* __restrict__ A,
    const float* __restrict__ B,
    const float* __restrict__ bias,
    float* __restrict__ C,
    int M, int N, int K)
{
    __shared__ float As[TILE_SIZE][TILE_SIZE];
    __shared__ float Bs[TILE_SIZE][TILE_SIZE];

    int bx = blockIdx.x, by = blockIdx.y;
    int tx = threadIdx.x, ty = threadIdx.y;
    
    int row = by * TILE_SIZE + ty;
    int col = bx * TILE_SIZE + tx;

    float sum = 0.0f;
    for (int p = 0; p < (N + TILE_SIZE - 1) / TILE_SIZE; ++p) {
        if (row < M && (p * TILE_SIZE + tx) < N)
            As[ty][tx] = A[row * N + p * TILE_SIZE + tx];
        else
            As[ty][tx] = 0.0f;

        if ((p * TILE_SIZE + ty) < N && col < K)
            Bs[ty][tx] = B[(p * TILE_SIZE + ty) * K + col];
        else
            Bs[ty][tx] = 0.0f;

        __syncthreads();

        for (int i = 0; i < TILE_SIZE; ++i)
            sum += As[ty][i] * Bs[i][tx];

        __syncthreads();
    }

    if (row < M && col < K) {
        sum += bias ? bias[col] : 0.0f;
        C[row * K + col] = sigmoid(sum);
    }
}

// C++ Sarmalayıcı: Fused MatMul+Bias+Activation
// activation: "relu" | "gelu" | "sigmoid"
inline Matris<float> matmul_fused(const Matris<float>& A, const Matris<float>& B,
                           const Vektor<float>* bias, const char* activation) {
    Matris<float> C = create_matris(B.genislik, A.yukseklik);
    dim3 threads(TILE_SIZE, TILE_SIZE);
    dim3 blocks((C.genislik + TILE_SIZE - 1) / TILE_SIZE, (C.yukseklik + TILE_SIZE - 1) / TILE_SIZE);
    float* bias_ptr = bias ? bias->data.get() : nullptr;
    
    std::string act(activation);
    if (act == "relu") {
        matmul_bias_relu_fused_kernel<<<blocks, threads>>>(
            A.data.get(), B.data.get(), bias_ptr, C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    } else if (act == "gelu") {
        matmul_bias_gelu_fused_kernel<<<blocks, threads>>>(
            A.data.get(), B.data.get(), bias_ptr, C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    } else if (act == "sigmoid") {
        matmul_bias_sigmoid_fused_kernel<<<blocks, threads>>>(
            A.data.get(), B.data.get(), bias_ptr, C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    } else {
        // Bilinmeyen aktivasyon: sadece MatMul + bias
        matris_carp_kernel<<<blocks, threads>>>(
            A.data.get(), B.data.get(), C.data.get(),
            A.yukseklik, A.genislik, B.genislik);
    }
    YKN_CUDA_KONTROL(cudaDeviceSynchronize());
    return C;
}
