#include <type_traits>
#include <iostream>
#include <typeinfo>
#include <cstdint>
#include <immintrin.h>

using u64_t = uint64_t;
struct base
{
    using type = __m256i;
    
    static type dup(int c)
    {
        return _mm256_setzero_si256();
    }

    static type load(int c)
    {
        return _mm256_setzero_si256();
    }
};

    template<typename base>
    struct simd64
    {
        alignas(64) base::type lo, hi;
    
        simd64() = default;
        simd64(base::type lo, base::type hi) : lo{lo}, hi{hi} {}
        
        static simd64 operator()(int c){
            return {_mm256_set1_epi8(c), _mm256_set1_epi8(c)};
        }

        simd64 &operator=(simd64 &x)
        {
            lo = x.lo;
            hi = x.hi;
            return *this;
        }
    
        simd64 &operator=(const void *p)
        {
            lo = _mm256_loadu_si256(static_cast<base::type *>(p)), hi = _mm256_loadu_si256(static_cast<base::type *>(p) + 1);
            return *this;
        }
        simd64 &operator=(int v)
        {
            lo = _mm256_set1_epi8(v);
            hi = _mm256_set1_epi8(v);
            return *this;
        }

        #if 0
        constexpr simd64 operator>(u8_t a)
        {
            return {_mm256_cmpgt1_epi8(lo, a), _mm256_cmpgt1_epi8(hi, a)};
        }
        #endif
        
        constexpr simd64 operator>(simd64 &v)
        {
            return {_mm256_cmpgt_epi8(lo, v.lo), _mm256_cmpgt_epi8(hi, v.hi)};
        }

        simd64 operator==(simd64 &v)
        {
            return {_mm256_cmpeq_epi8(lo, v.lo), _mm256_cmpeq_epi8(hi, v.hi)};
        }

        simd64 operator&(simd64 &v)
        {
            return {_mm256_and_si256(lo, v.lo), _mm256_and_si256(hi, v.hi)};
        }

        simd64 operator>>(const int r)
        {
            return {_mm256_srli_epi32(lo, r), _mm256_srli_epi32(hi, r)};
        }

         #if 0
        constexpr simd64 cmpglt(simd64 &v, u8_t a, u8_t b)
        {
            return {_mm256_cmpglt_epi8(v.lo, a, b), _mm256_cmpglt_epi8(v.hi, a, b)};
        }

        constexpr simd64 cmpglt(u8_t a, u8_t b)
        {
            return {_mm256_cmpglt_epi8(lo, a, b), _mm256_cmpglt_epi8(hi, a, b)};
        }
        #endif

        static u64_t movemask(void)
        {
            return U64(_mm256_movemask_epi8(hi)) << 32 | _mm256_movemask_epi8(lo);
        }

        static u64_t movemask(simd64 &v)
        {
            return U64(_mm256_movemask_epi8(v.hi)) << 32 | _mm256_movemask_epi8(v.lo);
        }

        bool testzero(void)
        {
            return _mm256_testz_si256(lo) or _mm256_testz_si256(hi);
        }

#ifdef HAVE_SHUFFLE__
        simd64 shufb(simd64 &tab)
        {
            return {_mm256_shuffle_epi8(u.lo, tab.lo), _mm256_shuffle_epi8(u.hi, tab.hi)};
        }
#endif
    };


int main(void)
{
    /*
    #if HAVE_SHUFLE__
        static const dhttp::simd64 lo = dhttp::simd64(dhttp::table::bitmap256_valid_request_charset_shufb);
        static const dhttp::simd64 hi = dhttp::simd64(dhttp::table::bitmap256_valid_request_charset_shufb + 64);
        return dhttp::simd64::movemask(simd64::shufb(lo, v) & dhttp::simd64::pshuf(hi, v >> 4)); // valid rfc chars
#else
        return dhttp::simd64::movemask((v & dhttp::simd64('\xf')) > '\x0') & dhttp::simd64::cmpglt(v >> 4, '\x1', '\x9');
#endif
    }
*/
    simd64<base> x;
    simd64<base> p = x(0);

    simd64<base> y = x;
    simd64<base> q = y(0);

    simd64<base>(0);
}