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
#include "utils/Vertex.hpp"
#include "utils/Viewport.hpp"
#include "utils/texture/Texture.hpp"
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

        // NOTE: Only with 2 plan , change to 24 after for 6 plans.
        Vertex vertices[12];

        vertices[0] = {transform * mesh.vertices[i0].asPoint(), mesh.uvs[i0], mesh.colors[i0]};
        vertices[1] = {transform * mesh.vertices[i1].asPoint(), mesh.uvs[i1], mesh.colors[i1]};
        vertices[2] = {transform * mesh.vertices[i2].asPoint(), mesh.uvs[i2], mesh.colors[i2]};

        auto clippedEnd = clip(vertices, vertices + 3);

        for (auto begin = vertices; begin != clippedEnd; begin += 3) {
            auto v0 = begin[0];
            auto v1 = begin[1];
            auto v2 = begin[2];

            this->drawSingleTriangle(v0, v1, v2, mesh.texture, viewport, culling);
        }
    }
}

void k3::Rasterizer::drawSingleTriangle(
    k3::Vertex v0,
    k3::Vertex v1,
    k3::Vertex v2,
    const Texture& texture,
    const k3::Viewport& viewport,
    Cull culling
)
{
    v0.point = Math::Vector4f::Perspective(v0.point);
    v1.point = Math::Vector4f::Perspective(v1.point);
    v2.point = Math::Vector4f::Perspective(v2.point);

    v0.point = viewport.applyTo(v0.point);
    v1.point = viewport.applyTo(v1.point);
    v2.point = viewport.applyTo(v2.point);

    auto det012 = Math::Vector4f::det(v1.point - v0.point, v2.point - v0.point);
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

    this->rasterizeTriangle(v0, v1, v2, texture, det012, xmin, xmax, ymin, ymax);
}

void k3::Rasterizer::computeBoundingBox(
    const k3::Vertex& v0,
    const k3::Vertex& v1,
    const k3::Vertex& v2,
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

    xmin = std::max<float>(std::min({std::floor(v0.point.x), std::floor(v1.point.x), std::floor(v2.point.x)}), xmin);
    xmax = std::min<float>(std::max({std::floor(v0.point.x), std::floor(v1.point.x), std::floor(v2.point.x)}), xmax);
    ymin = std::max<float>(std::min({std::floor(v0.point.y), std::floor(v1.point.y), std::floor(v2.point.y)}), ymin);
    ymax = std::min<float>(std::max({std::floor(v0.point.y), std::floor(v1.point.y), std::floor(v2.point.y)}), ymax);
}

void k3::Rasterizer::rasterizeTriangle(
    const k3::Vertex& v0,
    const k3::Vertex& v1,
    const k3::Vertex& v2,
    const Texture& texture,
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

            float det01 = Math::Vector4f::det(v1.point - v0.point, p - v0.point);
            float det12 = Math::Vector4f::det(v2.point - v1.point, p - v1.point);
            float det20 = Math::Vector4f::det(v0.point - v2.point, p - v2.point);

            if (det01 >= 0.f && det12 >= 0.f && det20 >= 0.f) {
                float l0 = det12 / det012;
                float l1 = det20 / det012;
                float l2 = det01 / det012;

                auto c0 = v2.color;
                auto c1 = v1.color;
                auto c2 = v2.color;

                k3::Math::Vector2f uv = {l0 * v0.uv + l1 * v1.uv + l2 * v2.uv};
                auto& color = texture.at(
                    static_cast<size_t>(uv.x * texture.width),
                    static_cast<size_t>(uv.y * texture.height)
                );

                // this->pixel(x, y) = Math::Color(
                //     l0 * c0.r + l1 * c1.r + l2 * c2.r,
                //     l0 * c0.g + l1 * c1.g + l2 * c2.g,
                //     l0 * c0.b + l1 * c1.b + l2 * c2.b,
                //     l0 * c0.a + l1 * c1.a + l2 * c2.a
                // );

                this->pixel(x, y) = color;;
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
