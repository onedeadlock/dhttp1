#pragma once

#include "../include/definiton.hpp"

namespace dhttp::common::constant
{
    static constexpr std::size_t intmaxWidth = sizeof (umax_t);

    constexpr umax_t c7f = 0x7f7f7f7f7f7f7f7fULL;
    constexpr umax_t c80 = 0x8080808080808080ULL;
    constexpr umax_t c01 = 0x0101010101010101ULL;
    constexpr umax_t c20 = 0x2020202020202020ULL;
    constexpr umax_t msb_64  = 0x8000000000000000ULL;
    constexpr umax_t max_c7f = UMAX_C(c7f) << 64 | c7f;
    constexpr umax_t max_c80 = UMAX_C(c80) << 64 | c80;
    constexpr umax_t max_c01 = UMAX_C(c01) << 64 | c01;
    constexpr umax_t max_c20 = UMAX_C(c20) << 64 | c20;

    constexpr u64_t  hyphen = UMAX('\x2d') * max_c01;
    
    constexpr u64_t AZ_const = max_c7f & 0xdfdfdfdfdfdfdfdfULL;
    constexpr u64_t A = UMAX('\x7f' - '\x40') * max_c01;
    constexpr u64_t Z = UMAX('\x7f' + '\x5b') * max_c01;

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
    inline constexpr umax_t _dup(u8_t v)
    {
        return UMAX(v) * constant::max_c01;
    }

    inline umax_t _cmpeqz(umax_t v)
    {
        return ~(v | ((v & constant::max_c7f) + constant::max_c7f)) & constant::max_c80;
    }

    inline umax_t _cmpeq(umax_t u, umax_t v)
    {
        return _cmpeqz(u ^ v);
    }

    inline umax_t _cmpeq(umax_t u, umax_t v, umax_t w)
    {
        return _cmpeqz((u ^ v) | (u ^ w));
    }

    inline umax_t _cmpgtz(umax_t v)
    {
        return (v | ((v & constant::max_c7f) + constant::max_c7f)) & constant::max_c80;
    }

    template<u8_t A>
    inline umax_t _cmplt(umax_t v)
    {
        static_assert(A < 0x7f);

        constexpr umax_t a = _dup(0x7f + A);
        return (a - (v & constant::max_c7f)) & (~v & constant::max_c80);
    }

    template<u8_t A>
    inline umax_t _cmpgt(umax_t v)
    {
        static_assert(A < 0x7f);

        constexpr umax_t a = _dup(0x7f - A);
        return (v | (a + (v & constant::max_c7f))) & constant::max_c80;
    }

    template <u8_t A, u8_t B>
    inline umax_t _cmpml(umax_t v)
    {
        static_assert(A < 0x7f && B < 0x80);

        constexpr umax_t a = _dup(0x7f - A);
        constexpr umax_t b = _dup(0x7f + B);
        return (b - (v & constant::max_c7f)) & (a + (v & constant::max_c7f)) & (~v & constant::max_c80);
    }

    inline umax_t ascii_letters(umax_t v)
    {
        return (constant::Z - (v & constant::AZ_const)) & (constant::A + (v & constant::AZ_const)) & (~v & constant::max_c80);
    }

    inline umax_t ascii_numbers(umax_t v)
    {
        return _cmpml<'\x2f', '\x3a'>(v);
    }

    inline umax_t ascii_hyphen(umax_t v)
    {
        return _cmpeqz(v ^ constant::hyphen);
    }

    inline u64_t ascii_fast_tchar(const u64_t v)
    {
        return ascii_letters(v) | ascii_numbers(v) | ascii_hyphen(v);
    }

    inline umax_t non_printable(umax_t v)
    {
        // Non printable characters here are 0x7f (DEL) or characters below 0x20 (sp)
        static constexpr umax_t u = constant::max_c80 | constant::max_c20;
        return (u - (((v & constant::max_c7f) + constant::max_c01) & constant::max_c7f)) & (~v & constant::max_c80);
    }
}