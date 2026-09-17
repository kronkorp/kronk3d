/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Rasterizer class impl
*/

#include "Rasterizer.hpp"
#include "utils/Mesh.hpp"
#include "utils/Viewport.hpp"
#include <algorithm>
#include <cstddef>

k3::Rasterizer::Rasterizer(
    std::size_t width,
    std::size_t height
) : m_frameBuffer(width * height), m_depthBuffer(width * height)
{
    // To impl
}

void k3::Rasterizer::draw(
    [[maybe_unused]] const k3::Mesh& mesh,
    [[maybe_unused]] const k3::Viewport& viewport
)
{
    for (size_t vertex = 0; vertex + 2 < mesh.count; vertex += 3) {
        auto i0 = vertex + 0;
        auto i1 = vertex + 1;
        auto i2 = vertex + 2;

        // If mesh.indices, then there is reusable vertexs
        if (!mesh.indices.empty()) {
            i0 = mesh.indices[i0];
            i1 = mesh.indices[i1];
            i2 = mesh.indices[i2];
        }

        auto v0 ) 
    }
}

[[nodiscard]]
const k3::Rasterizer::FrameBuffer& k3::Rasterizer::framebuffer(void) const noexcept
{
    return m_frameBuffer;
}

void k3::Rasterizer::clear(
    const k3::Math::Color& color
) noexcept
{
    std::fill_n(this->m_frameBuffer.begin(), this->m_frameBuffer.size(), color);
}
