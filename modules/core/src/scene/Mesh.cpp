#include "Mesh.hpp"
#include <cmath>

bool k3::Mesh::valid() const noexcept
{
    auto optional = [this](std::size_t size) { return size == 0 || size == positions.size(); };

    if (!optional(normals.size()) || !optional(uvs.size()) || !optional(colors.size()) || !optional(tangents.size()))
        return false;
    if (indexCount() % 3 != 0)
        return false;
    for (auto i : indices)
        if (i >= positions.size())
            return false;
    return true;
}

k3::Math::Bounds3f k3::Mesh::bounds() const noexcept
{
    Math::Bounds3f result;

    for (const auto& p : positions)
        result.expand(p);
    return result;
}

void k3::Mesh::computeNormals()
{
    normals.assign(positions.size(), Math::Vector3f{});

    for (std::size_t t = 0; t + 2 < indexCount(); t += 3) {
        const auto i0 = index(t), i1 = index(t + 1), i2 = index(t + 2);
        // Unnormalized cross product: its length is twice the triangle area, which gives the weighting.
        const auto n = Math::Vector3f::cross(positions[i1] - positions[i0], positions[i2] - positions[i0]);

        normals[i0] += n;
        normals[i1] += n;
        normals[i2] += n;
    }
    for (auto& n : normals)
        n = Math::Vector3f::normalize(n);
}

void k3::Mesh::computeTangents()
{
    tangents.clear();
    if (!hasNormals() || !hasUVs())
        return;

    std::vector<Math::Vector3f> tangentSum(positions.size()), bitangentSum(positions.size());

    for (std::size_t t = 0; t + 2 < indexCount(); t += 3) {
        const auto i0 = index(t), i1 = index(t + 1), i2 = index(t + 2);
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
            continue;

        const auto e1 = positions[i1] - positions[i0];
        const auto e2 = positions[i2] - positions[i0];
        // v grows downward in kronk3d's uvs, upward in normal maps: flip it.
        const float du1 = uvs[i1].x - uvs[i0].x, dv1 = uvs[i0].y - uvs[i1].y;
        const float du2 = uvs[i2].x - uvs[i0].x, dv2 = uvs[i0].y - uvs[i2].y;
        const float det = du1 * dv2 - du2 * dv1;

        if (std::abs(det) < 1e-12f)
            continue;

        const float r = 1.f / det;
        const auto tangent = (e1 * dv2 - e2 * dv1) * r;
        const auto bitangent = (e2 * du1 - e1 * du2) * r;

        for (auto i : {i0, i1, i2}) {
            tangentSum[i] += tangent;
            bitangentSum[i] += bitangent;
        }
    }

    tangents.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const auto n = Math::Vector3f::normalize(normals[i]);
        // Gram-Schmidt: make the tangent perpendicular to the normal.
        auto t = Math::Vector3f::normalize(tangentSum[i] - n * Math::Vector3f::dot(n, tangentSum[i]));

        if (Math::Vector3f::length(t) < 0.5f) {
            // No usable uv gradient (degenerate uvs): any vector perpendicular to the normal.
            const Math::Vector3f axis = std::abs(n.x) < 0.9f ? Math::Vector3f{1.f, 0.f, 0.f} : Math::Vector3f{0.f, 1.f, 0.f};
            t = Math::Vector3f::normalize(Math::Vector3f::cross(axis, n));
        }

        const float w = Math::Vector3f::dot(Math::Vector3f::cross(n, t), bitangentSum[i]) < 0.f ? -1.f : 1.f;
        tangents[i] = {t.x, t.y, t.z, w};
    }
}
