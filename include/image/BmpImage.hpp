
#pragma once

#include "image/Image.hpp"
#include "pixel/Pixel.hpp"
#include <cstddef>
#include <string_view>
#include <vector>

namespace k3
{

    class BmpImage : Image
    {
        public:
            BmpImage(const std::vector<Pixel>& pixels, size_t width, size_t height)
                : m_pixels(pixels), m_width(width), m_height(height) {}
            ~BmpImage() = default;

            void save(std::string_view filename) const override;

        private:
            std::vector<Pixel> m_pixels; //!< Copy of the framebuffer to export
            size_t              m_width;  //!< Image width in px
            size_t              m_height; //!< Image height in px
    };

}
