#pragma once
#include "include/definition.hpp"

namespace dhttp::simd::westmere
{
    template <int N> alignas(N) struct simdv;

    template<>
    alignas(32) struct simdv<32>
    {
        static constexpr int size = 32;

        __m128i lo, hi;


        simdv(simdv& v)  : lo{v.lo}, hi{v.hi}{}
        simdv(simdv&& v) : lo{v.lo}, hi{v.hi}{}
        simdv(__m128i u, __m128i v) : lo{u}, hi{v}{}

        TARGET("sse4")
        static inline simdv load(void *b)
        {
            return {_mm_loadu_si128(reinterpret_cast<__m128i *>(b)),
                    _mm_loadu_si128(reinterpret_cast<__m128i *>(reinterpret_cast<u8_t *>(b) + 16))};
        }

        TARGET("sse4")
        static inline simdv splat(u8_t v)
        {
            return {_mm_set1_epi8(v), _mm_set1_epi8(v)};
        }

        TARGET("sse4")
        static inline u32_t bitmask(simdv& x)
        {

            return static_cast<u32_t>(_mm_movemask_epi8(x.hi)) << 16 |
                   _mm_movemask_epi8(x.lo);
        }

        TARGET("sse4")
        static inline simdv cmp_zero(simdv& v)
        {
            static __m128i z = _mm_setzero_si128();
            return {_mm_cmpeq_epi8(v.lo, z),
                    _mm_cmpeq_epi8(v.hi, z)};
        }

        TARGET("sse4")
        static inline simdv cmp_eq(simdv& u, simdv& v)
        {
            return {_mm_cmpeq_epi8(u.lo, v.lo),
                    _mm_cmpeq_epi8(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv cmp_eq(simdv& u, simdv& v, simdv &w)
        {
            return {
                _mm_or_si128(_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.lo, w.lo)),
                _mm_or_si128(_mm_cmpeq_epi8(u.hi, v.hi), _mm_cmpeq_epi8(u.hi, w.hi)),
            };
        }

        TARGET("sse4")
        static inline simdv cmp_gt(simdv& v, u8_t a)
        {
            __m128i x = _mm_set1_epi8(a);
            return {_mm_cmpgt_epi8(v.lo, x),
                    _mm_cmpgt_epi8(v.hi, x)};
        }

        TARGET("sse4")
        static inline simdv cmp_gt(simdv& u, simdv& v)
        {
            return {_mm_cmpgt_epi8(u.lo, v.hi),
                    _mm_cmpgt_epi8(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv cmp_lt(simdv& v, u8_t a)
        {
            __m128i x = _mm_set1_epi8(a);
            return {_mm_cmpgt_epi8(v.lo, x),
                    _mm_cmpgt_epi8(v.hi, x)};
        }

        TARGET("sse4")
        static inline simdv cmp_lt(simdv& u, simdv& v)
        {
            return {_mm_cmplt_epi8(u.lo, v.hi),
                    _mm_cmplt_epi8(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv gt_and_lt(simdv& v, u8_t a, u8_t b)
        {
            __m128i x = _mm_set1_epi8(a);
            __m128i y = _mm_set1_epi8(b);
            return {
                _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
                _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
            };
        }

        TARGET("sse4")
        static inline simdv gt_and_lt(simdv& u, simdv& v, simdv& w)
        {
            return {
                _mm_and_si128(_mm_cmpgt_epi8(u.lo, v.lo), _mm_cmplt_epi8(u.lo, w.lo)),
                _mm_and_si128(_mm_cmpgt_epi8(u.lo, v.hi), _mm_cmplt_epi8(u.lo, w.hi)),
            };
        }

        TARGET("sse4")
        static inline simdv _and(simdv& u, simdv& v)
        {
            return {_mm_and_si128(u.lo, v.lo),
                    _mm_and_si128(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv _or(simdv& u, simdv& v)
        {
            return {_mm_or_si128(u.lo, v.lo),
                    _mm_or_si128(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv lshift(simdv& u, int r)
        {
            return {_mm_srli_si128(u.lo, r),
                    _mm_srli_si128(u.hi, r)};
        }

        TARGET("sse4")
        static inline bool is_zero(simdv& u)
        {
#ifdef HAVE__SSE4_2__
            return _mm_test_all_zeros(u.lo, u.lo) or _mm_test_all_zeros(u.hi, u.hi);
#else
            return static_cast<bool>(bitmask(cmp_zero(u)));
#endif
        }

#if HAVE__SSSE3__
#define HAVE_SHUFFLE__ 1
        static inline simdv shuffle(simdv& u, simdv& x)
        {
            return {_mm_shuffle_epi8(u.lo, x.lo),
                    _mm_shuffle_epi8(u.hi, x.hi)};
        }
#else
        [[gnu::unused]] static inline simdv shuffle(simdv& u, simdv& x){}
#endif
    };

    template<>
    alignas(64) struct simdv<64>
    {
        static constexpr int size = 64;
    
        simdv<32> lo, hi;

        TARGET("sse4")
        make_flat static inline simdv load(void *b)
        {
            return {simdv<32>::load(reinterpret_cast<__m128i *>(b)),
                    simdv<32>::load(reinterpret_cast<__m128i *>(reinterpret_cast<u8_t *>(b) + 32))};
        }

        TARGET("sse4")
        make_flat static inline simdv splat(u8_t v)
        {
            return {simdv<32>::splat(v), simdv<32>::splat(v)};
        }

        TARGET("sse4")
        make_flat static inline u64_t bitmask(simdv x)
        {

            return static_cast<u64_t>(simdv<32>::bitmask(x.hi)) << 32 | simdv<32>::bitmask(x.lo);
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_zero(simdv v)
        {
            return {simdv<32>::cmp_zero(v.lo),
                    simdv<32>::cmp_zero(v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_eq(simdv u, simdv v)
        {
            return {simdv<32>::cmp_eq(u.lo, v.lo),
                    simdv<32>::cmp_eq(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_eq(simdv u, simdv v, simdv w)
        {
            return {
                simdv<32>::_or(simdv<32>::cmp_eq(u.lo, v.lo), simdv<32>::cmp_eq(u.lo, w.lo)),
                simdv<32>::_or(simdv<32>::cmp_eq(u.hi, v.hi), simdv<32>::cmp_eq(u.hi, w.hi)),
            };
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_gt(simdv v, u8_t a)
        {
            simdv<32> x = simdv<32>::splat(a);
            return {simdv<32>::cmp_eq(v.lo, x), simdv<32>::cmp_eq(v.hi, x)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_gt(simdv u, simdv v)
        {
            return {simdv<32>::cmp_gt(u.lo, v.hi),
                    simdv<32>::cmp_gt(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_lt(simdv v, u8_t a)
        {
            simdv<32> x = simdv<32>::splat(a);
            return {simdv<32>::cmp_lt(v.lo, x), simdv<32>::cmp_lt(v.hi, x)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_lt(simdv u, simdv v)
        {
            return {simdv<32>::cmp_lt(v.lo, v.lo), simdv<32>::cmp_lt(v.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv gt_and_lt(simdv v, u8_t a, u8_t b)
        {
            simdv<32> x = simdv<32>::splat(a);
            simdv<32> y = simdv<32>::splat(b);
            return {
                simdv<32>::_and(simdv<32>::cmp_gt(v.lo, x), simdv<32>::cmp_lt(v.lo, y)),
                simdv<32>::_and(simdv<32>::cmp_gt(v.hi, x), simdv<32>::cmp_lt(v.hi, y)),
            };
        }

        TARGET("sse4")
        make_flat static inline simdv gt_and_lt(simdv u, simdv v, simdv w)
        {
             return {
                simdv<32>::_and(simdv<32>::cmp_gt(u.lo, v.lo), simdv<32>::cmp_lt(u.lo, w.lo)),
                simdv<32>::_and(simdv<32>::cmp_gt(u.hi, v.hi), simdv<32>::cmp_lt(u.hi, w.hi)),
            };
        }

        TARGET("sse4")
        make_flat static inline simdv andl(simdv &u, simdv &v)
        {
            return {simdv<32>::_and(u.lo, v.lo), simdv<32>::_and(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv orl(simdv u, simdv v)
        {
            return {simdv<32>::_or(u.lo, v.lo),
                    simdv<32>::_or(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv lshift(simdv u, int r)
        {
            return {simdv<32>::lshift(u.lo, r),
                    simdv<32>::lshift(u.hi, r)};
        }

        TARGET("sse4")
        make_flat static inline bool is_zero(simdv u)
        {
             return static_cast<bool>(bitmask(cmp_zero(u)));
        }

#if HAVE__SSSE3__
#define HAVE_SHUFFLE__ 1
        make_flat static inline simdv shuffle(simdv u, simdv v)
        {
            return {simdv<32>::shuffle(u.lo, v.lo),
                    simdv<32>::shuffle(u.hi, v.hi)};
        }
#else
        [[gnu::unused]] make_flat static inline simdv shuffle(simdv u, simdv x){}
#endif
    };
}