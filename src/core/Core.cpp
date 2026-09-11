#include <cstddef>
#include <kronk3d/core/Core.hpp>
#include <algorithm>
#include "kronk3d/mesh/Mesh.hpp"
#include "kronk3d/utils/Color.hpp"
#include "kronk3d/utils/Vector.hpp"

namespace k3
{

    void Rasterizer::clear(const Color& color)
    {
        std::fill_n(this->m_pixels.begin(), this->m_pixels.size(), color);
    }

    void Rasterizer::draw(const Mesh& mesh)
    {
        for (size_t vertex = 0; vertex + 2 < mesh.vertices.size(); vertex += 3)
        {
            // -> Vectors representing each points of the vertex
            auto v0 = mesh.vertices[vertex + 0].asPoint();
            auto v1 = mesh.vertices[vertex + 1].asPoint();
            auto v2 = mesh.vertices[vertex + 2].asPoint();

            for (size_t y = 0; y < this->m_viewHeight; ++y) {
                for (size_t x = 0; x < this->m_viewWidth; ++x) {
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
