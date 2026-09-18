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
        float r{}, g{}, b{}, a{};

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

    inline Color operator*(float s, const Color& v)
	{
		return {s * v.r, s * v.g, s * v.b, s * v.a};
	}

    inline Color operator-(const Color& v0, const Color& v1)
	{
		return {v0.r - v1.r, v0.g - v1.g, v0.b - v1.b, v0.a - v1.a};
	}

	inline Color operator+(const Color& v0, const Color& v1)
	{
		return {v0.r + v1.r, v0.g + v1.g, v0.b + v1.b, v0.a + v1.a};
	}

}
