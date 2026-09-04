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

    static constexpr u8_t mask_win[128]{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
}

namespace dhttp::Implementation
{
    constexpr int COMPLETE = 0;
    constexpr int EXPECT_DATA = 1;

    template <typename T, T N>
    struct req
    {
        static_assert(std::is_integral_v<T> and (sizeof(T) < sizeof(u64_t)) and N > 0);
        static constexpr T __size = N;
        T __used = 0;

        struct __pair
        {
            using int_type as T;
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

        auto &get(T i) const noexcept
        {
            assert( i < __size );
            return pair[i];
        }

        auto &operator[](T i) noexcept
        {
            return pair[i];
        }
    };

    struct Reader {
        Reader(u64_t i=0, u64_t incr=1)
        {
            assert (i < __max);
            assert (incr < __max);
            __i = i;
            __incr = incr;
        }

        u64_t set(u64_t i, u64_t incr) noexcept
        {
            assert (i < __max);
            assert (incr < __max);
            __incr = incr;
            return __i = i;
        }

        u64_t at(void) const noexcept
        {
            return __i;
        }

        u64_t count(void) const noexcept
        {
            return __i;
        }

        u64_t iszero(void) const noexcept
        {
            return __i == 0;
        }

        u64_t get_incr(void) const noexcept
        {
            return __incr;
        }

        u64_t incr(void) noexcept
        {
            return __i += __incr;
        }

        u64_t decr(void) noexcept
        {
            return __i -= __incr;
        }

        u64_t incr_by(u64_t i) noexcept
        {
            return __i += i;
        }

        u64_t decr_by(u64_t i) noexcept
        {
            return __i -= i;
        }

        u64_t safe_incr(void) noexcept
        {
            assert(__i < (__max - __incr));
            return __i += __incr;
        }

        u64_t safe_decr(void) noexcept
        {
            assert(__i > __incr);
            return __i -= __incr;
        }

        u64_t operator++(void)
        {
            return safe_incr();
        }

        u64_t operator--(void)
        {
            return safe_decr();
        }

        private:
        u64_t __i;
        u64_t __incr;
        const static u64_t __max = std::numeric_limits<u64_t>::max();
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
        u64_t req_line[4];
    };

    struct req_state
    {
        u64_t pos           = 0; // absolute index of last byte parsed
        u64_t j             = 3; // request line field count (0, 3)
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
        http()
            : req_type{_req_type::type::request},
              version(-1),
              out_reader(3),
              in_reader(0, read_size) {}

    private:
        /// READ SIZE
        static constexpr int read_size = 64;

        _req_type::type req_type;
        req_state state;
        req_line reqline;
        Reader out_reader, in_reader;
        int version;
        int   req_version(u8_t i);
        u16_t req_size(const u64_t (&req)[], const int i) const;
        bool  req_version_is_http_1(const void *ver_string);
        bool  req_version_tag(const u64_t (&req)[], const void *buf, const _req_type::req_index& i);
        template <typename T, T out_size>
        int parse(void *in, size_t in_size, req<T, out_size> &out);
        int parse_request_line(const void *in, const std::size_t size, const simd &v, u64_t &lf, u64_t &cr, u64_t &crlf);
        template <typename T, T out_size>
        int parse_header(void *in, size_t in_size, req<T, out_size> &out, const simd &v, u64_t lf, u64_t cr, u64_t crlf);
    };
};
