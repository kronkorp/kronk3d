#pragma once

#include "kronk3d/utils/Vector.hpp"
#include <cmath>
namespace k3
{

    struct Matrix4
	{
		float values[16];

		static Matrix4 identity()
		{
			return Matrix4{
				1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f,
			};
		}

		static Matrix4 scale(Vector3f const & s)
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

	inline Vector4f operator * (Matrix4 const & m, Vector4f const & v)
	{
		Vector4f result{0.f, 0.f, 0.f, 0.f};

		result.x = m.values[ 0] * v.x + m.values[ 1] * v.y + m.values[ 2] * v.z + m.values[ 3] * v.w;
		result.y = m.values[ 4] * v.x + m.values[ 5] * v.y + m.values[ 6] * v.z + m.values[ 7] * v.w;
		result.z = m.values[ 8] * v.x + m.values[ 9] * v.y + m.values[10] * v.z + m.values[11] * v.w;
		result.w = m.values[12] * v.x + m.values[13] * v.y + m.values[14] * v.z + m.values[15] * v.w;

		return result;
	}

	inline Matrix4 operator * (Matrix4 const & m1, Matrix4 const & m2)
	{
		Matrix4 result
		{
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f,
			0.f, 0.f, 0.f, 0.f,
		};

		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				for (int k = 0; k < 4; ++k)
					result.values[4 * i + j] += m1.values[4 * i + k] * m2.values[4 * k + j];

		return result;
	}
}
