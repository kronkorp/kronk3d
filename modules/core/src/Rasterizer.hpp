/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Rasterizer class def
*/

#pragma once

#include "Color.hpp"
#include "Matrix.hpp"
#include "utils/Mesh.hpp"
#include "utils/Vertex.hpp"
#include "utils/Viewport.hpp"
#include "utils/texture/Texture.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace k3
{

    class Rasterizer
    {
        using FrameBuffer = std::vector<Math::Color>;
        using DepthBuffer = std::vector<float>;

        public:
            Rasterizer(std::size_t width, std::size_t height);
            ~Rasterizer() = default;

            enum class Cull {
                None,
                CW,
                CCW
            };

            void draw(const Mesh& mesh, const Viewport& viewport, const Math::Matrix4& transform = Math::Matrix4::identity(), Cull culling = Cull::CW);

            void clear(const Math::Color& color) noexcept;

            [[nodiscard]] const FrameBuffer& framebuffer(void)                   const noexcept;

        protected:
            [[nodiscard]] const Math::Color& pixel(std::size_t x, std::size_t y) const;
            [[nodiscard]] Math::Color&       pixel(std::size_t x, std::size_t y);

            // Perspective divide, viewport transform, backface culling, then hands off to setup + rasterization.
            void drawSingleTriangle(
                Vertex v0, Vertex v1, Vertex v2,
                const Texture& texture,
                const Viewport& viewport, Cull culling
            );

            // Screen-space AABB of the (already viewport-transformed) triangle, clamped to the viewport/framebuffer.
            void computeBoundingBox(
                const Vertex& v0, const Vertex& v1, const Vertex& v2,
                const Viewport& viewport,
                std::int32_t& xmin, std::int32_t& xmax, std::int32_t& ymin, std::int32_t& ymax
            ) const noexcept;

            // Edge-function scan of the bounding box: barycentric test + color interpolation + pixel write.
            void rasterizeTriangle(
                const Vertex& v0, const Vertex& v1, const Vertex& v2,
                const Texture& texture,
                float det012,
                std::int32_t xmin, std::int32_t xmax, std::int32_t ymin, std::int32_t ymax
            );

        private:
            FrameBuffer m_frameBuffer;
            DepthBuffer m_depthBuffer;
            size_t      m_width, m_height;
    };

}
