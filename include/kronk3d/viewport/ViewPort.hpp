#pragma once

#include "kronk3d/utils/Vector.hpp"
#include <cstdint>

namespace k3
{

    struct ViewPort
    {
        long int xmin, xmax, ymin, ymax;

        inline Vector4f applyTo(Vector4f vec) const
        {
            vec.x = xmin + (xmax - xmin) * (0.5f + 0.5f * vec.x);
            vec.y = ymin + (ymax - ymin) * (0.5f + 0.5f * vec.y);
            return vec;
        };
    };

}
