#ifndef DHTTP_IMPLEMENTATION_MAIN_HPP
#define DHTTP_IMPLEMENTATION_MAIN_HPP
#include "implementation.hpp"
#include "scalar_impl.hpp"

namespace dhttp::Implementation
{   
    bool http_1 = true;
    bool done   = true;

    template <int N>
    inline bool req_header_value(void *in, const simdv<N>& v, u64_t lf, u64_t cr, u64_t crlf, bool done)
    {
        #if 0
        static simdv<N> sp   = simdv<N>::splat('\x20');
        static simdv<N> htab = simdv<N>::splat('\x9' );
        bool is_valid = simdv<N>::is_zero(simdv<N>::_or(simdv<N>::gt_or_lt(v, '\x19', '\x7f'), simdv<N>::_or(simdv<N>::sign(v), simdv::cmpeq(v, htab))));
        return not is_valid and ((cr & constant::msb_64 | lf) and crlf);
        #endif
    }

    template <int N> inline bool req_header_value_(const simdv<N>& v)
    {
        return false;
    }

    template<int N>
    int http::parse_request_line(void *in, std::size_t size, const simdv<N>& v, u64_t& lf, u64_t& cr, u64_t& crlf)
    {
        static const simdv<N> vsp   = simdv<N>::splat('\x20');
        static const simdv<N> vhtab = simdv<N>::splat('\x9' );

        if (this->unused and (crlf & 0b10)) [[unlikely]]
            return  (crlf & crlf >> 2) & 0b100 ? -400 /* empty request */ : -400 /* blank line TODO: skip */;
        if (state.has_trailing_ret()) [[unlikely]]
        {
            if not (lf & 0b1)
                return -400;
            lf &= ~0x1ULL;
            reqline.req_line[out_reader.at()] -= 1; // -cr
            in_reader.incr_by(1);                   // +lf
            goto end;
        }

        const u64_t sp    = simdv<N>::cmp_eq(v, vsp, vhtab).to_bitmask();
        const u64_t tchar = simdv<N>::gt_or_lt(v, '\x20', '\x7f').to_bitmask() | ~U64(state.has_trailing_whitespace()) & bits::ltrim(sp); // valid whitespace
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
        end:
        crlf &= crlf - 1;
        in_reader.incr_by(reqline.req_line[out_reader.at() + 1] + 2); // +2 for cr and lf
        state.completed_request_line(true);
        return -(mask or req_version_tag(reqline.req_line, in, Reqtype::index[this->req_type]) isnot http_1);
    }

    template <typename T, T out_size, int N>
    int http::parse_header(void *in, size_t in_size, req<T, out_size>& out, const simdv<N>& v, u64_t lf, u64_t cr, u64_t __crlf)
    {
        static const simdv<N> v_col = simdv<N>::splat('\x3a');
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
            if (stat = parse<T, out_size, simd::max>(in, in_size, out, n, r); parse_failed(stat) or r == 0) [[unlikely]]
                return stat;
        // Parse any 32 bytes
        if (in_reader.set_incr(32); r > 31) [[likely]]
        {
            n += r; r &= (32 - 1);
            if (stat = parse<T, out_size, 32>(in, in_size, out, n, r); parse_failed(stat) or r == 0) [[unlikely]]
                return stat;
        }
        // TRAILING BYTES
        // In order to keep things clean, we avoid backtracking and reparsing past bytes
        // Anything below 31, falls through to the scalar path. since N < 31, it should be cheap
        // However, if AVX512 is enabled, and it's desired, we could use mask_load to handle the trailing bytes
        if constexpr (simd<simd::max>::spec is simd::AVX512)
        {
            u8_t *b = reinterpret_cast<u8_t *>(in + n);
            // The cost here is that, we read memory twice: 1 for mask and 1 inside the parser itself
            // TODO: remove this path, if it doesn't prove any benefit
            alignas(32) u8_t bv[32];
            __mmask32 mask  = (0x1ULL << r) - 1;
            __mm256i  fill  = _mm256_set1_epi8('\x65'); // fill dummy but valid tchar
            __mm256i in_mask_load = _mm256_mask_loadu_epi8(fill, mask, b);
            __mm512i dup_hi = _mm256_or_si256(kb, _mm256_srli_si256(in_mask_load, 32 - r));
            _mm512_store_epi32(bv, dup_hi);
            if (stat = parse<T, out_size, 32>(in, in_size, out, 32, r)) [[unlikely]]
                return stat;
        }
        pure_scalar:
        // TODO
        return 0;
    }
}
#endif