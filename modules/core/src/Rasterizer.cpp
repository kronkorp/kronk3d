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
#include "scene/Texture.hpp"
#include "utils/Vertex.hpp"
#include "utils/Viewport.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

static float wrapCoordinate(float t, k3::TextureWrap mode) noexcept
{
    switch (mode) {
        case k3::TextureWrap::Repeat:
            return t - std::floor(t);
        case k3::TextureWrap::MirroredRepeat: {
            float m = t - 2.f * std::floor(t * 0.5f);
            return m > 1.f ? 2.f - m : m;
        }
        case k3::TextureWrap::ClampToEdge:
            break;
    }
    return std::clamp(t, 0.f, 1.f);
}

// Nearest-texel lookup. A missing texture samples as white, so it does not tint what it multiplies.
static k3::Math::Color sample(const k3::Texture* texture, const k3::Math::Vector2f& uv) noexcept
{
    if (!texture || texture->image().empty())
        return k3::Math::Color::White;

    const auto& image = texture->image();
    const float u = wrapCoordinate(uv.x, texture->sampler.wrapU);
    const float v = wrapCoordinate(uv.y, texture->sampler.wrapV);
    const auto x = std::min(static_cast<std::uint32_t>(u * image.width), image.width - 1);
    const auto y = std::min(static_cast<std::uint32_t>(v * image.height), image.height - 1);
    const std::uint8_t* texel = image.texel(x, y);

    return k3::Math::Color::fromRGB(texel[0], texel[1], texel[2], texel[3]);
}

k3::Rasterizer::Rasterizer(
    std::size_t width,
    std::size_t height
) : m_frameBuffer(width * height), m_depthBuffer(width * height, 1.f), m_width(width), m_height(height)
{
}

void k3::Rasterizer::draw(
    const k3::Model& model,
    const k3::Viewport& viewport,
    const k3::Math::Matrix4& transform,
    Cull culling
)
{
    for (const auto& primitive : model.primitives) {
        if (!primitive.mesh)
            continue;

        static const Material defaultMaterial{};
        const Material& material = primitive.material ? *primitive.material : defaultMaterial;

        this->draw(*primitive.mesh, material, viewport, transform, material.doubleSided ? Cull::None : culling);
    }
}

void k3::Rasterizer::draw(
    const k3::Mesh& mesh,
    const k3::Material& material,
    const k3::Viewport& viewport,
    const k3::Math::Matrix4& transform,
    Cull culling
)
{
    const bool hasUVs = mesh.hasUVs();
    const bool hasColors = mesh.hasColors();

    auto fetch = [&](std::uint32_t i) -> Vertex {
        return {
            transform * mesh.positions[i].asPoint(),
            hasUVs ? mesh.uvs[i] : Math::Vector2f{},
            hasColors ? mesh.colors[i] : Math::Color::White,
        };
    };

    for (std::size_t vertex = 0; vertex + 2 < mesh.indexCount(); vertex += 3) {
        const auto i0 = mesh.index(vertex + 0);
        const auto i1 = mesh.index(vertex + 1);
        const auto i2 = mesh.index(vertex + 2);

        if (i0 >= mesh.vertexCount() || i1 >= mesh.vertexCount() || i2 >= mesh.vertexCount())
            continue;

        // Clipping against 2 planes splits a triangle into at most 4 (12 vertices).
        Vertex vertices[12];

        vertices[0] = fetch(i0);
        vertices[1] = fetch(i1);
        vertices[2] = fetch(i2);

        auto clippedEnd = clip(vertices, vertices + 3);

        for (auto begin = vertices; begin != clippedEnd; begin += 3) {
            this->drawSingleTriangle(begin[0], begin[1], begin[2], material, viewport, culling);
        }
    }
}

void k3::Rasterizer::drawSingleTriangle(
    k3::Vertex v0,
    k3::Vertex v1,
    k3::Vertex v2,
    const Material& material,
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
    // Screen space has y pointing down, so a counter-clockwise triangle in NDC has a negative determinant.
    bool isCCW = det012 < 0.f;

    if (det012 == 0.f)
        return;

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

    this->rasterizeTriangle(v0, v1, v2, material, det012, xmin, xmax, ymin, ymax);
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
    const Material& material,
    float det012,
    std::int32_t xmin,
    std::int32_t xmax,
    std::int32_t ymin,
    std::int32_t ymax
)
{
    const bool alphaTest = material.alphaMode == AlphaMode::Mask;

    for (auto y = ymin; y <= ymax; ++y) {
        for (auto x = xmin; x <= xmax; ++x) {
            Math::Vector4f p{x + 0.5f, y + 0.5f, 0.f, 0.f};

            float det01 = Math::Vector4f::det(v1.point - v0.point, p - v0.point);
            float det12 = Math::Vector4f::det(v2.point - v1.point, p - v1.point);
            float det20 = Math::Vector4f::det(v0.point - v2.point, p - v2.point);

            if (det01 < 0.f || det12 < 0.f || det20 < 0.f)
                continue;

            // Screen-space barycentrics: NDC depth is affine in screen space, so it is interpolated with these.
            const float b0 = det12 / det012;
            const float b1 = det20 / det012;
            const float b2 = det01 / det012;

            // Convert from [-1, 1] to [0, 1], 0 being the near plane.
            const float depth = 0.5f + 0.5f * (b0 * v0.point.z + b1 * v1.point.z + b2 * v2.point.z);
            float& oldDepth = m_depthBuffer[y * m_width + x];

            if (depth >= oldDepth)
                continue;

            // Perspective-correct barycentrics for the other attributes (w is still the clip-space w).
            float l0 = b0 / v0.point.w;
            float l1 = b1 / v1.point.w;
            float l2 = b2 / v2.point.w;
            const float lsum = l0 + l1 + l2;

            l0 /= lsum;
            l1 /= lsum;
            l2 /= lsum;

            const Math::Vector2f uv = l0 * v0.uv + l1 * v1.uv + l2 * v2.uv;
            const Math::Color vertexColor = l0 * v0.color + l1 * v1.color + l2 * v2.color;
            Math::Color color = material.diffuse * vertexColor * sample(material.diffuseMap.get(), uv);

            if (material.opacityMap)
                color.a *= sample(material.opacityMap.get(), uv).r;
            if (alphaTest && color.a < material.alphaCutoff)
                continue;

            oldDepth = depth;
            this->pixel(x, y) = color;
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
    std::fill(this->m_frameBuffer.begin(), this->m_frameBuffer.end(), color);
    std::fill(this->m_depthBuffer.begin(), this->m_depthBuffer.end(), 1.f);
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
