#pragma once
#include "definition.hpp"

namespace dhttp::constant
{
    static constexpr std::size_t int_size   = sizeof (u64_t);
    static constexpr std::size_t int_size_p = (int_size / 2) - 1;

    static constexpr u64_t c7f = 0x7f7f7f7f7f7f7f7fULL;
    static constexpr u64_t cff = 0xffffffffffffffffULL;
    static constexpr u64_t c80 = 0x8080808080808080ULL;
    static constexpr u64_t c01 = 0x0101010101010101ULL;
    static constexpr u64_t c09 = 0x0909090909090909ULL;
    static constexpr u64_t c20 = 0x2020202020202020ULL;
    static constexpr u64_t c30 = 0x3030303030303030ULL;
    static constexpr u64_t cdf = 0xdfdfdfdfdfdfdfdfULL;

    static constexpr u64_t compress = 0x0002040810204081ULL;
    static constexpr u64_t msb_64   = 0x8000000000000000ULL;
    static constexpr u64_t msb_32   = 0x0000000080000000ULL;
    static constexpr u64_t msb3_64  = 0xe000000000000000ULL;
    static constexpr u64_t msb3_32  = 0x00000000e0000000ULL;
    

    static constexpr u64_t  hyphen = U64('\x2d') * c01;
    
    static constexpr u64_t AZ_const = c7f & cdf;
    static constexpr u64_t A = U64('\x7f' - '\x40') * c01;
    static constexpr u64_t Z = U64('\x7f' + '\x5b') * c01;

    static constexpr u64_t DeBruijn64_const = 0x03f79d71b4cb0a89ULL;

    static constexpr u8_t DeBruijn64_seq[64]{
        0,  47, 1,  56, 48, 27, 2,  60,
        57, 49, 41, 37, 28, 16, 3,  61,
        54, 58, 35, 52, 50, 42, 21, 44,
        38, 32, 29, 23, 17, 11, 4,  62,
        46, 55, 26, 59, 40, 36, 15, 53,
        34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30, 9,  24,
        13, 18, 8,  12, 7,  6,  5,  63};
}