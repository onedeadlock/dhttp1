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
    
    template <typename T>
    struct req
    {
        static_assert(std::is_integral_v<T>);
        struct
        {
            T len, pos;
        } name, value;
    };

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
        http() : req_type{_req_type::type::request}, version(99) {}

    private:
        _req_type::type req_type;
        req_state state;
        req_line reqline;
        int version;
        // methods
        int   req_version(const uint8_t i);
        u16_t req_size(const u16_t (&req)[], const int i) const;
        bool  req_version_is_http_1(const void *ver_string);
        bool  req_version_tag(const u16_t (&req)[], const void *buf, const _req_type::req_index& i);
        template <typename T = u16_t, std::size_t out_size> int parse(void *in, size_t in_size, std::array<req<T>, out_size> &out);
        int parse_request_line(const void *in, const std::size_t size, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf);
        template <typename T = u16_t, std::size_t out_size>
        int parse_header(void *in, size_t in_size, std::array<req<T>, out_size> &out, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf);
   
    };

    /////////////////////////////////////////////////////////////
    // ASCII LETTERS (C > 64/96 AND C < 91/123) FOR C != 1
    /////////////////////////////////////////////////////////////
    inline u64_t ascii_letters(const u64_t v)
    {
        static constexpr u64_t A = static_cast<u64_t>('\x7f' - '\x40') * 0x101010101010101ULL;
        static constexpr u64_t Z = static_cast<u64_t>('\x7f' + '\x5b') * 0x101010101010101ULL;
        return (Z - (v & 0x5f5f5f5f5f5f5f5fULL)) & (A + (v & 0x5f5f5f5f5f5f5f5fULL)) & (~v & 0x8080808080808080ULL); //  'a' & 0xdf -> 'A' and (v & 0xdf) & 0x7f -> v & (0xdf & 0x7f) -> v & 0x5f
    }

    //////////////////////////////////////////////////////
    // ASCII NUMBERS (C > 47 AND C < 58) FOR C != 1
    //////////////////////////////////////////////////////
    inline u64_t ascii_numbers(const u64_t v)
    {
        static constexpr u64_t _0 = static_cast<u64_t>('\x7f' - '\x2f') * 0x101010101010101ULL;
        static constexpr u64_t _9 = static_cast<u64_t>('\x7f' + '\x3a') * 0x101010101010101ULL;
        return (_9 - (v & 0x7f7f7f7f7f7f7f7fULL)) & (_0 + (v & 0x7f7f7f7f7f7f7f7fULL)) & (~v & 0x8080808080808080ULL);
    }

    inline u64_t ascii_hyphen(const u64_t v)
    {
        static constexpr u64_t hi = 0x0100010001000100ULL;
        static constexpr u64_t lo = 0x0001000100010001ULL;
        static constexpr u64_t h = static_cast<u64_t>('\x2d') * 0x101010101010101ULL;
        return (((v ^ h | lo) - hi) | ((v ^ h | hi) - lo)) & (~(v ^ h) & 0x8080808080808080ULL);
    }

    inline u64_t ascii_fast_tchar(const u64_t v)
    {
        return ascii_letters(v) | ascii_numbers(v) | ascii_hyphen(v); // a-zA-z, -, 0-9
    }

    inline u64_t valid_tchar(dhttp::simd &v)
    {
        if constexpr (HAVE_SHUFFLE__)
        {
            static const dhttp::simd lo{dhttp::tables::token_charset_bitmap};
            static const dhttp::simd hi{dhttp::tables::token_charset_bitmap + 64};
            return dhttp::simd::movemask(dhttp::simd::shufb(lo, v) & dhttp::simd::shufb(hi, v >> 4));
        }
        return 0;
    }

    inline u64_t req_valid_tchar(const uint8_t *b)
    {
        if constexpr (SUPPORT_FULL_TCHAR)
            return U64(tchar_map[b[0]]) | U64(tchar_map[b[1]]) << 8 | U64(tchar_map[b[2]]) << 16 | U64(tchar_map[b[3]]) << 24 | U64(tchar_map[b[4]]) << 32 | U64(tchar_map[b[5]]) << 40 | U64(tchar_map[b[6]]) << 48 | U64(tchar_map[b[7]]) << 56;
        return 0;
    }

    inline bool req_tchar(const void *b, const u64_t mask)
    {
        if constexpr (OPTIMIZE_FOR_MOST_CASE)
        {
            // Most tokens are a-zA-Z0-9 and -; extra cost of full classification if the first check fails
            return not(~ascii_fast_tchar(*reinterpret_cast<const u64_t *>(b)) & mask and ~req_valid_tchar(reinterpret_cast<const u8_t *>(b)) & mask);
        }
        return not (~req_valid_tchar(reinterpret_cast<const u8_t *>(b)) & mask);
    }

    inline bool req_single_tchar(const uint8_t b)
    {
        return tchar_map[b];
    }

    inline bool req_header_name(const uint8_t *buf, const uint16_t len)
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
        return (version = i ^ '\x30') < 10;
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

    inline bool http::req_version_tag(const u16_t (&req)[], const void *in, const _req_type::req_index &i)
    {
        static constexpr u16_t req_version_required_size = 8; // strlen(HTTP/1.x)
        return (req_size(req, i[0]) == req_version_required_size) and req_version_is_http_1(in + req[i[0]]);
    }

    #if 0
    inline bool req_single_header_value(simd &v, req_t &header, const uint8_t *, u64_t lf, u64_t cr, u64_t crlf)
    {
        // TODO: sp
        u64_t valid_field = simd::movemask(simd::cmpglt(v, '\x20', '\x7f') | (v & '\x80'));
        if unlikely (~(valid_field) | lf | cr)
            return false;
        return true;
    }

    inline bool req_header_value(req_t &header, const uint8_t *, u64_t lf, u64_t cr, u64_t crlf)
    {
        #if 0
        u64_t valid_field = simd::movemask(simd::cmpglt(v, '\x20', '\x7f') | (v & '\x80'));
        if unlikely (~(valid_field | sp) | lf | cr)
            return false;
        #endif
        return true;
    }
    #endif

    int http::parse_request_line(const void *in, const std::size_t size, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf)
    {
        static const simd vsp  {'\x20'};
        static const simd vhtab{'\x9' };

        const u64_t sp        = simd::movemask(simd::cmpgeq(v, vsp) | simd::cmpgeq(v, vhtab));
        const u64_t valid_sp  = ~static_cast<const u64_t>(state.trailing_sp) & trim(sp);
        const u64_t tchar     = simd::movemask(simd::cmpglt(v, '\x20', '\x7f')) | valid_sp;

        auto has_any_rejected_token = [&](void)
        { return (~tchar | lf | (cr & ~0x8000000000000000ULL)) & tzmask(crlf); };

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
        for (u64_t umask = mask & blsmask(cr | lf); umask and state.j; umask &= umask - 1)
            reqline.req_line[state.j--] = state.pos + tzcnt(umask);

        if not (not crlf)
        {
            state.trailing_cr = static_cast<bool>(cr & 0x8000000000000000ULL);
            state.trailing_sp = static_cast<bool>(sp & 0x8000000000000000ULL);
            return 0;
        }
        mask &= tzmask(crlf), crlf &= mask, lf &= mask, cr &= mask;
        state.pos = reqline.req_line[state.j + 1] + 2; // +2 for cr and lf
        state.req_line = done;
        return -((state.j isnot 0) or (req_version_tag(reqline.req_line, in, _req_type::index[req_type]) isnot dhttp::http_1));
    }

    template <typename T = u16_t, std::size_t out_size>
    int http::parse_header(void *in, size_t in_size, std::array<req<T>, out_size> &out, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf)
    {
        auto &header = out[state.j];
        if (state.pending_value)
        {
            if (not crlf)
                return (header.value.len += 64, 0);
            if unlikely (not req_header_value(header, in, lf, cr, crlf))
            {
                if (state.trailing_cr = static_cast<bool>(cr & 0x8000000000000000ULL); state.trailing_cr)
                    return 0;
                return -400;
            }
            crlf &= crlf - 1;
            state.j += 1;
        }

        if unlikely (state.pending_name)
        {
            // names are mostly short
            if unlikely(crlf)
                return -400;
            if unlikely (not col)
                return (header.name.len += 64, 0);
        }

        while (col)
        {
            const u64_t first_col = lsb(col);
            const u64_t eol = lsb(crlf & xlsfill(first_col)); // next crlf after first colon
            auto &header = out[state.j];

            if unlikely (eol and eol < first_col)
                return -400;

            //////////////// HEADER NAME ////////////////
            if likely (not state.pending_name)
                header.name.pos = state.pos;
            header.name.len += tzcnt(first_col);
            if unlikely (not req_header_name(static_cast<const u8_t *>(in) + header.name.pos, header.name.len))
                return -400;

            //////////////// HEADER VALUE /////////////////
            header.value.pos = header.value.pos + header.value.len + 1;
            if not (eol)
            {
                header.value.len = 1 + 64 - header.value.len;
                state.pending_name = true;
                return 0;
            }
            header.value.len = tzcnt(eol) - header.name.len;
            if unlikely (0 and not req_single_header_value(state.v, header, in, lf, cr, crlf))
             {
                if (state.trailing_cr = static_cast<bool>(cr & 0x8000000000000000ULL); state.trailing_cr)
                    return 0;
                return -400;
             }
        
            col  &= xlsfill(eol);
            crlf &= crlf - 1;
            state.j += 1;
        }
        state.pending_name = crlf and not col;
        return 0;
    }

    template <typename T = u16_t, std::size_t out_size>
    int http::parse(void *in, size_t in_size, std::array<req<T>, out_size> &out)
    {
        static_assert(out_size != 0);

        static const simd vlf {'\xa'};
        static const simd vcr {'\xd'};
        static const simd vcol{'\x3a'};

        const std::size_t n = (in_size + 63) & ~(std::size_t)63; // align read/load size to 64

        for (std::size_t j = 0; j < n; j += 64)
        {
            u8_t *b = static_cast<u8_t *>(in) + j;
            simd::v = b;

            u64_t lf   = simd::movemask(simd::cmpeq(v, vlf ));
            u64_t cr   = simd::movemask(simd::cmpeq(v, vcr ));
            u64_t col  = simd::movemask(simd::cmpeq(v, vcol));
            u64_t crlf = cr & (lf << 1);

            if (state.req_line isnot done and parse_request_line(in, size, v, lf, cr, crlf) < 0)
               return -400;
            if (state.req_line is done and parse_header<T, out_size>(in, in_size, out, v, lf, cr, crlf) < 0)
                return -400;
            
            if unlikely (crlf & crlf >> 2)
                return j + tzcnt(crlf & crlf >> 2);
            
            // handle any crlf carry
            if unlikely ((lf | cr) & 0xe000000000000000ull)
                if (('\xd' is b[-3]) && ('\xa' is b[-2]) && ('\xd' is b[-1]) && ('\xa' is b[0]))
                    return j + 4;
            
            // increment cursor
            state.pos += 64;
        }
        return dhttp::EXPECT_DATA;
    }
}