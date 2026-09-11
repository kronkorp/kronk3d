#pragma once

#include "kronk3d/utils/Vector.hpp"
namespace k3
{

    struct Matrix4
    {
        float values[4 * 4];

        inline static Matrix4 identity()
        {
            return Matrix4{
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f
            };
        }
    };

    inline Vector4f operator * (const Matrix4& m, const Vector4f& v)
    {
        Vector4f res{0, 0, 0, 0};

        res.x = m.values[ 0] * v.x + m.values[ 1] * v.y + m.values[ 2] * v.z + m.values[ 3] * v.w;
        res.y = m.values[ 4] * v.x + m.values[ 5] * v.y + m.values[ 6] * v.z + m.values[ 7] * v.w;
        res.z = m.values[ 8] * v.x + m.values[ 9] * v.y + m.values[10] * v.z + m.values[11] * v.w;
        res.w = m.values[12] * v.x + m.values[13] * v.y + m.values[14] * v.z + m.values[15] * v.w;
        return res;
    }
}
