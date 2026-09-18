#include "CubeTextured.hpp"
#include "Vector.hpp"
#include "utils/texture/Texture.hpp"
#include <cstddef>
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

// White so the (future) texture sample isn't tinted; kept only because the
// rasterizer currently indexes mesh.colors unconditionally.
static k3::Math::Color cube_colors[] =
{
    {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
    {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
    {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
    {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
    {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
    {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
};

static size_t cube_indices[] =
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

const k3::Mesh cubeTextured
{
    std::vector<k3::Math::Vector3f>(std::begin(cube_positions), std::end(cube_positions)),
    std::vector<k3::Math::Vector2f>(std::begin(cube_uvs), std::end(cube_uvs)),
    std::vector<k3::Math::Color>(std::begin(cube_colors), std::end(cube_colors)),
    std::vector<std::size_t>(std::begin(cube_indices), std::end(cube_indices)),
    36,
    k3::Texture::fromImage("example/assets/stone.png")
};
