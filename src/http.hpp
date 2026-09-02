#pragma once
#include "include/definiton.hpp"
#include "common/common.hpp"
#include "common/bits.hpp"
#include "simd/simd.hpp"

namespace dhttp::tables
{
    static constexpr u8_t U = 0x80;

    /* ! # \$ % & ' * + - . ^ _ ` | A-Za-z0-9 : / ? #, [ ] @ ! $ & ' ( ) * + , ; = */
    alignas(64) static constexpr uint8_t token_charset[256]{
        // low 4 bits map
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        // High 4 bits map
        0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

    alignas(64) static constexpr u8_t tchar_map[256]{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, U,
        0, U, U, U, U, 0, 0, 0, U, 0, U, U, U, 0, U, U,
        U, U, U, U, U, U, U, U, 0, 0, 0, 0, 0, 0, 0, U,
        U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
        U, U, U, U, U, U, U, U, U, 0, 0, 0, U, U, U, U,
        U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U,
        U, U, U, U, U, U, U, U, U, 0, U, 0, U, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

namespace dhttp::Implementation
{
    class http;

    constexpr int COMPLETE = 0;
    constexpr int EXPECT_DATA = 1;

    template <typename T, size_t N>
    struct req
    {
        static_assert(std::is_integral_v<T> && N > 0);
        static constexpr u64_t __size = N;
        u64_t __used = 0;

        using __pair = struct
        {
            T len, pos;
        };

        struct {
            __pair name, value;
        } pair [N];

        constexpr u64_t size(void) noexcept
        {
            return __size;
        }

        u64_t used(void) const noexcept
        {
            return __used;
        }

        u64_t set_used(T i) noexcept
        {
            assert( i < __size );
            return __used = i;
        }

        auto &&get(T i) const noexcept
        {
            assert( i < __size );
            return pair[i];
        }

        auto &&operator[](T i) noexcept
        {
            return pair[i];
        }
    };

    struct req_line
    {
        /*
            [N]   request | response
            ______________|________
            [3]  method   | version
            [2]  uri      | status
            [1]  version  | msg
            [0]  NULL     | NULL
        */
        u16_t req_line[4];
    };

    struct req_state
    {
        u16_t pos           = 0; // absolute index of last byte parsed
        u16_t j             = 3; // request line field count (0, 3)
        bool  trailing_sp   = 0; // carry of trailing sp
        bool  trailing_cr   = 0;
        bool  req_line      = 0; // request line
        bool  no_init       = 1; // true if decoding of request/status-line is pending (not started)
        bool  pending_name  = 0;
        bool  pending_value = 0;
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

    class http
    {
    public:
        http() : req_type{_req_type::type::request}, version(-1) {}

    private:
        _req_type::type req_type;
        req_state state;
        req_line reqline;
        int version;
        int   req_version(u8_t i);
        u16_t req_size(const u16_t (&req)[], const int i) const;
        bool  req_version_is_http_1(const void *ver_string);
        bool  req_version_tag(const u16_t (&req)[], const void *buf, const _req_type::req_index& i);
        template <typename T = u16_t, std::size_t out_size> int parse(void *in, size_t in_size, req<T, out_size> &out);
        int parse_request_line(const void *in, const std::size_t size, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf);
        template <typename T = u16_t, std::size_t out_size>
        int parse_header(void *in, size_t in_size, req<T, out_size> &out, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf);
    };
};
