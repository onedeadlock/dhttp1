#ifndef DHTTP_DEFINE_H
#define DHTTP_DEFINE_H
#include <cstdint>
#include <cstring>
#include <array>
#include <type_traits>

#if defined(__AVX2__) || defined(__SSSE3__) || defined(__SSE4_2__) || defined(__SSE2__)
#    if defined(__AVX2__)
#        define HAVE__AVX2__   1
#    elif defined(__SSE2__)
#        define HAVE__SSE2__   1
#    elif defined(__SSSE3__)
#        define HAVE__SSSE3__  1
#    elif defined (__SSE4_2__)
#        define HAVE__SSE4_2__ 1
#    endif
#    include <immintrin.h>
#elif defined(_ARM_NEON)
#    define HAVE__ARM_NEON__ 1
#    include <arm_neon.h>
#endif

#if defined(__SIZEOF_INT128__)
#    define HAVE__INT128__ 1
#endif

#define HAVE_SHUFFLE__ 0 // set if we have ssse3

/////////////////////////////////////
////////// PERFORMANCE //////////////
#ifndef OPTIMIZE_FOR_MOST_CASE
#    define OPTIMIZE_FOR_MOST_CASE 1
#endif
#ifndef SUPPORT_FULL_TCHAR
#    define SUPPORT_FULL_TCHAR 0
#endif
/////////////////////////////////////
/////////////////////////////////////

#if defined(__GNUC__) || (defined(__clang__) && !defined(_MSC_VER))
#    define HAVE_GNUC_C__ 1
#elif defined(_MSC_VER)
#    define HAVE_VIST_C__ 1
#endif

#if HAVE_GNUC_C__
#    define likely(x)   (__builtin_expect(!!(x), 1))
#    define unlikely(x) (__builtin_expect(!!(x), 0))
#elif defined(__cplusplus) && __cplusplus >= 202002L
#    define likely(x)   (x) [[likely]]
#    define unlikely(x) (x) [[unlikely]]
#else
#    define likely(x)   (x)
#    define unlikely(x) (x)
#endif

#if   HAVE_GNUC_C__
#    define inline  [[gnu::always_inline]] inline
#elif HAVE_VIST_C__
#    define inline  __forceinline
#else
#    define inline inline
#endif

#define U32(x)  static_cast<const u32_t>(x)
#define U64(x)  static_cast<const u64_t>(x)
#define U128(x) static_cast<const u128_t>(x)
#define UMAX(x) static_cast<const umax_t>(x)

#if HAVE__INT128__
#   define UMAX_C(x) static_cast<const umax_t>(x)
#else
#   define UMAX_C(x) 0
#endif

//////////////////////////////
//////////// DHTTP ///////////
//////////////////////////////
namespace Dhttp
{
    using u8_t  = std::uint8_t;
    using u16_t = std::uint16_t;
    using u32_t = std::uint32_t;
    using u64_t = std::uint64_t;
#if HAVE__INT128__
    using u128_t = __uint128_t;
    using umax_t = __uint128_t;
#else
    using umax_t = uint64_t;
#endif
}

#endif // DHTTP_DEFINE_H