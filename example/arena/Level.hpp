/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** The arena: a stone floor, walls, stairs, crates, torch pillars and a glass panel
*/
#pragma once

#include <kronk3d/Kronk3d.hpp>
#include <filesystem>
#include <memory>
#include <vector>

// An axis-aligned block, used for collisions and for the minimap.
struct Box
{
    k3::Math::Vector3f min, max;
    bool               glass = false;
};

class Level
{
    public:
        static constexpr float HALF_SIZE = 40.f;        // The floor covers [-HALF_SIZE, HALF_SIZE] on x and z

        explicit Level(const std::filesystem::path& assets);

        void draw(k3::IRasterizer& rasterizer) const;

        [[nodiscard]] const std::vector<Box>& solids() const noexcept { return m_solids; }
        [[nodiscard]] const std::vector<k3::Light>& torches() const noexcept { return m_torches; }

    private:
        struct Prop
        {
            std::shared_ptr<k3::Mesh>     mesh;
            std::shared_ptr<k3::Material> material;
        };

        // A box that collides and is drawn with `material`; `tile` is the world size of one texture repeat.
        void addSolid(const Box& box, const std::shared_ptr<k3::Material>& material, float tile = 4.f);
        void addTorch(float x, float z);

        std::vector<Prop>      m_props;
        Prop                   m_tuft;              // One small pyramid, drawn at thousands of places
        std::vector<k3::Math::Matrix4> m_tufts;
        std::vector<Box>       m_solids;
        std::vector<k3::Light> m_torches;
};
