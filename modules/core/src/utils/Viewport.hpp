/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Viewport struct for rasterizer
*/
#pragma once

#include <cstddef>
#include <vector>
#include <Vector.hpp>

namespace k3
{

    struct Viewport
    {
        long int xmin, xmax, ymin, ymax;

        inline Math::Vector4f applyTo(Math::Vector4f vec) const
        {
            vec.x = xmin + (xmax - xmin) * (0.5f + 0.5f * vec.x);
            vec.y = ymin + (ymax - ymin) * (0.5f - 0.5f * vec.y);
            return vec;
        };
    };

}
