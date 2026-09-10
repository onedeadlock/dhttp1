#pragma once
#include "common.hpp"
#if HAVE_VIST_C_
#   include <immintrin.h>
#endif

namespace dhttp::common::bits
{
    template <typename T=u64_t>
    inline T lsb(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return x & -x;
    }

    template <typename T=u64_t>
    inline T trim(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return x & ~(x << 1);
    }

    template <typename T=u64_t>
    inline T trim_u(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return x & ~(x >> 1);
    }

    template <typename T=u64_t>
    inline T tzmask(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return ~x & (x - 1);
    }

    template <typename T=u64_t>
    inline T blsmask(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return x ^ (x - 1);
    }

    template <typename T=u64_t>
    inline T blsr(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return x & (x - 1);
    }

    template <typename T=u64_t>
    inline T blsfill(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        return x | (x - 1);
    }

    template <typename T=u64_t>
    inline T xlsfill(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        // xlsfiil is not an actual instruction, but it does the opposite of blsfill
        return x ^ -x;
    }

    template <typename T=u64_t>
    inline T tzcnt(T x)
    {
        static_assert(std::is_integral_v<T> and !std::is_signed_v<T>);
        if constexpr (sizeof (T) == 8)
        {
#if __GNUC__
        return __builtin_ctzll(x);
#elif __MSVC__
        T vx;
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
#if __GNUC__
        return __builtin_ctz(static_cast<u32_t>(x));
#elif __MSVC__
        u32_t vx;
        _BitScanReverse32(&vx, static_cast<u32_t>(x));
        return vx;
#else
            u32_t v = static_cast<u32_t>(v);
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            return constant::DeBruijn32_seq[(v * constant::DeBruijn32_const) >> 24]; // TODO: DB table 32
#endif
    }
}