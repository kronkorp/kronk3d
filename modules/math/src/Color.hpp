/*
** KRONK CORP, 2026
** KRONKMATH
** File description:
** Colors
*/
#pragma once

#include <cmath>

namespace k3::Math
{

    // Exact sRGB transfer functions (IEC 61966-2-1).
    inline float srgbToLinear(float c)
    {
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    inline float linearToSrgb(float c)
    {
        return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
    }

    struct Color
    {
        float r{}, g{}, b{}, a{};

        static inline Color fromRGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        {
            return Color(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
        }

        // Renderers work in linear space: use this for colors picked in an image editor / color picker.
        static inline Color fromSRGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        {
            return Color(srgbToLinear(r / 255.f), srgbToLinear(g / 255.f), srgbToLinear(b / 255.f), a / 255.f);
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

    // Component-wise product (modulation), e.g. texture * vertex color.
	inline Color operator*(const Color& v0, const Color& v1)
	{
		return {v0.r * v1.r, v0.g * v1.g, v0.b * v1.b, v0.a * v1.a};
	}

	inline Color operator*(const Color& v, float s)
	{
		return {v.r * s, v.g * s, v.b * s, v.a * s};
	}

	inline Color& operator+=(Color& v0, const Color& v1)
	{
		v0.r += v1.r;
		v0.g += v1.g;
		v0.b += v1.b;
		v0.a += v1.a;
		return v0;
	}

}
