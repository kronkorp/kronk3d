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
    struct Vector3
    {
        T x{}, y{}, z{};
    };

    template<Numeric T>
    struct Vector4
    {
        T x{}, y{}, z{}, w{};
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
