#pragma once
#include "../include/definition.hpp"

namespace dhttp::simd
{
#if HAVE__SSE2__
#    include "westmere/implementation.hpp"
     using namespace simd::westmere;
#endif
}