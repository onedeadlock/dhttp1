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

    inline std::size_t rcount_whitespace(void *b, std::size_t len)
    {
        std::size_t i = 0;
        while (i < len and is_whitespace(reinterpret_cast<u8_t *>(b)[i++]))
            pass();
        return i;
    }
    
    inline std::size_t lcount_whitespace(void *b, std::size_t len)
    {
        std::size_t i = len;
        while (i and is_whitespace(reinterpret_cast<u8_t *>(b)[--i]))
            pass();
        return len - i;
    }

    inline bool trim_whitespace(void *b, u64_t &t_len, u64_t &l_len)
    {
        void *v = reinterpret_cast<u8_t *>(b) + t_len;
        std::size_t tsp_len = rcount_whitespace(v, 0);
        if unlikely (tsp_len == l_len)
            return 1;
        t_len += tsp_len;
        l_len -= lcount_whitespace(v, l_len);
        return 0;
    };
        
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

    inline bool req_header_name(const u8_t *buf, const u16_t len)
    {
        bool valid = true;
        const u16_t e = len >> constant::max_int_size_p;
        const u64_t r = len & (constant::max_int_size - 1);

        for (u16_t j = 0; j < e and valid; j++)
            valid = req_tchar(reinterpret_cast<const u64_t *>(buf) + j, constant::max_cff);
        if not (valid and r)
            return valid;
        const u64_t r_mask = (1U << (r << constant::max_int_size_p)) - 1;
        return r == 1 ? req_single_tchar(*(buf + e)) : req_tchar(reinterpret_cast<const u64_t *>(buf) + e, r_mask);
    }

    /////////////////////////////////////////////
    /////////////////////////////////////////////
    ////////////////          ///////////////////
    ////////////////   HTTP   ///////////////////
    ////////////////          ///////////////////
    /////////////////////////////////////////////
    /////////////////////////////////////////////

    inline int http::req_version(const u8_t i)
    {
        return (this->version = i ^ '\x30') < 10;
    }

    inline bool http::req_version_is_http_1(const void *ver_string)
    {
        static constexpr u64_t mask = U64('\x48') | U64('\x54') << 8 | U64('\x54') << 16 | U64('\x50') << 24 |
                                      U64('\x2f') << 32 | U64('\x2e') << 40 | U64('\x31') << 48; // H  T  T  P  /  1  .
        return mask == (*reinterpret_cast<const u64_t *>(ver_string) & 0x00ffffffffffffff) and req_version(reinterpret_cast<const u8_t *>(ver_string)[7]);
    }

    inline u16_t http::req_size(const u16_t (&req)[], const int i) const
    {
        return this->req_type is _req_type::type::request ? (req[i - 0] - (req[i + 1]) - 1)
                                                          : (req[i - 1] - (req[i - 0]) - 1); // -1 for the sp seperator
    }

    inline bool http::req_version_tag(const u16_t (&req)[], const void *in, const _req_type::req_index &i)
    {
        static constexpr u16_t req_version_required_size = 8; // len(HTTP/1.x)
        return (req_size(req, i[0]) == req_version_required_size) and req_version_is_http_1(in + req[i[0]]);
    }

    inline bool req_header_value(const simd &v, const u64_t lf, const u64_t cr, const u64_t crlf)
    {
        return false;
    }

    inline bool req_header_value(const simd &v, const u64_t lf, const u64_t cr, const u64_t crlf, u64_t &mask)
    {
        if constexpr (HAVE_SHUFFLE__)
        {
            // TODO: use shuffle (pshufb)
            return 0;
        }
        static const simd sp{'\x20'}, htab{'\x9'};
        simd z = simd::cmpglt(v, '\x19', '\x7f') | simd::sign(v);
        // mask   = simd::movemask(simd::andnot(z, simd::cmpeq(v, sp) | simd::cmpeq(v, htab)));
        return not simd::testzero(z | htab) and ((cr & constant::msb_64 | lf) and crlf);
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

    template <typename T = u16_t, std::size_t out_size>
    int http::parse_header(void *in, size_t in_size, req<T, out_size> &out, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf)
    {
        auto &header = out[state.j];

        if (state.pending_value)
        {
            if (not crlf)
            {
                header.value.len += 64, state.pos += 64;
                return req_header_value(v, lf, cr, crlf);
            }
            state.j += 1;
            crlf &= crlf - 1;
            state.pending_value = false;
            header.value.len += bits::tzcnt(crlf);
            bool all_wsp = trim_whitespace(in, header.value.pos, header.value.len);
             if unlikely (all_wsp or req_header_value(v, lf, cr, crlf) < 0)
                return -400;
        }

        static const simd v_col {'\x3a'};
        u64_t col = simd::movemask(simd::cmpeq(v, v_col));
        if unlikely (state.pending_name and not col)
            return not (header.name.len += 64);
        if (col)
        {
            u64_t mask = 0;
            if unlikely (req_header_value(v, lf, cr, crlf, mask) is false)
                return -400;
            
            while (col)
            {
                const u64_t first_col = bits::lsb(col);
                const u64_t pre_col   = bits::xlsfill(first_col);
                const u64_t eol       = bits::lsb(crlf & pre_col); // next crlf after first colon

                if unlikely ((eol and eol < first_col) and not state.pending_name)
                    return -400;

                //////////////// HEADER ////////////////
                 auto &header = out[state.j];
                //// NAME
                if likely (not state.pending_name)
                {
                    header.name.len = 0;
                    header.name.pos = state.pos;
                }
                header.name.len += bits::tzcnt(first_col) - 1;
                if unlikely (not req_header_name(static_cast<const u8_t *>(in) + header.name.pos, header.name.len))
                    return -400;

                //// VALUE
                mask &= pre_col;
                header.value.pos = bits::tzcnt(mask);
                if not (eol)
                {
                    header.value.len = 63 - header.value.len;
                    state.pending_name = true;
                    return 0;
                }
                header.value.len = bits::tzcnt(bits::trim_u(mask)) - header.name.len;

                col  &= pre_col;
                crlf &= crlf - 1;
                state.j += 1;
            }
        }
        state.pending_name = crlf and not col;
        state.trailing_cr  = static_cast<bool>(cr & constant::msb_64);
        return 0;
    }

    template <typename T = u16_t, std::size_t out_size>
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
