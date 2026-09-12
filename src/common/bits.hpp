#pragma once
#include "common.hpp"
#if __HAVE_MSVC__
#   include <intrin.h>
#elif __GNUC__
#include <x86intrin.h>
#endif

#ifdef __HAVE_MSVC__
#endif
namespace dhttp::common::bits
{
    template <typename T>
    inline T lsb(T x)
    {
        return x & -x;
    }

    template <typename T>
    make_flat inline T trim(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
            return __andn_u64(x, x << 1);
        return x & ~(x << 1);
    }

    template <typename T>
    make_flat inline T trim_u(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
             return __andn_u64(U64(x), U64(x) >> 1);
        return x & ~(x >> 1);
    }

    template <typename T>
    make_flat inline T tzmask(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
             return __andn_u64(U64(x) - 1, U64(x));
        return ~x & (x - 1);
    }

    template <typename T>
    inline T blsmask(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
             return __blsmsk_u64(U64(x));
        return x ^ (x - 1);
    }

    template <typename T>
    inline T blsr(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
             return __blsr_u64(U64(x));
        return x & (x - 1);
    }

    template <typename T>
    inline T blsfill(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
             return __blsfill_u64(U64(x));
        return x | (x - 1);
    }

    template <typename T>
    inline T xlsfill(T x)
    {
        if constexpr (__HAVE_MSVC__ or __HAVE_GNUC__)
             return ~__blsfill_u64(U64(x));
        return x ^ -x;
    }

    template <typename T>
    inline T tzcnt(T x)
    {
        if constexpr (sizeof (T) == 32)
        {
#if defined(_tzcnt_u32) || defined(__HAVE_MSVC__)
            return _tzcnt_u32(x);
#elif __HAVE_GNUC__
        return __builtin_ctz(x);
#else
            x |= x >> 1; x |= x >> 2;
            x |= x >> 4; x |= x >> 8;
            return constant::DeBruijn32_seq[((x | x >> 16) * constant::DeBruijn32_const) >> 24];
#endif
        }

#if defined(_tzcnt_u64) || defined(__HAVE_MSVC__)
            return _tzcnt_u64(x);
#elif __HAVE_GNUC__
        return __builtin_ctzll(x);
#else
            x |= x >> 1;  x |= x >> 2;
            x |= x >> 4;  x |= x >> 8;
            x |= x >> 16; x |= x >> 32;
            return constant::DeBruijn64_seq[(x * constant::DeBruijn64_const) >> 58];
#endif
    }
}