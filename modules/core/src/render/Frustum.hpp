/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** View frustum, to skip draws that cannot be seen (shared by every backend)
*/
#pragma once

#include <kronk3d/math/Bounds.hpp>
#include <kronk3d/math/Matrix.hpp>
#include <array>

namespace k3
{

    // The six planes of a view-projection matrix (OpenGL clip space: -w <= x, y, z <= w), pointing inward.
    class Frustum
    {
        public:
            explicit Frustum(const Math::Matrix4& viewProjection)
            {
                const float* m = viewProjection.values;

                for (int axis = 0; axis < 3; ++axis) {
                    for (int side = 0; side < 2; ++side) {
                        const float sign = side == 0 ? 1.f : -1.f;      // w + c, then w - c
                        Plane& plane = m_planes[static_cast<std::size_t>(axis * 2 + side)];

                        plane.normal = {m[12] + sign * m[axis * 4], m[13] + sign * m[axis * 4 + 1], m[14] + sign * m[axis * 4 + 2]};
                        plane.offset = m[15] + sign * m[axis * 4 + 3];
                    }
                }
            }

            // False only when `bounds` is entirely outside one plane: conservative, a box near a corner
            // of the frustum can be kept although nothing of it shows.
            [[nodiscard]] bool intersects(const Math::Bounds3f& bounds) const noexcept
            {
                if (bounds.empty())
                    return false;
                for (const Plane& plane : m_planes) {
                    // The corner the furthest along the normal: if it is behind the plane, the whole box is.
                    const Math::Vector3f corner{
                        plane.normal.x >= 0.f ? bounds.max.x : bounds.min.x,
                        plane.normal.y >= 0.f ? bounds.max.y : bounds.min.y,
                        plane.normal.z >= 0.f ? bounds.max.z : bounds.min.z,
                    };

                    if (plane.normal.x * corner.x + plane.normal.y * corner.y + plane.normal.z * corner.z + plane.offset < 0.f)
                        return false;
                }
                return true;
            }

        private:
            struct Plane
            {
                Math::Vector3f normal{};
                float          offset = 0.f;
            };

            std::array<Plane, 6> m_planes{};
    };

}
