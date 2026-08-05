#include "dm.hxx"

#ifdef M
#undef M
#endif
#ifdef H
#undef H
#endif
#define U64(x) static_cast<const uint64_t>(x)
#define U32(x) static_cast<const uint32_t>(x)

#define lsb(x)   ((x) & -(x))                    // lsb
#define trun(x)  ((x) & ~((x) << 1))            // set only lsb of every bit run (0b100111100001 -> 0b100000100001)
#define hib64(x) ((x) & 0x8000000000000000ULL) // x & 0b10...0 (msb at bit index 64)

namespace Http
{
    using u64_t = uint64_t;
    using u32_t = uint32_t;
    using u16_t = uint16_t;
    using u8_t  = uint8_t;

    constexpr size_t HTTP_HEADER_BUF_SIZE_MAX = 16284;
    constexpr size_t HTTP_HEADER_BUF_SIZE     = 8192;
    constexpr size_t HTTP_RL_MAX_SIZE     = 8192;
    constexpr size_t HTTP_MAX_HEADER          = 64;
    constexpr size_t HTTP_MAX_LOAD_SIZE       = 64;

    enum
    {
        HTTP_COMPLETE          = 0,
        HTTP_INCOMPLETE        = 1,
        HTTP_EXPECT_DATA       = 2,
        HTTP_ERROR             = -1,
        HTTP_MAX_SIZE_EXCEEDED = -2,
        HTTP_RESUME            = 0x80,
        HTTP_RL_INCOMPLETE     = 0x02,
        HTTP_RL_COMPLETE       = 0x01,
        HTTP_RL_MALFORMED      = -1,
        HTTP_STATE_TRAILING_CR = 1
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
    } http_req_t;

    typedef struct
    {
        http_req_t *req_buf;
        size_t n;
    } http_reqbuf_t;

    typedef struct
    {
        // TODO: array?
        http_req_t rl[4]; // [3] = method, [2] = uri, [1] = version, [0] = 0
    } http_reqline_t;

    typedef struct
    {
        u8_t * recvbuf;
        size_t capacity;
        size_t overflow;
    } http_recvbuf_t;

    typedef struct
    {
        http_reqline_t reql;  // request/response line
        http_reqbuf_t  hf;     // header fields
        http_recvbuf_t recvb; // buffer
        size_t size;
        bool   done;
    } http_header_t;

    typedef struct
    {
        __m512i_ v; // keep vector lanes for reuse (helpful in scalar falllback)
        uint16_t line_start, line_end;
        uint16_t i = 0,
                 j = 0;        // request line field count [0, 3)
        u8_t state = 0;        // header state
        u8_t sz    = 0;           // save size of skipped bytes
        bool st    = true;        // true if decoding of request/status-line is pending (not started)
        bool sp    = false;       // carry last trailing sp as a single bit boolean
        bool done  = false;
    } http_state_t;

    // Bit nibble of all accepted rfc9110 spec: !, #, \$, %, &, ', *, +, -, ., ^, _, `, |, A-Za-z0-9, :, /, ?, #, [, ], @, !, $, &, ', (, ), *, +, ,, ;, =
    alignas(64) static constexpr uint8_t vrfc_class_tab[128]{
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

    // Valid RFC Request-Line Character Index Table
    alignas(64) static constexpr uint8_t vrci_tab[256]{
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

#if HAVE__AVX2__
#define HAVE_SHUFFLE__ 1
#elif HAVE__SSE2__
        typedef struct alignas(32)
        {
            __m128i lo, hi;
        } __m256i;

        __attribute__((always_inline)) inline __m256i _mm256_loadu_si256(const void *b)
        {
            return {_mm_loadu_si128(static_cast<const __m128i *>(b)), _mm_loadu_si128(static_cast<const __m128i *>(b) + 1)};
        }

        __attribute__((always_inline)) inline __m256i _mm256_set1_epi8(const u8_t c)
        {
            return {_mm_set1_epi8(c), _mm_set1_epi8(c)};
        }

        __attribute__((always_inline)) inline u32_t _mm256_movemask_epi8(const __m256i x)
        {

            return static_cast<u32_t>(_mm_movemask_epi8(x.hi)) << 16 | _mm_movemask_epi8(x.lo);
        }

        __attribute__((always_inline)) inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
        {
            return {_mm_cmpeq_epi8(u.lo, v.lo), _mm_cmpeq_epi8(u.hi, v.hi)};
        }

        __attribute__((always_inline)) inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
        {
            return {_mm_and_si128(u.lo, v.lo), _mm_and_si128(u.hi, v.hi)};
        }

        __attribute__((always_inline)) inline __m256i _mm256_srli_epi32(const __m256i u, const int r)
        {
            return {_mm_srli_epi16(u.lo, r), _mm_srli_epi16(u.hi, r)}; // TODO
        }

        __attribute__((always_inline)) inline bool _mm256_testz_si256(const __m256i u)
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
        __attribute__((always_inline)) inline __m256i _mm256_shuffle_epi8(const __m256i u, const __m256i x)
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

        __attribute__((always_inlne)) __uint128_t _mm_mul_epu64(const u64_t u, const u64_t v)
        {
#if HAVE_GGC_VARIANT__
            return static_cast<const __uint128_t>(u) * v;
#elif HAVE_MSVC__ && (defined(_M_X64) || defined(_M_ARM64))
            u64_t xx = 0;
            u64_t x = _umul128(u, v, &xx);
            return static_cast<const __uint128_t>(x) << 64 | xx;
#elif HAVE_MSVC__ && defined(_M_ARM64)
            return static_cast<const __uint128_t>(_umulh128(u, x)) << 64 | (u * x);
#endif
        }

        __attribute__((always_inline)) inline __m256i _mm256_loadu_si256(const void *b)
        {
            const __uint128_t *x = reinterpret_cast<const __uint128_t *>(b);
            return {x[0], x[1]};
        }

        __attribute__((always_inline)) inline __m256i _mm256_set1_epi8(const u8_t c)
        {
            static constexpr __uint128_t o = (__uint128_t)0x101010101010101ULL << 64 | (__uint128_t)0x101010101010101ULL;
            return {static_cast<const __uint128_t>(c) * o, static_cast<const __uint128_t>(c) * o};
        }

        __attribute__((always_inline)) inline uint16_t _mm256_movemask_epi8(__m256i const u)
        {
            static constexpr u64_t M = 0x0002040810204081ULL;
            static constexpr u64_t H = 0x8080808080808080ULL;

#if defined(__BMI2__) || HAVE_USE_PEXT__
            return static_cast<const u64_t>(_pext_u64(u.hi >> 64, H)) << 48 | _pext_u64(u.hi & 0xffffffffffffffffULL, H) << 32 |
                   static_cast<const u64_t>(_pext_u64(u.lo >> 64, H)) << 16 | _pext_u64(u.lo & 0xffffffffffffffffULL, H);
#else
            return (static_cast<const u64_t>(u.hi >> 64) * M >> 32) & 0xff000000ULL | (static_cast<const u64_t>(u.hi) * M >> 40) & 0xff0000ULL |
                   (static_cast<const u64_t>(u.lo >> 64) * M >> 48) & 0x0000ff00ULL | (static_cast<const u64_t>(u.lo) * M) >> 56;
#endif
        }

        __attribute__((always_inline)) inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
        {
            static constexpr __uint128_t odd  = static_cast<constexpr __uint128_t>(0x1000100010001000ULL) << 64 | 0x100010001000100ULL;
            static constexpr __uint128_t even = static_cast<constexpr __uint128_t>(0x1000100010001000ULL) << 64 | 0x001000100010001ULL;

            __m256i x = {u.lo ^ v.lo, u.hi ^ v.hi};
            x.lo = (((x.lo | even) - odd) | ((x.lo | odd) - even)) & (~(x.lo) & c);
            x.hi = (((x.hi | even) - odd) | ((x.hi | odd) - even)) & (~(x.hi) & c);
            return x;
        }

        __attribute__((always_inline)) inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
        {
            return {u.lo & v.lo, u.hi & v.hi};
        }

        __attribute__((always_inline)) inline __m256i _mm256_srli_epi32(const __m256i u, const int r)
        {
            return {u.lo >> r, u.hi >> r}; // TODO
        }

        __attribute__((always_inline)) inline bool _mm256_testz_si256(const __m256i u)
        {
            return u.lo or u.hi;
        }
#else
#define HAVE__128__ 1
        typedef struct alignas(32)
        {
            u64_t lo, vlo, hi, vhi;
        } __m256i;

        __attribute__((always_inline)) inline __m256i _mm256_loadu_si256(void const *b)
        {
            const u64_t *x = reinterpret_cast<const u64_t *>(b);
            return {x[0], x[1], x[2], x[3]};
        }

        __attribute__((always_inline)) inline __m256i _mm256_set1_epi8(const u8_t c)
        {
            const u64_t x = static_cast<const u64_t>(c) * 0x101010101010101ULL;
            return {x, x, x, x};
        }

        __attribute__((always_inline)) inline u32_t _mm256_movemask_epi8(const __m256i u)
        {
            static constexpr u64_t M = 0x0002040810204081ULL;
            static constexpr u64_t H = 0x8080808080808080ULL;

#if defined(__BMI2__) && HAVE_USE_PEXT__
            const u16_t x = static_cast<const u16_t>(_pext_u64(u.lo, H) << 8 | _pext_u64(u.vlo, H));
            const u16_t y = static_cast<const u16_t>(_pext_u64(u.hi, H) << 8 | _pext_u64(u.vhi, H));
#else
            const u16_t x = static_cast<const u16_t>((((u.lo * M) >> 48) & 0xff00ULL) | ((u.vlo * M) >> 56));
            const u16_t y = static_cast<const u16_t>((((u.hi * M) >> 48) & 0xff00ULL) | ((u.vhi * M) >> 56));
#endif
            return static_cast<const u32_t>(y) << 16 | x;
        }

        __attribute__((always_inline)) inline __m256i _mm256_cmpeq_epi8(const __m256i u, const __m256i v)
        {
            static constexpr u64_t odd  = 0x100010001000100ULL;
            static constexpr u64_t even = 0x001000100010001ULL;

            __m256i x = {u.lo ^ v.lo, u.vlo ^ v.vlo, u.hi ^ v.hi, u.vhi ^ v.vhi};
            x.lo = (((x.lo | even) - odd) | ((x.lo | odd) - even)) & (~(x.lo) & H);
            x.hi = (((x.hi | even) - odd) | ((x.hi | odd) - even)) & (~(x.hi) & H);

            x.vlo = (((x.vlo | even) - odd) | ((x.vlo | odd) - even)) & (~(x.lo) & H);
            x.vhi = (((x.vhi | even) - odd) | ((x.vhi | odd) - even)) & (~(x.hi) & H);

            return x;
        }

        __attribute__((always_inline)) inline __m256i _mm256_and_si256(const __m256i u, const __m256i v)
        {
            return {u.lo & v.lo, u.vlo & v.vlo, u.hi & v.hi, u.vhi & v.vhi};
        }

        __attribute__((always_inline)) inline __m256i _mm256_srli_epi32(const __m256i u, const int r)
        {
            return {u.lo >> r, u.vlo >> r, u.hi >> r, u.vhi >> r};
        }

        __attribute__((always_inline)) inline bool _mm256_testz_si256(const __m256i u)
        {
            return u.lo or u.vlo or u.hi or u.vhi;
        }
#endif

#ifndef HAVE_AVX512_EMU
        typedef struct alignas(64)
        {
            __m256i lo, hi;
        } __m512i_;

        __attribute__((always_inline)) inline __m512i_ _mm512_loadu_si512_(const void *b)
        {
            return {_mm256_loadu_si256(static_cast<const __m256i *>(b)), _mm256_loadu_si256(static_cast<const __m256i *>(b) + 1)};
        }

        __attribute__((always_inline)) inline __m512i_ _mm512_set1_epi8_(const u8_t c)
        {
            return {_mm256_set1_epi8(c), _mm256_set1_epi8(c)};
        }

        __attribute__((always_inline)) inline u64_t _mm512_movemask_epi8_(const __m512i_ u)
        {

            return static_cast<u64_t>(_mm256_movemask_epi8(u.hi)) << 32 | _mm256_movemask_epi8(u.lo);
        }

        __attribute__((always_inline)) inline __m512i_ _mm512_cmpeq_epi8_(const __m512i_ u, const __m512i_ v)
        {
            return {_mm256_cmpeq_epi8(u.lo, v.lo), _mm256_cmpeq_epi8(u.hi, v.hi)};
        }

        __attribute__((always_inline)) inline __m512i_ _mm512_and_si512_(const __m512i_ u, const __m512i_ v)
        {
            return {_mm256_and_si256(u.lo, v.lo), _mm256_and_si256(u.hi, v.hi)};
        }

        __attribute__((always_inline)) inline __m512i_ _mm512_srli_epi64_(const __m512i_ u, const int r)
        {
            return {_mm256_srli_epi32(u.lo, r), _mm256_srli_epi32(u.hi, r)};
        }

        __attribute__((always_inline)) inline bool _mm512_testz_si512_(const __m512i_ u)
        {
            return _mm256_testz_si256(u.lo) or _mm256_testz_si256(u.hi);
        }

#ifdef HAVE_SHUFFLE__
        __attribute__((always_inline)) inline __m512i_ _mm512_shuffle_epi8_(const __m512i_ u, const __m512i_ v)
        {
            return {_mm256_shuffle_epi8(u.lo, v.lo), _mm256_shuffle_epi8(u.hi, v.hi)};
        }
#endif
#endif
    };