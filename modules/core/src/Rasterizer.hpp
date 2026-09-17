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
#include "utils/Viewport.hpp"
#include <cstddef>
#include <vector>

namespace k3
{

    class Rasterizer
    {
        using FrameBuffer = std::vector<Math::Color>;
        using DepthBuffer = std::vector<float>;

        enum class Cull {
            None,
            CW,
            CCW
        };

        public:
            Rasterizer(std::size_t width, std::size_t height);
            ~Rasterizer() = default;

            void draw(const Mesh& mesh, const Viewport& viewport, const Math::Matrix4& transform = Math::Matrix4::identity(), Cull culling = Cull::CW);

            void clear(const Math::Color& color) noexcept;

            [[nodiscard]] const FrameBuffer& framebuffer(void) const noexcept;
            [[nodiscard]] const Math::Color& pixel(std::size_t x, std::size_t y) const;
            [[nodiscard]] Math::Color&       pixel(std::size_t x, std::size_t y);

        private:
            FrameBuffer m_frameBuffer;
            DepthBuffer m_depthBuffer;
            size_t      m_width, m_height;
    };

}
