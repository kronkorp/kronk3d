#pragma once

#include "kronk3d/utils/Vector.hpp"
#include "kronk3d/utils/Color.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace k3
{

    template<typename T>
    struct Box
    {
        void const *ptr = nullptr;
        size_t typeSize = sizeof(T);

        const T& operator[](size_t idx) const {
            return *reinterpret_cast<const T*>(static_cast<const char*>(ptr) + idx * typeSize);
        }
    };

    struct Vertex
    {
        Vector4f point = Vector4f(0, 0, 0, 0);
        Color    color = Color::Black;

        static Vertex* clip(Vertex * begin, Vertex * end)
        {
            static Vector4f const equations[2] =
            {
                {0.f, 0.f,  1.f, 1.f}, // Z > -W  <=>   Z + W > 0
                {0.f, 0.f, -1.f, 1.f}, // Z <  W  <=> - Z + W > 0
            };

            Vertex result[12];

            for (auto equation : equations) {
                auto result_end = result;
                for (Vertex * triangle = begin; triangle != end; triangle += 3)
                    result_end = clip(triangle, equation, result_end);
                end = std::copy(result, result_end, begin);
            }

            return end;
        }

        static Vertex * clip(Vertex* triangle, Vector4f equation, Vertex* result)
        {
            float values[3] =
            {
                Vector4f::dot(triangle[0].point, equation),
                Vector4f::dot(triangle[1].point, equation),
                Vector4f::dot(triangle[2].point, equation),
            };

            std::uint8_t mask = 
                (values[0] < 0.f ? 1 : 0)
                | (values[1] < 0.f ? 2 : 0)
                | (values[2] < 0.f ? 4 : 0);

            switch (mask)
            {
                case 0b110:
                    // Vertices 1 and 2 are outside allowed half-space
                    // Replace them with points on edges 10 and 20
                    *result++ = triangle[0];
                    *result++ = clip_intersect_edge(triangle[1], triangle[0], values[1], values[0]);
                    *result++ = clip_intersect_edge(triangle[2], triangle[0], values[2], values[0]);
                    break;
                case 0b001:
                // Vertex 0 is outside allowed half-space
                // Replace it with points on edges 01 and 02
                // And re-triangulate
                    {
                        auto v01 = clip_intersect_edge(triangle[0], triangle[1], values[0], values[1]);
                        auto v02 = clip_intersect_edge(triangle[0], triangle[2], values[0], values[2]);
                        *result++ = v01;
                        *result++ = triangle[1];
                        *result++ = triangle[2];
                        *result++ = v01;
                        *result++ = triangle[2];
                        *result++ = v02;
                    }
                    break;
            }

            return result;
        }

        static Vertex clip_intersect_edge(const Vertex& v0, const Vertex& v1, float value0, float value1)
        {
            float t = value0 / (value0 - value1);

            Vertex v;
            // v.point = v0.point * (1.f - t) + v1.point * t;
            // v.color = v0.color * (1.f - t) + v1.color * t;
            return v;
        }

    };

    struct Mesh
    {
        Box<Vector3f> vertices     = {};
        Box<Color>    colors       = {};
        size_t*       indices      = nullptr;
        size_t        count        = 0;
    };

}
