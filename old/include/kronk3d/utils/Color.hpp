#pragma once

namespace k3
{

    struct Color
    {

        float r;
        float g;
        float b;
        float a;

        inline Color() : r(0.f), g(0.f), b(0.f), a(1.f) {}
        inline Color(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}

        static const Color Black;
        static const Color Red;
        static const Color Blue;
        static const Color Green;
        static const Color Yellow;
        static const Color White;

    };

    inline Color operator*(const Color& color, float t)
    {
        return Color(color.r * t, color.g * t, color.b * t, color.a);
    }

}
