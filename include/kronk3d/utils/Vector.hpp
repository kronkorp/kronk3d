#pragma once

namespace k3
{

    template<typename T>
    struct Vector2
    {
        T x, y;

        inline Vector2(T x, T y) : x(x), y(y) {}
    };

    // NOTE: Vector2's
    using Vector2f = Vector2<float>;
    using Vector2u = Vector2<unsigned int>;
    using Vector2i = Vector2<int>;
    using Vector2d = Vector2<double>;


    template<typename T>
    struct Vector4
    {
        T x, y, z, w;

        inline Vector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

        inline Vector4<T> operator-(const Vector4<T>& other)
        {
            return Vector4<T>(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        inline float det(const Vector4<T>& other)
        {
            return x * other.y - y * other.x;
        }
    };

    using Vector4f = Vector4<float>;
    using Vector4u = Vector4<unsigned int>;
    using Vector4i = Vector4<int>;
    using Vector4d = Vector4<double>;


    template<typename T>
    struct Vector3
    {
        T x, y, z;

        inline Vector3(T x, T y, T z) : x(x), y(y), z(z) {}

        inline Vector4<T> asVector()
        {
            return Vector4<T>(x, y, z, 0.f);
        }

        inline Vector4<T> asPoint()
        {
            return Vector4<T>(x, y, z, 1.f);
        }
    };

    // NOTE: Vector3's
    using Vector3f = Vector3<float>;
    using Vector3u = Vector3<unsigned int>;
    using Vector3i = Vector3<int>;
    using Vector3d = Vector3<double>;

}
