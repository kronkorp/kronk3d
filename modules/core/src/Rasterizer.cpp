/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Rasterizer class impl
*/

#include "Rasterizer.hpp"
#include "Color.hpp"
#include "Matrix.hpp"
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
    const k3::Viewport& viewport,
    const k3::Math::Matrix4& transform,
    Cull culling
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

        const auto v0 = transform * mesh.vertices[i0].asPoint();
        const auto v1 = transform * mesh.vertices[i1].asPoint();
        const auto v2 = transform * mesh.vertices[i2].asPoint();

        // v0/v1/v2 are still in clip space here (pre perspective-divide) — this is where
        // near-plane clipping against `w` will need to hook in, before drawSingleTriangle divides.

        this->drawSingleTriangle(v0, v1, v2, mesh.colors[i0], mesh.colors[i1], mesh.colors[i2], viewport, culling);
    }
}

void k3::Rasterizer::drawSingleTriangle(
    k3::Math::Vector4f v0,
    k3::Math::Vector4f v1,
    k3::Math::Vector4f v2,
    const k3::Math::Color& c0,
    const k3::Math::Color& c1,
    const k3::Math::Color& c2,
    const k3::Viewport& viewport,
    Cull culling
)
{
    v0 = Math::Vector4f::Perspective(v0);
    v1 = Math::Vector4f::Perspective(v1);
    v2 = Math::Vector4f::Perspective(v2);

    v0 = viewport.applyTo(v0);
    v1 = viewport.applyTo(v1);
    v2 = viewport.applyTo(v2);

    auto det012 = Math::Vector4f::det(v1 - v0, v2 - v0);
    bool isCCW = det012 < 0.f;

    switch (culling) {
        case Cull::None:
            break;
        case Cull::CW:
            if (!isCCW) return;
            break;
        case Cull::CCW:
            if (isCCW) return;
            break;
    }

    if (isCCW) {
        std::swap(v1, v2);
        det012 = -det012;
    }

    std::int32_t xmin, xmax, ymin, ymax;

    this->computeBoundingBox(v0, v1, v2, viewport, xmin, xmax, ymin, ymax);

    this->rasterizeTriangle(v0, v1, v2, c0, c1, c2, det012, xmin, xmax, ymin, ymax);
}

void k3::Rasterizer::computeBoundingBox(
    const k3::Math::Vector4f& v0,
    const k3::Math::Vector4f& v1,
    const k3::Math::Vector4f& v2,
    const k3::Viewport& viewport,
    std::int32_t& xmin,
    std::int32_t& xmax,
    std::int32_t& ymin,
    std::int32_t& ymax
) const noexcept
{
    xmin = std::max<std::int32_t>(viewport.xmin, 0);
    xmax = std::min<std::int32_t>(viewport.xmax, m_width) - 1;
    ymin = std::max<std::int32_t>(viewport.ymin, 0);
    ymax = std::min<std::int32_t>(viewport.ymax, m_height) - 1;

    xmin = std::max<float>(std::min({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), xmin);
    xmax = std::min<float>(std::max({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), xmax);
    ymin = std::max<float>(std::min({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), ymin);
    ymax = std::min<float>(std::max({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), ymax);
}

void k3::Rasterizer::rasterizeTriangle(
    const k3::Math::Vector4f& v0,
    const k3::Math::Vector4f& v1,
    const k3::Math::Vector4f& v2,
    const k3::Math::Color& c0,
    const k3::Math::Color& c1,
    const k3::Math::Color& c2,
    float det012,
    std::int32_t xmin,
    std::int32_t xmax,
    std::int32_t ymin,
    std::int32_t ymax
)
{
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

[[nodiscard]]
const k3::Math::Color& k3::Rasterizer::pixel(std::size_t x, std::size_t y) const
{
    return this->m_frameBuffer[x + y * m_width];
}

[[nodiscard]]
k3::Math::Color& k3::Rasterizer::pixel(std::size_t x, std::size_t y)
{
    return this->m_frameBuffer[x + y * m_width];
}
