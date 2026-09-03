#include "http.hpp"

namespace dhttp::Implementation
{
    using namespace common;
    
    bool http_1 = true;
    bool done   = true;
    auto pass   = []{};

    inline u8_t is_whitespace(u8_t x)
    {
        return (x == '\x20') or (x == '\x09'); // only for space and horizontal tab
    };

    inline std::size_t rcount_whitespace(void *b, u64_t len)
    {
        std::size_t i = 0;
        while (i < len and is_whitespace(reinterpret_cast<u8_t *>(b)[i++]))
            pass();
        return i;
    }
    
    inline std::size_t lcount_whitespace(void *b, u64_t len)
    {
        std::size_t i = len;
        while (i and is_whitespace(reinterpret_cast<u8_t *>(b)[--i]))
            pass();
        return len - i;
    }
        
    inline u64_t req_valid_tchar(const u8_t *b)
    {
        if constexpr (SUPPORT_FULL_TCHAR)
            return U64(tables::tchar_map[b[0]]) << 0U | U64(tables::tchar_map[b[1]]) << 8U |
                   U64(tables::tchar_map[b[2]]) << 16 | U64(tables::tchar_map[b[3]]) << 24 |
                   U64(tables::tchar_map[b[4]]) << 32 | U64(tables::tchar_map[b[5]]) << 40 |
                   U64(tables::tchar_map[b[6]]) << 48 | U64(tables::tchar_map[b[7]]) << 56;
        return 0;
    }

    inline bool req_tchar(const void *b, const umax_t mask)
    {
        if constexpr (OPTIMIZE_FOR_MOST_CASE)
            return not(~scalar::ascii_fast_tchar(*reinterpret_cast<const u64_t *>(b)) & mask and ~req_valid_tchar(reinterpret_cast<const u8_t *>(b)) & mask); // Most tokens are a-zA-Z0-9 and -
        return not (~req_valid_tchar(reinterpret_cast<const u8_t *>(b)) & mask);
    }

    inline bool req_single_tchar(const u8_t b)
    {
        return tables::tchar_map[b];
    }

    inline bool req_header_name(u8_t *b, u64_t len)
    {
        if constexpr (not STRICT_HTTP or IGNORE_LEADING_SP)
            len -= is_whitespace(b[len - 1]);
        bool valid = true;
        const u16_t e = len >> constant::max_int_size_p;
        const u64_t r = len & (constant::max_int_size - 1);

        for (u16_t j = 0; j < e and valid; j++)
            valid = req_tchar(reinterpret_cast<const u64_t *>(b) + j, constant::max_cff);
        if not (valid and r)
            return valid;
        const u64_t r_mask = (1U << (r << constant::max_int_size_p)) - 1;
        return r == 1 ? req_single_tchar(*(b + e)) : req_tchar(reinterpret_cast<const u64_t *>(b) + e, r_mask);
    }

    /////////////////////////////////////////////
    /////////////////////////////////////////////
    ////////////////          ///////////////////
    ////////////////   HTTP   ///////////////////
    ////////////////          ///////////////////
    /////////////////////////////////////////////
    /////////////////////////////////////////////

    inline int http::req_version(u8_t i)
    {
        return (this->version = i ^ '\x30') < 10;
    }

    inline bool http::req_version_is_http_1(const void *ver_string)
    {
        static constexpr u64_t mask = U64('\x48') | U64('\x54') << 8 | U64('\x54') << 16 | U64('\x50') << 24 |
                                      U64('\x2f') << 32 | U64('\x2e') << 40 | U64('\x31') << 48; // H  T  T  P  /  1  .
        return mask == (*reinterpret_cast<const u64_t *>(ver_string) & 0x00ffffffffffffff) and req_version(reinterpret_cast<const u8_t *>(ver_string)[7]);
    }

    inline u16_t http::req_size(const u64_t (&req)[], const int i) const
    {
        return this->req_type is _req_type::type::request ? (req[i - 0] - (req[i + 1]) - 1)
                                                          : (req[i - 1] - (req[i - 0]) - 1); // -1 for the sp seperator
    }

    inline bool http::req_version_tag(const u64_t (&req)[], const void *in, const _req_type::req_index &i)
    {
        static constexpr u16_t req_version_required_size = 8; // len(HTTP/1.x)
        return (req_size(req, i[0]) == req_version_required_size) and req_version_is_http_1(in + req[i[0]]);
    }

    inline bool req_header_value(const simd &v)
    {
        return false;
    }

    template <typename T>
    inline bool trim_whitespace(void *b, T &pos, T &len)
    {
        static_assert(sizeof(T) <= sizeof(u64_t));
        void *bv = reinterpret_cast<u8_t *>(b) + pos;
        std::size_t t_pos = rcount_whitespace(bv, static_cast<u64_t>(len));

        if unlikely (t_pos == len)
            return 1; // all whitespace
        pos += t_pos;
        len -= lcount_whitespace(bv, static_cast<u64_t>(len));
        return 0;
    };

    template <typename V>
    inline bool req_header_value(void *in, simd &v, V &value, u64_t lf, u64_t cr, u64_t crlf, bool done)
    {
        static const simd sp{'\x20'}, htab{'\x9'};
        bool is_valid = simd::testzero(simd::cmpglt(v, '\x19', '\x7f') | simd::sign(v) | simd::cmpeq(v, htab));
        return not is_valid and ((cr & constant::msb_64 | lf) and crlf);
    }

    int http::parse_request_line(const void *in, const std::size_t size, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf)
    {
        static const simd vsp  {'\x20'};
        static const simd vhtab{'\x9' };

        const u64_t sp        = simd::movemask(simd::cmpeq2(v, vsp, vhtab));
        const u64_t valid_sp  = ~static_cast<const u64_t>(state.trailing_sp) & bits::trim(sp);
        const u64_t tchar     = simd::movemask(simd::cmpglt(v, '\x20', '\x7f')) | valid_sp;

        auto has_any_rejected_token = [&]{ return (~tchar | lf | (cr & ~constant::msb_64)) & bits::tzmask(crlf); };

        if unlikely ((crlf & 0x02) and state.no_init)
            return  ((crlf & crlf >> 2) & 0x04) ? -400 /* empty request */ : -400 /* blank line */;
        if unlikely (state.pos > size or has_any_rejected_token())
            return -400;
        if unlikely (state.trailing_cr is true)
        {
            if not (lf & 0x01)
                return -400;
            lf &= ~0x1ULL;
            reqline.req_line[state.j] -= 1; // -cr
            state.pos += 1;                 // +lf
            return 0;
        }

        u64_t mask = sp | cr | lf;
        for (u64_t umask = mask & bits::blsmask(cr | lf); umask and state.j; umask &= umask - 1)
            reqline.req_line[state.j--] = state.pos + bits::tzcnt(umask);

        if not (crlf)
        {
            state.trailing_cr = static_cast<bool>(cr & constant::msb_64);
            state.trailing_sp = static_cast<bool>(sp & constant::msb_64);
            return state.pos += 64, 0;
        }
        mask &= bits::tzmask(crlf), crlf &= mask, lf &= mask, cr &= mask;
        state.pos = reqline.req_line[state.j + 1] + 2; // +2 for cr and lf
        state.req_line = done;
        return -((state.j isnot 0) or (req_version_tag(reqline.req_line, in, _req_type::index[req_type]) isnot http_1));
    }

    template <typename T, T out_size>
    int http::parse_header(void *in, size_t in_size, req<T, out_size> &out, const simd &v, u64_t lf, u64_t cr, u64_t __crlf)
    {
        static const simd v_col {'\x3a'};
        u64_t crlf = __crlf; // copy
        auto set_header = [](auto& cp, auto &np, auto pos, auto mask, int skip)
            {
                u64_t end = pos + tzcnt(mask);
                cp.len = end - cp.pos;
                np.pos = end + skip;
            };

        if (state.pending_value)
        {
            auto& value = out[state.j].value;
            if not (crlf)
                return state.pos += 64, req_header_value(v);
            state.j += 1;
            set_header(value, out[state.j].name, state.pos, crlf, 2);
            crlf &= crlf - 1;
            if unlikely (not req_header_value(v, lf, cr, __crlf) or trim_whitespace<T>(in, value.pos, value.len))
                return -400;
        }
        for (u64_t col = simd::movemask(simd::cmpeq(v, v_col)); true; )
        {
            auto& name = out[state.j].name, &value = out[state.j].value;
            const u64_t first_col = bits::lsb(col);

            if constexpr (not OPTIMIZE_FOR_MOST_CASE)
                if unlikely (crlf and bits::lsb(crlf) < bits::lsb(col))
                    return -400;
            if not (col)
                return state.pos += 64;
            set_header(name, value, state.pos, col, 1);
            if not (crlf)
                return state.pos += 64, req_header_value(v);
            state.j += 1;
            set_header(value, out[state.j].name, state.pos, crlf & bits::xlsfill(first_col), 2);
            col  &= bits::xlsfill(crlf);
            crlf &= crlf - 1;
            bool all_wsp = trim_whitespace<T>(in, value.pos, value.len);
            if unlikely (all_wsp or not req_header_name(in, name.len) or not req_header_value(v, lf, cr, __crlf, 0))
                return -400;
        }
        return 0;
    }

    template <typename T, T out_size>
    int http::parse(void *in, size_t in_size, req<T, out_size> &out)
    {
        static_assert(out_size != 0);

        static const simd v_lf {'\xa'};
        static const simd v_cr {'\xd'};

        const std::size_t n = (in_size + 63) & ~(std::size_t)63; // align read/load size to 64

        for (std::size_t j = 0; j < n; j += 64)
        {
            u8_t *b = static_cast<u8_t *>(in) + j;
            simd::v = b;

            u64_t lf   = simd::movemask(simd::cmpeq(v, v_lf ));
            u64_t cr   = simd::movemask(simd::cmpeq(v, v_cr ));
            u64_t crlf = cr & (lf << 1);

            if (state.req_line isnot done and parse_request_line(in, size, v, lf, cr, crlf) < 0)
               return -400;
            if (state.req_line is done and parse_header<T, out_size>(in, in_size, out, v, lf, cr, crlf) < 0)
                return -400;
            
            if unlikely (crlf & crlf >> 2)
                return j + bits::tzcnt(crlf & crlf >> 2);
            
            // handle any crlf carry
            if unlikely ((lf | cr) & 0xe000000000000000ull)
                if (('\xd' is b[-3]) && ('\xa' is b[-2]) && ('\xd' is b[-1]) && ('\xa' is b[0]))
                    return j + 4;
        }
        return dhttp::EXPECT_DATA;
    }
}
