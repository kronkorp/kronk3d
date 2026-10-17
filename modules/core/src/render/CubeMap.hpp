/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Cube map addressing, as in the OpenGL specification (shared by every backend)
*/
#pragma once

#include <kronk3d/math/Vector.hpp>
#include <cmath>

namespace k3::cube
{

    // Faces: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z. (s, t) are in [0, 1], t = 0 on the first row of the image.
    struct Lookup
    {
        int   face;
        float s, t;
    };

    inline Lookup lookup(const Math::Vector3f& d) noexcept
    {
        const float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
        float major, sc, tc;
        int face;

        if (ax >= ay && ax >= az) {
            major = ax;
            face = d.x > 0.f ? 0 : 1;
            sc = d.x > 0.f ? -d.z : d.z;
            tc = -d.y;
        } else if (ay >= az) {
            major = ay;
            face = d.y > 0.f ? 2 : 3;
            sc = d.x;
            tc = d.y > 0.f ? d.z : -d.z;
        } else {
            major = az;
            face = d.z > 0.f ? 4 : 5;
            sc = d.z > 0.f ? d.x : -d.x;
            tc = -d.y;
        }
        return {face, 0.5f * (sc / major + 1.f), 0.5f * (tc / major + 1.f)};
    }

    // The way back: a direction (not normalized) that looks at (s, t) of `face`.
    inline Math::Vector3f direction(int face, float s, float t) noexcept
    {
        const float sc = 2.f * s - 1.f, tc = 2.f * t - 1.f;

        switch (face) {
            case 0:  return {1.f, -tc, -sc};
            case 1:  return {-1.f, -tc, sc};
            case 2:  return {sc, 1.f, tc};
            case 3:  return {sc, -1.f, -tc};
            case 4:  return {sc, -tc, 1.f};
            default: return {-sc, -tc, -1.f};
        }
    }

}
