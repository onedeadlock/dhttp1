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

    template<int N>
    inline bool req_header_value(simdv<N>& v)
    {
        return false;
    }

    template <typename T>
    make_flat inline bool trim_whitespace(void *b, T &pos, T &len)
    {
        static_assert(sizeof(T) <= sizeof(u64_t));
        void *bv = reinterpret_cast<u8_t *>(b) + pos;
        std::size_t t_pos = rcount_whitespace(bv, static_cast<u64_t>(len));

        if (t_pos == len) [[unlikely]]
            return 1; // all whitespace
        pos += t_pos;
        len -= lcount_whitespace(bv, static_cast<u64_t>(len));
        return 0;
    };

    template <typename V, int N>
    inline bool req_header_value(void *in, simdv<N> &v, V &value, u64_t lf, u64_t cr, u64_t crlf, bool done)
    {
        static simdv<N> sp   = simdv<N>::splat('\x20');
        static simdv<N> htab = simdv<N>::splat('\x9' );
        bool is_valid = simdv<N>::is_zero(simdv<N>::_or(simdv<N>::gt_or_lt(v, '\x19', '\x7f'), simdv<N>::_or(simdv<N>::sign(v), simdv::cmpeq(v, htab))));
        return not is_valid and ((cr & constant::msb_64 | lf) and crlf);
    }

    template<int N>
    int http::parse_request_line(const void *in, const std::size_t size, const simdv<N>& v, u64_t& lf, u64_t& cr, u64_t& crlf)
    {
        static const simdv<N> vsp   = simdv<N>::splat('\x20');
        static const simdv<N> vhtab = simdv<N>::splat('\x9' );

        if ((crlf & 0x02) and this->unused) [[unlikely]]
            return  ((crlf & crlf >> 2) & 0x04) ? -400 /* empty request */ : -400 /* blank line TODO: skip */;
        if (has_trailing_ret()) [[unlikely]]
        {
            if not (lf & 0x01)
                return -400;
            lf &= ~0x1ULL;
            reqline.req_line[out_reader.at()] -= 1; // -cr
            in_reader.incr_by(1);                 // +lf
            return 0;
        }

        const u64_t sp    = simdv<N>::cmp_eq(v, vsp, vhtab).to_bitmask();
        const u64_t wsp   = ~static_cast<const u64_t>(has_trailing_whitespace()) & bits::trim(sp); // valid whitespace
        const u64_t tchar = simdv<N>::gt_or_lt(v, '\x20', '\x7f').to_bitmask() | wsp;

        if (auto has_any_rejected_token = (~tchar | lf | (cr & ~simd<N>::msb)) & bits::tzmask(crlf))
            return -400;
        this->unused = false;
        for (u64_t umask = (sp |cr | lf) & bits::blsmask(cr | lf); umask and not out_reader.is_zero(); umask &= umask - 1)
            reqline.req_line[out_reader.decr()] = in_reader.at() + bits::tzcnt(umask);

        if not (crlf)
        {
            set_trailing_ret(static_cast<bool>(cr & simd<N>::msb));
            set_trailing_whitespace(static_cast<bool>(sp & simd<N>::msb));
            return in_reader.incr(), 0;
        }
        crlf &= crlf - 1;
        in_reader.incr_by(reqline.req_line[out_reader.at() + 1] + 2); // +2 for cr and lf
        completed_request_line();
        return -(out_reader.iszero() or (req_version_tag(reqline.req_line, in, _req_type::index[this->req_type]) isnot http_1));
    }

    template <typename T, T out_size, int N>
    int http::parse_header(void *in, size_t in_size, req<T, out_size> &out, const simdv<N> & v, u64_t lf, u64_t cr, u64_t __crlf)
    {
        static const simdv<N> = simdv<N>::splat('\x3a');
        u64_t crlf = __crlf; // copy
        auto set_header = [](auto& cp, auto &np, auto pos, auto mask, int skip)
            {
                u64_t end = pos + tzcnt(mask);
                cp.len = end - cp.pos;
                np.pos = end + skip;
            };

        if (has_pending_value())
        {
            auto& value = out[out_reader.at()].value;
            if not (crlf)
                return in_reader.incr(), req_header_value(v);
            set_header(value, out[out_reader.incr()].name, in_reader.at(), crlf, 2);
            crlf &= crlf - 1;
            unset_pending_value();
            if not (req_header_value(v, lf, cr, __crlf) or trim_whitespace<T>(in, value.pos, value.len)) [[unlikely]]
                return -400;
        }
        for (u64_t col = simdv<N>::cmp_eq(v, v_col).to_bitmask(); true; )
        {
            auto& name = out[out_reader.at()].name, &value = out[out_reader.at()].value;
            const u64_t first_col = bits::lsb(col);

            if constexpr (not OPTIMIZE_FOR_MOST_CASE)
                if (crlf and bits::lsb(crlf) < bits::lsb(col)) [[unlikely]]
                    return -400;
            if not (col)
                return in_reader.incr();
            // set position and length of name
            set_header(name, value, in_reader.at(), col, 1);
            if not (crlf)
            {
                set_pending_value();
                in_reader.incr();
                return req_header_value(v);
            }
            // set position and length of value
            set_header(value, out[out_reader.incr()].name, in_reader.at(), crlf & bits::xlsfill(first_col), 2);
            col  &= bits::xlsfill(crlf);
            crlf &= crlf - 1;
            bool all_wsp = trim_whitespace<T>(in, value.pos, value.len);
            if (all_wsp or not req_header_name(in, name.len) or not req_header_value(v, lf, cr, __crlf, 0)) [[unlikely]]
                return -400;
        }
        in_reader.incr();
        return 0;
    }

    template <typename T, T out_size, int N>
    inline int http::parse(void *in, size_t in_size, req<T, out_size> &out, std::size_t run_size, std::size_t rem)
    {
        // nly handle 32 and 64 byte chunks
        static_assert(N >= 32 and (N & 1) == 0);

        std::size_t j = 0;
        bool run = true;
        do {
            static const simdv v_lf = simdv<N>::splat('\xa');
            static const simdv v_cr = simdv<N>::splat('\xd');

            u8_t *b = reinterpret<u8_t *>(reinterpret_cast<u64_t *>(in) + j);
            simdv<N> v = simd<N>::load(b);

            u64_t lf   = simdv<N>::cmp_eq(v, v_lf ).to_bitmask();
            u64_t cr   = simdv<N>::cmp_eq(v, v_cr ).to_bitmask();
            u64_t crlf = cr & (lf << 1);

            if (incomplete_request_line() and parse_request_line<N>(in, size, v, lf, cr, crlf) < 0) [[unlikely]]
               return -400;
            if (completed_request_line() and parse_header<T, out_size, N>(in, in_size, out, v, lf, cr, crlf) < 0)
                return -400;
            
            // maybe the end of us parsing this buffer (eop)
            if (auto eop = crlf & crlf >> 2)
                return j * N - (N - bits::tzcnt(eop));
            // incr
            run = ++j < run_size;
            // or maybe eop is incomplete: like cr, crlf, crlfcr
            if (auto eop = (lf | cr) & simdv<N>::msb3) [[unlikely]]
            {
                // the top three bits of intN in the eop mask can either be 100, 110 or 111
                // in each case tab[top_three_bits_in_eop] gives us the number of bytes we need to check
                // also tab[tab[last_three_bits_in_eop]] gives the number of times we need to shift backward in order to read a complete crlfcrlf word
                static constexpr eop_tab[8]{0, 2, 1, 0, 3, 0, 2, 1};
                int n = eop_tab[eop >> N - 3];
                if (run or rem >= n) [[likely]]
                    return -((reinterpret_cast<u32_t *>(in) + (j - 1) * N - eop_tab[n])[0] == 0xd0a0d0a);
                return -(this->n_bytes_to_complete = n);  // we need atleast <= 3 bytes to confirm an exact eop
            }
        } while (run);
        return 0;
    }

    template <typename T, T out_size>
    int http::nparse(void *in, size_t in_size, req<T, out_size> &out)
    {
        static_assert(std::is_integral_v(T) and sizeof(T) <= sizeof(u64_t));
        static_assert(out_size > 0);

        if (in_reader.set(in_size, 64) < 0 or out_reader.set(out_size) < 0)
            return -400;
  
        const std::size_t n = in_size / 64; // read 64 bytes chunks
        u64_t rem = in_size % 64;
        int stat = 0;

        if (n)
        {
            stat = parse<T, out_size, 64>(in, in_size, out, n, rem);
            if unlikely (parse_failed(stat) or not re)
                return stat;
        }
        // Handle trailing 32 bytes
        if (rem > 31)
        {
            // TODO: Handle EOPARSE
            rem %= 32
            in_reader.set_incr(32);
            stat = parse<T, out_size, 32>(in, in_size, out, 1, rem);
            if unlikely (; parse_failed(stat) or not re)
                return stat;
        }
        // Trailing bytes < 31. safely copy to buffer and process
        u8_t b[32];
        memcpy(b, in + n, re);
        // place the last re::byte in b[last]
        b[32] = b[rem - 1];

        #if HANDLE_TRAIL_LAZY
        // TODO
        #endif
        return stat;
}
