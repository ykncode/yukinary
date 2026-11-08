// Yukinary native LLM SIMD ABI.
// Scalar implementation remains the numerical reference; these functions are
// optional and are selected at runtime by the host executable.
#pragma once
#include <cstddef>
#include <cstdint>

extern "C" int ykn_llm_simd_backend();
// YKN `tamsayi` is a signed 64-bit scalar. Pointer arguments therefore point
// to packed int64_t elements, not C int32_t elements.
extern "C" int64_t ykn_llm_dot_q16(const int64_t * a, const int64_t * b, std::size_t n);
extern "C" void ykn_llm_silu_q16(const int64_t * gate, const int64_t * up, int64_t * output, std::size_t n);
extern "C" int64_t ykn_llm_iq2s_dot_q16(const int64_t * arg1, const int64_t * arg2, int64_t arg3, int64_t arg4);
extern "C" int64_t ykn_llm_f16_dot_q16(const int64_t * activation, int64_t * qact, std::size_t K);
extern "C" int64_t ykn_llm_q1_0_dot_q16(const int64_t * arg1, const int64_t * arg2, int64_t arg3);
// Runtime telemetry used by the real-model benchmark. Values are monotonic
// nanoseconds and peak resident bytes; they do not affect inference results.
extern "C" uint64_t ykn_llm_clock_ns();
extern "C" uint64_t ykn_llm_peak_rss_bytes();
