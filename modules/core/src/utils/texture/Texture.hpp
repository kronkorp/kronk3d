/*
** KRONK CORP, 2026
** KRONKMATH
** File description:
** Ftg victor
*/
#pragma once

#include "Color.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace k3
{

    struct Texture
    {
        int width{}, height{}, channels{};
        std::vector<Math::Color> pixels{};

        static Texture fromImage(const std::string& filename);

        inline const Math::Color& operator[](size_t idx) const
        {
            return pixels[idx];
        }

        inline const Math::Color& at(size_t x, size_t y) const
        {
            if (pixels.empty()) {
                return Math::Color::Grey;
            }
            return pixels[(y * width + x)];
        }
    };

}
