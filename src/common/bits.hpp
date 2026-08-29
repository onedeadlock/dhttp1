#pragma once
#include "common.hpp"

namespace Dhttp::common::bits
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
        // xlsfiil is not an actual instrinsic, but it does the opposite of blsfill
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
        static constexpr u8_t DBT64[64]{
            0,  47, 1,  56, 48, 27, 2,  60,
            57, 49, 41, 37, 28, 16, 3,  61,
            54, 58, 35, 52, 50, 42, 21, 44,
            38, 32, 29, 23, 17, 11, 4,  62,
            46, 55, 26, 59, 40, 36, 15, 53,
            34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30, 9,  24,
            13, 18, 8,  12, 7,  6,  5,  63};

            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            x |= x >> 32;

            return DBT64[(x * 0x03f79d71b4cb0a89ULL) >> 56];
#endif
    }
}