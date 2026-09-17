/*
** KRONK CORP, 2026
** KRONKMATH
** File description:
** Ftg victor
*/
#pragma once

#include <type_traits>

namespace k3::Math
{

    template<typename T>
    concept Numeric = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

    template<Numeric T>
    struct Vector2
    {
        T x{}, y{};
    };

    template<Numeric T>
    struct Vector4
    {
        T x{}, y{}, z{}, w{};

        inline Vector4<T> operator-(const Vector4<T>& other) const
        {
            return Vector4<T>(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        static inline float det(const Vector4<T>& v0, const Vector4<T>& v1)
        {
            return v0.x * v1.y - v0.y * v1.x;
        }

        static inline Vector4<T> Perspective(Vector4<T> v)
        {
            v.x /= v.w;
            v.y /= v.w;
            v.z /= v.w;
            return v;
        }

    };

    template<Numeric T>
    struct Vector3
    {
        T x{}, y{}, z{};

        inline Vector4<T> asVector() const
        {
            return Vector4<T>(x, y, z, 0.f);
        }

        inline Vector4<T> asPoint() const
        {
            return Vector4<T>(x, y, z, 1.f);
        }
    };

    using Vector2f = Vector2<float>;
    using Vector2i = Vector2<int>;
    using Vector2d = Vector2<double>;
    using Vector2u = Vector2<unsigned int>;

    using Vector3f = Vector3<float>;
    using Vector3i = Vector3<int>;
    using Vector3d = Vector3<double>;
    using Vector3u = Vector3<unsigned int>;

    using Vector4f = Vector4<float>;
    using Vector4i = Vector4<int>;
    using Vector4d = Vector4<double>;
    using Vector4u = Vector4<unsigned int>;

}
