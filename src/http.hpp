#pragma once
#include "include/definiton.hpp"
#include "common/common.hpp"
#include "common/bits.hpp"

namespace dhttp
{
    template <typename base> struct simd64;
    class http;

    constexpr int COMPLETE = 0;
    constexpr int EXPECT_DATA = 1;

    template <typename T, size_t N>
    struct req
    {
        static_assert(std::is_integral_v<T> && N > 0);
        static constexpr u64_t __size = N;
        u64_t __used = 0;

        using __pair = struct
        {
            T len, pos;
        };

        struct {
            __pair name, value;
        } pair [N];

        constexpr u64_t size(void) noexcept
        {
            return __size;
        }

        u64_t used(void) const noexcept
        {
            return __used;
        }

        u64_t set_used(T i) noexcept
        {
            assert( i < __size );
            return __used = i;
        }

        auto &&get(T i) const noexcept
        {
            assert( i < __size );
            return pair[i];
        }

        auto &&operator[](T i) noexcept
        {
            return pair[i];
        }
    };

    struct req_line
    {
        /*
            N     req    |   res
            _____________|________
            [3] = method | version
            [2] = uri    | status
            [1] = version| msg
            [0] = NULL   | NULL
        */
        u16_t req_line[4];
    };

    struct req_state
    {
        u16_t pos           = 0; // absolute index of last byte parsed
        u16_t j             = 3; // request line field count (0, 3)
        bool  trailing_sp   = 0; // carry of trailing sp
        bool  trailing_cr   = 0;
        bool  req_line      = 0; // request line
        bool  no_init       = 1; // true if decoding of request/status-line is pending (not started)
        bool  pending_name  = 0;
        bool  pending_value = 0;
    };


    namespace tables
    {
        /* High/low bitmap of ascii: !, #, \$, %, &, ', *, +, -, .,
                                          ^, _, `, |, A-Za-z0-9, :, /, ?, #,
                                          [, ], @, !, $, &, ', (, ), *, +, ,
                                          , ;, =
        */
        alignas(64) uint8_t token_charset[256]{
            // low 4 bits map of supported ASCII characters
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            // High 4 bits map of supported ASCII characters
            0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

        static constexpr u8_t tchar_map[]{
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x80\x00\x80\x80\x80\x80\x00\x00\x00\x80\x00\x80\x80\x80\x00"
            "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x00\x00\x00\x00\x00\x00"
            "\x00\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"
            "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x00\x00\x00\x80\x80"
            "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"
            "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x00\x80\x00\x80\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"};
    }

#if HAVE__AVX2__
#   define HAVE_SHUFFLE__ 1
#   error "TODO implement cmpgt1, cmpeq0, cmpeq2 in common.hpp"
#elif HAVE__SSE2__
    ///////////////////////////
    ///////////////////////////
    typedef struct
    {
        __m128i lo, hi;
    } __m256i;
    //////////////////////////
    //////////////////////////

    inline __m256i _mm256_loadu_si256(const void *b)
    {
        return {_mm_loadu_si128(static_cast<const __m128i *>(b)), _mm_loadu_si128(static_cast<const __m128i *>(b) + 1)};
    }

    inline __m256i _mm256_set1_epi8(const u8_t v)
    {
        return {_mm_set1_epi8(v), _mm_set1_epi8(v)};
    }

    inline u32_t _mm256_movemask_epi8(const __m256i x)
    {

        return static_cast<u32_t>(_mm_movemask_epi8(x.hi)) << 16 | _mm_movemask_epi8(x.lo);
    }

    inline __m256i _mm256_cmpeqz_epi8(const __m256i v)
    {
        __m128i z = _mm_setzero_si128();
        return {_mm_cmpeq_epi8(v.lo, z), _mm_cmpeq_epi8(v.hi, z)};
    }

    inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
    {
         return {_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.hi, v.hi)};
    }

    inline __m256i _mm256_cmpeq2_epi8(const __m256i u, const __m256i v, const __m256i w)
    {
        return {
            _mm_or_si128(_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.lo, w.lo)),
            _mm_or_si128(_mm_cmpeq_epi8(u.hi, v.hi), _mm_cmpeq_epi8(u.hi, w.hi)),
        };
    }

    inline __m256i _mm256_cmpgt1_epi8(const __m256i v, const u8_t a)
    {
        static __m128i x = _mm_set1_epi8(a);
        return {_mm_cmpgt_epi8(v.lo, x), _mm_cmpgt_epi8(v.hi, x)};
    }

    inline __m256i _mm256_cmpglt_epi8(const __m256i v, const u8_t a, const u8_t b)
    {
        static __m128i x = _mm_set1_epi8(a);
        static __m128i y = _mm_set1_epi8(b);
        return {
            _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
            _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
        };
    }

    inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
    {
        return {_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.hi, v.hi)};
    }

    inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
    {
        return {_mm_and_si128(u.lo, v.lo), _mm_and_si128(u.hi, v.hi)};
    }

    inline __m256i _mm256_or_si256(const __m256i u, const __m256i v)
    {
        return {_mm_or_si128(u.lo, v.lo), _mm_or_si128(u.hi, v.hi)};
    }

    inline __m256i _mm256_srli_epi8(const __m256i u, const int r)
    {
        return {_mm_srli_si128(u.lo, r), _mm_srli_si128(u.hi, r)};
    }

    inline bool _mm256_testz_si256(const __m256i u)
    {
#ifdef HAVE__SSE4_2__
        return _mm_test_all_zeros(u.lo, u.lo) or _mm_test_all_zeros(u.hi, u.hi);
#else
        return static_cast<bool>(_mm256_movemask_epi8(_mm256_cmpeqz_epi8(u)));
#endif
    }

#if HAVE__SSSE3__
#   define HAVE_SHUFFLE__ 1
    inline __m256i _mm256_shuffle_epi8(const __m256i u, const __m256i x)
    {
        return {_mm_shuffle_epi8(u.lo, x.lo), _mm_shuffle_epi8(u.hi, x.hi)};
    }
#endif
#elif HAVE__ARM_NEON__
// TODO
#elif HAVE__INT128__
    ///////////////////////////
    ///////////////////////////
    typedef struct alignas(32)
    {
        u128_t lo, hi;
    } __m256i;
    /////////////////////////
    /////////////////////////

    inline __m256i _mm256_loadu_si256(const void *b)
    {
        const u128_t *x = reinterpret_cast<const u128_t *>(b);
        return {x[0], x[1]};
    }

    constexpr inline __m256i _mm256_set1_epi8(const u8_t v)
    {
        static constexpr u128_t c = U128(0x101010101010101ULL) << 64 | 0x101010101010101ULL;
        return {U128(v) * c, U128(v) * c};
    }

    inline uint16_t _mm256_movemask_epi8(__m256i const u)
    {
        static constexpr u64_t p = 0x0002040810204081ULL;
        static constexpr u64_t c  = 0x8080808080808080ULL;
        return ((u.hi >> 64) * p >> 32) & 0xff000000ULL | (u.hi * p  >> 40) & 0xff0000ULL |
               ((u.lo >> 64) * p >> 48) & 0x0000ff00ULL | (u.lo * p) >> 56;
    }

    inline u128_t _cmpeqz(const u128_t v)
    {
        static constexpr u128_t c7f = U128(0x7f7f7f7f7f7f7f7fULL) << 64 | 0x7f7f7f7f7f7f7f7fULL;
        static constexpr u128_t c80 = U128(0x8080808080808080ULL) << 64 | 0x8080808080808080ULL;
        return ~(v | ((v & c7f) + c7f)) & c80;
    }

    inline u128_t _cmpgtz(const u128_t v)
    {
        static constexpr u128_t c7f = U128(0x7f7f7f7f7f7f7f7fULL) << 64 | 0x7f7f7f7f7f7f7f7fULL;
        static constexpr u128_t c80 = U128(0x8080808080808080ULL) << 64 | 0x8080808080808080ULL;
        return (v | ((v & c7f) + c7f)) & c80;
    }

    inline __m256i _mm256_cmpeqz_epi8(const __m256i u)
    {
        return {_cmpeqz(u.lo), _cmpeqz(u.hi)};
    }

    inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
    {
        return {_cmpeqz(u.lo ^ v.lo), _cmpeqz(u.hi ^ v.hi)};
    }

    inline __m256i _mm256_cmpeq2_epi8(const __m256i u, const __m256i v, const __m256i w)
    {
        return {_cmpeqz((u.lo ^ v.lo) | (u.lo ^ w.lo)), _cmpeqz((u.hi ^ v.lo) | (u.hi ^ w.hi))};
    }

    inline __m256i _mm256_cmpgt1_epi8(const __m256i v, const u8_t a)
    {
        static constexpr u64_t c = 0x8080808080808080ULL;
        const u64_t x = (0x7f - a) * 0x101010101010101ULL;
        return {((v.lo + x) | v.lo) & c, ((v.hi + x) | v.hi) & c};
    }
    
    inline __m256i _mm256_cmpglt_epi8(const __m256i v, const u8_t a, const u8_t b)
    {
        static constexpr u128_t c1 = U128(0x101010101010101ULL) << 64 | 0x101010101010101ULL;
        static constexpr u128_t c2 = c1 * 127;
        static constexpr u128_t c3 = c1 * 128;

        const u128_t x = (0x7f - a) * c1;
        const u128_t y = (0x7f + b) * c1;

        return {
            y - (v.lo & c2) & x + (v.lo & c2) & (~v.lo & c3),
            y - (v.hi & c2) & x + (v.hi & c2) & (~v.hi & c3),
        };
    }

    constexpr inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
    {
        return {u.lo & v.lo, u.hi & v.hi};
    }

    constexpr inline __m256i _mm256_or_si256(const __m256i u, const __m256i v)
    {
        return {u.lo | v.lo, u.hi | v.hi};
    }

    constexpr inline __m256i _mm256_srli_epi8(const __m256i u, const int r)
    {
        return {u.lo >> r, u.hi >> r}; // TODO
    }

    inline bool _mm256_testz_si256(const __m256i u)
    {
        return u.lo or u.hi;
    }
#else
#define HAVE__128__ 1
    //////////////////////////
    //////////////////////////
    typedef struct
    {
        u64_t lo, vlo, hi, vhi;
    } __m256i;
    /////////////////////////
    /////////////////////////

    inline __m256i _mm256_loadu_si256(void const *b)
    {
        const u64_t *x = reinterpret_cast<const u64_t *>(b);
        return {x[0], x[1], x[2], x[3]};
    }

    inline constexpr __m256i _mm256_set1_epi8(const u8_t v)
    {
        const u64_t x = U64(v) * 0x101010101010101ULL;
        return {x, x, x, x};
    }

    inline u32_t _mm256_movemask_epi8(const __m256i u)
    {
        static constexpr u64_t p = 0x0002040810204081ULL;
        static constexpr u64_t c  = 0x8080808080808080ULL;

        const u32_t x = ((((u.lo * p) >> 48) & 0xff00ULL) | ((u.vlo * p) >> 56));
        const u32_t y = ((((u.hi * p) >> 48) & 0xff00ULL) | ((u.vhi * p) >> 56));
        return y << 16 | x;
    }

    inline constexpr __m256i _mm256_cmpglt_epi8(const __m256i v, const u64_t a, const u64_t b)
    {
        const u64_t x = (0x7f - a) * 0x101010101010101ULL;
        const u64_t y = (0x7f + b) * 0x101010101010101ULL;
        // a < v < b
        return {
            y - (v.lo  & 0x7f7f7f7f7f7f7f7fULL) & x + (v.lo  & 0x7f7f7f7f7f7f7f7fULL) & (~v.lo  & 0x8080808080808080ULL),
            y - (v.vlo & 0x7f7f7f7f7f7f7f7fULL) & x + (v.vlo & 0x7f7f7f7f7f7f7f7fULL) & (~v.vlo & 0x8080808080808080ULL),
            y - (v.hi  & 0x7f7f7f7f7f7f7f7fULL) & x + (v.hi  & 0x7f7f7f7f7f7f7f7fULL) & (~v.hi  & 0x8080808080808080ULL),
            y - (v.vhi & 0x7f7f7f7f7f7f7f7fULL) & x + (v.vhi & 0x7f7f7f7f7f7f7f7fULL) & (~v.vhi & 0x8080808080808080ULL),
        };
    }
 
    inline u64_t _cmpeqz(const u64_t v)
    {
        return ~(v | ((v & 0x7f7f7f7f7f7f7f7fULL) + 0x7f7f7f7f7f7f7f7fULL)) & 0x8080808080808080ULL;
    }

    inline u64_t _cmpgtz(const u64_t v)
    {
        return (v | ((v & 0x7f7f7f7f7f7f7f7fULL) + 0x7f7f7f7f7f7f7f7fULL)) & 0x8080808080808080ULL;
    }

     inline __m256i _mm256_cmpeqz_epi8(const __m256i u)
    {
        return {_cmpeqz(u.lo),  _cmpeqz(u.vlo),  _cmpeqz(u.hi),  _cmpeqz(u.vhi)};
    }

    inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
    {
        return {_cmpeqz(u.lo ^ v.lo),  _cmpeqz(u.vlo ^ v.vlo),  _cmpeqz(u.hi ^ v.hi),  _cmpeqz(u.vhi ^ v.vhi)};
    }

    inline __m256i _mm256_cmpeq2_epi8(const __m256i u, const __m256i v, const __m256i w)
    {
        return {
            _cmpeqz((u.lo ^ v.lo) | (u.lo ^ w.lo)), _cmpeqz((u.vlo ^ v.vlo) | (u.vlo ^ w.vlo)),
            _cmpeqz((u.hi ^ v.lo) | (u.hi ^ w.hi)), _cmpeqz((u.vhi ^ v.vhi) | (u.vhi ^ w.vhi)),
        };
    }
    
    inline constexpr __m256i _mm256_cmpgt1_epi8(const __m256i v, const u64_t a)
    {
        constexpr u64_t c = 0x8080808080808080ULL;
        const u64_t x = (0x7f - a) * 0x101010101010101ULL;

        return {
            (((v.lo  + x) | v.lo)  & c),
            (((v.vlo + x) | v.vlo) & c),
            (((v.hi  + x) | v.hi)  & c),
            (((v.vhi + x) | v.vhi) & c),
        };
    }

    inline __m256i _mm256_cmpgtz_epi8(const __m256i v)
    {
        return {_cmpgtz(v.lo), _cmpgtz(v.vlo), _cmpgtz(v.hi), _cmpgtz(v.vhi)};
    }

    inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
    {
        return {u.lo & v.lo, u.vlo & v.vlo, u.hi & v.hi, u.vhi & v.vhi};
    }

    inline __m256i _mm256_or_si256(const __m256i u, const __m256i v)
    {
        return {u.lo | v.lo, u.vlo | v.vlo, u.hi | v.hi, u.vhi | v.vhi};
    }

    inline __m256i _mm256_srli_epi8(const __m256i u, const int r)
    {
        return {u.lo >> r, u.vlo >> r, u.hi >> r, u.vhi >> r};
    }

    inline bool _mm256_testz_si256(const __m256i u)
    {
        return u.lo or u.vlo or u.hi or u.vhi;
    }
#endif

    ///////////////////////////////////////////////
    struct base
    {
        using type = __m256i;
    };
    ///////////////////////////////////////////////
    using simd = simd64<base>;
    //////////////////////////////////////////////
    //////////////////////////////////////////////

    template <typename base>
    struct simd64
    {
        alignas(64) base::type lo, hi;

        simd64() = default;
        simd64(int c):v{c} {}
        simd64(void *p):v{p} {}
        simd64(base::type lo, base::type hi) : lo{lo}, hi{hi} {}
        simd64(sim464& v) : lo{v.lo}, hi{v.hi} {}

        static simd64 &operator()()
        {
            return *this;
        }

        simd64 &operator=(simd64 &x)
        {
            lo = x.lo;
            hi = x.hi;
            return *this;
        }

        simd64 &operator=(void *p)
        {
            lo = _mm256_loadu_si256(static_cast<base::type *>(p)), hi = _mm256_loadu_si256(static_cast<base::type *>(p) + 1);
            return *this;
        }
        simd64 &operator=(int v)
        {
            lo = _mm256_set1_epi8(staic_cast<u8_t>(v & 0xff));
            hi = _mm256_set1_epi8(stati_cast<u8_t>(v & 0xff));
            return *this;
        }

        static bool testzero(const simd64& v)
        {
            return _mm256_testz_si256(v.lo) or _mm256_testz_si256(v.hi);
        }

        static u64_t movemask(const simd64& v)
        {
            return U64(_mm256_movemask_epi8(v.hi)) << 32 | _mm256_movemask_epi8(v.lo);
        }

        static simd64 cmpglt(const simd64& v, u8_t a, u8_t b)
        {
            return {_mm256_cmpglt_epi8(v.lo, a, b), _mm256_cmpglt_epi8(v.hi, a, b)};
        }

        static simd64 cmpeq(const simd64& u, const simd64& v)
        {
            return {_mm256_cmpeq_epi8(u.lo, v.lo), _mm256_cmpeq_epi8(u.hi, v.hi)};
        }

        static simd64 cmpeq(const simd64& u, const u8_t& c)
        {
            base::type v = _mm256_set1_epi8(c);
            return {_mm256_cmpeq_epi8(u.lo, v.lo), _mm256_cmpeq_epi8(u.hi, v.hi)};
        }

        static simd64 cmpeqz(const simd64& u)
        {
            return {_mm256_cmpeqz_epi8(u.lo, v.lo), _mm256_cmpeqz_epi8(u.hi, v.hi)};
        }

        static simd64 cmpeq2(const simd64& u, const simd64& v, const simd64& w)
        {
            return {_mm256_cmpeq2_epi8(u.lo, v.lo), _mm256_cmpeq2_epi8(u.hi, v.hi)};
        }

         static simd64 sign(const simd64& u)
        {
            assert(( "implement me", 0 )); // TODO
            return {0, 0};
        }

        static simd64 andnot(const simd64& u, simd64_t &v)
        {
            assert(( "implement me", 0 )); // TODO
            return {0, 0};
        }

#if     defined(HAVE_SHUFFLE__)
        static simd64 shufb(const simd64& u, const simd64& v)
        {
            return {_mm256_shuffle_epi8(u.lo, v.lo), _mm256_shuffle_epi8(u.hi, v.hi)};
        }
#endif
    };

    template <typename base>
    inline simd64<base> operator>(const simd64<base> &u, const simd64<base> &v)
    {
        return {_mm256_cmpgt_epi8(lo, v.lo), _mm256_cmpgt_epi8(hi, v.hi)};
    }

    template <typename base>
    inline simd64<base> operator>(const simd64<base>& v, u8_t a)
    {
        return {_mm256_cmpgt1_epi8(v.lo, a), _mm256_cmpgt1_epi8(v.hi, a)};
    }
    template <typename base>
    inline simd64<base> operator==(const simd64<base>& u, const simd64<base>& v)
    {
        return {_mm256_cmpeq_epi8(u.lo, v.lo), _mm256_cmpeq_epi8(u.hi, v.hi)};
    }

     template <typename base>
    inline constexpr simd64<base> operator==(const simd64<base>& lhs, const int& rhs)
    {
        base::type v = _mm256_set1_epi16(static_cast<uint8_t>(rhs));
        return {_mm256_cmpeq_epi8(lhs.lo, v), _mm256_cmpeq_epi8(lhs.hi, v)};
    }

    template <typename base>
    inline simd64<base> operator&(const simd64<base>& lhs, const simd64<base>& rhs)
    {
        return {_mm256_and_si256(lhs.lo, rhs.lo), _mm256_and_si256(lhs.hi, rhs.hi)};
    }

    template <typename base>
    inline simd64<base> operator&(const simd64<base>& lhs, const u8_t& rhs)
    {
        __m256i x = _mm256_set1_epi8(rhs);
        return {_mm256_and_si256(lhs.lo, x.lo), _mm256_and_si256(lhs.hi, x.hi)};
    }

    template <typename base>
    inline simd64<base> operator|(const simd64<base>& lhs, const simd64<base>& rhs)
    {
        return {_mm256_or_si256(lhs.lo, rhs.lo), _mm256_or_si256(lhs.hi, rhs.hi)};
    }

    template <typename base>
    inline simd64<base> operator>>(const simd64<base>& lhs, const int& r)
    {
        return {_mm256_srli_epi8(lhs.lo, r), _mm256_srli_epi8(lhs.hi, r)};
    }
};
