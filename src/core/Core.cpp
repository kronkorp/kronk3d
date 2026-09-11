#include <cmath>
#include <cstddef>
#include <kronk3d/core/Core.hpp>
#include <algorithm>
#include <utility>
#include "kronk3d/mesh/Mesh.hpp"
#include "kronk3d/utils/Color.hpp"
#include "kronk3d/utils/Vector.hpp"

namespace k3
{

    void Rasterizer::clear(const Color& color)
    {
        std::fill_n(this->m_pixels.begin(), this->m_pixels.size(), color);
    }

    void Rasterizer::draw(const Mesh& mesh, Cull culling)
    {
        for (size_t vertex = 0; vertex + 2 < mesh.vertices.size(); vertex += 3)
        {
            // -> Vectors representing each points of the vertex
            auto v0 = mesh.vertices[vertex + 0].asPoint();
            auto v1 = mesh.vertices[vertex + 1].asPoint();
            auto v2 = mesh.vertices[vertex + 2].asPoint();

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

            auto xmin = std::max<size_t>(std::min({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), 0);
            auto xmax = std::min<size_t>(std::max({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)}), this->m_viewWidth - 1);
            auto ymin = std::max<size_t>(std::min({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), 0);
            auto ymax = std::min<size_t>(std::max({std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)}), this->m_viewHeight - 1);

            for (size_t y = ymin; y <= ymax; ++y) {
                for (size_t x = xmin; x <= xmax; ++x) {
                    Vector4f p(x + 0.5f, y + 0.5f, 0.f, 0.f);

                    float det01 = Vector4f::det(v1 - v0, p - v0);
                    float det12 = Vector4f::det(v2 - v1, p - v1);
                    float det20 = Vector4f::det(v0 - v2, p - v2);

                    if (det01 >= 0.f && det12 >= 0.f && det20 >= 0.f) {
                        this->at(x, y) = mesh.color;
                    }
                }
            }
        }
    }

}
