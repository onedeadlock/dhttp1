#include "http.hpp"

#define is ==
#define isnot !=
#define not(x) (!(x))

 namespace dhttp
{
    bool http_1 = true;
    bool done   = true;

    auto tzmask  = [](u64_t x){ return ~x & x - 1; };
    auto blsmask = [](u64_t x){ return  x ^ x - 1; };
    auto blsr    = [](u64_t x){ return  x & x - 1; };
    auto blsfill = [](u64_t x){ return  x | x - 1; };
    auto xlsfill = [](u64_t x){ return  x ^ -x; }; // ~blsfill
    auto tzcnt   = [](u64_t v){ return __builtin_ctzll(v); };

    static constexpr u8_t tchar_map[] = "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0" ////////////////////////////////////
                                        "\x0\x80\x0\x80\x80\x80\x80\x0\x0\x0\x80\x0\x80\x80\x80\x0\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x0\x0\x0\x0\x0\x00" ////////////////
                                        "\x0\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x0\x0\x0" ////////////////
                                        "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x0\x80\x0\x80\x0"
                                        "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x00"
                                        "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x00"
                                        "\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"; ///////////

    struct _req_type
    {
        using req_index = const int (&)[];
        enum type : int {
            request  = 0,
            response = 1,
        };

        static constexpr int index[2][3] = {
            ////////////////////////////////////////////////////
            //// REQUEST {req_method, req_uri, req_version} ////
            ////////////////////////////////////////////////////
            {1, 2, 3},
            ////////////////////////////////////////////////////
            //// RESPONSE {req_version, req_stat, req_msg} /////
            ////////////////////////////////////////////////////
            {3, 2, 1},
        };
    };

    class dhttp::http
    {
    public:
        http() : start_index{0}, req_type{_req_type::type::request}, version(99) {}

    private:
        _req_type::type req_type;
        int version;
        u16_t start_index; // buffer start index
        int   req_version(const uint8_t i);
        u16_t req_size(const u16_t (&req)[], const int i) const;
        bool  req_version_is_http_1(const void *ver_string);
        bool  req_version_tag(const u16_t (&req)[], const void *buf, const _req_type::req_index& i);
        int   extract_fields(header_t &input, state_t &state, u64_t lf, u64_t cr, u64_t crlf, u64_t col);
        int   parse(header_t &input, state_t &state);
        
    };

    /////////////////////////////////////////////////////////////
    // ASCII LETTERS (C > 64/96 AND C < 91/123) FOR C != 1
    /////////////////////////////////////////////////////////////
    inline u64_t dhttp::ascii_letters(const u64_t v)
    {
        static constexpr u64_t A = static_cast<u64_t>('\x7f' - '\x40') * 0x101010101010101ULL;
        static constexpr u64_t Z = static_cast<u64_t>('\x7f' + '\x5b') * 0x101010101010101ULL;
        return (Z - (v & 0x5f5f5f5f5f5f5f5fULL)) & (A + (v & 0x5f5f5f5f5f5f5f5fULL)) & (~v & 0x8080808080808080ULL); //  'a' & 0xdf -> 'A' and (v & 0xdf) & 0x7f -> v & (0xdf & 0x7f) -> v & 0x5f
    }

    //////////////////////////////////////////////////////
    // ASCII NUMBERS (C > 47 AND C < 58) FOR C != 1
    //////////////////////////////////////////////////////
    inline u64_t dhttp::ascii_numbers(const u64_t v)
    {
        static constexpr u64_t _0 = static_cast<u64_t>('\x7f' - '\x2f') * 0x101010101010101ULL;
        static constexpr u64_t _9 = static_cast<u64_t>('\x7f' + '\x3a') * 0x101010101010101ULL;
        return (_9 - (v & 0x7f7f7f7f7f7f7f7fULL)) & (_0 + (v & 0x7f7f7f7f7f7f7f7fULL)) & (~v & 0x8080808080808080ULL);
    }

    inline u64_t dhttp::ascii_hyphen(const u64_t v)
    {
        static constexpr u64_t hi = 0x0100010001000100ULL;
        static constexpr u64_t lo = 0x0001000100010001ULL;
        static constexpr u64_t h = static_cast<u64_t>('\x2d') * 0x101010101010101ULL;
        return (((v ^ h | lo) - hi) | ((v ^ h | hi) - lo)) & (~(v ^ h) & 0x8080808080808080ULL);
    }

    inline u64_t dhttp::ascii_fast_tchar(const u64_t v)
    {
        return ascii_letters(v) | ascii_numbers(v) | ascii_hyphen(v); // a-zA-z, -, 0-9
    }

    inline u64_t dhttp::valid_tchar(dhttp::simd &v)
    {
        if constexpr (HAVE_SHUFFLE__)
        {
            static const dhttp::simd lo{dhttp::tables::token_charset_bitmap};
            static const dhttp::simd hi{dhttp::tables::token_charset_bitmap + 64};
            return dhttp::simd::movemask(dhttp::simd::shufb(lo, v) & dhttp::simd::shufb(hi, v >> 4));
        }
        return 0;
    }

    inline u64_t dhttp::req_valid_tchar(const uint8_t *b)
    {
        if constexpr (SUPPORT_FULL_TCHAR)
            return U64(tchar_map[b[0]]) | U64(tchar_map[b[1]]) << 8 | U64(tchar_map[b[2]]) << 16 | U64(tchar_map[b[3]]) << 24 | U64(tchar_map[b[4]]) << 32 | U64(tchar_map[b[5]]) << 40 | U64(tchar_map[b[6]]) << 48 | U64(tchar_map[b[7]]) << 56;
        return 0;
    }

    inline bool dhttp::req_tchar(const void *b, const u64_t mask)
    {
        if constexpr (OPTIMIZE_FOR_MOST_CASE)
        {
            // Most tokens are a-zA-Z0-9 and -; extra cost of full classification if the first check fails
            return not(~ascii_fast_tchar(*reinterpret_cast<const u64_t *>(b)) & mask and ~req_valid_tchar(reinterpret_cast<const u8_t *>(b)) & mask);
        }
        return not (~req_valid_tchar(reinterpret_cast<const u8_t *>(b)) & mask);
    }

    inline bool dhttp::req_single_tchar(const uint8_t b)
    {
        return tchar_map[b];
    }

    inline bool dhttp::req_header_name(const uint8_t *buf, const uint16_t len)
    {
        bool valid = true;
        const u16_t e = len >> 3;
        const u64_t r = len & 7;

        for (u16_t j = 0; j < e and valid; j++)
            valid = req_tchar(reinterpret_cast<const u64_t *>(buf) + j, 0);
        if likely (valid and r)
            return r == 1 ? req_single_tchar(*(buf + e)) : req_tchar(reinterpret_cast<const u64_t *>(buf) + e, (1U << (r << 3)) - 1); // only check 'r' bytes
        return valid;
    }

    /////////////////////////////////////////////
    /////////////////////////////////////////////
    ////////////////          ///////////////////
    ////////////////   HTTP   ///////////////////
    ////////////////          ///////////////////
    /////////////////////////////////////////////
    /////////////////////////////////////////////

    inline int http::req_version(const uint8_t i)
    {
        return (this->version = i ^ '\x30') < 10;
    }

    inline bool http::req_version_is_http_1(const void *ver_string)
    {
        static constexpr u64_t mask = U64('\x48') | U64('\x54') << 8 | U64('\x54') << 16 | U64('\x50') << 24 |
                                      U64('\x2f') << 32 | U64('\x2e') << 40 | U64('\x31') << 48; // H  T  T  P  /  1  .
        return mask == *reinterpret_cast<const u64_t *>(ver_string) & 0x00ffffffffffffff and req_version(reinterpret_cast<const uint8_t *>(ver_string)[7]);
    }

    inline u16_t http::req_size(const u16_t (&req)[], const int i) const
    {
        return http::req_type is _req_type::type::request ? (req[i - 0] - req[i + 1]) - 1
                                                                 : (req[i - 1] - req[i - 0]) - 1; // -1 for the sp seperator
    }

    inline bool http::req_version_tag(const u16_t (&req)[], const void *buf, const _req_type::req_index &i)
    {
        static constexpr u16_t req_version_required_size = 8; // strlen(HTTP/1.x)
        return (req_size(req, i[0]) == req_version_required_size) and req_version_is_http_1(buf + req[i[0]]);
    }

    int http::extract_fields(header_t &input, state_t &state, u64_t lf, u64_t cr, u64_t crlf, u64_t col)
    {
        static const simd vsp{'\x20'};
        static const simd vhtab{'\x9'};
        const u64_t wsp = simd::movemask(vsp == state.v | vhtab == state.v);
        const u64_t valid_char = simd::movemask(simd::cmpglt(state.v, '\x20', '\x7f'));
        const u64_t valid_sp = ~static_cast<const u64_t>(state.trailing_sp) & trim(wsp);

        if (state.req_line isnot done)
        {
            ///////////////////////////////////////////////////
            ////////// PARSE REQUEST-STATUS LINE //////////////
            ///////////////////////////////////////////////////
            u64_t mask = wsp | cr | lf;

            u16_t(&req)[] = input.request.request_line;

            if unlikely (state.trailing_cr is true)
            {
                if (lf & 0x01)
                    return -400;
                lf ^= 0x01;
                state.pos += 1; // lf
                goto post_req_line;
            }

            if unlikely (state.pos < dhttp::REQUEST_LINE_MAX_SIZE)
                return -400;

            // reject blank line at the start of request/response
            if unlikely ((crlf & 0x02) && state.parse_uinit)
            {
                if ((crlf & crlf >> 2) & 0x04)
                    return 0; // empty request (TODO: reject any further attempt to parse from the buffer)
                return -400;
            }

            if ((~(valid_char | valid_sp) | lf | (cr & ~0x8000000000000000ULL)) & tzmask(crlf))
                return -400;

            for (u64_t umask = mask & blsmask(cr | lf); umask and state.j; state.j--)
            {
                req[state.j] = state.pos + tzcnt(umask);
                umask &= umask - 1;
            }

            state.trailing_cr = static_cast<bool>(cr & 0x8000000000000000ULL);
            state.trailing_sp = static_cast<bool>(wsp & 0x8000000000000000ULL);
            if not(crlf)
            {
                state.pos += 64;
                return 0;
            }
            mask &= tzmask(crlf);
            crlf &= mask, lf &= mask, cr &= mask;
            state.pos = req[state.j + 1] + 2; // +2 for cr and lf

            //////////////////////////////////////////////
            post_req_line:
            //////////////////////////////////////////////
            state.req_line = done; // done
            if (state.j isnot 0 or req_version_tag(req, input.recvb.recvbuf, _req_type::index[req_type]) isnot dhttp::http_1)
                return -400;
        }

        ///////////////////////////////////////////////////
        //////////////// PARSE HEADERS ////////////////////
        ///////////////////////////////////////////////////

        if (state.resume is true)
        {
            const u64_t first_lf = lsb(lf);
            if (not first_lf)
                return 0; // still data, no line-feed(lf)

            // TODO: extract field-name, field_value here

            state.resume = false;
            // unset colon within values; TODO: false colon is between first lf/cr and next lf/cr not just after first
            col &= xlsfill(first_lf);
            lf &= ~first_lf;
        }

        while (col)
        {
            const u64_t first_col = lsb(col);
            const u64_t eol = lsb(crlf & xlsfill(first_col)); // next crlf after first colon

            if (eol and (eol < first_col))
                return -400; // missing header name

            /////////////////////////////////////////////
            /////////////// HEADER VALUE ////////////////
            u64_t valid_field = simd::movemask(simd::cmpglt(state.v, '\x20', '\x7f') | (state.v & '\x80'));
            if unlikely ((~valid_field | lf | cr) & -eol)
            {
                if (state.trailing_cr = static_cast<bool>(cr & 0x8000000000000000ULL); state.trailing_cr)
                    return 0;
                return -400;
            }
            /////////////////////////////////////////////
            /////////////////////////////////////////////

            uint16_t pos_col = tzcnt(first_col);
            uint16_t pos_eol = tzcnt(eol);

            /////////////////////////////////////////////
            //////////////// HEADER NAME ////////////////
            req_t &name = input.hf.req_buf[state.j];
            name.pos = state.pos;
            name.len = state.j + pos_col;
            if (not req_header_name(reinterpret_cast<const uint8_t *>(input.recvb.recvbuf) + name.pos, name.len))
                return -400;
            /////////////////////////////////////////////
            /////////////////////////////////////////////

            if not(eol)
                return state.resume = true; // no linefeed(lf), all data
            col &= -first_col;
            crlf &= -eol;
        }
        return 0;
    }
    
    int http::parse(header_t &input, state_t &state)
    {
        static const simd LF{'\xa'};
        static const simd CR{'\xd'};
        static const simd CL{'\x3a'};

        const size_t n = (input.size + 63) & ~(size_t)63; // align read/load size to 64

        for (size_t j = 0; j < n; j += 64)
        {
            u8_t *b = input.recvb.recvbuf + j;
            state.v = b;

            u64_t lf = simd::movemask(state.v == LF);
            u64_t cr = simd::movemask(state.v == CR);
            u64_t col = simd::movemask(state.v == CL);
            u64_t crlf = lf & (cr >> 1);

            if unlikely (extract_fields(input, state, lf, cr, crlf, col) < 0)
                return -400;
            // stop, if \r\n\r\n is found
            if unlikely (crlf & crlf >> 2)
                return j + tzcnt(crlf & crlf >> 2);
            // handle any crlf carry
            if unlikely ((lf | cr) & 0xe000000000000000ull)
                if (('\xd' is b[-3]) && ('\xa' is b[-2]) && ('\xd' is b[-1]) && ('\xa' is b[0]))
                    return j + 4;
        }
        return dhttp::EXPECT_DATA;
    }
}

#undef inline
#undef is
#undef isnot