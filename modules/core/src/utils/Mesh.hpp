/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Mesh struct for rasterizer
*/
#pragma once

#include "Color.hpp"
#include <cstddef>
#include <vector>
#include <Vector.hpp>

namespace k3
{

    struct Mesh
    {
        std::vector<Math::Vector3f> vertices;
        std::vector<Math::Vector2f> uvs;
        std::vector<Math::Color>    colors;
        std::vector<std::size_t>    indices;
        std::size_t                 count;
    };

}
