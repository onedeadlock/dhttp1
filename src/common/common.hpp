#ifndef DHTTP_COMMON_HPP
#define DHTTP_COMMON_HPP
#include "../include/definition.hpp"

namespace dhttp::common
{
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

    inline u64_t ascii_fast_tchar(u64_t v)
    {
        return ascii_letters(v) | ascii_numbers(v) | ascii_hyphen(v);
    }

    inline u64_t non_printable(u64_t v)
    {
        // Non printable characters here are 0x7f (DEL) or characters below 0x20 (sp)
        static constexpr u64_t u = constant::c80 | constant::c20;
        return (u - (((v & constant::c7f) + constant::c01) & constant::c7f)) & (~v & constant::c80);
    }

    inline u8_t is_whitespace(u8_t x)
    {
        return (x == '\x20') or (x == '\x09'); // only for space and horizontal tab
    };

    inline std::size_t rcount_whitespace(void *b, u64_t len)
    {
        std::size_t i = 0;
        while (i < len and is_whitespace(reinterpret_cast<u8_t *>(b)[i++])) pass();
        return i;
    }
    
    inline std::size_t lcount_whitespace(void *b, u64_t len)
    {
        std::size_t i = len;
        while (i and is_whitespace(reinterpret_cast<u8_t *>(b)[--i])) pass();
        return len - i;
    }
    
    inline bool is_valid_name_token_(u8_t *b)
    {
        auto &x = tables::tchar_map;
        if constexpr (OPTIMIZE_FOR_MOST_CASE)
        {
            return x[b[0]] & x[b[1]] & x[b[2]] & x[b[3]] &
                   x[b[4]] & x[b[5]] & x[b[6]] & x[b[7]];
        }
        // most compilers will unroll this anyway
        int i = 0;
        while (i < 8 and x[b[i++]]) [[likely]] pass();
        return i == 8;
    }

    make_flat inline bool is_valid_name_token(void *b)
    {
        if constexpr (OPTIMIZE_FOR_MOST_CASE)
        {
            // Most tokens in  header names are usually a-z, A-Z, 0-9 or -
            return ascii_fast_tchar(reinterpret_cast<u64_t *>(b)[0]) or is_valid_name_token_(reinterpret_cast<u8_t *>(b));
        }
        return is_valid_name_token_(reinterpret_cast<u8_t *>(b));
    }

    inline bool is_valid_name_token_loop(u8_t *b, std::size_t len)
    {
        auto &x = tables::tchar_map;
        int i = 0;
        while (i < len and x[b[i++]]) [[likely]] pass();
        return i == len;
    }

    make_flat inline bool is_valid_name(u8_t *b, std::size_t& len)
    {
        if constexpr (not STRICT_HTTP or IGNORE_LEADING_SP)
            len -= is_whitespace(b[len - 1]);
        const u8_t *end = b + (len & ~(constant::int_size - 1));
        for (; b != end and is_valid_name_token(b); b += 8) [[likely]] pass();
        const u64_t r = len % constant::int_size;
        if (b != end or not r)
            return b == end;
        return is_valid_name_token_loop(b, r);
    }

    inline bool version_is_http_1(void *b)
    {
        static constexpr u64_t mask = U64('\x48') | U64('\x54') << 8 | U64('\x54') << 16 | U64('\x50') << 24 |
                                      U64('\x2f') << 32 | U64('\x2e') << 40 | U64('\x31') << 48; // H  T  T  P  /  1  .
        return mask == (reinterpret_cast<u64_t *>(b)[0] & 0x00ffffffffffffff);
    }  
}
#endif