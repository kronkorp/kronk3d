#pragma once

namespace k3
{

    struct Pixel
    {

        float r;
        float g;
        float b;
        float a;

        Pixel() : r(0.f), g(0.f), b(0.f), a(1.f) {}
        Pixel(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}
    };

}
