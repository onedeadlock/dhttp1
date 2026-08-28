#ifndef DHTTP1_COMMON_H
#define DHTTP1_COMMON_H
#include <cstdint>


#define U32(x)  static_cast<const u32_t>(x)
#define U64(x)  static_cast<const u64_t>(x)
#define UMAX(x) static_cast<const u64_t>(x)
#define U128(x) static_cast<const u128_t>(x)

namespace dhttp
{
    using u8_t  = uint8_t;
    using u16_t = uint16_t;
    using u32_t = uint32_t;
    using u64_t = uint64_t;

#ifdef __SIZEOF_INT128__
    using u128_t = __uint128_t;
#endif

    #ifdef __SIZEOF_INT128__
    using umax_t = __uint128_t;
#   define UMAX_C(x) static_cast<const umax_t>(x)
#else
    using umax_t = uint64_t;
#   define UMAX_C(x) 0
#endif
}

namespace dhttp::Common::Constants
{
    static constexpr std::size_t intmaxWidth = sizeof (umax_t);

    constexpr umax_t c7f = 0x7f7f7f7f7f7f7f7fULL;
    constexpr umax_t c80 = 0x8080808080808080ULL;
    constexpr umax_t c01 = 0x0101010101010101ULL;
    constexpr umax_t c20 = 0x2020202020202020ULL;
    constexpr umax_t max_c7f = UMAX_C(c7f) << 64 | c7f;
    constexpr umax_t max_c80 = UMAX_C(c80) << 64 | c80;
    constexpr umax_t max_c01 = UMAX_C(c01) << 64 | c01;
    constexpr umax_t max_c20 = UMAX_C(c20) << 64 | c20;


    constexpr u64_t  hyphen = UMAX('\x2d') * max_c01;


    //  'a' & 0xdf -> 'A' -> (v & 0xdf) & 0x7f -> v & (0xdf & 0x7f) -> v & 0x5f
    constexpr u64_t AZ_const = max_c7f & 0xdfdfdfdfdfdfdfdfULL;
    constexpr u64_t A = UMAX('\x7f' - '\x40') * max_c01;
    constexpr u64_t Z = UMAX('\x7f' + '\x5b') * max_c01;
    
}

namespace dhttp::Common::scalar
{
    inline umax_t _cmpeqz(umax_t v)
    {
        return ~(v | ((v & Constants::max_c7f) + Constants::max_c7f)) & Constants::max_c80;
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
        return (v | ((v & Constants::max_c7f) + Constants::max_c7f)) & Constants::max_c80;
    }

    template <u8_t A, u8_t B>
    inline umax_t _cmpml(umax_t v)
    {
        static_assert(A < 0x7f && B < 0x80);

        constexpr u64_t a = UMAX(0x7f - A) * Constants::max_c01;
        constexpr u64_t b = UMAX(0x7f + B) * Constants::max_c01;
        return (a - (v & Constants::max_c7f)) & (b + (v & Constants::max_c7f)) & (~v & Constants::max_c80);
    }

    constexpr inline umax_t _dup(u8_t v)
    {
        return UMAX(v) * Constants::max_c01;
    }

    inline u64_t ascii_letters(umax_t v)
    {
        return (Constants::Z - (v & Constants::AZ_const)) & (Constants::A + (v & Constants::AZ_const)) & (~v & Constants::max_c80);
    }

    inline u64_t ascii_numbers(umax_t v)
    {
        return _cmpml<'\x2f', '\x3a'>(v);
    }

    inline u64_t ascii_hyphen(umax_t v)
    {
        return _cmpeqz(v ^ Constants::hyphen);
    }

    inline u64_t non_printable(umax_t v)
    {
        // Non printable characters here are 0x7f (DEL) or characters below 0x20 (sp)
        static constexpr umax_t u = Constant::max_c80 | Constants::max_c20;
        return (u - (((v & Constants::max_c7f) + Constants::max_c01) & Constants::max_c7f)) & (~v & Constants::max_c80)
    }
}
#endif