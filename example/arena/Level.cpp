#include "Level.hpp"
#include <cmath>
#include <iostream>

namespace
{

    using k3::Math::Vector3f;

    // Six faces, counter-clockwise from outside, with texture coordinates in world units so that a block
    // of any size shows the same pattern at the same scale.
    std::shared_ptr<k3::Mesh> makeBox(const Vector3f& a, const Vector3f& b, float tile)
    {
        auto mesh = std::make_shared<k3::Mesh>();
        struct Face
        {
            Vector3f corners[4];
            Vector3f normal;
        };
        const Face faces[6] = {
            {{{a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z}}, {0.f, 1.f, 0.f}},
            {{{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}}, {0.f, -1.f, 0.f}},
            {{{a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}}, {0.f, 0.f, 1.f}},
            {{{b.x, a.y, a.z}, {a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}}, {0.f, 0.f, -1.f}},
            {{{b.x, a.y, b.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}}, {1.f, 0.f, 0.f}},
            {{{a.x, a.y, a.z}, {a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}}, {-1.f, 0.f, 0.f}},
        };

        for (const Face& face : faces) {
            const auto first = static_cast<std::uint32_t>(mesh->positions.size());

            for (const Vector3f& corner : face.corners) {
                mesh->positions.push_back(corner);
                mesh->normals.push_back(face.normal);
                // The two axes of the face (the normal's own axis is left out).
                const float u = face.normal.x != 0.f ? corner.z : corner.x;
                const float v = face.normal.y != 0.f ? corner.z : corner.y;
                mesh->uvs.push_back({u / tile, v / tile});
            }
            mesh->indices.insert(mesh->indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
        }
        mesh->computeTangents();
        return mesh;
    }

    std::shared_ptr<k3::Material> stone(const std::shared_ptr<k3::Texture>& color, const std::shared_ptr<k3::Texture>& normal, const k3::Math::Color& tint)
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = tint;
        material->diffuseMap = color;
        material->normalMap = normal;
        material->normalScale = 0.8f;
        material->specular = {0.12f, 0.12f, 0.12f, 1.f};
        material->shininess = 24.f;
        return material;
    }

    // Small deterministic generator: the arena is the same on every run.
    struct Random
    {
        std::uint32_t state = 1234567u;

        float next()
        {
            state = state * 1664525u + 1013904223u;
            return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
        }
        float range(float lo, float hi) { return lo + (hi - lo) * next(); }
    };

}

Level::Level(const std::filesystem::path& assets)
{
    auto color = k3::Texture::load(assets / "stone.png");

    if (!color) {
        std::cerr << "Cannot load stone.png: using a flat texture" << std::endl;
        color = std::make_shared<k3::Texture>(k3::Image(4, 4, {0.5f, 0.5f, 0.5f, 1.f}));
    }
    // The photo doubles as a height map for the normal map.
    const auto normal = k3::Texture::normalMapFromHeight(color->image(), 3.f);

    const auto floor = stone(color, normal, {0.75f, 0.78f, 0.72f, 1.f});
    const auto wall = stone(color, normal, {0.95f, 0.92f, 0.85f, 1.f});
    const auto crate = stone(color, normal, {0.85f, 0.6f, 0.38f, 1.f});
    const auto pillar = stone(color, normal, {0.65f, 0.68f, 0.78f, 1.f});

    // Floor. It is not a solid: the ground is y = 0.
    {
        Prop prop{makeBox({-HALF_SIZE, -0.5f, -HALF_SIZE}, {HALF_SIZE, 0.f, HALF_SIZE}, 4.f), floor};
        m_props.push_back(prop);
    }

    // Outer walls.
    const float h = HALF_SIZE;
    addSolid({{-h - 1.f, 0.f, -h - 1.f}, {h + 1.f, 5.f, -h}}, wall);
    addSolid({{-h - 1.f, 0.f, h}, {h + 1.f, 5.f, h + 1.f}}, wall);
    addSolid({{-h - 1.f, 0.f, -h}, {-h, 5.f, h}}, wall);
    addSolid({{h, 0.f, -h}, {h + 1.f, 5.f, h}}, wall);

    // Inner walls, with gaps to go through.
    addSolid({{-26.f, 0.f, -14.f}, {-6.f, 3.5f, -13.f}}, wall);
    addSolid({{2.f, 0.f, -14.f}, {18.f, 3.5f, -13.f}}, wall);
    addSolid({{18.f, 0.f, -14.f}, {19.f, 3.5f, 2.f}}, wall);
    addSolid({{-26.f, 0.f, 12.f}, {-25.f, 3.5f, 28.f}}, wall);
    addSolid({{-25.f, 0.f, 20.f}, {-12.f, 3.5f, 21.f}}, wall);

    // A staircase up to a platform: every step needs a jump.
    for (int step = 0; step < 4; ++step) {
        const float top = 0.7f * static_cast<float>(step + 1);
        const float x = 6.f + 2.5f * static_cast<float>(step);

        addSolid({{x, 0.f, 8.f}, {x + 2.5f, top, 11.f}}, pillar, 2.f);
    }
    addSolid({{16.f, 0.f, 8.f}, {22.f, 2.8f, 14.f}}, pillar, 2.f);

    // Torch pillars around the center.
    for (int i = 0; i < 6; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / 6.f + 0.3f;
        const float x = 11.f * std::cos(angle), z = 11.f * std::sin(angle);

        addSolid({{x - 0.6f, 0.f, z - 0.6f}, {x + 0.6f, 3.2f, z + 0.6f}}, pillar, 2.f);
        addTorch(x, z);
    }

    // A glass panel: transparent, lit, no shadow.
    {
        auto glass = std::make_shared<k3::Material>();

        glass->diffuse = {0.35f, 0.75f, 1.f, 0.35f};
        glass->specular = {0.9f, 0.9f, 0.9f, 1.f};
        glass->shininess = 96.f;
        glass->alphaMode = k3::AlphaMode::Blend;
        glass->doubleSided = true;
        glass->castShadows = false;

        const Box box{{-6.f, 0.f, 3.f}, {2.f, 2.8f, 3.2f}, true};

        m_props.push_back({makeBox(box.min, box.max, 4.f), glass});
        m_solids.push_back(box);
    }

    // Scattered crates, more than the camera sees at once: this is what the culling is for.
    Random random;
    for (int i = 0; i < 170; ++i) {
        const float x = random.range(-h + 3.f, h - 3.f), z = random.range(-h + 3.f, h - 3.f);
        const float size = random.range(0.7f, 1.8f), height = random.range(0.7f, 2.f) * (random.next() < 0.2f ? 1.6f : 1.f);
        const Box box{{x - size, 0.f, z - size}, {x + size, height, z + size}};

        // Keep the spawn point and the stairs clear, and do not bury a crate in a wall.
        if (std::abs(x) < 4.f && std::abs(z) < 4.f)
            continue;
        bool overlaps = false;
        for (const Box& other : m_solids) {
            overlaps = overlaps || (box.min.x < other.max.x + 1.5f && box.max.x > other.min.x - 1.5f && box.min.z < other.max.z + 1.5f && box.max.z > other.min.z - 1.5f);
        }
        if (!overlaps)
            addSolid(box, crate, 1.5f);
    }
}

void Level::addSolid(const Box& box, const std::shared_ptr<k3::Material>& material, float tile)
{
    m_props.push_back({makeBox(box.min, box.max, tile), material});
    m_solids.push_back(box);
}

void Level::addTorch(float x, float z)
{
    static const auto flame = [] {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = {1.f, 0.55f, 0.15f, 1.f};
        material->unlit = true;
        material->castShadows = false;
        return material;
    }();

    m_props.push_back({makeBox({x - 0.25f, 3.2f, z - 0.25f}, {x + 0.25f, 3.7f, z + 0.25f}, 1.f), flame});
    m_torches.push_back(k3::Light::point({x, 3.9f, z}, {1.f, 0.55f, 0.2f, 1.f}, 14.f, 16.f));
}

void Level::draw(k3::IRasterizer& rasterizer) const
{
    for (const Prop& prop : m_props)
        rasterizer.draw(prop.mesh, prop.material, k3::Math::Matrix4::identity());
}
