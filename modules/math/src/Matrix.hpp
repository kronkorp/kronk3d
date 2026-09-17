/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Matrix (4x4) struct for kronK3d
*/
#pragma once

#include "Vector.hpp"
#include <cmath>

namespace k3::Math
{

    struct Matrix4
    {
        float values[4 * 4];

        static Matrix4 identity()
        {
            return Matrix4{
				1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f,
			};
        };

        static Matrix4 scale(const Vector3f& s)
        {
            return Matrix4{
				s.x, 0.f, 0.f, 0.f,
				0.f, s.y, 0.f, 0.f,
				0.f, 0.f, s.z, 0.f,
				0.f, 0.f, 0.f, 1.f,
			};
        }

        static Matrix4 scale(float s)
        {
            return scale(Vector3f{s, s, s});
        }

        static Matrix4 translate(Vector3f const & s)
		{
			return Matrix4{
				1.f, 0.f, 0.f, s.x,
				0.f, 1.f, 0.f, s.y,
				0.f, 0.f, 1.f, s.z,
				0.f, 0.f, 0.f, 1.f,
			};
		}

		static Matrix4 rotateXY(float angle)
		{
			float cos = std::cos(angle);
			float sin = std::sin(angle);

			return Matrix4{
				cos, -sin, 0.f, 0.f,
				sin,  cos, 0.f, 0.f,
				0.f,  0.f, 1.f, 0.f,
				0.f,  0.f, 0.f, 1.f,
			};
		}

		static Matrix4 rotateYZ(float angle)
		{
			float cos = std::cos(angle);
			float sin = std::sin(angle);

			return Matrix4{
				1.f, 0.f,  0.f, 0.f,
				0.f, cos, -sin, 0.f,
				0.f, sin,  cos, 0.f,
				0.f, 0.f,  0.f, 1.f,
			};
		}

		static Matrix4 rotateZX(float angle)
		{
			float cos = std::cos(angle);
			float sin = std::sin(angle);

			return Matrix4{
                cos, 0.f, sin, 0.f,
                0.f, 1.f, 0.f, 0.f,
				-sin, 0.f, cos, 0.f,
                0.f, 0.f, 0.f, 1.f,
			};
		}

		static Matrix4 perspective(float near, float far, float fovY, float aspect_ratio)
		{
			float top = near * std::tan(fovY / 2.f);
			float right = top * aspect_ratio;

			return Matrix4
			{
				near / right, 0.f, 0.f, 0.f,
				0.f, near / top, 0.f, 0.f,
				0.f, 0.f, -(far + near) / (far - near), - 2.f * far * near / (far - near),
				0.f, 0.f, -1.f, 0.f,
			};
		}
    };

}
