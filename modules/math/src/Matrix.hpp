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

		// OpenGL-style orthographic projection: maps the box to NDC, z in [-1, 1].
		static Matrix4 orthographic(float left, float right, float bottom, float top, float near, float far)
		{
			return Matrix4
			{
				2.f / (right - left), 0.f, 0.f, -(right + left) / (right - left),
				0.f, 2.f / (top - bottom), 0.f, -(top + bottom) / (top - bottom),
				0.f, 0.f, -2.f / (far - near), -(far + near) / (far - near),
				0.f, 0.f, 0.f, 1.f,
			};
		}

		// Right-handed view matrix: the camera sits at `eye` and looks down its -Z axis toward `target`.
		static Matrix4 lookAt(const Vector3f& eye, const Vector3f& target, const Vector3f& up)
		{
			const Vector3f f = Vector3f::normalize(target - eye);
			const Vector3f s = Vector3f::normalize(Vector3f::cross(f, up));
			const Vector3f u = Vector3f::cross(s, f);

			return Matrix4
			{
				 s.x,  s.y,  s.z, -Vector3f::dot(s, eye),
				 u.x,  u.y,  u.z, -Vector3f::dot(u, eye),
				-f.x, -f.y, -f.z,  Vector3f::dot(f, eye),
				 0.f,  0.f,  0.f,  1.f,
			};
		}

		// Rotation of `angle` radians around an arbitrary (normalized) axis.
		static Matrix4 rotate(const Vector3f& axis, float angle)
		{
			const Vector3f a = Vector3f::normalize(axis);
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			const float t = 1.f - c;

			return Matrix4
			{
				t * a.x * a.x + c,       t * a.x * a.y - s * a.z, t * a.x * a.z + s * a.y, 0.f,
				t * a.x * a.y + s * a.z, t * a.y * a.y + c,       t * a.y * a.z - s * a.x, 0.f,
				t * a.x * a.z - s * a.y, t * a.y * a.z + s * a.x, t * a.z * a.z + c,       0.f,
				0.f,                     0.f,                     0.f,                     1.f,
			};
		}

		Matrix4 transpose() const
		{
			Matrix4 result{};

			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 4; ++j)
					result.values[4 * j + i] = values[4 * i + j];
			return result;
		}

		// General inverse (cofactor expansion). A singular matrix returns the identity.
		Matrix4 inverse() const
		{
			const float* m = values;
			Matrix4 inv{};
			float* o = inv.values;

			o[0]  =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
			o[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
			o[8]  =  m[4] * m[9]  * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
			o[12] = -m[4] * m[9]  * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
			o[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
			o[5]  =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
			o[9]  = -m[0] * m[9]  * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
			o[13] =  m[0] * m[9]  * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
			o[2]  =  m[1] * m[6]  * m[15] - m[1] * m[7]  * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7]  - m[13] * m[3] * m[6];
			o[6]  = -m[0] * m[6]  * m[15] + m[0] * m[7]  * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7]  + m[12] * m[3] * m[6];
			o[10] =  m[0] * m[5]  * m[15] - m[0] * m[7]  * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7]  - m[12] * m[3] * m[5];
			o[14] = -m[0] * m[5]  * m[14] + m[0] * m[6]  * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6]  + m[12] * m[2] * m[5];
			o[3]  = -m[1] * m[6]  * m[11] + m[1] * m[7]  * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9]  * m[2] * m[7]  + m[9]  * m[3] * m[6];
			o[7]  =  m[0] * m[6]  * m[11] - m[0] * m[7]  * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8]  * m[2] * m[7]  - m[8]  * m[3] * m[6];
			o[11] = -m[0] * m[5]  * m[11] + m[0] * m[7]  * m[9]  + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]  - m[8]  * m[1] * m[7]  + m[8]  * m[3] * m[5];
			o[15] =  m[0] * m[5]  * m[10] - m[0] * m[6]  * m[9]  - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]  + m[8]  * m[1] * m[6]  - m[8]  * m[2] * m[5];

			const float det = m[0] * o[0] + m[1] * o[4] + m[2] * o[8] + m[3] * o[12];

			if (det == 0.f)
				return identity();

			for (float& value : inv.values)
				value /= det;
			return inv;
		}

		// Matrix to transform normals with: inverse-transpose of the upper 3x3 (translation dropped).
		Matrix4 normalMatrix() const
		{
			Matrix4 upper = *this;

			upper.values[3] = upper.values[7] = upper.values[11] = 0.f;
			upper.values[12] = upper.values[13] = upper.values[14] = 0.f;
			upper.values[15] = 1.f;
			return upper.inverse().transpose();
		}

		Vector3f transformPoint(const Vector3f& p) const
		{
			const float* m = values;
			const float w = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
			const Vector3f r{
				m[0] * p.x + m[1] * p.y + m[2]  * p.z + m[3],
				m[4] * p.x + m[5] * p.y + m[6]  * p.z + m[7],
				m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11],
			};
			return (w != 0.f && w != 1.f) ? r / w : r;
		}

		Vector3f transformDirection(const Vector3f& d) const
		{
			const float* m = values;
			return {
				m[0] * d.x + m[1] * d.y + m[2]  * d.z,
				m[4] * d.x + m[5] * d.y + m[6]  * d.z,
				m[8] * d.x + m[9] * d.y + m[10] * d.z,
			};
		}
    };

	inline Vector4f operator*(const Matrix4& m, const Vector4f& v)
	{
		Vector4f result{0.f, 0.f, 0.f, 0.f};

		result.x = m.values[ 0] * v.x + m.values[ 1] * v.y + m.values[ 2] * v.z + m.values[ 3] * v.w;
		result.y = m.values[ 4] * v.x + m.values[ 5] * v.y + m.values[ 6] * v.z + m.values[ 7] * v.w;
		result.z = m.values[ 8] * v.x + m.values[ 9] * v.y + m.values[10] * v.z + m.values[11] * v.w;
		result.w = m.values[12] * v.x + m.values[13] * v.y + m.values[14] * v.z + m.values[15] * v.w;
		return result;
	}

	inline Matrix4 operator*(const Matrix4& m1, const Matrix4& m2)
	{
		Matrix4 result{
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
