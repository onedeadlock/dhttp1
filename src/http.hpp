#include <cstdint>
#include <csignal>
#include <cstring>
#include <memory>

#if defined(__AVX2__) || defined(__SSSE3__) || defined(__SSE4_2__) || defined(__SSE2__)
#   if defined(__AVX2__)
#      define HAVE__AVX2__ 1
#   elif defined(__SSE2__)
#      define HAVE__SSE2__ 1
#   endif
#   ifdef __SSSE3__
#      define HAVE__SSSE3__ 1
#   endif
#   ifdef __SSE4_2__
#      define HAVE__SSE4_2__ 1
#   endif
#   include <immintrin.h>
#endif

#define U32(x)  static_cast<const uint32_t>(x)
#define U64(x)  static_cast<const uint64_t>(x)
#define U128(x) static_cast<const __uint128_t>(x)

#define lsb(x)   ((x) & -(x))                    // lsb
#define trim(x)  ((x) & ~((x) << 1))            // set only lsb of every bit run (0b100111100001 -> 0b100000100001)
#define hib64(x) ((x) & 0x8000000000000000ULL) // x & 0b10...0 (msb at bit index 64)

#define dhttp_attr(v)

namespace dhttp
{
    using u64_t = uint64_t;
    using u32_t = uint32_t;
    using u16_t = uint16_t;
    using u8_t  = uint8_t;

    constexpr size_t HEADER_BUF_SIZE_MAX = 16284;
    constexpr size_t HEADER_BUF_SIZE     = 8192;
    constexpr size_t REQUEST_LINE_MAX_SIZE     = 8192;
    constexpr size_t MAX_HEADER          = 64;
    constexpr size_t MAX_LOAD_SIZE       = 64;

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

    enum
    {
        CR  = '\r',
        LF  = '\n',
        COL = ':',
        SP  = ' '
    };

    typedef struct
    {
        uint16_t line_start, line_end;
    } req_t;

    typedef struct
    {
        req_t *req_buf;
        size_t n;
    } reqbuf_t;

    typedef struct
    {
        // TODO: array?
        req_t request_line[4]; // [3] = method, [2] = uri, [1] = version, [0] = 0
    } request_line_t;

    typedef struct
    {
        u8_t * recvbuf;
        size_t capacity;
        size_t overflow;
    } recvbuf_t;

    typedef struct
    {
        request_line_t request;  // request/response line
        reqbuf_t  hf;     // header fields
        recvbuf_t recvb; // buffer
        size_t size;
        bool   done;
    } header_t;

    typedef struct
    {
        __m512i_ v; // keep vector lanes for reuse (helpful in scalar falllback)
        uint16_t line_start, line_end;
        uint16_t pos = 0, j = 0;        // request line field count [0, 3)
        u8_t state = 0;           // header state
        bool decode_once    = true;        // true if decoding of request/status-line is pending (not started)
        bool trailing_sp    = false;       // carry last trailing sp as a single bit boolean
        bool done  = false;
    } state_t;

    namespace tables {
    /* High/low bitmap of characters: !, #, \$, %, &, ', *, +, -, .,
                                      ^, _, `, |, A-Za-z0-9, :, /, ?, #, 
                                      [, ], @, !, $, &, ', (, ), *, +, ,
                                      , ;, = 
    */
    alignas(64) uint8_t bitmap128_valid_request_charset_shufb[128]{
        // lo nibbles
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // hi nibbles
        0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    alignas(64) uint8_t bitmap256_valid_request_charset_shufb[256]{
        // lo nibbles
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // hi nibbles
        0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    alignas(64) uint8_t bitmap256_valid_request_charset[256]{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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
#define HAVE_SHUFFLE__ 1
#elif HAVE__SSE2__
        typedef struct alignas(32)
        {
            __m128i lo, hi;
        } __m256i;

        dhttp_attr(inline) __m256i _mm256_loadu_si256(const void *b)
        {
            return {_mm_loadu_si128(static_cast<const __m128i *>(b)), _mm_loadu_si128(static_cast<const __m128i *>(b) + 1)};
        }

        dhttp_attr(inline) __m256i _mm256_set1_epi8(const u8_t v)
        {
            return {_mm_set1_epi8(v), _mm_set1_epi8(v)};
        }

        dhttp_attr(inline) u32_t _mm256_movemask_epi8(const __m256i x)
        {

            return static_cast<u32_t>(_mm_movemask_epi8(x.hi)) << 16 | _mm_movemask_epi8(x.lo);
        }

        dhttp_attr(inline) __m256i _mm256_cmpgt1_epi8(const __m256i v, const u8_t a)
        {
            static __m128i x = _mm_set1_epi8(a);
            return {_mm_cmpgt_epi8(v.lo, x), _mm_cmpgt_epi8(v.hi, x)};
        }

        dhttp_attr(inline) __m256i _mm256_cmpglt_epi8(const __m256i v, const u8_t a, const u8_t b)
        {
            static __m128i x = _mm_set1_epi8(a);
            static __m128i y = _mm_set1_epi8(b);
            return {
                _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
                _mm_and_si128(_mm_cmpgt_epi8(v.lo, x), _mm_cmplt_epi8(v.lo, y)),
            };
        }

        dhttp_attr(inline) __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
        {
            return {_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.hi, v.hi)};
        }

        dhttp_attr(inline) __m256i _mm256_and_si256(const __m256i u, const __m256i v)
        {
            return {_mm_and_si128(u.lo, v.lo), _mm_and_si128(u.hi, v.hi)};
        }

        dhttp_attr(inline) __m256i _mm256_srli_epi32(const __m256i u, const int r)
        {
            return {_mm_srli_epi16(u.lo, r), _mm_srli_epi16(u.hi, r)}; // TODO
        }

        dhttp_attr(inline) bool _mm256_testz_si256(const __m256i u)
        {
#ifdef HAVE__SSE4_2__
            return _mm_test_all_zeros(u.lo, u.lo) or _mm_test_all_zeros(u.hi, u.hi);
#else
            static const __m128i z = _mm_setzero_si128();
            return static_cast<bool>(_mm_movemask_epi8(_mm_and_si128(_mm_cmpeq_epi8(u.lo, z), _mm_cmpeq_epi8(u.hi, z))));
#endif
        }

#if HAVE__SSSE3__
#define HAVE_SHUFFLE__
        dhttp_attr(inline) __m256i _mm256_shuffle_epi8(const __m256i u, const __m256i x)
        {
            return {_mm_shuffle_epi8(u.lo, x.lo), _mm_shuffle_epi8(u.hi, x.hi)};
        }
#endif
#elif HAVE__ARM_NEON__
// TODO
#elif HAVE__INT128__
        typedef struct alignas(32)
        {
            __uint128_t lo, hi;
        } __m256i;

        dhttp_attr(inline) __m256i _mm256_loadu_si256(const void *b)
        {
            const __uint128_t *x = reinterpret_cast<const __uint128_t *>(b);
            return {x[0], x[1]};
        }

        constexpr dhttp_attr(inline) __m256i _mm256_set1_epi8(const u8_t v)
        {
            static constexpr __uint128_t c = U128(0x101010101010101ULL) << 64 | 0x101010101010101ULL;
            return {U128(v) * c, U128(v) * c};
        }

        dhttp_attr(inline) uint16_t _mm256_movemask_epi8(__m256i const u)
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

        dhttp_attr(inline) __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
        {
            static constexpr __uint128_t c1 = U128(0x1000100010001000ULL) << 64 | 0x0100010001000100ULL;
            static constexpr __uint128_t c2 = U128(0x1000100010001000ULL) << 64 | 0x0001000100010001ULL;
            static constexpr __uint128_t c3 = U128(0x8080808080808080ULL) << 64 | 0x8080808080808080ULL;

            return {
                ((((u.lo ^ v.lo) | c2) - c1) | (((u.lo ^ v.lo) | c1) - c2)) & (~(u.lo ^ v.lo) & c3),
                ((((u.hi ^ v.hi) | c2) - c1) | (((u.hi ^ v.hi) | c1) - c2)) & (~(u.hi ^ v.hi) & c3),
            };
        }

        constexpr dhttp_attr(inline) u64_t _mm256_cmpgt1_epi8(const __m256i v, const u8_t a)
        {
            static constexpr u64_t c = 0x8080808080808080ULL;
            const u64_t x = (0x7f - a) * 0x101010101010101ULL;
           
            return {((v.lo + x) | v.lo) & c, ((v.hi + x) | v.hi) & c};
        }
        constexpr dhttp_attr(inline) __uint128_t _mm256_cmpglt_epi8(const __m256i v, const u8_t a, const u8_t b)
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

        dhttp_attr(inline) __m256i _mm256_and_si256(const __m256i u, const __m256i v)
        {
            return {u.lo & v.lo, u.hi & v.hi};
        }

        dhttp_attr(inline) __m256i _mm256_srli_epi32(const __m256i u, const int r)
        {
            return {u.lo >> r, u.hi >> r}; // TODO
        }

        dhttp_attr(inline) bool _mm256_testz_si256(const __m256i u)
        {
            return u.lo or u.hi;
        }
#else
#define HAVE__128__ 1
        typedef struct alignas(32)
        {
            u64_t lo, vlo, hi, vhi;
        } __m256i;

        dhttp_attr(inline) __m256i _mm256_loadu_si256(void const *b)
        {
            const u64_t *x = reinterpret_cast<const u64_t *>(b);
            return {x[0], x[1], x[2], x[3]};
        }

        dhttp_attr(inline) __m256i _mm256_set1_epi8(const u8_t v)
        {
            const u64_t x = U64(v) * 0x101010101010101ULL;
            return {x, x, x, x};
        }

        dhttp_attr(inline) u32_t _mm256_movemask_epi8(const __m256i u)
        {
            static constexpr u64_t mp = 0x0002040810204081ULL;
            static constexpr u64_t c = 0x8080808080808080ULL;

#if defined(__BMI2__) && HAVE_USE_PEXT__
            const u32_t x = _pext_u64(u.lo, c) << 8 | _pext_u64(u.vlo, c);
            const u32_t y = _pext_u64(u.hi, c) << 8 | _pext_u64(u.vhi, c);
#else
            const u16_t x = ((((u.lo * mp) >> 48) & 0xff00ULL) | ((u.vlo * mp) >> 56));
            const u32_t y = ((((u.hi * mp) >> 48) & 0xff00ULL) | ((u.vhi * mp) >> 56));
#endif
            return y << 16 | x;
        }

        constexpr dhttp_attr(inline) u64_t _mm256_cmpgt1_epi8(const __m256i v, const u64_t a)
        {
            static constexpr u64_t c = 0x8080808080808080ULL;
            const u64_t x = (0x7f - a) * 0x101010101010101ULL;
           
            return {
                (((v.lo + x) | v.lo) & c), (((v.vlo + x) | v.vlo) & c),
                (((v.hi + x) | v.hi) & c), (((v.vhi + x) | v.vhi) & c),
            };
        }

        constexpr dhttp_attr(inline) u64_t _mm256_cmpglt_epi8(const __m256i v, const u64_t a, const u64_t b)
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

        dhttp_attr(inline) __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
        {
            static constexpr u64_t c1  = 0x0100010001000100ULL; // even bits
            static constexpr u64_t c2 = 0x0001000100010001ULL;  // odd bits
            static constexpr u64_t c3   = 0x8080808080808080ULL;

            return {
                       ((((u.lo  ^ v.lo)  | c2) - c1) | (((u.lo  ^ v.lo)  | c1) - c2)) & (~(u.lo  ^ v.lo)  & c3),
                       ((((u.vlo ^ v.vlo) | c2) - c1) | (((u.vlo ^ v.vlo) | c1) - c2)) & (~(u.vlo ^ v.vlo) & c3),
                       ((((u.hi  ^ v.hi)  | c2) - c1) | (((u.hi  ^ v.hi)  | c1) - c2)) & (~(u.hi  ^ v.hi)  & c3),
                       ((((u.vhi ^ v.vhi) | c2) - c1) | (((u.vhi ^ v.vhi) | c1) - c2)) & (~(u.vhi ^ v.vhi) & c3),
                   };
        }

        dhttp_attr(inline) __m256i _mm256_and_si256(const __m256i u, const __m256i v)
        {
            return {u.lo & v.lo, u.vlo & v.vlo, u.hi & v.hi, u.vhi & v.vhi};
        }

        dhttp_attr(inline) __m256i _mm256_srli_epi32(const __m256i u, const int r)
        {
            return {u.lo >> r, u.vlo >> r, u.hi >> r, u.vhi >> r};
        }

        dhttp_attr(inline) bool _mm256_testz_si256(const __m256i u)
        {
            return u.lo or u.vlo or u.hi or u.vhi;
        }
#endif

#ifndef HAVE_AVX512_EMU
        typedef struct alignas(64)
        {
            __m256i lo, hi;
        } __m512i_;

        dhttp_attr(inline) __m512i_ _mm512_loadu_si512_(const void *b)
        {
            return {_mm256_loadu_si256(static_cast<const __m256i *>(b)), _mm256_loadu_si256(static_cast<const __m256i *>(b) + 1)};
        }

        dhttp_attr(inline) __m512i_ _mm512_set1_epi8_(const u8_t v)
        {
            return {_mm256_set1_epi8(v), _mm256_set1_epi8(v)};
        }

        dhttp_attr(inline) u64_t _mm512_movemask_epi8_(const __m512i_ u)
        {

            return U64(_mm256_movemask_epi8(u.hi)) << 32 | _mm256_movemask_epi8(u.lo);
        }

        dhttp_attr(inline) constexpr __m512i_ _mm512_cmpgt1_epi8_(const __m512i_ v, const u8_t a)
        {
            return {_mm256_cmpgt1_epi8(v.lo, a), _mm256_cmpgt1_epi8(v.hi, a)};
        }

        dhttp_attr(inline) constexpr __m512i_ _mm512_cmpglt_epi8_(const __m512i_ v, const u8_t a, const u8_t b)
        {
            return {_mm256_cmpglt_epi8(v.lo, a, b), _mm256_cmpglt_epi8(v.hi, a, b)};
        }

        dhttp_attr(inline) __m512i_ _mm512_cmpeq_epi8_(const __m512i_ u, const __m512i_ v)
        {
            return {_mm256_cmpeq_epi8(u.lo, v.lo), _mm256_cmpeq_epi8(u.hi, v.hi)};
        }

        dhttp_attr(inline) __m512i_ _mm512_and_si512_(const __m512i_ u, const __m512i_ v)
        {
            return {_mm256_and_si256(u.lo, v.lo), _mm256_and_si256(u.hi, v.hi)};
        }

        dhttp_attr(inline) __m512i_ _mm512_srli_epi64_(const __m512i_ u, const int r)
        {
            return {_mm256_srli_epi32(u.lo, r), _mm256_srli_epi32(u.hi, r)};
        }

        dhttp_attr(inline) bool _mm512_testz_si512_(const __m512i_ u)
        {
            return _mm256_testz_si256(u.lo) or _mm256_testz_si256(u.hi);
        }

#ifdef HAVE_SHUFFLE__
        dhttp_attr(inline) __m512i_ _mm512_shuffle_epi8_(const __m512i_ u, const __m512i_ v)
        {
            return {_mm256_shuffle_epi8(u.lo, v.lo), _mm256_shuffle_epi8(u.hi, v.hi)};
        }
#endif
#endif
    };