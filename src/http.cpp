#include "http.hpp"
using namespace dhttp;

#define is ==
#define isnot !=

class dhttp1
{
    using u64_t = uint64_t;
    using u32_t = uint32_t;
    using u16_t = uint16_t;
    using u8_t  = uint8_t;

private:
    dhttp_attr(always_inline) bool reset(dhttp::state_t &state) const
    {
        return true;
    }

   dhttp_attr(always_inline) bool has_obstext(const dhttp::__m512i_ &v) const
    {
        static const dhttp::__m512i_ m  = dhttp::_mm512_set1_epi8_(0x80);
        return dhttp::_mm512_testz_si512_(dhttp::_mm512_and_si512_(v, m));
    }

   dhttp_attr(always_inline) u64_t classify_rfc(const dhttp::__m512i_ &v) const
    {
#if HAVE_SHUFLE__
        static const dhttp::__m512i_ lo = dhttp::_mm512_loadu_si512_(dhttp::table::bitmap256_valid_request_charset_shufb);
        static const dhttp::__m512i_ hi = dhttp::_mm512_loadu_si512_(dhttp::table::bitmap256_valid_request_charset_shufb + 64);
        return dhttp::_mm512_movemask_epi8_(dhttp::_mm512_and_si512_(dhttp::_mm512_shuffle_epi8_(lo, v), dhttp::_mm512_shuffle_epi8_(hi, dhttp::_mm512_srli_epi64_(v, 4)))); // valid rfc chars
#else
        return dhttp::_mm512_movemask_epi8_(
            dhttp::_mm512_and_si512_(dhttp::_mm512_cmpgt1_epi8_(dhttp::_mm512_and_si512_(v, dhttp::_mm512_set1_epi8_('\xf')), '\x0'),
                                     dhttp::_mm512_cmpglt_epi8_(dhttp::_mm512_srli_epi64_(v, 4), '\x1', '\x9')));
#endif
    }

   dhttp_attr(always_inline) void dump_state(dhttp::state_t &state, bool sp, bool cr, bool done) const
    {
        state.done   = done;  // done
        state.state |= cr; // save trailing CR
        state.trailing_sp = sp; // save trailing SP
    }

    int extract_fields(dhttp::header_t &input, dhttp::state_t &state, u64_t lf, u64_t cr, u64_t crlf, u64_t col)
    {
        static const dhttp::__m512i_ SP = dhttp::_mm512_set1_epi8_(dhttp::SP);
        const u64_t sp = dhttp::_mm512_movemask_epi8_(dhttp::_mm512_cmpeq_epi8_(state.v, SP));

        const u64_t valid_sp = ~static_cast<const u64_t>(state.trailing_sp) & trim(sp); // reject multiple sp
        const u64_t valid_char = classify_rfc(state.v); // valid tokens

        if (state.done is true)
        {
            // PARSE REQUEST/STATUS LINE
            u64_t mask = sp | crlf | cr | lf;
            u64_t umask = mask & blsr(crlf | cr | lf);

            if (state.state & dhttp::STATE_TRAILING_CR)
                return lf & 0x01 ? 0 : -1;

            // reject blank line before request/status line
            if (unlikely((crlf & 0x02) && state.decode_once))
            {
                if ((crlf & crlf >> 2) & 0x04)
                    return 0; // empty request (TODO: reject any further attempt to parse from the buffer)
                return -1;
            }

            if ((~(valid_char | valid_sp) | lf | (cr & ~0x8000000000000000ULL)) & -lsb(crlf))
                return -1;

            for (; umask and state.j; state.j--)
            {
                input.request.request_line[state.j].line_end = state.pos + tzcnt(umask);
                umask &= umask - 1;
            }
            mask &= ~umask;
            state.pos += 64;
            
            if(unlikely(state.pos < (dhttp::REQUEST_LINE_MAX_SIZE + 64)))
                return -1;
            
            if (state.done is false)
            {
                dump_state(state, valid_sp & 0x8000000000000000ULL, cr & 0x8000000000000000ULL, cr | lf | crlf);
                return 0;
            }
            lf   &= mask;
            cr   &= mask;
            crlf &= mask;
            // TODO: state.i
        }

        if (unlikely(state.state & dhttp::RESUME))
        {
            const u64_t first_lf = lsb(lf);
            if (not first_lf)
                return 0; // still data, no line-feed(lf)

            // TODO: extract field-name, field_value

            state.state |= dhttp::RESUME;
            // unset false colon seperator (multiple colons in a line, only the first is the seperator)
            col &= ~blsfill(first_lf);
            lf  &= ~first_lf;
        }

        while (unlikely(col))
        {
            const u64_t first_lf  = lsb(lf),
                        first_col = lsb(col),
                        next_lf   = lsb(lf & ~blsfill(first_col)); // next lf after first colon

            if (first_lf and (first_lf < first_col))
                return -1; // missing header name
            if (not next_lf)
            {
                state.state |= dhttp::RESUME; // no linefeed(lf), all data
                break;
            }

            uint16_t s = tzcnt(first_lf);
            uint16_t e = tzcnt(next_lf);

            // TODO: extract field-name, field-value

            col &= ~blsfill(first_lf);
            lf  &= ~first_lf;
        }
    }

    int parse_header(dhttp::header_t &input, dhttp::state_t &state)
    {
        const dhttp::__m512i_ LF  = dhttp::_mm512_set1_epi8_(dhttp::LF);
        const dhttp::__m512i_ CR  = dhttp::_mm512_set1_epi8_(dhttp::CR);
        const dhttp::__m512i_ COL = dhttp::_mm512_set1_epi8_(dhttp::COL);

        const size_t n = (input.size + (size_t)63) & ~(size_t)63; // align read/load size to 64

        for (size_t j = 0; j < n; j += 64)
        {
            u8_t *b = input.recvb.recvbuf + j;
            state.v = dhttp::_mm512_loadu_si512_(b);

            u64_t cr   = dhttp::_mm512_movemask_epi8_(dhttp::_mm512_cmpeq_epi8_(state.v, CR));
            u64_t lf   = dhttp::_mm512_movemask_epi8_(dhttp::_mm512_cmpeq_epi8_(state.v, LF));
            u64_t col  = dhttp::_mm512_movemask_epi8_(dhttp::_mm512_cmpeq_epi8_(state.v, COL));
            u64_t crlf = lf & (cr >> 1); // \r\n\r\n

            if (unlikely(extract_fields(input, state, lf, cr, crlf, col) < 0))
                return -1;
            // stop, if \r\n\r\n is found
            if (unlikely(crlf & crlf >> 2))
                return j + tzcnt(crlf & crlf >> 2);
            // handle any crlf carry
            if (unlikely((lf | cr) & 0xe000000000000000ull))
                if ((dhttp::CR is b[-3]) && (dhttp::LF is b[-2]) && (dhttp::CR is b[-1]) && (dhttp::LF is b[0]))
                    return j + 4;
        }
        return dhttp::EXPECT_DATA;
    }
};