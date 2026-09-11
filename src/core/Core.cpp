#include "core/CoreException.hpp"
#include "pixel/Pixel.hpp"
#include <core/Core.hpp>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string_view>

namespace k3
{

    void Rasterizer::clear(const Color& color)
    {
        std::fill_n(this->m_pixels.begin(), this->m_pixels.size(), color);
    }

}
