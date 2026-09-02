#pragma once
#include "common.hpp"
#if HAVE_VIST_C_
#   include <immintrin.h>
#endif

namespace dhttp::common::bits
{
    inline u64_t lsb(u64_t x)
    {
        return x & -x;
    }

    inline u64_t trim(u64_t x)
    {
        return x & ~(x << 1);
    }

    inline u64_t trim_u(u64_t x)
    {
        return x & ~(x >> 1);
    }

    inline u64_t tzmask(u64_t x)
    {
        return ~x & (x - 1);
    }

    inline u64_t blsmask(u64_t x)
    {
        return x ^ (x - 1);
    }

    inline u64_t blsr(u64_t x)
    {
        return x & (x - 1);
    }

    inline u64_t blsfill(u64_t x)
    {
        return x | (x - 1);
    }

    inline u64_t xlsfill(u64_t x)
    {
        // xlsfiil is not an actual instruction, but it does the opposite of blsfill
        return x ^ -x;
    }

    inline u64_t tzcnt(u64_t x)
    {
#if HAVE_GNUC_C__
        return __builtin_ctzll(x);
#elif HAVE_VIST_C__
        u64_t vx;
        _BitScanReverse(&vx, x);
        return vx;
#else
            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            x |= x >> 32;
            return constant::DeBruijn64_seq[(x * constant::DeBruijn64_const) >> 58];
#endif
    }

    #if HAVE_GNUC_C__
    __attribute__((const, optimize("no-if-conversion")))
    #endif
    inline u64_t _tzcnt_x(umax_t x)
    {
        if constexpr (constant::max_int_size == 16)
            return static_cast<u64_t>(x) ? tzcnt(static_cast<u64_t>(x)) : 64 + tzcnt(static_cast<u64_t>(x >> 64));
        return tzcnt(static_cast<u64_t>(x));
    }
}