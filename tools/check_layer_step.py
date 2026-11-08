# Let's inspect where G_BITNET_LAYER_PARAMS can be defined
with open('std/ai/cuda_backend.ykn', 'r', encoding='utf-8') as f:
    cuda_backend = f.read()

decl = """degisken G_BITNET_LAYER_PARAMS: [600]tamsayi;
degisken G_BITNET_LAYER_READY: [32]tamsayi;
"""

assert 'degisken YKN_CUDA_WEIGHT_HASH_KEY: [512]tamsayi;' in cuda_backend
cuda_backend = cuda_backend.replace('degisken YKN_CUDA_WEIGHT_HASH_KEY: [512]tamsayi;', decl + 'degisken YKN_CUDA_WEIGHT_HASH_KEY: [512]tamsayi;')

# In ykn_cuda_bitnet_norm_qkv: pass katman or store params
# Wait, let's see how ykn_cuda_bitnet_norm_qkv is called in transformer_forward.ykn:
# nqkv_st = ykn_cuda_bitnet_norm_qkv(layer_tensor->veri_offset, p_an, ...)
# Let's add katman parameter to ykn_cuda_bitnet_norm_qkv and ykn_cuda_bitnet_post_attn_ffn_fused!

