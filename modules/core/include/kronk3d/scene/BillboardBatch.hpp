/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Camera-facing quads drawn in one go: particles, sparks, smoke, flames
*/
#pragma once

#include <kronk3d/math/Color.hpp>
#include <kronk3d/math/Vector.hpp>
#include <kronk3d/render/IRasterizer.hpp>
#include <kronk3d/scene/Camera.hpp>
#include <kronk3d/scene/Material.hpp>
#include <kronk3d/scene/Mesh.hpp>
#include <memory>
#include <vector>

namespace k3
{

    // One quad that always faces the camera.
    struct Billboard
    {
        Math::Vector3f position{};
        Math::Vector2f size{1.f, 1.f};              // World units
        Math::Color    color = Math::Color::White;  // Multiplies the material's texture; its alpha fades the quad
        float          rotation = 0.f;              // Radians around the view axis
        // The part of the material's texture to show (u0, v0, u1, v1; v = 0 is the top): a cell of a sprite sheet.
        Math::Vector4f uv{0.f, 0.f, 1.f, 1.f};
    };

    // The billboards of a frame, drawn with a single draw call. The simulation is yours: fill the batch each
    // frame (clear, add...), then draw it.
    //
    //     batch.clear();
    //     for (const Spark& spark : sparks)
    //         batch.add({.position = spark.position, .size = {0.2f, 0.2f}, .color = spark.color});
    //     batch.draw(rasterizer, camera, sparkMaterial);
    //
    // The quads are sorted back to front, for blended materials. The mesh behind the batch is kept from one
    // frame to the next, so the GPU backends reuse their buffers. For particles, use an unlit material
    // (or a lit one, which sees the quads facing the camera) and set `castShadows` to false.
    class BillboardBatch
    {
        public:
            void clear() noexcept { m_billboards.clear(); }
            void add(const Billboard& billboard) { m_billboards.push_back(billboard); }
            void reserve(std::size_t count) { m_billboards.reserve(count); }

            [[nodiscard]] std::size_t size() const noexcept { return m_billboards.size(); }

            // Builds the quads for `camera` and draws them in the current layer (nothing when the batch is empty).
            // The batch keeps its billboards until clear(): call draw() once per frame.
            void draw(IRasterizer& rasterizer, const Camera& camera, std::shared_ptr<const Material> material);

        private:
            std::vector<Billboard> m_billboards{};
            std::shared_ptr<Mesh>  m_mesh = std::make_shared<Mesh>();
    };

}
