#pragma once
#include "include/definition.hpp"
#include "common/common.hpp"

namespace dhttp::simd::fallback
{
    using namespace dhttp::common;

    template <int N> alignas(N) struct simdv;

    template<>
    alignas(32) struct simdv<32>
    {
        static constexpr int size = 32;

        u64_t lo, xlo, hi, xhi;

        simdv(void) {}
        simdv(const simdv &v) : lo{v.lo},
                                xlo{v.xlo},
                                hi{v.hi},
                                xhi{v.xhi} {}
        simdv(u64_t w, u64_t x, u64_t y, u64_t z) : lo {w},
                                                    xlo{x},
                                                    hi {y},
                                                    xhi{z} {}

        
        inline bool is_zero(void)
        {
            return static_cast<bool>(lo | xlo | hi | xhi);
        }

        inline u64_t to_bitmask(void)
        {
            const u32_t x = ((((xlo * constant::compress) >> 48) & 0xff00ULL) | ((lo * constant::compress) >> 56));
            const u32_t y = ((((xhi * constant::compress) >> 48) & 0xff00ULL) | ((hi * constant::compress) >> 56));
            return y << 16 | x;
        }

        static inline simdv load(void *b)
        {
            simdv v;
            if constexpr(__GNUC__)
                __builtin_memcpy(&v, b, 32);
            else
                std::memcpy(&v, b, 32);
            return v;
        }

        make_flat static inline simdv splat(u8_t v)
        {
            u64_t x = scalar::_dup(v);
            return {x, x, x, x};
        }


        static inline u64_t bitmask(const simdv& v)
        {
            const u32_t x = ((((v.lo * constant::compress) >> 48) & 0xff00ULL) | ((v.xlo * constant::compress) >> 56));
            const u32_t y = ((((v.hi * constant::compress) >> 48) & 0xff00ULL) | ((v.xhi * constant::compress) >> 56));
            return y << 16 | x;
        }

        make_flat static inline simdv cmp_zero(const simdv& v)
        {
            return {scalar::_cmpeqz(v.lo ),
                    scalar::_cmpeqz(v.xlo),
                    scalar::_cmpeqz(v.hi ),
                    scalar::_cmpeqz(v.xhi)};
        }

        make_flat static inline simdv cmp_eq(const simdv& u, const simdv& v)
        {
            return {scalar::_cmpeq(u.lo,  v.lo ),
                    scalar::_cmpeq(u.xlo, v.xlo),
                    scalar::_cmpeq(u.hi,  v.hi ),
                    scalar::_cmpeq(u.xhi, v.xhi)};
        }

        make_flat static inline simdv cmp_eq(const simdv& u, const simdv& v, const simdv& w)
        {
            return {
                scalar::_cmpeq(u.lo, v.lo ) | scalar::_cmpeq(u.lo, w.lo ),
                scalar::_cmpeq(u.lo, v.xlo) | scalar::_cmpeq(u.lo, w.xlo),
                scalar::_cmpeq(u.lo, v.hi ) | scalar::_cmpeq(u.lo, w.hi ),
                scalar::_cmpeq(u.lo, v.xhi) | scalar::_cmpeq(u.lo, w.xhi)};
        }

        make_flat static inline simdv cmp_gt(const simdv& v, u8_t a)
        {
            u64_t x = scalar::_dup(a);
            return {scalar::_cmpgt<0>(v.lo ),
                    scalar::_cmpgt<0>(v.xlo),
                    scalar::_cmpgt<0>(v.hi ),
                    scalar::_cmpgt<0>(v.xhi)};
        }

        static inline simdv cmp_gt(const simdv& u, const simdv& v)
        {
            return {0, 0, 0, 0};
        }

        static inline simdv cmp_lt(const simdv& v, u8_t a)
        {
            u64_t x = scalar::_dup(a);
            return {0, 0, 0, 0};
        }

        static inline simdv cmp_lt(const simdv& u, const simdv& v)
        {
            return {0, 0, 0, 0};
        }

        static inline simdv gt_and_lt(const simdv& v, u8_t a, u8_t b)
        {
            u64_t x = scalar::_dup(a);
            u64_t y = scalar::_dup(b);
            return {0, 0, 0, 0};
        }

        static inline simdv gt_and_lt(const simdv& u, const simdv& v, const simdv& w)
        {
            return {0, 0, 0, 0};
        }

        static inline simdv _and(const simdv& u, const simdv& v)
        {
            return {u.lo  & v.lo,
                    u.xlo & v.xlo,
                    u.hi  & v.hi,
                    u.xhi & v.xhi};
        }

        static inline simdv _or(const simdv& u, const simdv& v)
        {
            return {u.lo  | v.lo,
                    u.xlo | v.xlo,
                    u.hi  | v.hi,
                    u.xhi | v.xhi};
        }

        static inline simdv lshift(const simdv& u, int r)
        {
            return {0, 0, 0, 0};
        }

        static inline bool is_zero(const simdv& u)
        {
            return static_cast<bool>(u.lo | u.xlo | u.hi | u.xhi);
        }

        static inline simdv shuffle(const simdv &u, const simdv &x)
        {
            return {0, 0, 0, 0};
        }
    };

    template<>
    alignas(64) struct simdv<64>
    {
        static constexpr int size = 64;
    
        simdv<32> lo, hi;

        
        make_flat inline bool is_zero(void)
        {
            return lo.is_zero() or hi.is_zero();
        }

        make_flat inline u32_t to_bitmask(void)
        {
            return hi.to_bitmask() << 32 | lo.to_bitmask();
        }

        make_flat static inline simdv load(void *b)
        {
            return {simdv<32>::load(reinterpret_cast<__m128i *>(b)),
                    simdv<32>::load(reinterpret_cast<__m128i *>(reinterpret_cast<u8_t *>(b) + 32))};
        }

        make_flat static inline simdv splat(u8_t v)
        {
            return {simdv<32>::splat(v), simdv<32>::splat(v)};
        }

        make_flat static inline u64_t bitmask(const simdv& x)
        {

            return simdv<32>::bitmask(x.hi) << 32 | simdv<32>::bitmask(x.lo);
        }

        make_flat static inline simdv cmp_zero(const simdv& v)
        {
            return {simdv<32>::cmp_zero(v.lo),
                    simdv<32>::cmp_zero(v.hi)};
        }

        make_flat static inline simdv cmp_eq(const simdv& u, const simdv& v)
        {
            return {simdv<32>::cmp_eq(u.lo, v.lo),
                    simdv<32>::cmp_eq(u.hi, v.hi)};
        }

        make_flat static inline simdv cmp_eq(const simdv& u, const simdv& v, const simdv& w)
        {
            return {
                simdv<32>::_or(simdv<32>::cmp_eq(u.lo, v.lo), simdv<32>::cmp_eq(u.lo, w.lo)),
                simdv<32>::_or(simdv<32>::cmp_eq(u.hi, v.hi), simdv<32>::cmp_eq(u.hi, w.hi)),
            };
        }

        make_flat static inline simdv cmp_gt(const simdv& v, u8_t a)
        {
            simdv<32> x = simdv<32>::splat(a);
            return {simdv<32>::cmp_eq(v.lo, x), simdv<32>::cmp_eq(v.hi, x)};
        }

        make_flat static inline simdv cmp_gt(const simdv& u, const simdv& v)
        {
            return {simdv<32>::cmp_gt(u.lo, v.hi),
                    simdv<32>::cmp_gt(u.hi, v.hi)};
        }

        make_flat static inline simdv cmp_lt(const simdv& v, u8_t a)
        {
            simdv<32> x = simdv<32>::splat(a);
            return {simdv<32>::cmp_lt(v.lo, x), simdv<32>::cmp_lt(v.hi, x)};
        }

        make_flat static inline simdv cmp_lt(const simdv& u, const simdv& v)
        {
            return {simdv<32>::cmp_lt(v.lo, v.lo), simdv<32>::cmp_lt(v.hi, v.hi)};
        }

        make_flat static inline simdv gt_and_lt(const simdv& v, u8_t a, u8_t b)
        {
            simdv<32> x = simdv<32>::splat(a);
            simdv<32> y = simdv<32>::splat(b);
            return {
                simdv<32>::_and(simdv<32>::cmp_gt(v.lo, x), simdv<32>::cmp_lt(v.lo, y)),
                simdv<32>::_and(simdv<32>::cmp_gt(v.hi, x), simdv<32>::cmp_lt(v.hi, y)),
            };
        }

        make_flat static inline simdv gt_and_lt(const simdv& u, const simdv& v, const simdv& w)
        {
             return {
                simdv<32>::_and(simdv<32>::cmp_gt(u.lo, v.lo), simdv<32>::cmp_lt(u.lo, w.lo)),
                simdv<32>::_and(simdv<32>::cmp_gt(u.hi, v.hi), simdv<32>::cmp_lt(u.hi, w.hi)),
            };
        }

        make_flat static inline simdv andl(const simdv&u, simdv &v)
        {
            return {simdv<32>::_and(u.lo, v.lo), simdv<32>::_and(u.hi, v.hi)};
        }

        make_flat static inline simdv orl(const simdv& u, const simdv& v)
        {
            return {simdv<32>::_or(u.lo, v.lo),
                    simdv<32>::_or(u.hi, v.hi)};
        }

        make_flat static inline simdv lshift(const simdv& u, int r)
        {
            return {simdv<32>::lshift(u.lo, r),
                    simdv<32>::lshift(u.hi, r)};
        }

        make_flat static inline bool is_zero(const simdv& u)
        {
             return simdv<32>::is_zero(u.lo) or simdv<32>::is_zero(u.hi);
        }

        make_flat static inline simdv shuffle(const simdv& u, const simdv& x)
        {
            return {simdv<32>(0, 0, 0, 0), simdv<32>(0, 0, 0, 0)};
        }
    };
}