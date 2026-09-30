/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Model: a set of meshes, each drawn with its own material
*/
#pragma once

#include "Bounds.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include <memory>
#include <string>
#include <vector>

namespace k3
{

    struct Primitive
    {
        std::shared_ptr<Mesh>     mesh{};
        std::shared_ptr<Material> material{};
    };

    struct Model
    {
        std::string            name{};
        std::vector<Primitive> primitives{};

        [[nodiscard]] Math::Bounds3f bounds() const noexcept
        {
            Math::Bounds3f result;

            for (const auto& primitive : primitives)
                if (primitive.mesh)
                    result.merge(primitive.mesh->bounds());
            return result;
        }
    };

}
