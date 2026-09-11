#include <core/Core.hpp>
#include <algorithm>
#include "utils/Color.hpp"

namespace k3
{

    void Rasterizer::clear(const Color& color)
    {
        std::fill_n(this->m_pixels.begin(), this->m_pixels.size(), color);
    }

}
