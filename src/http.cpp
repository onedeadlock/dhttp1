#include "http.hpp"

#define is ==
#define isnot !=
#define inline __attribute__((always_inline)) inline

namespace dhttp
{
    struct _req_type {
        using req_index = const int (&)[];
        enum type : bool {
            request  = 0,
            response = 1,
        };

        static constexpr int index[2][3] = {
            ////////////////////////////////////////////////////
            //// REQUEST {req_method, req_uri, req_version} ////
            ////////////////////////////////////////////////////
            {3, 2, 1},
            ////////////////////////////////////////////////////
            //// RESPONSE {req_version, req_stat, req_msg} /////
            ////////////////////////////////////////////////////
            {1, 2, 3},
        };
    };

    class dhttp::http
    {
    public:
        dhttp::_req_type::type req_type;
        http() : start_index{0}, req_type{dhttp::_req_type::type::request} {}

    private:
        u16_t start_index; // buffer start index

        bool done = true;

        inline bool has_obstext(dhttp::simd &v) const
        {
            static const dhttp::simd hi{'\x80'};
            return dhttp::simd::testzero(v & hi);
        }

        inline u64_t classify_rfc(dhttp::simd &v) const
        {
#if defined(HAVE_SHUFFLE__)
            static const dhttp::simd lo{dhttp::tables::bitmap_valid_charset};
            static const dhttp::simd hi{dhttp::tables::bitmap_valid_charset + 64};
            return dhttp::simd::movemask(dhttp::simd::shufb(lo, v) & dhttp::simd::shufb(hi, v >> 4));
#else
            return dhttp::simd::movemask(((dhttp::simd('\xf') & v) > '\x0') & dhttp::simd::cmpglt(v >> 4, '\x1', '\x9'));
#endif
        }

        bool req_vtag_is_http_1(const void *ver_string)
        {
            static constexpr u64_t mask = U64('\x48') | U64('\x54') << 8 | U64('\x54') << 16 | U64('\x50') << 24 |
                                          U64('\x2f') << 32 | U64('\x2e') << 40 | U64('\x31') << 48; // H  T  T  P  /  1  .
            return mask == (*static_cast<const u64_t *>(ver_string) & 0x00ffffffffffffff);
        }

        u16_t req_size(const dhttp::req_t (&req)[], const int i)
        {
            return http::req_type is dhttp::_req_type::type::request ? (req[i - 0].end - req[i + 1].end)
                                                                     : (req[i - 1].end - req[i - 0].end);
        }

        bool req_version_tag(const dhttp::req_t (&req)[], const u8_t *buf, const dhttp::_req_type::req_index& i)
        {
            static constexpr u16_t req_version_required_size = 8; // strlen(HTTP/1.x)
            return (req_size(req, i[0]) == req_version_required_size) and req_vtag_is_http_1(buf + req[i[0]].end);
        }

        int extract_fields(dhttp::header_t &input, state_t &state, u64_t lf, u64_t cr, u64_t crlf, u64_t col)
        {
            static const dhttp::simd wsp{'\x20'};
            const u64_t sp = dhttp::simd::movemask(wsp == state.mv);

            const u64_t valid_sp   = ~static_cast<const u64_t>(state.trailing_sp) & trim(sp);
            const u64_t valid_char = classify_rfc(state.mv);

            if (state.req_line is done)
            {
                ///////////////////////////////////////////////////
                ////////// PARSE REQUEST-STATUS LINE //////////////
                ///////////////////////////////////////////////////
                if (state.state & dhttp::STATE_TRAILING_CR)
                {
                    if (lf & 0x01)
                        return -400;
                    state.req_line = true;
                    goto post_req_line;
                }

                if (unlikely(state.pos < dhttp::REQUEST_LINE_MAX_SIZE))
                    return -400;

                // reject blank line at the start of request/response
                if (unlikely((crlf & 0x02) && state.parse_uinit))
                {
                    if ((crlf & crlf >> 2) & 0x04)
                        return 0; // empty request (TODO: reject any further attempt to parse from the buffer)
                    return -400;
                }

                if ((~(valid_char | valid_sp) | lf | (cr & ~0x8000000000000000ULL)) & -lsb(crlf))
                    return -400;

                u64_t mask  = sp | cr | lf;
                u64_t umask = mask & blsr(cr | lf);

                dhttp::req_t(&req)[] = input.request.request_line;
                for (; umask and state.j; state.j--)
                {
                    req[state.j].end = state.pos + tzcnt(umask);
                    umask &= umask - 1;
                }

                if (not crlf)
                {
                    state.pos += 64;
                    state.trailing_cr = cr & 0x8000000000000000ULL;
                    state.trailing_sp = sp & 0x8000000000000000ULL;
                    return 0;
                }
                mask &= ~umask;
                crlf &= mask, lf &= mask, cr &= mask;

                state.pos += req[state.j + 1].end;
                state.req_line = true;                 // done
                if (state.j isnot 0 or req_version_tag(req, input.recvb.recvbuf, dhttp::_req_type::index[req_type]))
                    return -400;
            }

            ///////////////////////////////////////////////////
            //////////////// PARSE HEADERS ////////////////////
            post_req_line:
            ///////////////////////////////////////////////////

            if (unlikely(state.state & dhttp::RESUME))
            {
                const u64_t first_lf = lsb(lf);
                if (not first_lf)
                    return 0; // still data, no line-feed(lf)

                // TODO: extract field-name, field_value here

                state.state |= dhttp::RESUME;
                // unset colon within values; TODO: false colon is between first lf/cr and next lf/cr not just after first
                col &= ~blsfill(first_lf);
                lf  &= ~first_lf;
            }

            while (unlikely(col))
            {
                const u64_t first_lf  = lsb(lf);
                const u64_t first_col = lsb(col);
                const u64_t next_lf   = lsb(lf & ~blsfill(first_col)); // next lf after first colon

                if (first_lf and (first_lf < first_col))
                    return -400; // missing header name
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
            return 0;
        }

        int parse_header(dhttp::header_t &input, state_t &state)
        {
            static const dhttp::simd LF{'\xa'};
            static const dhttp::simd CR{'\xd'};
            static const dhttp::simd CL{'\x3a'};

            const size_t n = (input.size + 63) & ~(size_t)63; // align read/load size to 64

            for (size_t j = 0; j < n; j += 64)
            {
                u8_t *b = input.recvb.recvbuf + j;
                state.mv = b;

                u64_t lf   = dhttp::simd::movemask(state.mv == LF);
                u64_t cr   = dhttp::simd::movemask(state.mv == CR);
                u64_t col  = dhttp::simd::movemask(state.mv == CL);
                u64_t crlf = lf & (cr >> 1);

                if (unlikely(extract_fields(input, state, lf, cr, crlf, col) < 0))
                    return -400;
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
}

#undef inline