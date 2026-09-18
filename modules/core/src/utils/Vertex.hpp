/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Vertex struct for kronK3d
*/
#pragma once

#include "Color.hpp"
#include "Vector.hpp"
#include "utils/texture/Texture.hpp"
#include <algorithm>
#include <cstdint>

namespace k3
{

    struct Vertex
    {
        Math::Vector4f point;
        Math::Vector2f uv;
        Math::Color    color;
    };

	Vertex clipIntersected(const Vertex& v0, const Vertex& v1, float value0, float value1);
	Vertex* clip(Vertex * triangle, Math::Vector4f equation, Vertex * result);
	Vertex* clip(Vertex* begin, Vertex* end);

}
