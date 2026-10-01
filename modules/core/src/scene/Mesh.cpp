#include "Mesh.hpp"

bool k3::Mesh::valid() const noexcept
{
    auto optional = [this](std::size_t size) { return size == 0 || size == positions.size(); };

    if (!optional(normals.size()) || !optional(uvs.size()) || !optional(colors.size()))
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
