
#pragma once

#include "pixel/Pixel.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace k3
{

    class Rasterizer
    {
        public:
            Rasterizer(size_t width, size_t height) : m_pixels(width * height), m_zbuffer(width * height), m_viewWidth(width), m_viewHeight(height) {}
            Rasterizer(const Rasterizer& other) = delete;
            ~Rasterizer() = default;

            const std::vector<Pixel>& framebuffer() const noexcept { return m_pixels; }
            size_t                    viewWidth() const noexcept { return m_viewWidth; }
            size_t                    viewHeight() const noexcept { return m_viewHeight; }

        private:
            std::vector<Pixel>  m_pixels;   //!< The framebuffer of pixels
            std::vector<double> m_zbuffer;  //!< The zbuffer of pixels

            size_t              m_viewWidth;  //!< The widht of view in px
            size_t              m_viewHeight; //!< The height of view in px
    };

}
