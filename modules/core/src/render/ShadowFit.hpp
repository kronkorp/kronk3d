/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Light-space projection of a directional shadow map (shared by every backend)
*/
#pragma once

#include "Bounds.hpp"
#include "Matrix.hpp"
#include "Vector.hpp"
#include <optional>

namespace k3
{

    struct ShadowProjection
    {
        Math::Matrix4 viewProjection;   // World -> light clip space (orthographic, depth in [-1, 1])
        float         radius;           // Half-size of the covered square, in world units
    };

    // Orthographic box around the bounding sphere of `bounds`, looking along `direction` (normalized).
    // Nothing when the bounds are empty.
    std::optional<ShadowProjection> fitShadowProjection(const Math::Bounds3f& bounds, const Math::Vector3f& direction);

}
