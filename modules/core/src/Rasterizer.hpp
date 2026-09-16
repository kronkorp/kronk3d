/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Rasterizer class def
*/

#pragma once

#include "Vector.hpp"
#include "utils/Mesh.hpp"
#include "utils/Viewport.hpp"
#include <cstddef>
#include <vector>

namespace k3
{

    class Rasterizer
    {
        using FrameBuffer = std::vector<Math::Vector4f>;
        using DepthBuffer = std::vector<float>;

        public:
            Rasterizer(std::size_t width, std::size_t height);
            ~Rasterizer() = default;

            void draw(const Mesh& mesh, const Viewport& viewport);

            void clear(const Math::Vector4f& color) noexcept;

            [[nodiscard]] const FrameBuffer& framebuffer(void) const noexcept;

        private:
            FrameBuffer m_frameBuffer;
            DepthBuffer m_depthBuffer;
    };

}
