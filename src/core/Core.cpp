#include <kronk3d/core/Core.hpp>
#include <algorithm>
#include "kronk3d/utils/Color.hpp"

namespace k3
{

    void Rasterizer::clear(const Color& color)
    {
        std::fill_n(this->m_pixels.begin(), this->m_pixels.size(), color);
    }

}
