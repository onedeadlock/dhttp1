#include "http.hpp"

#define is ==
#define isnot !=
#define inline __attribute__((always_inline)) inline

class dhttp::dhttp1
{
private:
    inline bool reset(state_t &state) const
    {
        return true;
    }

   inline bool has_obstext(dhttp::simd &v) const
    {
        static const dhttp::simd hi = '\x80';
        return dhttp::simd::testzero(v & hi);
    }

   inline u64_t classify_rfc(dhttp::simd &v) const
    {
#if defined(HAVE_SHUFFLE__)
        static const dhttp::simd lo = dhttp::tables::bitmap_valid_charset;
        static const dhttp::simd hi = dhttp::tables::bitmap_valid_charset + 64;
        return dhttp::simd::movemask(dhttp::simd::shufb(lo, v) & dhttp::simd::shufb(hi, v >> 4)); // valid rfc chars
#else
        return dhttp::simd::movemask(((dhttp::simd('\xf') & v) > '\x0') & dhttp::simd::cmpglt(v >> 4, '\x1', '\x9'));
#endif
    }

   inline void dump_state(state_t &state, bool sp, bool cr, bool done) const
    {
        state.done   = done;  // done
        state.state |= cr; // save trailing CR
        state.trailing_sp = sp; // save trailing SP
    }

    int extract_fields(dhttp::header_t &input, state_t &state, u64_t lf, u64_t cr, u64_t crlf, u64_t col)
    {
        static const dhttp::simd wsp = '\x20';
        const u64_t sp = dhttp::simd::movemask(wsp == state.mv);

        const u64_t valid_sp = ~static_cast<const u64_t>(state.trailing_sp) & trim(sp); // reject multiple sp
        const u64_t valid_char = classify_rfc(state.mv); // valid tokens

        if (state.done is true)
        {
            // PARSE REQUEST/STATUS LINE
            u64_t mask  = sp | crlf | cr | lf;
            u64_t umask = mask & blsr(crlf | cr | lf);

            if (state.state & dhttp::STATE_TRAILING_CR)
                return lf & 0x01 ? 0 : -1;

            // reject blank line before request/status line
            if (unlikely((crlf & 0x02) && state.parse_uinit))
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

    int parse_header(dhttp::header_t &input, state_t &state)
    {
        static const dhttp::simd LF = '\xa';
        static const dhttp::simd CR = '\xd';
        static const dhttp::simd CL = '\x3a';

        const size_t n = (input.size + (size_t)63) & ~(size_t)63; // align read/load size to 64

        for (size_t j = 0; j < n; j += 64)
        {
            u8_t *b = input.recvb.recvbuf + j;
            state.mv = b;

            u64_t lf  = dhttp::simd::movemask(LF == state.mv);
            u64_t cr  = dhttp::simd::movemask(CR == state.mv);
            u64_t col = dhttp::simd::movemask(CL == state.mv);
            u64_t crlf = lf & (cr >> 1);

            if (unlikely(extract_fields(input, state, lf, cr, crlf, col) < 0))
                return -1;
            // stop, if \r\n\r\n is found
            if (unlikely(crlf & crlf >> 2))
                return j + tzcnt(crlf & crlf >> 2);
            // handle any crlf carry
            if (unlikely((lf | cr) & 0xe000000000000000ull))
                if (('\xd' is b[-3]) && ('\xa' is b[-2]) && ('\xd' is b[-1]) && ('\xa' is b[0]))
                    return j + 4;
        }
        return dhttp::EXPECT_DATA;
    }
};

#undef inline