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
#include "stb_image.h"

namespace k3
{

    struct Texture
    {
        int width{}, height{}, channels{};
        std::vector<Math::Color> pixels{};

        static inline Texture fromImage(const std::string& filename)
        {
            Texture t;
            unsigned char *stb = stbi_load(filename.c_str(), &t.width, &t.height, &t.channels, 4);

            if (!stb) {
                // Throw;
                return Texture{};
            }

            for (auto y = 0; y < t.height; ++y) {
                for (auto x = 0; x < t.width; ++x) {
                    int index = (y * t.width + x) * 4;
                    t.pixels.push_back(Math::Color::fromRGB(stb[index], stb[index + 1], stb[index + 2], stb[index + 3]));
                }
            }

            stbi_image_free(stb);

            return t;
        }

        inline const Math::Color& operator[](size_t idx) const
        {
            return pixels[idx];
        }

        inline const Math::Color& at(size_t x, size_t y) const
        {
            return pixels[(y * width + x) * 4];
        }
    };

}
