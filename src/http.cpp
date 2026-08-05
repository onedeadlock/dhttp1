#include "http.hxx"

class Http1
{
    using u64_t = uint64_t;
    using u32_t = uint32_t;
    using u16_t = uint16_t;
    using u8_t  = uint8_t;

private:
    __attribute__((always_inline)) bool http_reset_state(Http::http_state_t &state) const
    {
        return true;
    }

    __attribute__((always_inline)) uint64_t http_classify_rfc_char_(const Http::__m512i_ &v) const
    {
        const uint8_t *const x = reinterpret_cast<const uint8_t *const>(&v);
        auto vc = [](const uint8_t *x, const uint8_t i) -> const uint8_t
        {
            return Http::vrci_tab[x[i + 0]] << 0 | Http::vrci_tab[x[i + 1]] << 1 | Http::vrci_tab[x[i + 2]] << 2 | Http::vrci_tab[x[i + 3]] << 3 |
                   Http::vrci_tab[x[i + 4]] << 4 | Http::vrci_tab[x[i + 5]] << 5 | Http::vrci_tab[x[i + 6]] << 6 | Http::vrci_tab[x[i + 7]] << 7;
        };
        return U32(vc(x, 0U)) << 0U | U32(vc(x, 8U)) << 8U | U32(vc(x, 16)) << 16 | U32(vc(x, 24)) << 24 |
               U64(vc(x, 32)) << 32 | U64(vc(x, 40)) << 40 | U64(vc(x, 48)) << 48 | U64(vc(x, 56)) << 56;
    }

    __attribute__((always_inline)) bool http_has_obstext(const Http::__m512i_ &v) const
    {
#if defined(HAVE_MICRO_BENCHMARK__) && HAVE_MICRO_BENCHMARK__ > 2
        static const Http::__m512i_ m  = Http::_mm512_set1_epi8_(0x80);
        return Http::_mm512_testz_si512_(Http::_mm512_and_si512_(v, m));
#elif defined(HAVE_MICRO_BENCHMARK__)
        const uint8_t *const x = reinterpret_cast<const uint8_t *const>(&v);
        // TODO: Remove: use small loop
        auto vc = [](const uint8_t *x, const uint8_t i) -> bool
        {
            return static_cast<bool>((x[i + 0] | x[i + 1] | x[i + 2] | x[i + 3] |
                                      x[i + 4] | x[i + 5] | x[i + 6] | x[i + 7]) &
                                     0x80);
        };
        return vc(x, 0U) || vc(x, 8U) || vc(x, 16) || vc(x, 24) || vc(x, 32) || vc(x, 40) || vc(x, 48) || vc(x, 56);
#else
        const uint8_t *const x = reinterpret_cast<const uint8_t *const>(&v);
        uint8_t k = 0;
        for (int i = 0; i < 64; i++)
            k |= x[i] & 0x80;
        return static_cast<bool>(k);
#endif
    }

    __attribute__((always_inline)) u64_t http_classify_rfc_char(const Http::__m512i_ &v) const
    {
#if HAVE_SHUFLE__
        static const Http::__m512i_ lo = Http::_mm512_loadu_si512_(Http::vrfc_class_tab);
        static const Http::__m512i_ hi = Http::_mm512_loadu_si512_(Http::vrfc_class_tab + 64);
        return Http::_mm512_movemask_epi8_(Http::_mm512_and_si512_(Http::_mm512_shuffle_epi8_(lo, v), Http::_mm512_shuffle_epi8_(hi, Http::_mm512_srli_epi64_(v, 4)))); // valid rfc chars
#else
        return http_classify_rfc_char_(v);
#endif
    }

    int http_extract_request_line(Http::http_header_t &b, Http::http_state_t &s, u64_t &lf, u64_t &cr, u64_t &crlf)
    {
        auto blsr = [&](u64_t x) -> u64_t
        { return x & (x - 1); };
        static const Http::__m512i_ _sp = Http::_mm512_set1_epi8_(Http::SP);
        const u64_t sp = Http::_mm512_movemask_epi8_(Http::_mm512_cmpeq_epi8_(s.v, _sp));

        // Blank line before request line historically added by some servers (as sort of a Bug)
        if (unlikely((crlf & 0x02) & s.st))
        {
            // TODO: only permit in response only (set st = false for request)
#if !defined(HAVE_STRICT_HTTP) || !HAVE_STRICT_HTTP
            if (unlikely((crlf & crlf >> 2) & 0x04))
                return -1; // http request with no header
            // CONTINUE: turn off every lf, cf, and crlf at the start
            cr   &= 0xfffffffffffffff7ULL;
            lf   &= 0xfffffffffffffffbULL;
            crlf &= 0xfffffffffffffffbULL;
            s.sz  = 2;     // skipped two bytes
            s.st  = false; // permit blank line only once
#else
            return -1; // reject!
#endif
        }
        // Trailing CR in request line in previous state
        if (unlikely(s.state & Http::HTTP_STATE_TRAILING_CR))
            return lf & 0x01 ? 0 : -1;

        // Multiple sp per token is illegal
        const u64_t vsp = ~static_cast<const u64_t>(s.sp) & trun(sp); // valid SPs. lsb(sp) discarded if previous state saved a trailing space
        const u64_t ivc = ~http_classify_rfc_char(s.v);               // invalid rfc chars
        const u64_t rc  = ((ivc ^ vsp)                                // exclude all valid single SP from invalid set
                          | lf | (cr & ~0x8000000000000000ULL))       // include CR (except the last trailing CR bit, since the next load may have a corresponding LF) and LF
                         & -lsb(crlf);                                // include only the bytes within the first EOL
        if (unlikely(rc))
            return -1;

        for (u64_t mask = (sp | crlf) & blsr(crlf); mask and s.j; s.j--)
        {
            b.reql.rl[s.j].line_end = s.i + __builtin_ctzll(sp);
            mask &= mask - 1;
        }
        s.i += 64;
        s.sp = static_cast<bool>(hib64(vsp));    // save trailing SP
        s.state |= static_cast<bool>(hib64(cr)); // save trailing CR
        s.done   = static_cast<bool>(crlf);      // done

        return s.i < Http::HTTP_RL_MAX_SIZE + 64;
    }

    int http_extract_fields(Http::http_header_t &b, Http::http_state_t &state, u64_t lf, u64_t cr, u64_t crlf, u64_t col)
    {
        auto blsfill = [&](u64_t x) -> u64_t
        { return x | (x - 1); };
        if ((state.state & Http::HTTP_RL_INCOMPLETE) and http_extract_request_line(b, state, lf, cr, crlf) < 0)
            return -1;

        if (unlikely(state.state & Http::HTTP_RESUME))
        {
            const u64_t first_lf = lsb(lf);
            if (not first_lf)
                return 0; // still data, no line-feed(lf)

            // TODO: extract field-name, field_value

            state.state |= Http::HTTP_RESUME;
            // unset false colon seperator (multiple colons in a line, only the first is the seperator)
            col &= ~blsfill(first_lf);
            lf  &= ~first_lf;
        }

        while (unlikely(col))
        {
            const u64_t first_lf  = lsb(lf),
                        first_col = lsb(col),
                        next_lf   = lsb(lf & ~blsfill(first_col)); // next lf after first colon

            // reject empty/malformed line (a line without a header)
            if (first_lf and (first_lf < first_col))
            {
                // but it could be \r\n\r\n
                const u64_t crlf_crlf = crlf & crlf >> 2; // \r\n\r\n bit mask
                if (crlf_crlf & first_lf)
                    return 0;
                return -1; // error
            }
            if (not next_lf)
            {
                // no line-feed(lf), all data
                state.state |= Http::HTTP_RESUME;
                break;
            }

            uint16_t s = __builtin_ctzll(first_lf);
            uint16_t e = __builtin_ctzll(next_lf);

            // TODO: extract field-name, field-value

            col &= ~blsfill(first_lf);
            lf  &= ~first_lf;
        }
    }

    int http_parse_header(Http::http_header_t &buf, Http::http_state_t &s)
    {
        const Http::__m512i_ _lf  = Http::_mm512_set1_epi8_(Http::LF);
        const Http::__m512i_ _cr  = Http::_mm512_set1_epi8_(Http::CR);
        const Http::__m512i_ _col = Http::_mm512_set1_epi8_(Http::COL);

        const size_t n = (buf.size + (size_t)63) & ~(size_t)63; // align read/load size to 64

        for (size_t j = 0; j < n; j += 64)
        {
            u8_t *b = buf.recvb.recvbuf + j;
            s.v = Http::_mm512_loadu_si512_(b);

            u64_t cr   = Http::_mm512_movemask_epi8_(Http::_mm512_cmpeq_epi8_(s.v, _cr));
            u64_t lf   = Http::_mm512_movemask_epi8_(Http::_mm512_cmpeq_epi8_(s.v, _lf));
            u64_t col  = Http::_mm512_movemask_epi8_(Http::_mm512_cmpeq_epi8_(s.v, _col));
            u64_t crlf = lf & (cr >> 1); // \r\n\r\n

            if (unlikely(http_extract_fields(buf, s, lf, cr, crlf, col) < 0))
                return -1;
            // stop, if \r\n\r\n is found
            if (unlikely(crlf & crlf >> 2))
                return j + __builtin_ctzll(crlf & crlf >> 2);
            // handle any crlf carry
            if (unlikely((lf | cr) & 0xe000000000000000ull))
                if ((Http::CR == b[-3]) && (Http::LF == b[-2]) && (Http::CR == b[-1]) && (Http::LF == b[0]))
                    return j + 4;
        }
        return Http::HTTP_EXPECT_DATA;
    }
};