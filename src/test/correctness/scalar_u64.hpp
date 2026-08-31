#include <cstdint>
#include <cstddef>

#define U32(x)  static_cast<const u32_t>(x)
#define U64(x)  static_cast<const u64_t>(x)

using u64_t = std::uint64_t;
using u32_t = std::uint32_t;
using u16_t = std::uint16_t;
using u8_t  = std::uint8_t;

namespace scalar
{
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
}