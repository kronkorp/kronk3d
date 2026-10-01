/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Clip-space polygon clipping
*/
#pragma once

#include "Pipeline.hpp"
#include <cstddef>

namespace k3::sw
{

    // Near, far, and a guard band on x/y (wide enough to rarely trigger, small enough to keep
    // fixed-point screen coordinates far from overflowing): each plane adds at most one vertex.
    inline constexpr std::size_t MAX_CLIPPED_VERTICES = 3 + 6;

    // Clips a triangle and writes the resulting convex polygon to `out` (0, or 3 to 9 vertices).
    std::size_t clipTriangle(const ClipVertex& v0, const ClipVertex& v1, const ClipVertex& v2, ClipVertex out[MAX_CLIPPED_VERTICES]);

}
