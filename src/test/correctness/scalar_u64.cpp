#include "scalar_u64.hpp"
#include <immintrin.h>
#include <new>
#include <random>
#include <numeric>
#include <functional>
#include <iostream>

void fill(std::vector<uint8_t>& x)
{
    static std::uniform_int_distribution<uint8_t> character_dist(0, 255);
    x.push_back(character_dist.a());
}

int main(void)
{
    std::size_t buf_size = 1024;
    std::vector<uint8_t> buf(buf_size);
    

    //scalar::__m256i s_x = scalar::_mm256_loadu_si256(b);
    for (int i = 0; i < buf_size; i++)
        fill(buf);
    
    
    std::cout << std::endl;

    return 0;
}