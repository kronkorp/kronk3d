/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Mesh struct for rasterizer
*/
#pragma once

#include "Bounds.hpp"
#include "Color.hpp"
#include "Vector.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace k3
{

    // Triangle list. Every attribute but positions is optional: leave it empty,
    // or give it exactly one entry per position.
    // Front faces are counter-clockwise (OpenGL / OBJ convention).
    struct Mesh
    {
        std::vector<Math::Vector3f> positions{};
        std::vector<Math::Vector3f> normals{};
        std::vector<Math::Vector2f> uvs{};
        std::vector<Math::Color>    colors{};
        std::vector<std::uint32_t>  indices{};   // Empty: positions are read three by three

        [[nodiscard]] std::size_t vertexCount() const noexcept { return positions.size(); }
        [[nodiscard]] std::size_t indexCount() const noexcept { return indices.empty() ? positions.size() : indices.size(); }
        [[nodiscard]] std::size_t triangleCount() const noexcept { return indexCount() / 3; }

        [[nodiscard]] std::uint32_t index(std::size_t i) const noexcept
        {
            return indices.empty() ? static_cast<std::uint32_t>(i) : indices[i];
        }

        [[nodiscard]] bool hasNormals() const noexcept { return !positions.empty() && normals.size() == positions.size(); }
        [[nodiscard]] bool hasUVs() const noexcept { return !positions.empty() && uvs.size() == positions.size(); }
        [[nodiscard]] bool hasColors() const noexcept { return !positions.empty() && colors.size() == positions.size(); }

        // Attribute sizes are consistent and every index is in range.
        [[nodiscard]] bool valid() const noexcept;

        [[nodiscard]] Math::Bounds3f bounds() const noexcept;

        // Smooth, area-weighted vertex normals (replaces the existing ones).
        void computeNormals();
    };

}
