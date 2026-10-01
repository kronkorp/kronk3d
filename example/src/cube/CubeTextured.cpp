#include "CubeTextured.hpp"
#include "Vector.hpp"
#include "scene/Texture.hpp"
#include <cstdint>
#include <memory>
#include <iterator>
#include <vector>

static k3::Math::Vector3f cube_positions[] =
{
    // -X face
    {-1.f, -1.f, -1.f},
    {-1.f,  1.f, -1.f},
    {-1.f, -1.f,  1.f},
    {-1.f,  1.f,  1.f},

    // +X face
    { 1.f, -1.f, -1.f},
    { 1.f,  1.f, -1.f},
    { 1.f, -1.f,  1.f},
    { 1.f,  1.f,  1.f},

    // -Y face
    {-1.f, -1.f, -1.f},
    { 1.f, -1.f, -1.f},
    {-1.f, -1.f,  1.f},
    { 1.f, -1.f,  1.f},

    // +Y face
    {-1.f,  1.f, -1.f},
    { 1.f,  1.f, -1.f},
    {-1.f,  1.f,  1.f},
    { 1.f,  1.f,  1.f},

    // -Z face
    {-1.f, -1.f, -1.f},
    { 1.f, -1.f, -1.f},
    {-1.f,  1.f, -1.f},
    { 1.f,  1.f, -1.f},

    // +Z face
    {-1.f, -1.f,  1.f},
    { 1.f, -1.f,  1.f},
    {-1.f,  1.f,  1.f},
    { 1.f,  1.f,  1.f},
};

// Same (0,0)/(1,0)/(0,1)/(1,1) corner pattern repeated for each face: every
// face samples the whole texture on its own, independently of the others.
static k3::Math::Vector2f cube_uvs[] =
{
    // -X face
    {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f},

    // +X face
    {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f},

    // -Y face
    {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f},

    // +Y face
    {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f},

    // -Z face
    {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f},

    // +Z face
    {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f},
};

static std::uint32_t cube_indices[] =
{
    // -X face
    0,  2,  1,
    1,  2,  3,

    // +X face
    4,  5,  6,
    6,  5,  7,

    // -Y face
    8,  9, 10,
    10,  9, 11,

    // +Y face
    12, 14, 13,
    14, 15, 13,

    // -Z face
    16, 18, 17,
    17, 18, 19,

    // +Z face
    20, 21, 22,
    21, 23, 22,
};

k3::Model makeTexturedCube(const std::filesystem::path& texturePath)
{
    auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = std::vector<k3::Math::Vector3f>(std::begin(cube_positions), std::end(cube_positions)),
        .uvs       = std::vector<k3::Math::Vector2f>(std::begin(cube_uvs), std::end(cube_uvs)),
        .indices   = std::vector<std::uint32_t>(std::begin(cube_indices), std::end(cube_indices)),
    });
    mesh->computeNormals();

    auto material = std::make_shared<k3::Material>();
    material->name = "stone";
    material->diffuseMap = k3::Texture::load(texturePath);

    if (material->diffuseMap) {
        // No normal map ships with the stone texture: derive one from its brightness, the dark mortar
        // between the stones being the low parts.
        k3::Image height = material->diffuseMap->image();
        for (std::size_t i = 0; i < height.pixels.size(); i += 4) {
            const auto luminance = static_cast<std::uint8_t>((height.pixels[i] * 54 + height.pixels[i + 1] * 183 + height.pixels[i + 2] * 19) / 256);
            height.pixels[i] = height.pixels[i + 1] = height.pixels[i + 2] = luminance;
        }
        material->normalMap = k3::Texture::normalMapFromHeight(height, 3.f);
        mesh->computeTangents();
    }

    return k3::Model{"cube", {{mesh, material}}};
}
