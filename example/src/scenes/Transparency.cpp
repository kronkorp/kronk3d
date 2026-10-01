#include "Transparency.hpp"
#include "Matrix.hpp"
#include "cube/Cube.hpp"
#include "scene/Image.hpp"
#include "scene/Texture.hpp"
#include <cstdint>
#include <memory>

namespace
{

    k3::Mesh transformed(const k3::Mesh& mesh, const k3::Math::Matrix4& transform)
    {
        k3::Mesh result = mesh;

        for (auto& p : result.positions)
            p = transform.transformPoint(p);
        return result;
    }

    // Wooden bars every 32 texels, fully transparent in between.
    std::shared_ptr<k3::Texture> latticeTexture()
    {
        constexpr std::uint32_t SIZE = 128;
        k3::Image image(SIZE, SIZE);

        for (std::uint32_t y = 0; y < SIZE; ++y) {
            for (std::uint32_t x = 0; x < SIZE; ++x) {
                const bool bar = x % 32 < 7 || y % 32 < 7;
                std::uint8_t* texel = image.texel(x, y);

                texel[0] = 150;
                texel[1] = 105;
                texel[2] = 65;
                texel[3] = bar ? 255 : 0;
            }
        }
        return std::make_shared<k3::Texture>(image);
    }

}

k3::Model makeTransparencyScene()
{
    k3::Model model{"transparency", {}};

    // Opaque, vertex-colored core.
    auto core = std::make_shared<k3::Mesh>(transformed(cube, k3::Math::Matrix4::scale(0.45f)));
    model.primitives.push_back({core, std::make_shared<k3::Material>()});

    // Glass box: blended, double-sided so its far faces show through its near ones.
    auto box = std::make_shared<k3::Mesh>(transformed(cube, k3::Math::Matrix4::identity()));
    box->colors.clear();
    auto glass = std::make_shared<k3::Material>();
    glass->name = "glass";
    glass->diffuse = {0.35f, 0.6f, 1.f, 0.3f};
    glass->specular = {0.8f, 0.8f, 0.8f, 1.f};
    glass->shininess = 96.f;
    glass->alphaMode = k3::AlphaMode::Blend;
    glass->doubleSided = true;
    model.primitives.push_back({box, glass});

    // Lattice behind: alpha-tested, so it writes depth and needs no sorting.
    auto lattice = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-2.5f, -1.5f, -1.8f}, {2.5f, -1.5f, -1.8f}, {2.5f, 1.5f, -1.8f}, {-2.5f, 1.5f, -1.8f}},
        .normals = {{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}},
        .uvs = {{0.f, 3.f}, {5.f, 3.f}, {5.f, 0.f}, {0.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });
    auto wood = std::make_shared<k3::Material>();
    wood->name = "lattice";
    wood->diffuseMap = latticeTexture();
    wood->alphaMode = k3::AlphaMode::Mask;
    wood->doubleSided = true;
    model.primitives.push_back({lattice, wood});

    return model;
}
