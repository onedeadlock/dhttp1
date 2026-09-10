#pragma once

#include "../include/definition.hpp"

namespace dhttp::common::constant
{
    static constexpr std::size_t int_size   = sizeof (u64_t);
    static constexpr std::size_t int_size_p = (int_size / 2) - 1;

    constexpr u64_t c7f = 0x7f7f7f7f7f7f7f7fULL;
    constexpr u64_t cff = 0xffffffffffffffffULL;
    constexpr u64_t c80 = 0x8080808080808080ULL;
    constexpr u64_t c01 = 0x0101010101010101ULL;
    constexpr u64_t c09 = 0x0909090909090909ULL;
    constexpr u64_t c20 = 0x2020202020202020ULL;
    constexpr u64_t c30 = 0x3030303030303030ULL;
    constexpr u64_t cdf = 0xdfdfdfdfdfdfdfdfULL;

    constexpr u64_t compress  = 0x0002040810204081ULL;

    constexpr u64_t msb_64   = 0x8000000000000000ULL;
    constexpr u64_t msb_32   = 0x0000000080000000ULL;

    constexpr u64_t msb3_64  = 0x8000000000000000ULL;
    constexpr u64_t msb3_32  = 0x0000000080000000ULL;
    

    constexpr u64_t  hyphen = U64('\x2d') * c01;
    
    constexpr u64_t AZ_const = c7f & cdf;
    constexpr u64_t A = U64('\x7f' - '\x40') * c01;
    constexpr u64_t Z = U64('\x7f' + '\x5b') * c01;

    constexpr u64_t DeBruijn64_const = 0x03f79d71b4cb0a89ULL;

    static constexpr u8_t DeBruijn64_seq[64]{
        0,  47, 1,  56, 48, 27, 2,  60,
        57, 49, 41, 37, 28, 16, 3,  61,
        54, 58, 35, 52, 50, 42, 21, 44,
        38, 32, 29, 23, 17, 11, 4,  62,
        46, 55, 26, 59, 40, 36, 15, 53,
        34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30, 9,  24,
        13, 18, 8,  12, 7,  6,  5,  63};
}

namespace dhttp::common::scalar
{
    #if __GNUC__
    #endif

    inline constexpr u64_t _dup(u8_t v)
    {
        return U64(v) * constant::c01;
    }

    inline u64_t _cmpeqz(u64_t v)
    {
        return ~(v | ((v & constant::c7f) + constant::c7f)) & constant::c80;
    }

    inline u64_t _cmpeqz_(u64_t v)
    {
        return ((v & constant::c7f) - constant::c01) & ~v & constant::c80;
    }

    inline u64_t _cmpeq(u64_t u, u64_t v)
    {
        return _cmpeqz(u ^ v);
    }

    inline u64_t _cmpeq(u64_t u, u64_t v, u64_t w)
    {
        return _cmpeqz((u ^ v) | (u ^ w));
    }

    inline u64_t _cmpgtz(u64_t v)
    {
        return (v | ((v & constant::c7f) + constant::c7f)) & constant::c80;
    }

    template<u8_t A>
    inline u64_t _cmplt(u64_t v)
    {
        static_assert(A < 0x7f);

        static constexpr u64_t a = _dup(0x7f + A);
        return (a - (v & constant::c7f)) & (~v & constant::c80);
    }

    template<u8_t A>
    inline u64_t _cmpgt(u64_t v)
    {
        static_assert(A < 0x7f);

        static constexpr u64_t a = _dup(0x7f - A);
        return (v | (a + (v & constant::c7f))) & constant::c80;
    }

    template <u8_t A, u8_t B>
    inline u64_t _cmp_gt_and_lt(u64_t v)
    {
        static_assert(A < 0x7f && B < 0x80);

        static constexpr u64_t a = _dup(0x7f - A);
        static constexpr u64_t b = _dup(0x7f + B);
        return (b - (v & constant::c7f)) & (a + (v & constant::c7f)) & (~v & constant::c80);
    }

    inline u64_t ascii_letters(u64_t v)
    {
        return (constant::Z - (v & constant::AZ_const)) & (constant::A + (v & constant::AZ_const)) & (~v & constant::c80);
    }

    inline u64_t ascii_numbers_v(u64_t v)
    {
        return _cmp_gt_and_lt<'\x2f', '\x3a'>(v);
    }

    inline u64_t ascii_numbers(u64_t v)
    {
        return _cmplt<10>(v ^ constant::c30);
    }

    inline u64_t ascii_hyphen(u64_t v)
    {
        return _cmpeqz(v ^ constant::hyphen);
    }

    inline u64_t ascii_fast_tchar(const u64_t v)
    {
        return ascii_letters(v) | ascii_numbers(v) | ascii_hyphen(v);
    }

    inline u64_t non_printable(u64_t v)
    {
        // Non printable characters here are 0x7f (DEL) or characters below 0x20 (sp)
        static constexpr u64_t u = constant::c80 | constant::c20;
        return (u - (((v & constant::c7f) + constant::c01) & constant::c7f)) & (~v & constant::c80);
    }
}