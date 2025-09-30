#pragma once

#include <cstddef>
#include <new>

namespace spapq
{

#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t CACHE_LINE_SIZE = 64U;
#endif

} // end namespace spapq