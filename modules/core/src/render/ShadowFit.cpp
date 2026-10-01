#include "ShadowFit.hpp"
#include <algorithm>
#include <cmath>

std::optional<k3::ShadowProjection> k3::fitShadowProjection(const Math::Bounds3f& bounds, const Math::Vector3f& direction)
{
    if (bounds.empty())
        return std::nullopt;

    const Math::Vector3f center = bounds.center();
    const float radius = std::max(bounds.radius(), 1e-3f);
    const Math::Vector3f up = std::abs(direction.y) > 0.99f ? Math::Vector3f{0.f, 0.f, 1.f} : Math::Vector3f{0.f, 1.f, 0.f};
    const Math::Matrix4 view = Math::Matrix4::lookAt(center - direction * radius, center, up);
    const Math::Matrix4 projection = Math::Matrix4::orthographic(-radius, radius, -radius, radius, 0.f, 2.f * radius);

    return ShadowProjection{projection * view, radius};
}
