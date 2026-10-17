#include "Animation.hpp"
#include <algorithm>
#include <cmath>

namespace
{

    using k3::Math::Vector3f;
    using k3::Math::Vector4f;

    Vector4f normalized(const Vector4f& q)
    {
        const float length = std::sqrt(Vector4f::dot(q, q));

        return length > 0.f ? (1.f / length) * q : Vector4f{0.f, 0.f, 0.f, 1.f};
    }

    Vector4f slerp(const Vector4f& a, Vector4f b, float t)
    {
        float cosine = Vector4f::dot(a, b);

        if (cosine < 0.f) {     // q and -q are the same rotation: take the short way round
            b = -1.f * b;
            cosine = -cosine;
        }
        if (cosine > 0.9995f)   // Nearly parallel: sin(angle) would be ~0
            return normalized((1.f - t) * a + t * b);

        const float angle = std::acos(cosine);
        const float sine = std::sin(angle);

        return (std::sin((1.f - t) * angle) / sine) * a + (std::sin(t * angle) / sine) * b;
    }

    // Value of the key `index` (cubic splines store in-tangent, value, out-tangent for each key).
    const Vector4f& keyValue(const k3::AnimationChannel& channel, std::size_t index)
    {
        return channel.values[channel.interpolation == k3::AnimationChannel::Interpolation::CubicSpline ? 3 * index + 1 : index];
    }

    Vector4f evaluate(const k3::AnimationChannel& channel, float time)
    {
        const auto& times = channel.times;
        const auto after = static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), time) - times.begin());

        if (after == 0)
            return keyValue(channel, 0);
        if (after == times.size())
            return keyValue(channel, times.size() - 1);

        const std::size_t before = after - 1;
        const float span = times[after] - times[before];
        const float t = span > 0.f ? (time - times[before]) / span : 0.f;
        const Vector4f& a = keyValue(channel, before);
        const Vector4f& b = keyValue(channel, after);
        const bool rotation = channel.path == k3::AnimationChannel::Path::Rotation;

        switch (channel.interpolation) {
            case k3::AnimationChannel::Interpolation::Step:
                return a;
            case k3::AnimationChannel::Interpolation::Linear:
                return rotation ? slerp(a, b, t) : (1.f - t) * a + t * b;
            case k3::AnimationChannel::Interpolation::CubicSpline: {
                const Vector4f& outTangent = channel.values[3 * before + 2];
                const Vector4f& inTangent = channel.values[3 * after];
                const float t2 = t * t, t3 = t2 * t;
                const Vector4f result = (2.f * t3 - 3.f * t2 + 1.f) * a + (span * (t3 - 2.f * t2 + t)) * outTangent
                                      + (-2.f * t3 + 3.f * t2) * b + (span * (t3 - t2)) * inTangent;

                return rotation ? normalized(result) : result;
            }
        }
        return a;
    }

}

k3::Math::Matrix4 k3::Transform::matrix() const
{
    const float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;

    return Math::Matrix4{
        (1.f - 2.f * (y * y + z * z)) * scale.x, 2.f * (x * y - z * w) * scale.y,         2.f * (x * z + y * w) * scale.z,         translation.x,
        2.f * (x * y + z * w) * scale.x,         (1.f - 2.f * (x * x + z * z)) * scale.y, 2.f * (y * z - x * w) * scale.z,         translation.y,
        2.f * (x * z - y * w) * scale.x,         2.f * (y * z + x * w) * scale.y,         (1.f - 2.f * (x * x + y * y)) * scale.z, translation.z,
        0.f,                                     0.f,                                     0.f,                                     1.f,
    };
}

k3::Transform k3::Transform::fromMatrix(const Math::Matrix4& matrix)
{
    const float* m = matrix.values;
    Transform result;

    result.translation = {m[3], m[7], m[11]};
    result.scale = {
        std::sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]),
        std::sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]),
        std::sqrt(m[2] * m[2] + m[6] * m[6] + m[10] * m[10]),
    };

    const float determinant = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) + m[2] * (m[4] * m[9] - m[5] * m[8]);

    if (determinant < 0.f)      // A mirroring: carry it on one axis
        result.scale.x = -result.scale.x;
    if (result.scale.x == 0.f || result.scale.y == 0.f || result.scale.z == 0.f)
        return result;

    // Rotation part, columns normalized.
    const float r00 = m[0] / result.scale.x, r10 = m[4] / result.scale.x, r20 = m[8] / result.scale.x;
    const float r01 = m[1] / result.scale.y, r11 = m[5] / result.scale.y, r21 = m[9] / result.scale.y;
    const float r02 = m[2] / result.scale.z, r12 = m[6] / result.scale.z, r22 = m[10] / result.scale.z;
    const float trace = r00 + r11 + r22;

    if (trace > 0.f) {
        const float s = std::sqrt(trace + 1.f) * 2.f;
        result.rotation = {(r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s, 0.25f * s};
    } else if (r00 > r11 && r00 > r22) {
        const float s = std::sqrt(1.f + r00 - r11 - r22) * 2.f;
        result.rotation = {0.25f * s, (r01 + r10) / s, (r02 + r20) / s, (r21 - r12) / s};
    } else if (r11 > r22) {
        const float s = std::sqrt(1.f + r11 - r00 - r22) * 2.f;
        result.rotation = {(r01 + r10) / s, 0.25f * s, (r12 + r21) / s, (r02 - r20) / s};
    } else {
        const float s = std::sqrt(1.f + r22 - r00 - r11) * 2.f;
        result.rotation = {(r02 + r20) / s, (r12 + r21) / s, 0.25f * s, (r10 - r01) / s};
    }
    result.rotation = normalized(result.rotation);
    return result;
}

k3::Transform k3::Transform::mix(const Transform& a, const Transform& b, float t)
{
    return {
        (1.f - t) * a.translation + t * b.translation,
        slerp(a.rotation, b.rotation, t),
        (1.f - t) * a.scale + t * b.scale,
    };
}

void k3::Animation::sample(float time, std::vector<Transform>& pose) const
{
    time = std::clamp(time, 0.f, duration);
    for (const auto& channel : channels) {
        if (channel.times.empty() || channel.node < 0 || static_cast<std::size_t>(channel.node) >= pose.size())
            continue;

        const Vector4f value = evaluate(channel, time);
        Transform& target = pose[static_cast<std::size_t>(channel.node)];

        switch (channel.path) {
            case AnimationChannel::Path::Translation:
                target.translation = {value.x, value.y, value.z};
                break;
            case AnimationChannel::Path::Rotation:
                target.rotation = value;
                break;
            case AnimationChannel::Path::Scale:
                target.scale = {value.x, value.y, value.z};
                break;
        }
    }
}

int k3::AnimatedModel::findAnimation(std::string_view clip) const noexcept
{
    for (std::size_t i = 0; i < animations.size(); ++i)
        if (animations[i].name == clip)
            return static_cast<int>(i);
    return -1;
}

std::vector<k3::Math::Matrix4> k3::AnimatedModel::globalTransforms(const std::vector<Transform>& pose) const
{
    std::vector<Math::Matrix4> globals(nodes.size());

    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const Math::Matrix4 local = pose[i].matrix();

        globals[i] = nodes[i].parent < 0 ? local : globals[static_cast<std::size_t>(nodes[i].parent)] * local;
    }
    return globals;
}

k3::Math::Bounds3f k3::AnimatedModel::bounds() const
{
    std::vector<Transform> rest(nodes.size());
    Math::Bounds3f result;

    for (std::size_t i = 0; i < nodes.size(); ++i)
        rest[i] = nodes[i].local;

    const auto globals = globalTransforms(rest);

    for (const auto& primitive : primitives) {
        if (!primitive.mesh)
            continue;
        // A skinned mesh is stored in the space its joints put it in at bind time.
        const auto box = primitive.mesh->bounds();

        result.merge(primitive.skin >= 0 ? box : box.transformed(globals[static_cast<std::size_t>(primitive.node)]));
    }
    return result;
}
