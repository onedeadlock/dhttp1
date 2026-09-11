#include "implementation.hpp"

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
        if constexpr (OPTIMIZE_FOR_MOST_CASE > 3)
        {
            return U64(x[b[0]]) & U64(x[b[1]]) & U64(x[b[2]]) & U64(x[b[3]]) &
                   U64(x[b[4]]) & U64(x[b[5]]) & U64(x[b[6]]) & U64(x[b[7]]);
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
            return scalar::ascii_fast_tchar(reinterpret_cast<u64_t *>(b)[0]) or is_valid_name_token_(reinterpret_cast<u8_t *>(b));
        }
        return is_valid_name_token_(reinterpret_cast<u8_t *>(b));
    }

    inline u64_t is_valid_name_token_loop(u8_t *b, std::size_t len)
    {
        auto &x = tables::tchar_map;
        int i = 0;
        while (i < len and x[b[i++]]) [[likely]] pass();
        return i == len;
    }

    inline bool req_header_name(u8_t *b, std::size_t len)
    {
        // TODO: modify len
        if constexpr (not STRICT_HTTP or IGNORE_LEADING_SP)
            len -= is_whitespace(b[len - 1]);
        const u8_t *end = b + (len & ~(constant::int_size - 1));
        for (; b != end and is_valid_name_token(b); b += 8) [[likely]] pass();
        const u64_t r = len % constant::int_size;
        if (b != end or not r)
            return b == end;
        return is_valid_name_token_loop(b, r);
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

    inline bool http::req_version_is_http_1(void *b)
    {
        static constexpr u64_t mask = U64('\x48') | U64('\x54') << 8 | U64('\x54') << 16 | U64('\x50') << 24 |
                                      U64('\x2f') << 32 | U64('\x2e') << 40 | U64('\x31') << 48; // H  T  T  P  /  1  .
        return mask == (reinterpret_cast<u64_t *>(b)[0] & 0x00ffffffffffffff) and req_version(reinterpret_cast<u8_t *>(b)[7]);
    }

    inline u16_t http::req_size(const u64_t (&req)[], const int i) const
    {
        return this->req_type is Reqtype::type::request ? (req[i - 0] - (req[i + 1]) - 1)
                                                          : (req[i - 1] - (req[i - 0]) - 1); // -1 for the sp seperator
    }

    inline bool http::req_version_tag(u64_t (&req)[], void *in, Reqtype::req_index &i)
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
        std::size_t t_pos = rcount_whitespace(bv, U64(len));

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

        if (this->unused and (crlf & 0b10)) [[unlikely]]
            return  (crlf & crlf >> 2) & 0b100 ? -400 /* empty request */ : -400 /* blank line TODO: skip */;
        if (state.has_trailing_ret()) [[unlikely]]
        {
            if not (lf & 0x01)
                return -400;
            lf &= ~0x1ULL;
            reqline.req_line[out_reader.at()] -= 1; // -cr
            in_reader.incr_by(1);                   // +lf
            return 0;
        }

        const u64_t sp    = simdv<N>::cmp_eq(v, vsp, vhtab).to_bitmask();
        const u64_t tchar = simdv<N>::gt_or_lt(v, '\x20', '\x7f').to_bitmask() | ~U64(state.has_trailing_whitespace()) & bits::trim(sp); // valid whitespace
        if ((~tchar | lf | (cr & ~simd<N>::msb)) & bits::tzmask(crlf))
            return -400; /* invalid token */
        this->unused = false;
        u64_t mask = (sp |cr | lf) & bits::blsmask(cr | lf); 
        for (; mask and not out_reader.is_zero(); mask &= mask - 1)
            reqline.req_line[out_reader.decr()] = in_reader.at() + bits::tzcnt(mask);

        if not (crlf)
        {
            state.set_trailing_ret(static_cast<bool>(cr & simd<N>::msb));
            state.set_trailing_whitespace(static_cast<bool>(sp & simd<N>::msb));
            in_reader.incr();
            return -(mask and out_reader.is_zero());
        }
        crlf &= crlf - 1;
        in_reader.incr_by(reqline.req_line[out_reader.at() + 1] + 2); // +2 for cr and lf
        state.completed_request_line(true);
        return -(mask or req_version_tag(reqline.req_line, in, Reqtype::index[this->req_type]) isnot http_1);
    }

    template <typename T, T out_size, int N>
    int http::parse_header(void *in, size_t in_size, req<T, out_size>& out, simdv<N>& v, u64_t lf, u64_t cr, u64_t __crlf)
    {
        static const simdv<N> = simdv<N>::splat('\x3a');
        u64_t crlf = __crlf; // copy
        auto set_header = [](auto& cp, auto& np, auto pos, auto mask, int skip)
            {
                u64_t end = pos + tzcnt(mask);
                cp.len = end - cp.pos;
                np.pos = end + skip;
            };

        if (state.has_pending_value())
        {
            auto& value = out[out_reader.at()].value;
            if not (crlf)
                return in_reader.incr(), req_header_value(v);
            set_header(value, out[out_reader.incr()].name, in_reader.at(), crlf, 2);
            crlf &= crlf - 1;
            state.set_pending_value(false);
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
                state.set_pending_value(true);
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
    inline int http::parse(void *in, size_t in_size, req<T, out_size> &out, std::size_t run_size, std::size_t r)
    {
        // only handle 32 and 64 byte chunks
        static_assert(N >= 32 and (N & 1) == 0);

        u8_t *b   = reinterpret_cast<u8_t>(in) + in_reader.size();
        u8_t *end = reinterpret_cast<u8_t>(in) + run_size;

        do {
            static const simdv v_lf = simdv<N>::splat('\xa');
            static const simdv v_cr = simdv<N>::splat('\xd');

            simdv<N> v = simd<N>::load(b);

            u64_t lf   = simdv<N>::cmp_eq(v, v_lf ).to_bitmask();
            u64_t cr   = simdv<N>::cmp_eq(v, v_cr ).to_bitmask();
            u64_t crlf = cr & (lf << 1);

            if (not state.completed_request_line() and parse_request_line<N>(in, size, v, lf, cr, crlf) < 0) [[unlikely]]
               return -400;
            if (state.completed_request_line() and parse_header<T, out_size, N>(in, in_size, out, v, lf, cr, crlf) < 0) [[unlikely]]
                return -400;
            
            // maybe the end of us parsing this buffer (eop)
            if (auto eop = crlf & crlf >> 2)
                return 0;
            // next chunk
            b += N;
            // or maybe eop is incomplete; cases like cr, crlf, crlfcr
            if (auto eop = (lf | cr) >> N - 3; n > 0b100) [[unlikely]]
            {
                // fast fail for (cr/lf)_*Non-crlf*_(cr/lf)
                if (eop & 0b101) [[unlikely]]
                    return -400;
                // the top three bits of intN in the eop mask can be 100, 110 or 111
                // in any of the cases, tab[top_three_bits_in_eop] gives us the number of bytes we need to check
                // also tab[tab[last_three_bits_in_eop]] gives the number of times we need to shift backward in order to read a complete crlfcrlf word
                static constexpr alignas(8) u8_t eop_tab[8]{0, 2, 1, 0, 3, 0, 2, 1};
                int n = eop_tab[eop];
                if (b != end or r >= n) [[likely]]
                    return -(reinterpret_cast<u32_t *>(b - N - eop_tab[n])[0] == 0xd0a0d0a);
                return -(this->n_bytes_to_complete = n);  // we need atleast <= 3 bytes to confirm an exact eop
            }
        } while (b != end);
        return 0;
    }

    template <typename T, T out_size>
    int http::nparse_no_rescan(void *in, size_t in_size, size_t run_size, req<T, out_size> &out)
    {
        assert(in != std::nullptr and out != std::nullptr and in_size >= run_size); 

        /*
         *  Specialization 
         */
        static constexpr int ceil = 15;

        if (auto n = this->n_bytes_to_complete)
        {
            static constexpr alignas(4) u8_t eop_shift[4] = {0, 2, 1, 0};
            if (run_size < in_reader.size() or (run_size - in_reader.size()) < n)
                return 0; /* need more bytes */
            return -(reinterpret_cast<u32_t *>(b + in_reader.at() - eop_shift[n])[0] == 0xd0a0d0a);
        }
        int stat = 0;
        auto n = run_size & ~(simd::max - 1);
        auto r = run_size &  (simd::max - 1);
        // First, try parsing buffer with specialization size
        if (this->reset(run_size, simd::max); n != 0)
            if unlikely (stat = parse<T, out_size, simd::max>(in, in_size, out, n, r); parse_failed(stat) or r == 0)
                return stat;
        // AVX512 here is an overkill (and not recommended for parsing most likely small bytes as http headers - my opinion anyways)
        // however, if it is enabled, we could use its useful mask_load to handle trailing bytes if they are above ceil
        if constexpr (simd<simd::max>::spec is simd::AVX512) 
        {
            if (r > ceil)
            {
                alignas(64) u8_t b[64];
                // TODO: mask load and store to b
            }
            if constexpr (not simd::mix_avx512_avx2)
                goto pure_scalar;
            }
        }
        // parse trailing 32 bytes
        if (in_reader.set_incr(32); r > 31) [[likely]]
        {
            n += r; r &= (32 - 1);
            simd<32>::zero();
            if unlikely (stat = parse<T, out_size, 32>(in, in_size, out, n, r); parse_failed(stat) or r == 0)
                return stat;
        }
        // trailing bytes or input < 31; if buffer is padded with atleast 32 bytes
        if (r > ceil)
        {
            if ((in_size - n) > 31) [[likely]]
            {
                // TODO
                if (r == 0) return 0;
            }
            if constexpr (not NO_COPY_TRAILS)
            {
                alignas(32) u8_t b[32]{};
                memcpy(b, reinterpret_cast<u8_t *>(in + n), r);
                if (r == 0)
                    return 0; // TODO: process copy
            }
        }

        pure_scalar:
        // fallthrough to scalar
        return stat;
}
