#pragma once

namespace k3
{

    template<typename T>
    struct Vector2
    {
        T x, y;

        inline Vector2(T x, T y) : x(x), y(y) {}
    };

    template<typename T>
    struct Vector3
    {
        T x, y, z;

        inline Vector3(T x, T y, T z) : x(x), y(y), z(z) {}
    };

    // NOTE: Vector2's
    using Vector2f = Vector2<float>;
    using Vector2u = Vector2<unsigned int>;
    using Vector2i = Vector2<int>;
    using Vector2d = Vector2<double>;

    // NOTE: Vector3's
    using Vector3f = Vector3<float>;
    using Vector3u = Vector3<unsigned int>;
    using Vector3i = Vector3<int>;
    using Vector3d = Vector3<double>;

}
