#include "ShadowFit.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

    using k3::Math::Vector3f;

    // Light orientation only (eye at the origin): positions are expressed in light space with it.
    k3::Math::Matrix4 lightRotation(const Vector3f& direction)
    {
        const Vector3f up = std::abs(direction.y) > 0.99f ? Vector3f{0.f, 0.f, 1.f} : Vector3f{0.f, 1.f, 0.f};
        return k3::Math::Matrix4::lookAt({0.f, 0.f, 0.f}, direction, up);
    }

    // Light-space depth range of the bounds' corners (z grows toward the light).
    void depthRange(const k3::Math::Bounds3f& bounds, const k3::Math::Matrix4& view, float& zMin, float& zMax)
    {
        zMin = std::numeric_limits<float>::max();
        zMax = -std::numeric_limits<float>::max();
        for (int i = 0; i < 8; ++i) {
            const Vector3f corner{(i & 1) ? bounds.max.x : bounds.min.x, (i & 2) ? bounds.max.y : bounds.min.y, (i & 4) ? bounds.max.z : bounds.min.z};
            const float z = view.transformPoint(corner).z;
            zMin = std::min(zMin, z);
            zMax = std::max(zMax, z);
        }
    }

    // Orthographic projection of a light-space square (center, half-size `radius`), deep enough for `scene`.
    k3::ShadowCascade cascade(const k3::Math::Matrix4& view, Vector3f center, float radius, const k3::Math::Bounds3f& scene, std::uint32_t resolution, float splitDistance)
    {
        const float texel = 2.f * radius / static_cast<float>(std::max(resolution, 1u));

        // Moving the square by whole texels only keeps every shadow edge on the same texels.
        center.x = std::floor(center.x / texel) * texel;
        center.y = std::floor(center.y / texel) * texel;

        float zMin, zMax;
        depthRange(scene, view, zMin, zMax);
        const float margin = std::max(0.01f * (zMax - zMin), 1e-3f);
        const auto projection = k3::Math::Matrix4::orthographic(
            center.x - radius, center.x + radius, center.y - radius, center.y + radius,
            -zMax - margin, -zMin + margin
        );

        return {projection * view, texel, splitDistance};
    }

}

std::vector<k3::ShadowCascade> k3::fitShadowCascades(
    const ShadowSettings& settings, const Camera& camera, float aspectRatio,
    const Math::Bounds3f& sceneBounds, const Math::Vector3f& direction
)
{
    constexpr float EVERYWHERE = std::numeric_limits<float>::max();
    const Math::Matrix4 view = lightRotation(direction);
    const int count = std::clamp(settings.cascades, 1, static_cast<int>(MAX_SHADOW_CASCADES));

    // One fixed map: over the given region, or over the whole scene.
    const Math::Bounds3f& fixed = settings.bounds.empty() ? sceneBounds : settings.bounds;
    auto single = [&]() -> std::vector<ShadowCascade> {
        if (fixed.empty())
            return {};
        Math::Bounds3f covered = fixed;
        covered.merge(sceneBounds);
        return {cascade(view, view.transformPoint(fixed.center()), std::max(fixed.radius(), 1e-3f), covered, settings.resolution, EVERYWHERE)};
    };

    if (!settings.bounds.empty() || count == 1 || sceneBounds.empty())
        return single();

    // View range to cover: up to the farthest drawn point (or maxDistance), within the camera's clip range.
    const Math::Vector3f position = camera.position, forward = camera.forward();
    float farthest = 0.f;
    for (int i = 0; i < 8; ++i) {
        const Vector3f corner{(i & 1) ? sceneBounds.max.x : sceneBounds.min.x, (i & 2) ? sceneBounds.max.y : sceneBounds.min.y, (i & 4) ? sceneBounds.max.z : sceneBounds.min.z};
        farthest = std::max(farthest, Vector3f::dot(corner - position, forward));
    }

    // The automatic distance follows the camera: rounded up by steps (1/8 of the power of two below
    // it), otherwise every move would resize the cascades and their texel grid with them.
    if (settings.maxDistance <= 0.f && farthest > 0.f) {
        const float step = std::exp2(std::floor(std::log2(farthest))) / 8.f;
        farthest = std::ceil(farthest / step) * step;
    }

    const float nearDistance = std::max(camera.nearPlane, 1e-4f);
    const float farDistance = std::min(settings.maxDistance > 0.f ? settings.maxDistance : farthest, camera.farPlane);
    if (farDistance <= nearDistance)
        return single();

    // Half-size of the view slice at a distance.
    auto halfHeight = [&](float distance) {
        return camera.projection == Projection::Orthographic ? camera.orthoHeight * 0.5f : distance * std::tan(camera.fovY * 0.5f);
    };

    std::vector<ShadowCascade> cascades;
    float sliceStart = nearDistance;
    for (int i = 1; i <= count; ++i) {
        // "Practical" split scheme: blend of logarithmic and uniform splits.
        const float t = static_cast<float>(i) / static_cast<float>(count);
        const float logarithmic = nearDistance * std::pow(farDistance / nearDistance, t);
        const float uniform = nearDistance + (farDistance - nearDistance) * t;
        const float sliceEnd = settings.splitLambda * logarithmic + (1.f - settings.splitLambda) * uniform;

        // Smallest sphere centered on the view axis holding both ends of the slice.
        const float h0 = halfHeight(sliceStart), h1 = halfHeight(sliceEnd);
        const float r0 = h0 * h0 * (1.f + aspectRatio * aspectRatio);   // Squared corner distances to the axis
        const float r1 = h1 * h1 * (1.f + aspectRatio * aspectRatio);
        const float length = sliceEnd - sliceStart;
        const float offset = std::clamp(0.5f * length + (r1 - r0) / (2.f * length), 0.f, length);
        const float radius = std::sqrt(std::max((length - offset) * (length - offset) + r1, offset * offset + r0));
        const Vector3f center = position + forward * (sliceStart + offset);

        cascades.push_back(cascade(view, view.transformPoint(center), radius, sceneBounds, settings.resolution, i == count ? EVERYWHERE : sliceEnd));
        sliceStart = sliceEnd;
    }
    return cascades;
}
