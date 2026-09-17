#include "Vertex.hpp"

namespace k3
{

    Vertex clipIntersected(
        const Vertex& v0,
        const Vertex& v1,
        float value0,
        float value1
    )
    {
        float t = value0 / (value0 - value1);

        Vertex v;
        v.point = (1.f - t) * v0.point + t * v1.point;
		v.uv    = (1.f - t) * v0.uv    + t * v1.uv;
		v.color = (1.f - t) * v0.color + t * v1.color;

        return v;
    }

    Vertex* clip(Vertex * triangle, Math::Vector4f equation, Vertex * result)
    {
        float values[3] =
        {
            Math::Vector4f::dot(triangle[0].point, equation),
            Math::Vector4f::dot(triangle[1].point, equation),
            Math::Vector4f::dot(triangle[2].point, equation),
        };

        std::uint8_t mask = 
            (values[0] < 0.f ? 1 : 0)
            | (values[1] < 0.f ? 2 : 0)
            | (values[2] < 0.f ? 4 : 0);

        switch (mask)
        {
            case 0b000:
				// All vertices are inside allowed half-space
				// No clipping required, copy the triangle to output
				*result++ = triangle[0];
				*result++ = triangle[1];
				*result++ = triangle[2];
				break;
			case 0b001:
				// Vertex 0 is outside allowed half-space
				// Replace it with points on edges 01 and 02
				// And re-triangulate
				{
					auto v01 = clipIntersected(triangle[0], triangle[1], values[0], values[1]);
					auto v02 = clipIntersected(triangle[0], triangle[2], values[0], values[2]);
					*result++ = v01;
					*result++ = triangle[1];
					*result++ = triangle[2];
					*result++ = v01;
					*result++ = triangle[2];
					*result++ = v02;
				}
				break;
			case 0b010:
				// Vertex 1 is outside allowed half-space
				// Replace it with points on edges 10 and 12
				// And re-triangulate
				{
					auto v10 = clipIntersected(triangle[1], triangle[0], values[1], values[0]);
					auto v12 = clipIntersected(triangle[1], triangle[2], values[1], values[2]);
					*result++ = triangle[0];
					*result++ = v10;
					*result++ = triangle[2];
					*result++ = triangle[2];
					*result++ = v10;
					*result++ = v12;
				}
				break;
			case 0b011:
				// Vertices 0 and 1 are outside allowed half-space
				// Replace them with points on edges 02 and 12
				*result++ = clipIntersected(triangle[0], triangle[2], values[0], values[2]);
				*result++ = clipIntersected(triangle[1], triangle[2], values[1], values[2]);
				*result++ = triangle[2];
				break;
			case 0b100:
				// Vertex 2 is outside allowed half-space
				// Replace it with points on edges 20 and 21
				// And re-triangulate
				{
					auto v20 = clipIntersected(triangle[2], triangle[0], values[2], values[0]);
					auto v21 = clipIntersected(triangle[2], triangle[1], values[2], values[1]);
					*result++ = triangle[0];
					*result++ = triangle[1];
					*result++ = v20;
					*result++ = v20;
					*result++ = triangle[1];
					*result++ = v21;
				}
				break;
			case 0b101:
				// Vertices 0 and 2 are outside allowed half-space
				// Replace them with points on edges 01 and 21
				*result++ = clipIntersected(triangle[0], triangle[1], values[0], values[1]);
				*result++ = triangle[1];
				*result++ = clipIntersected(triangle[2], triangle[1], values[2], values[1]);
				break;
			case 0b110:
				// Vertices 1 and 2 are outside allowed half-space
				// Replace them with points on edges 10 and 20
				*result++ = triangle[0];
				*result++ = clipIntersected(triangle[1], triangle[0], values[1], values[0]);
				*result++ = clipIntersected(triangle[2], triangle[0], values[2], values[0]);
				break;
				break;
			case 0b111:
				// All vertices are outside allowed half-space
				// Clip the whole triangle, result is empty
				break;
        }

        return result;
    }

    Vertex* clip(
        Vertex* begin,
        Vertex* end
    )
    {
        static Math::Vector4f const equations[2] =
        {
            {0.f, 0.f,  1.f, 1.f}, // Z > -W  <=>   Z + W > 0
            {0.f, 0.f, -1.f, 1.f}, // Z <  W  <=> - Z + W > 0
        };

        Vertex result[12];

        for (auto equation : equations)
        {
            auto result_end = result;

            for (Vertex* triangle = begin; triangle != end; triangle += 3)
                result_end = clip(triangle, equation, result_end);
            end = std::copy(result, result_end, begin);
        }
        return end;
    }

}
