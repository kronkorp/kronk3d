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
        for (size_t vertex = 0; vertex + 2 < mesh.count; vertex += 3)
        {
            auto i0 = vertex + 0;
            auto i1 = vertex + 1;
            auto i2 = vertex + 2;

            if (mesh.indices) {
                i0 = mesh.indices[i0];
                i1 = mesh.indices[i1];
                i2 = mesh.indices[i2];
            }

            // -> 2 triangles clipped will produce at most 4 triangles so 3 * 4 vertices
            Vertex clipped_vertices[12];

            clipped_vertices[0].point = transform * mesh.vertices[i0].asPoint();
            clipped_vertices[1].point = transform * mesh.vertices[i1].asPoint();
            clipped_vertices[2].point = transform * mesh.vertices[i2].asPoint();

            clipped_vertices[0].color = mesh.colors[i0];
            clipped_vertices[1].color = mesh.colors[i1];
            clipped_vertices[2].color = mesh.colors[i2];

            auto clipped_end = Vertex::clip(clipped_vertices, &clipped_vertices[3]);

            for (auto begin = clipped_vertices; begin != clipped_end; begin += 3) {

                // -> Vectors representing each points of the vertex
                auto v0 = begin[0];
                auto v1 = begin[1];
                auto v2 = begin[2];

                v0.point = Vector4f::perspective_divide(v0.point);
                v1.point = Vector4f::perspective_divide(v1.point);
                v2.point = Vector4f::perspective_divide(v2.point);
                
                // -> Apply viewport
                v0.point = viewport.applyTo(v0.point);
                v1.point = viewport.applyTo(v1.point);
                v2.point = viewport.applyTo(v2.point);
                
                // -> Colors
                auto c0 = begin[0].color;
                auto c1 = begin[1].color;
                auto c2 = begin[2].color;

                auto det012 = Vector4f::det(v1.point - v0.point, v2.point - v0.point);
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

            xmin = std::max<float>(std::min({std::floor(v0.point.x), std::floor(v1.point.x), std::floor(v2.point.x)}), xmin);
            xmax = std::min<float>(std::max({std::floor(v0.point.x), std::floor(v1.point.x), std::floor(v2.point.x)}), xmax);
            ymin = std::max<float>(std::min({std::floor(v0.point.y), std::floor(v1.point.y), std::floor(v2.point.y)}), ymin);
            ymax = std::min<float>(std::max({std::floor(v0.point.y), std::floor(v1.point.y), std::floor(v2.point.y)}), ymax);

            for (size_t y = ymin; y <= ymax; ++y) {
                for (size_t x = xmin; x <= xmax; ++x) {
                    Vector4f p(x + 0.5f, y + 0.5f, 0.f, 0.f);

                    float det01 = Vector4f::det(v1.point - v0.point, p - v0.point);
                    float det12 = Vector4f::det(v2.point - v1.point, p - v1.point);
                    float det20 = Vector4f::det(v0.point - v2.point, p - v2.point);

                    if (det01 >= 0.f && det12 >= 0.f && det20 >= 0.f) {
                        float l0 = det12 / det012;
                        float l1 = det20 / det012;
                        float l2 = det01 / det012;

                        this->m_at(x, y) = Color(
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

}
