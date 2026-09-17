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

        static inline Color fromRGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        {
            return Color(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
        }

        static const Color Black;
        static const Color Red;
        static const Color Blue;
        static const Color Green;
        static const Color Yellow;
        static const Color White;
        static const Color Magenta;
        static const Color Transparent;
        static const Color Cyan;
        static const Color Grey;
    };

}
