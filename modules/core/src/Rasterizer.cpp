/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Rasterizer class impl
*/

#include "Rasterizer.hpp"
#include "Color.hpp"
#include "Vector.hpp"
#include "utils/Mesh.hpp"
#include "utils/Viewport.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

k3::Rasterizer::Rasterizer(
    std::size_t width,
    std::size_t height
) : m_frameBuffer(width * height), m_depthBuffer(width * height), m_width(width), m_height(height)
{
    // To impl
}

void k3::Rasterizer::draw(
    const k3::Mesh& mesh,
    const k3::Viewport& viewport
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

        // Vector getting
        auto v0 = mesh.vertices[i0];
        auto v1 = mesh.vertices[i1];
        auto v2 = mesh.vertices[i2];

        v0 = viewport.applyTo(v0);
        v1 = viewport.applyTo(v1);
        v2 = viewport.applyTo(v2);

        auto c0 = mesh.colors[i0];
        auto c1 = mesh.colors[i1];
        auto c2 = mesh.colors[i2];

        auto det012 = Math::Vector4f::det(v1 - v0, v2 - v0);
        // To do face culling
        bool isCCW = det012 < 0.f;

        if (isCCW) {
            std::swap(v1, v2);
            det012 = -det012;
        }

        std::int32_t xmin = std::max<std::int32_t>(viewport.xmin, 0);
        std::int32_t xmax = std::min<std::int32_t>(viewport.xmax, m_width) - 1;
        std::int32_t ymin = std::max<std::int32_t>(viewport.ymin, 0);
        std::int32_t ymax = std::min<std::int32_t>(viewport.ymax, m_height) - 1;

        xmin = std::max<float>(std::min({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), xmin);
        xmax = std::min<float>(std::max({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), xmax);
        ymin = std::max<float>(std::min({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), ymin);
        ymax = std::min<float>(std::max({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), ymax);

        for (auto y = ymin; y <= ymax; ++y) {
            for (auto x = xmin; x <= xmax; ++x) {
                Math::Vector4f p{x + 0.5f, y + 0.5f, 0.f, 0.f};

                float det01 = Math::Vector4f::det(v1 - v0, p - v0);
                float det12 = Math::Vector4f::det(v2 - v1, p - v1);
                float det20 = Math::Vector4f::det(v0 - v2, p - v2);

                if (det01 >= 0.f && det12 >= 0.f && det20 >= 0.f) {
                    float l0 = det12 / det012;
                    float l1 = det20 / det012;
                    float l2 = det01 / det012;

                    this->pixel(x, y) = Math::Color(
                        l0 * c0.r + l1 * c1.r + l2 * c2.r,
                        l0 * c0.g + l1 * c1.g + l2 * c2.g,
                        l0 * c0.b + l1 * c1.b + l2 * c2.b,
                        l0 * c0.a + l1 * c1.a + l2 * c2.a
                    );
                }
            }
        }

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

const k3::Math::Color& k3::Rasterizer::pixel(std::size_t x, std::size_t y) const
{
    return this->m_frameBuffer[x + y * m_width];
}

k3::Math::Color& k3::Rasterizer::pixel(std::size_t x, std::size_t y)
{
    return this->m_frameBuffer[x + y * m_width];
}
