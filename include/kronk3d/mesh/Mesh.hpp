#pragma once

#include "kronk3d/utils/Vector.hpp"
#include "kronk3d/utils/Color.hpp"
#include <vector>

namespace k3
{

    struct Mesh
    {
        std::vector<Vector3f> vertices;
        Color                 color;
    };

}
