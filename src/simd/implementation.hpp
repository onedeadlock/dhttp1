#pragma once
#include "../include/definition.hpp"

namespace dhttp::simd
{
     static constexpr bool mix_avx512_avx2 = MIX_AVX512_AVX2;
     static constexpr bool mix_avx2_sse4   = MIX_AVX2_SSE;

     static constexpr u8_t AVX512 = 1;
     static constexpr u8_t AVX2   = 2;
     static constexpr u8_t SSE4   = 3;
     static constexpr u8_t INT64  = 4;

     #ifndef FORCE_SIMD_32
          static constexpr int max = 64;
     #else
          static constexpr int max = 32;
     #endif
     
     #if HAVE__SSE2__
     #    include "westmere/implementation.hpp"
          using namespace dhttp::simd::westmere;
     #else
     #    include "fallback/implementation.hpp"
#         using namespace dhttp::simd::fallback; 
     #endif
}