/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Vertex struct for kronK3d
*/
#pragma once

#include "Color.hpp"
#include "Vector.hpp"
namespace k3
{

    struct Vertex
    {
        Math::Vector4f point;
        Math::Color    color;
    };

}
