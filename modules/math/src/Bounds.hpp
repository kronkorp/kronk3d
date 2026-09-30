/*
** KRONK CORP, 2026
** KRONKMATH
** File description:
** Axis-aligned bounding box
*/
#pragma once

#include "Matrix.hpp"
#include "Vector.hpp"
#include <algorithm>
#include <limits>

namespace k3::Math
{

    struct Bounds3f
    {
        Vector3f min{ std::numeric_limits<float>::max(),  std::numeric_limits<float>::max(),  std::numeric_limits<float>::max()};
        Vector3f max{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};

        bool empty() const noexcept
        {
            return min.x > max.x || min.y > max.y || min.z > max.z;
        }

        void expand(const Vector3f& p) noexcept
        {
            min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
            max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
        }

        void merge(const Bounds3f& b) noexcept
        {
            if (b.empty())
                return;
            expand(b.min);
            expand(b.max);
        }

        Vector3f center() const noexcept
        {
            return 0.5f * (min + max);
        }

        Vector3f size() const noexcept
        {
            return max - min;
        }

        float radius() const noexcept
        {
            return 0.5f * Vector3f::length(size());
        }

        // AABB enclosing the 8 transformed corners.
        Bounds3f transformed(const Matrix4& m) const noexcept
        {
            Bounds3f result;

            if (empty())
                return result;
            for (int i = 0; i < 8; ++i) {
                result.expand(m.transformPoint({
                    (i & 1) ? max.x : min.x,
                    (i & 2) ? max.y : min.y,
                    (i & 4) ? max.z : min.z,
                }));
            }
            return result;
        }
    };

}
