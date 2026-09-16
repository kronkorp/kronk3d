/*
** KRONK CORP, 2026
** KRONKMATH
** File description:
** Colors
*/
#pragma once

namespace k3::Math
{

    struct Color
    {
        float r{}, g{}, b{}, a{1.f};

        static inline Color fromRGB(unsigned char r, unsigned char g, unsigned char b)
        {
            return Color(r / 255.f, g / 255.f, b / 255.f);
        }

        static inline Color fromRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
        {
            return Color(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
        }
    };

}
