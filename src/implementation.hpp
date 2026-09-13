#ifndef DHTTP_IMPLEMENTAION_HPP
#define DHTTP_IMPLEMENTATION_HPP

#include "include/definition.hpp"
#include "simd/implementation.hpp"
#include "include/bits.hpp"
#include "include/constants.hpp"
#include "common/common.hpp"

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
        Reader(u64_t max=std::numeric_limits<u64_t>::max()-1, u64_t incr=1) noexcept
        {
            assert (max  < std::numeric_limits<u64_t>::max());
            assert (incr < __max);
            __i    = 0;
            __incr = incr;
            __max  = max;
        }


        int set(u64_t max, u64_t incr=1) noexcept
        {
             if (__i > max or incr > max)
                return -1;
            __incr = incr;
            __max  = max;
            return 0;
        }

        inline int set_incr(u64_t incr) noexcept
        {
            if (incr > __max)
                return -1;
            __incr = incr;
            return 0;
        }

        inline u64_t get_incr(void) const noexcept
        {
            return __incr;
        }

        u64_t at(void) const noexcept
        {
            return __i;
        }

        u64_t size(void) const noexcept
        {
            return __i;
        }

        u64_t capacity(void) const noexcept
        {
            return __max;
        }
        
        u64_t iszero(void) const noexcept
        {
            return __i == 0;
        }

        inline u64_t incr_by(u64_t incr) noexcept
        {
            assert(__i <= (__max - incr));
            return __i += incr;
        }

        inline u64_t decr_by(u64_t decr) noexcept
        {
            assert(__i >= (__max - decr));
            return __i -= decr;
        }

        inline u64_t incr(void) noexcept
        {
            assert(__i <= (__max - __incr));
            return __i += __incr;
        }

        inline u64_t decr(void) noexcept
        {
            assert(__i >= (__max - __incr));
            return __i -= __incr;
        }

        inline u64_t operator++(void)
        {
            return incr();
        }

        inline u64_t operator--(void)
        {
            return decr();
        }

        private:
        u64_t __i;
        u64_t __incr;
        u64_t __max;
    };

    alignas(1) struct State
    {
        bool request_completed : 1 = 0;
        bool pending_value     : 1 = 0;
        bool trailing_ret      : 1 = 0;
        bool trailing_wsp      : 1 = 0;
        bool parse_completed   : 1 = 0;

        inline bool completed_request_line(bool x)  { return request_completed = x; }
        inline void set_pending_value(bool x)       { pending_value = x; }
        inline void set_trailing_ret(bool x)        { trailing_ret  = x; }
        inline void set_trailing_whitespace(bool x) { trailing_wsp  = x; }
        inline bool completed_request_line(void)  const { return request_completed; }
        inline bool has_pending_value(void)       const { return pending_value;     }
        inline bool has_trailing_ret(void)        const { return trailing_ret;      }
        inline bool has_trailing_whitespace(void) const { return trailing_wsp;      }
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

    struct Reqtype
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
        http(void) { reset(); }

        void reset(std::size_t run_size=0, int incr=0, std::size_t out_size=3)
        {
            req_type  = Reqtype::type::request; out_reader = {out_size, 1};
            in_reader = {run_size, incr}; version = -1; n_bytes_to_complete = 0;
            state     = {0}; unused  = true;
        }

    private:
        // header line (version, method, version, status, message)
        req_line reqline;
        // internal in & out buffer counter
        Reader in_reader, out_reader;
        // request type (request or response)
        Reqtype::type req_type;
        // http minor version (the major is tested to be 1)
        int  version;
        // number of expected eop (end of parse) bytes (crlfcrlf)
        int  n_bytes_to_complete;
        // state
        State state;
        // true after reset
        bool unused;

        template <typename T, T out_size, int N>
        int parse(void *in, std::size_t in_size, req<T, out_size>& out, std::size_t run_size, std::size_t rem);
        template<int N>
        int parse_request_line(void *in, std::size_t size, const simdv<N>& v, u64_t& lf, u64_t& cr, u64_t& crlf);
        template <typename T, T out_size, int N>
        int parse_header(void *in, std::size_t in_size, req<T, out_size>& out, const simdv<N>& v, u64_t lf, u64_t cr, u64_t crlf);
        template <typename T, T out_size>
        int nparse_no_rescan(void *in, std::size_t in_size, std::size_t run_size, req<T, out_size> &out);

        int http::nparse_header_line_fallback(void *in, std::size_t in_size, std::size_t run_size, u64_t *out, std::size_t out_size);
        inline bool parse_failed(int stat)
        {
            return stat < 0;
        }

        inline int set_version(u8_t i)
        {
            return (this->version = i ^ '\x30') < 10;
        }

        inline bool req_version_is_http_1(void *b)
        {
            return common::version_is_http_1(b) and set_version(reinterpret_cast<u8_t *>(b)[7]);
        }

        inline u16_t req_size(u64_t (&req)[], int i)
        {
            return this->req_type is Reqtype::type::request ? (req[i - 0] - (req[i + 1]) - 1)
                                                            : (req[i - 1] - (req[i - 0]) - 1); // -1 for the sp seperator
        }

        inline bool req_version_tag(u64_t (&req)[], void *in, Reqtype::req_index &i)
        {
            static constexpr u16_t req_version_required_size = 8; // len(HTTP/1.x)
            return (req_size(req, i[0]) == req_version_required_size) and req_version_is_http_1(in + req[i[0]]);
        }
    };
};
#endif //IMPLEMENTATION_HPP