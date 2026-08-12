#ifndef DHTTP_H
#define DHTTP_H
#include <cstdint>
#include <cstring>
#include <cstddef>

#if defined(__AVX2__) || defined(__SSSE3__) || defined(__SSE4_2__) || defined(__SSE2__)
#    if defined(__AVX2__)
#        define HAVE__AVX2__   1
#    elif defined(__SSE2__)
#        define HAVE__SSE2__   1
#    elif defined(__SSSE3__)
#        define HAVE__SSSE3__  1
#    elif defined (__SSE4_2__)
#        define HAVE__SSE4_2__ 1
#    endif
#    include <immintrin.h>
#elif defined(_ARM_NEON)
#    if defined(__arm__) || (defined(__ARM_ARCH) && __ARM_ARCH == 1)
#       include <arm_neon.h>
#    else
#       include <arm_acle.h>
#    endif
#    define HAVE__ARM_NEON__ 1
#elif defined(UINT128_MAX) || defined(__INT128__)
#    define HAVE__INT128__ 1
#endif

#define inline __attribute__((always_inline)) inline

#define U32(x)  static_cast<const uint32_t>(x)
#define U64(x)  static_cast<const uint64_t>(x)
#define U128(x) static_cast<const __uint128_t>(x)

#define lsb(x)   ((x) & -(x))        // isolate lsb
#define trim(x)  ((x) & ~((x) << 1)) // set only lsb of every bit run, that is, given 0b100111100001, return 0b100000100001

namespace dhttp
{
    template <typename base> struct simd64;
    class http;

    using u64_t = uint64_t;
    using u32_t = uint32_t;
    using u16_t = uint16_t;
    using u8_t  = uint8_t;

    constexpr size_t HEADER_BUF_SIZE_MAX   = 16284;
    constexpr size_t HEADER_BUF_SIZE       = 8192;
    constexpr size_t REQUEST_LINE_MAX_SIZE = 8192;
    constexpr size_t MAX_HEADER            = 64;
    constexpr size_t MAX_LOAD_SIZE         = 64;

    enum
    {
        COMPLETE          = 0,
        INCOMPLETE        = 1,
        EXPECT_DATA       = 2,
        ERROR             = -1,
        MAX_SIZE_EXCEEDED = -2,
        RESUME            = 0x80,
        RL_INCOMPLETE     = 0x02,
        RL_COMPLETE       = 0x01,
        RL_MALFORMED      = -1,
        STATE_TRAILING_CR = 1
    };

    typedef struct
    {
        uint16_t start;
        uint16_t end;
    } req_t;

    typedef struct
    {
        req_t *req_buf;
        size_t n;
    } reqbuf_t;

    typedef struct
    {
        /// N     req     res
        // [3] = method/version
        // [2] = uri/status
        // [1] = version/msg
        // [0] = 0
        req_t request_line[2][4];
    } request_line_t;

    typedef struct
    {
        u8_t  *recvbuf;
        size_t capacity;
        size_t overflow;
    } recvbuf_t;

    typedef struct
    {
        request_line_t request; // request/response line
        reqbuf_t       hf;      // header fields
        recvbuf_t      recvb;   // input buffer
        size_t         size;    // size of bytes parsed
        bool           done;
    } header_t;

    using state_t = struct
    {
        
        simd  mv          = 0; // current vector lane
        u16_t pos         = 0; // absolute index of last byte parsed
        u16_t line_start  = 0; // start index of token
        u16_t line_end    = 0; // end index of token
        u16_t j           = 3; // request line field count (0, 3)
        u8_t  state       = 0; // general header state
        bool  parse_uinit = 1; // true if decoding of request/status-line is pending (not started)
        bool  trailing_sp = 0; // carry of trailing sp
        bool  trailing_cr = 0;
        bool  req_line    = 0;
    };


    namespace tables
    {
        /* High/low bitmap of ascii: !, #, \$, %, &, ', *, +, -, .,
                                          ^, _, `, |, A-Za-z0-9, :, /, ?, #,
                                          [, ], @, !, $, &, ', (, ), *, +, ,
                                          , ;, =
        */
        alignas(64) uint8_t bitmap_valid_charset[256]{
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
    }

    auto blsr = [&](u64_t x) -> u64_t
    {
        return x & (x - 1);
    };
    auto tzcnt = [&](u64_t v)
    {
        return __builtin_ctzll(v);
    };
    auto likely = [&](bool v)
    {
        return __builtin_expect(v, 1);
    };
    auto unlikely = [&](bool v)
    {
        return __builtin_expect(v, 0);
    };
    auto blsfill = [&](u64_t x) -> u64_t
    { return x | (x - 1); };

#if HAVE__AVX2__
#   define HAVE_SHUFFLE__ 1
#elif HAVE__SSE2__
    ///////////////////////////
    ///////////////////////////
    typedef struct alignas(32)
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

    inline __m256i _mm256_srli_epi32(const __m256i u, const int r)
    {
        return {_mm_srli_epi16(u.lo, r), _mm_srli_epi16(u.hi, r)}; // TODO
    }

    inline bool _mm256_testz_si256(const __m256i u)
    {
#ifdef HAVE__SSE4_2__
        return _mm_test_all_zeros(u.lo, u.lo) or _mm_test_all_zeros(u.hi, u.hi);
#else
        static const __m128i z = _mm_setzero_si128();
        return static_cast<bool>(_mm_movemask_epi8(_mm_and_si128(_mm_cmpeq_epi8(u.lo, z), _mm_cmpeq_epi8(u.hi, z))));
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
        __uint128_t lo, hi;
    } __m256i;
    /////////////////////////
    /////////////////////////

    inline __m256i _mm256_loadu_si256(const void *b)
    {
        const __uint128_t *x = reinterpret_cast<const __uint128_t *>(b);
        return {x[0], x[1]};
    }

    constexpr inline __m256i _mm256_set1_epi8(const u8_t v)
    {
        static constexpr __uint128_t c = U128(0x101010101010101ULL) << 64 | 0x101010101010101ULL;
        return {U128(v) * c, U128(v) * c};
    }

    inline uint16_t _mm256_movemask_epi8(__m256i const u)
    {
        static constexpr u64_t mp = 0x0002040810204081ULL;
        static constexpr u64_t c  = 0x8080808080808080ULL;

#if defined(__BMI2__) || HAVE_USE_PEXT__
        return _pext_u64(u.hi >> 64, c) << 48 | _pext_u64(u.hi & 0xffffffffffffffffULL, c) << 32 |
               _pext_u64(u.lo >> 64, c) << 16 | _pext_u64(u.lo & 0xffffffffffffffffULL, c);
#else
        return ((u.hi >> 64) * mp >> 32) & 0xff000000ULL | (u.hi * mp  >> 40) & 0xff0000ULL |
               ((u.lo >> 64) * mp >> 48) & 0x0000ff00ULL | (u.lo * mp) >> 56;
#endif
    }

    inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
    {
        static constexpr __uint128_t c1 = U128(0x1000100010001000ULL) << 64 | 0x0100010001000100ULL;
        static constexpr __uint128_t c2 = U128(0x1000100010001000ULL) << 64 | 0x0001000100010001ULL;
        static constexpr __uint128_t c3 = U128(0x8080808080808080ULL) << 64 | 0x8080808080808080ULL;

        return {
            ((((u.lo ^ v.lo) | c2) - c1) | (((u.lo ^ v.lo) | c1) - c2)) & (~(u.lo ^ v.lo) & c3),
            ((((u.hi ^ v.hi) | c2) - c1) | (((u.hi ^ v.hi) | c1) - c2)) & (~(u.hi ^ v.hi) & c3),
        };
    }

    constexpr inline u64_t _mm256_cmpgt1_epi8(const __m256i v, const u8_t a)
    {
        static constexpr u64_t c = 0x8080808080808080ULL;
        const u64_t x = (0x7f - a) * 0x101010101010101ULL;

        return {((v.lo + x) | v.lo) & c, ((v.hi + x) | v.hi) & c};
    }
    constexpr inline __uint128_t _mm256_cmpglt_epi8(const __m256i v, const u8_t a, const u8_t b)
    {
        static constexpr __uint128_t c1 = U128(0x101010101010101ULL) << 64 | 0x101010101010101ULL;
        static constexpr __uint128_t c2 = c1 * 127;
        static constexpr __uint128_t c3 = c1 * 128;

        const __uint128_t x = (0x7f - a) * c1;
        const __uint128_t y = (0x7f + b) * c1;

        return {
            y - (v.lo & c2) & x + (v.lo & c2) & (~v.lo & c3),
            y - (v.hi & c2) & x + (v.hi & c2) & (~v.hi & c3),
        };
    }

    constexpr inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
    {
        return {u.lo & v.lo, u.hi & v.hi};
    }

    constexpr inline __m256i _mm256_srli_epi32(const __m256i u, const int r)
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
    typedef struct alignas(32)
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

    inline __m256i _mm256_set1_epi8(const u8_t v)
    {
        const u64_t x = U64(v) * 0x101010101010101ULL;
        return {x, x, x, x};
    }

    inline u32_t _mm256_movemask_epi8(const __m256i u)
    {
        static constexpr u64_t mp = 0x0002040810204081ULL;
        static constexpr u64_t c  = 0x8080808080808080ULL;

#if defined(__BMI2__) && HAVE_USE_PEXT__
        const u32_t x = _pext_u64(u.lo, c) << 8 | _pext_u64(u.vlo, c);
        const u32_t y = _pext_u64(u.hi, c) << 8 | _pext_u64(u.vhi, c);
#else
        const u16_t x = ((((u.lo * mp) >> 48) & 0xff00ULL) | ((u.vlo * mp) >> 56));
        const u32_t y = ((((u.hi * mp) >> 48) & 0xff00ULL) | ((u.vhi * mp) >> 56));
#endif
        return y << 16 | x;
    }

    constexpr inline u64_t _mm256_cmpgt1_epi8(const __m256i v, const u64_t a)
    {
        static constexpr u64_t c = 0x8080808080808080ULL;
        const u64_t x = (0x7f - a) * 0x101010101010101ULL;

        return {
            (((v.lo  + x) | v.lo)  & c),
            (((v.vlo + x) | v.vlo) & c),
            (((v.hi  + x) | v.hi)  & c),
            (((v.vhi + x) | v.vhi) & c),
        };
    }

    constexpr inline u64_t _mm256_cmpglt_epi8(const __m256i v, const u64_t a, const u64_t b)
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

    inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
    {
        static constexpr u64_t c1 = 0x0100010001000100ULL; // even bits
        static constexpr u64_t c2 = 0x0001000100010001ULL; // odd bits
        static constexpr u64_t c3 = 0x8080808080808080ULL;

        return {
            ((((u.lo  ^ v.lo)  | c2) - c1) | (((u.lo  ^ v.lo)  | c1) - c2)) & (~(u.lo  ^ v.lo)  & c3),
            ((((u.vlo ^ v.vlo) | c2) - c1) | (((u.vlo ^ v.vlo) | c1) - c2)) & (~(u.vlo ^ v.vlo) & c3),
            ((((u.hi  ^ v.hi)  | c2) - c1) | (((u.hi  ^ v.hi)  | c1) - c2)) & (~(u.hi  ^ v.hi)  & c3),
            ((((u.vhi ^ v.vhi) | c2) - c1) | (((u.vhi ^ v.vhi) | c1) - c2)) & (~(u.vhi ^ v.vhi) & c3),
        };
    }

    inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
    {
        return {u.lo & v.lo, u.vlo & v.vlo, u.hi & v.hi, u.vhi & v.vhi};
    }

    inline __m256i _mm256_srli_epi32(const __m256i u, const int r)
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
        
        static bool testzero(const simd64& v) const
        {
            return _mm256_testz_si256(v.lo) or _mm256_testz_si256(v.hi);
        }

        static u64_t movemask(const simd64& v) const
        {
            return U64(_mm256_movemask_epi8(v.hi)) << 32 | _mm256_movemask_epi8(v.lo);
        }

        static simd64 cmpglt(const simd64& v, u8_t a, u8_t b) const 
        {
            return {_mm256_cmpglt_epi8(v.lo, a, b), _mm256_cmpglt_epi8(v.hi, a, b)};
        }

#if     defined(HAVE_SHUFFLE__)
        static simd64 shufb(const simd64& v, const simd64& tab) const
        {
            return {_mm256_shuffle_epi8(u.lo, tab.lo), _mm256_shuffle_epi8(u.hi, tab.hi)};
        }
#endif
    };
    
    template <typename base>
    inline simd64<base> operator>(const simd64<base> &u, const simd64<base> &v)
    {
        return simd(_mm256_cmpgt_epi8(lo, v.lo), _mm256_cmpgt_epi8(hi, v.hi));
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
        return simd64<base>{_mm256_and_si256(lhs.lo, rhs.lo), _mm256_and_si256(lhs.hi, rhs.hi)};
    }

    template <typename base>
    inline simd64<base> operator>>(const simd64<base>& lhs, const int& r)
    {
        return {_mm256_srli_epi32(lhs.lo, r), _mm256_srli_epi32(lhs.hi, r)};
    }
};

#undef inline
#endif