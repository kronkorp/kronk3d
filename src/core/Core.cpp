#include <cmath>
#include <cstddef>
#include <cstdint>
#include <kronk3d/core/Core.hpp>
#include <algorithm>
#include <utility>
#include "kronk3d/mesh/Mesh.hpp"
#include "kronk3d/utils/Color.hpp"
#include "kronk3d/utils/Matrix.hpp"
#include "kronk3d/utils/Vector.hpp"
#include "kronk3d/viewport/ViewPort.hpp"

namespace k3
{

    void Rasterizer::clear(const Color& color)
    {
        std::fill_n(this->m_pixels.begin(), this->m_pixels.size(), color);
    }

    void Rasterizer::draw(
        const Mesh& mesh,
        const ViewPort& viewport,
        const Matrix4& transform,
        Cull culling
    )
    {
        for (size_t vertex = 0; vertex + 2 < mesh.vertex_count; vertex += 3)
        {
            // -> Vectors representing each points of the vertex
            auto v0 = transform * mesh.vertices[vertex + 0].asPoint();
            auto v1 = transform * mesh.vertices[vertex + 1].asPoint();
            auto v2 = transform * mesh.vertices[vertex + 2].asPoint();

            // -> Apply viewport
            v0 = viewport.applyTo(v0);
            v1 = viewport.applyTo(v1);
            v2 = viewport.applyTo(v2);

            // -> Colors
            auto c0 = mesh.colors[vertex + 0];
            auto c1 = mesh.colors[vertex + 1];
            auto c2 = mesh.colors[vertex + 2];

            auto det012 = Vector4f::det(v1 - v0, v2 - v0);
            bool isCCW = det012 < 0.f;

            switch (culling) {
                case Cull::None:
                    break;
                case Cull::CW:
                    if (!isCCW) continue;
                    break;
                case Cull::CCW:
                    if (isCCW) continue;
                    break;
            }

            if (isCCW) {
                std::swap(v1, v2);
                det012 = -det012;
            }

            std::int32_t xmin = std::max<std::int32_t>(viewport.xmin, 0);
            std::int32_t xmax = std::min<std::int32_t>(viewport.xmax, m_viewWidth) - 1;
            std::int32_t ymin = std::max<std::int32_t>(viewport.ymin, 0);
            std::int32_t ymax = std::min<std::int32_t>(viewport.ymax, m_viewHeight) - 1;

            xmin = std::max<float>(std::min({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), xmin);
            xmax = std::min<float>(std::max({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), xmax);
            ymin = std::max<float>(std::min({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), ymin);
            ymax = std::min<float>(std::max({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), ymax);

            for (size_t y = ymin; y <= ymax; ++y) {
                for (size_t x = xmin; x <= xmax; ++x) {
                    Vector4f p(x + 0.5f, y + 0.5f, 0.f, 0.f);

                    float det01 = Vector4f::det(v1 - v0, p - v0);
                    float det12 = Vector4f::det(v2 - v1, p - v1);
                    float det20 = Vector4f::det(v0 - v2, p - v2);

                    if (det01 >= 0.f && det12 >= 0.f && det20 >= 0.f) {
                        float l0 = det12 / det012;
                        float l1 = det20 / det012;
                        float l2 = det01 / det012;

                        this->at(x, y) = Color(
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

}
