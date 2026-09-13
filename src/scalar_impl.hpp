#ifndef IMPLEMENTATION_SCALAR_HPP
#define IMPLEMENTATION_SCALAR_HPP
#include "implementation.hpp"
#include "common/common.hpp"

// ROUGH
namespace dhttp::Implementation
{
    inline bool is_valid(u8_t i) { return i > 0x20 and i < 0x7f; }
    
    int http::nparse_header_line_fallback(void *in, std::size_t in_size, std::size_t run_size, u64_t *out, std::size_t out_size)
    {
        static constexpr u64_t v_sp  = constant::c20;
        static constexpr u64_t v_tab = common::_dup('\x9');
        static constexpr u64_t v_lf  = common::_dup('\xa');
        static constexpr u64_t v_cr  = common::_dup('\xd');
        static constexpr u64_t v_col = common::_dup('\x3a');

        std::size_t j = out_reader.at();
        std::size_t i = in_reader.at();

        u64_t *b = reinterpret_cast<u64_t *>(in + i);
        bool tcr = false, tsp = false;
        
        for (; out_size > 8; out_size -= 8)
        {
            u64_t v = b[0];

            u64_t cr = common::_cmpeq(v, v_cr);
            u64_t lf = common::_cmpeq(v, v_lf);
            u64_t crlf = cr & (lf << 1);

            if (tcr) [[unlikely]]
            {
                if (not (lf & 1))
                    return -400;
                goto end;
            }
            if (crlf & 0b10) [[unlikely]]
                return 0;

            u64_t sp = common::_cmpeq(v, v_sp) | common::_cmpeq(v, v_tab);
            u64_t tchar = ~common::_cmp_gt_and_lt<'\x20', '\x7f'>(v) | lf | bits::ltrim(sp) | (cr ^ constant::msb_64);
            
            if (0 and tchar & bits::blsmask(crlf));
            if (tchar)
                return -400;
            tcr = cr & constant::msb_64;
            u64_t mask = sp;
            for (; mask and j; j--, mask &= mask - 1)
                reinterpret_cast<u8_t *>(out)[i] = i + bits::tzcnt(mask);
            i += 8;
            if (not crlf)
            {
                if (j == 0) [[unlikely]]
                    return -400;
                continue;
            }
            goto end;
        }
        b += 0;
        while (j and out_size--)
        {
            u8_t c = b[out_size];
            if (c == '\xa' or c == '\xd')
                break;
            if (c == '\x20' or c == '\x9'))
                {
                    if (tsp) [[unlikely]]
                        return -400;
                    out[j] += i;
                    j -= 1;
                }
            i++;
        }
        u8_t c = b[i];
        bool x = 0;
        switch (out_size)
        {
            case 1: x = is_valid(b[i]); break;
            case 2: x = is_valid(b[i]) & is_valid(b[i+1]); break;
            case 3: x = is_valid(b[i]) & is_valid(b[i+1]) & is_valid(b[i+1]); break;
            case 4: x = (common::_cmp_gt_and_lt<'\x20', '\x7f'>(reinterpret_cast<u32 *>(b+i)) == 0b1111U); break;
            case 5: x = (common::_cmp_gt_and_lt<'\x20', '\x7f'>(reinterpret_cast<u32 *>(b+i)) == 0b1111U) & is_valid(b[i+4]); break;
            case 6: x = (common::_cmp_gt_and_lt<'\x20', '\x7f'>(reinterpret_cast<u32 *>(b+i)) == 0b1111U) & is_valid(b[i+4]) & is_valid(b[i+4]); break;
            case 7: x = (common::_cmp_gt_and_lt<'\x20', '\x7f'>(reinterpret_cast<u32 *>(b+i)) == 0b1111U) & is_valid(b[i+4]) & is_valid(b[i+4]) & is_valid(b[i+4]); break;
        }
        if (x == 0) [[unlikely]]
            return -400;
        end:
        //state.completed_request_line(true);
        //return -(mask or req_version_tag(reqline.req_line, in, Reqtype::index[this->req_type]) isnot http_1);
        return 0;
    }

    int nparse_no_rescan(void *in, std::size_t in_size, std::size_t run_size, void *out, std::size_t out_size)
    {
    }
}
#endif