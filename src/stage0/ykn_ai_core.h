#ifndef YKN_AI_CORE_H
#define YKN_AI_CORE_H

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <map>
#include <string>

// GGUF Context and Types
struct GGUFModel {
    long long mmap_addr;
    std::map<std::string, std::string> kv_string;
    std::map<std::string, long long> kv_int;
    std::map<std::string, long long> tensors; // Name -> absolute memory pointer
    long long tensor_data_offset;
    
    // Kelime Dagarcigi (Vocab)
    std::vector<std::string> tokens;
    std::vector<long long> token_scores; // GGUF floats will be mapped to long long (rank)
    std::map<std::string, long long> token_to_id;
};

std::map<long long, GGUFModel*> gguf_models;
long long next_model_id = 1;


// 1.58-bit BitLinear Core (Ternary Matrix Multiplication)
// ykn_benchmark'taki hizli logic
inline void bit_linear_carp(const std::vector<int8_t>& girdi, const std::vector<int8_t>& agirliklar, std::vector<int32_t>& cikti, int satir, int sutun) {
    for (int i = 0; i < satir; i += 1) {
        int32_t toplam = 0;
        int row_offset = i * sutun;
        for (int j = 0; j < sutun; j += 1) {
            int8_t w = agirliklar[row_offset + j];
            if (w == 1) {
                toplam += girdi[j];
            } else if (w == -1) {
                toplam -= girdi[j];
            }
        }
        cikti[i] = toplam;
    }
}

// Memory Mapped IO (mmap)
inline long long mmap_dosya_ac(const std::string& yol) {
#ifdef _WIN32
    // Windows fallback if needed, but we assume Linux docker for now
    HANDLE hFile = CreateFileA(yol.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMap == NULL) { CloseHandle(hFile); return 0; }
    void* ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    return reinterpret_cast<long long>(ptr);
#else
    int fd = open(yol.c_str(), O_RDONLY);
    if (fd == -1) return 0;
    struct stat sb;
    if (fstat(fd, &sb) == -1) { close(fd); return 0; }
    void* ptr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); // safe to close after mmap
    if (ptr == MAP_FAILED) return 0;
    return reinterpret_cast<long long>(ptr);
#endif
}

inline long long isaretciden_uzun_tamsayi_oku(long long isaretci, long long offset) {
    if (isaretci == 0) return 0;
    uint64_t val = 0;
    std::memcpy(&val, reinterpret_cast<void*>(isaretci + offset), sizeof(uint64_t));
    return static_cast<long long>(val);
}

inline std::string isaretciden_metin_oku(long long isaretci, long long offset, long long uzunluk) {
    if (isaretci == 0) return "";
    return std::string(reinterpret_cast<const char*>(isaretci + offset), uzunluk);
}

inline long long mmap_kapat(long long isaretci) {
    // Basic stub
    return 0;
}

// GGUF YKN API (Extern "C" for Assembly Linking)
extern "C" {

long long gguf_yukle(const char* dosya_adi) {
    long long ptr = mmap_dosya_ac(dosya_adi);
    if (ptr == 0) return 0; // Dosya açılamadı
    
    // GGUF Sihirli kelimesi (0x46554747) kontrolü yapılabilir
    uint32_t magic = 0;
    std::memcpy(&magic, reinterpret_cast<void*>(ptr), 4);
    if (magic != 0x46554747) {
        // GGUF degil, ama simdilik mock amacli devam edebilir
    }
    
    GGUFModel* model = new GGUFModel();
    model->mmap_addr = ptr;
    
    // TODO: Gercek GGUF parsing (KV ve Tensor tablolari okunacak)
    // Simdilik YKN'nin test edilebilmesi icin mock tensor adresleri atiyoruz
    model->tensors["blk.0.attn_q.weight"] = ptr + 1024;
    model->tensors["blk.0.attn_k.weight"] = ptr + 2048;
    model->tensors["blk.0.attn_v.weight"] = ptr + 4096;
    model->tensors["blk.0.attn_output.weight"] = ptr + 8192;
    model->tensors["blk.0.ffn_gate.weight"] = ptr + 16384;
    model->tensors["blk.0.ffn_up.weight"] = ptr + 32768;
    model->tensors["blk.0.ffn_down.weight"] = ptr + 65536;
    model->kv_int["layer_count"] = 1; // 1 katmanli mock
    
    // Mock Kelime Dagarcigi (BPE Testi Icin)
    model->tokens = {"M", "e", "r", "h", "a", "b", " ", "Y", "u", "k", "i", "!", "Mer", "haba", " Yuki", "Merhaba"};
    // BPE Skorlari (Buyuk olan daha oncelikli birlesir)
    model->token_scores = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 20, 50}; 
    for(size_t i = 0; i < model->tokens.size(); ++i) {
        model->token_to_id[model->tokens[i]] = i;
    }
    model->kv_int["tokenizer.ggml.tokens"] = model->tokens.size();
    
    long long id = next_model_id++;
    gguf_models[id] = model;
    return id;
}

long long gguf_tensor_al(long long model_id, const char* tensor_adi) {
    if (gguf_models.find(model_id) != gguf_models.end()) {
        GGUFModel* m = gguf_models[model_id];
        if (m->tensors.find(tensor_adi) != m->tensors.end()) {
            return m->tensors[tensor_adi];
        }
    }
    return 0; // Bulunamadi
}

long long gguf_meta_okunusu(long long model_id, const char* meta_adi) {
    if (gguf_models.find(model_id) != gguf_models.end()) {
        GGUFModel* m = gguf_models[model_id];
        if (m->kv_int.find(meta_adi) != m->kv_int.end()) {
            return m->kv_int[meta_adi];
        }
    }
    return 0;
}

// BPE Tokenizer API
long long gguf_token_id_bul(long long model_id, const char* metin) {
    if (gguf_models.find(model_id) != gguf_models.end()) {
        GGUFModel* m = gguf_models[model_id];
        if (m->token_to_id.find(metin) != m->token_to_id.end()) {
            return m->token_to_id[metin];
        }
    }
    return -1;
}

long long gguf_token_skoru(long long model_id, long long token_id) {
    if (gguf_models.find(model_id) != gguf_models.end()) {
        GGUFModel* m = gguf_models[model_id];
        if (token_id >= 0 && token_id < (long long)m->token_scores.size()) {
            return m->token_scores[token_id];
        }
    }
    return -999999;
}

const char* gguf_token_metni(long long model_id, long long token_id) {
    if (gguf_models.find(model_id) != gguf_models.end()) {
        GGUFModel* m = gguf_models[model_id];
        if (token_id >= 0 && token_id < (long long)m->tokens.size()) {
            return m->tokens[token_id].c_str();
        }
    }
    return "";
}

// Tensor Ops (AI ops.ykn stubs)
long long ai_matmul(long long A_ptr, long long B_ptr, long long C_ptr, long long M, long long N, long long K) {
    // Stub for standard FP32 matmul
    return 1;
}

long long ai_silu(long long X_ptr, long long eleman_sayisi) {
    if (X_ptr == 0) return 0;
    float* x = reinterpret_cast<float*>(X_ptr);
    for (long long i = 0; i < eleman_sayisi; ++i) {
        float val = x[i];
        x[i] = val / (1.0f + std::exp(-val)); // x * sigmoid(x)
    }
    return 1;
}

long long ai_rmsnorm(long long X_ptr, long long agirlik_ptr, long long eleman_sayisi) {
    if (X_ptr == 0) return 0;
    float* x = reinterpret_cast<float*>(X_ptr);
    float* w = reinterpret_cast<float*>(agirlik_ptr);
    float sum_sq = 0.0f;
    for (long long i = 0; i < eleman_sayisi; ++i) {
        sum_sq += x[i] * x[i];
    }
    float rms = std::sqrt(sum_sq / eleman_sayisi + 1e-6f);
    for (long long i = 0; i < eleman_sayisi; ++i) {
        x[i] = (x[i] / rms) * (w ? w[i] : 1.0f);
    }
    return 1;
}

long long ai_softmax(long long X_ptr, long long satir, long long sutun) {
    if (X_ptr == 0) return 0;
    float* x = reinterpret_cast<float*>(X_ptr);
    for (long long i = 0; i < satir; ++i) {
        float max_val = x[i * sutun];
        for (long long j = 1; j < sutun; ++j) max_val = std::max(max_val, x[i * sutun + j]);
        float sum = 0.0f;
        for (long long j = 0; j < sutun; ++j) {
            x[i * sutun + j] = std::exp(x[i * sutun + j] - max_val);
            sum += x[i * sutun + j];
        }
        for (long long j = 0; j < sutun; ++j) x[i * sutun + j] /= sum;
    }
    return 1;
}

} // extern "C"

#endif // YKN_AI_CORE_H
