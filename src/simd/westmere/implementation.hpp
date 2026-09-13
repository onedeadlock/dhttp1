#ifndef DHTTP_SIMD_WESTMERE_HPP
#define DHTTP_SIMD_WESTMERE_HPP
#include "../../include/definition.hpp"
#include "../../common/common.hpp"

namespace dhttp::simd::westmere
{
    using namespace common;
    template <int N> alignas(N) struct simdv;

    template<>
    alignas(32) struct simdv<32>
    {
        static constexpr int   spec = simd::SSE4;
        static constexpr int   size = 32;
        static constexpr u64_t msb  = constant::msb_32;
        static constexpr u64_t msb3 = constant::msb3_32;

        __m128i lo, hi;


        simdv(const simdv& v)  : lo{v.lo}, hi{v.hi}{}
        simdv(simdv&& v)       : lo{v.lo}, hi{v.hi}{}
        simdv(__m128i u, __m128i v) : lo{u}, hi{v}{}

        TARGET("sse4")
        inline bool is_zero(void)
        {
            return is_zero({lo, hi});
        }

        TARGET("sse4")
        inline u64_t to_bitmask(void)
        {

            return static_cast<u32_t>(_mm_movemask_epi8(hi)) << 16 |
                   _mm_movemask_epi8(lo);
        }

        TARGET("sse4")
        static inline simdv load(void *b)
        {
            return {_mm_loadu_si128(reinterpret_cast<__m128i *>(b)),
                    _mm_loadu_si128(reinterpret_cast<__m128i *>(reinterpret_cast<u8_t *>(b) + 16))};
        }

        static inline void zero(void) {}
       
        TARGET("sse4")
        static inline simdv splat(u8_t v)
        {
            return {_mm_set1_epi8(v), _mm_set1_epi8(v)};
        }

        TARGET("sse4")
        static inline u64_t bitmask(const simdv& v)
        {

            return static_cast<u32_t>(_mm_movemask_epi8(v.hi)) << 16 |
                   _mm_movemask_epi8(v.lo);
        }

        TARGET("sse4")
        static inline simdv cmp_zero(const simdv& v)
        {
            static __m128i z = _mm_setzero_si128();
            return {_mm_cmpeq_epi8(v.lo, z),
                    _mm_cmpeq_epi8(v.hi, z)};
        }

        TARGET("sse4")
        static inline simdv cmp_eq(const simdv& u, const simdv& v)
        {
            return {_mm_cmpeq_epi8(u.lo, v.lo),
                    _mm_cmpeq_epi8(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv cmp_eq(const simdv& u, const simdv& v, const simdv& w)
        {
            return {
                _mm_or_si128(_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.lo, w.lo)),
                _mm_or_si128(_mm_cmpeq_epi8(u.hi, v.hi), _mm_cmpeq_epi8(u.hi, w.hi)),
            };
        }

        TARGET("sse4")
        static inline simdv cmp_gt(const simdv& v, u8_t a)
        {
            __m128i x = _mm_set1_epi8(a);
            return {_mm_cmpgt_epi8(v.lo, x),
                    _mm_cmpgt_epi8(v.hi, x)};
        }

        TARGET("sse4")
        static inline simdv cmp_gt(const simdv& u, const simdv& v)
        {
            return {_mm_cmpgt_epi8(u.lo, v.hi),
                    _mm_cmpgt_epi8(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv cmp_lt(const simdv& v, u8_t a)
        {
            __m128i x = _mm_set1_epi8(a);
            return {_mm_cmpgt_epi8(v.lo, x),
                    _mm_cmpgt_epi8(v.hi, x)};
        }

        TARGET("sse4")
        static inline simdv cmp_lt(const simdv& u, const simdv& v)
        {
            return {_mm_cmplt_epi8(u.lo, v.hi),
                    _mm_cmplt_epi8(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv gt_and_lt(const simdv& v, u8_t a, u8_t b)
        {
            __m128i x = _mm_set1_epi8(a);
            __m128i y = _mm_set1_epi8(b);
            return {
                _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
                _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
            };
        }

        TARGET("sse4")
        static inline simdv gt_and_lt(const simdv& u, const simdv& v, const simdv& w)
        {
            return {
                _mm_and_si128(_mm_cmpgt_epi8(u.lo, v.lo), _mm_cmplt_epi8(u.lo, w.lo)),
                _mm_and_si128(_mm_cmpgt_epi8(u.lo, v.hi), _mm_cmplt_epi8(u.lo, w.hi)),
            };
        }

        TARGET("sse4")
        static inline simdv _and(const simdv& u, const simdv& v)
        {
            return {_mm_and_si128(u.lo, v.lo),
                    _mm_and_si128(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv _or(const simdv& u, const simdv& v)
        {
            return {_mm_or_si128(u.lo, v.lo),
                    _mm_or_si128(u.hi, v.hi)};
        }

        TARGET("sse4")
        static inline simdv lshift(const simdv& u, int r)
        {
            return {_mm_srli_si128(u.lo, r),
                    _mm_srli_si128(u.hi, r)};
        }

        TARGET("sse4")
        static inline bool is_zero(const simdv& u)
        {
            return _mm_test_all_zeros(u.lo, u.lo) or _mm_test_all_zeros(u.hi, u.hi);
        }

        static inline simdv shuffle(const simdv& u, const simdv& x)
        {
            return {_mm_shuffle_epi8(u.lo, x.lo),
                    _mm_shuffle_epi8(u.hi, x.hi)};
        }
    };

    template<>
    alignas(64) struct simdv<64>
    {
        static constexpr int   spec = simd::SSE4;
        static constexpr int   size = 64;
        static constexpr u64_t msb  = constant::msb_64;
        static constexpr u64_t msb3 = constant::msb3_32;
    
        simdv<32> lo, hi;

        TARGET("sse4")
        inline bool is_zero(void)
        {
            return lo.is_zero() or hi.is_zero();
        }
        
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
        make_flat inline u64_t to_bitmask(void)
        {

            return hi.to_bitmask() << 32 | lo.to_bitmask();
        }

        TARGET("sse4")
        make_flat static inline u64_t bitmask(const simdv& v)
        {

            return simdv<32>::bitmask(v.hi) << 32 | simdv<32>::bitmask(v.lo);
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_zero(const simdv& v)
        {
            return {simdv<32>::cmp_zero(v.lo),
                    simdv<32>::cmp_zero(v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_eq(const simdv& u, const simdv& v)
        {
            return {simdv<32>::cmp_eq(u.lo, v.lo),
                    simdv<32>::cmp_eq(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_eq(const simdv& u, const simdv& v, const simdv& w)
        {
            return {
                simdv<32>::_or(simdv<32>::cmp_eq(u.lo, v.lo), simdv<32>::cmp_eq(u.lo, w.lo)),
                simdv<32>::_or(simdv<32>::cmp_eq(u.hi, v.hi), simdv<32>::cmp_eq(u.hi, w.hi)),
            };
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_gt(const simdv& v, u8_t a)
        {
            simdv<32> x = simdv<32>::splat(a);
            return {simdv<32>::cmp_eq(v.lo, x), simdv<32>::cmp_eq(v.hi, x)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_gt(const simdv& u, const simdv& v)
        {
            return {simdv<32>::cmp_gt(u.lo, v.hi),
                    simdv<32>::cmp_gt(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_lt(const simdv& v, u8_t a)
        {
            simdv<32> x = simdv<32>::splat(a);
            return {simdv<32>::cmp_lt(v.lo, x), simdv<32>::cmp_lt(v.hi, x)};
        }

        TARGET("sse4")
        make_flat static inline simdv cmp_lt(const simdv& u, const simdv& v)
        {
            return {simdv<32>::cmp_lt(v.lo, v.lo), simdv<32>::cmp_lt(v.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv gt_and_lt(const simdv& v, u8_t a, u8_t b)
        {
            simdv<32> x = simdv<32>::splat(a);
            simdv<32> y = simdv<32>::splat(b);
            return {
                simdv<32>::_and(simdv<32>::cmp_gt(v.lo, x), simdv<32>::cmp_lt(v.lo, y)),
                simdv<32>::_and(simdv<32>::cmp_gt(v.hi, x), simdv<32>::cmp_lt(v.hi, y)),
            };
        }

        TARGET("sse4")
        make_flat static inline simdv gt_and_lt(const simdv& u, const simdv& v, const simdv& w)
        {
             return {
                simdv<32>::_and(simdv<32>::cmp_gt(u.lo, v.lo), simdv<32>::cmp_lt(u.lo, w.lo)),
                simdv<32>::_and(simdv<32>::cmp_gt(u.hi, v.hi), simdv<32>::cmp_lt(u.hi, w.hi)),
            };
        }

        TARGET("sse4")
        make_flat static inline simdv _and(const simdv&u, simdv &v)
        {
            return {simdv<32>::_and(u.lo, v.lo), simdv<32>::_and(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv _or(const simdv& u, const simdv& v)
        {
            return {simdv<32>::_or(u.lo, v.lo),
                    simdv<32>::_or(u.hi, v.hi)};
        }

        TARGET("sse4")
        make_flat static inline simdv lshift(const simdv& u, int r)
        {
            return {simdv<32>::lshift(u.lo, r),
                    simdv<32>::lshift(u.hi, r)};
        }

        TARGET("sse4")
        make_flat static inline bool is_zero(const simdv& u)
        {
             return simdv<32>::is_zero(u.lo) or simdv<32>::is_zero(u.hi);
        }


        make_flat static inline simdv shuffle(const simdv& u, const simdv& v)
        {
            return {simdv<32>::shuffle(u.lo, v.lo),
                    simdv<32>::shuffle(u.hi, v.hi)};
        }
    };
}
#endif // DHTTP_SIMD_WESTMERE_HPP