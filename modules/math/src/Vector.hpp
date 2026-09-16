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
    concept Numeric = std::is_integral<T>() || std::is_floating_point<T>();

    template<Numeric T>
    struct Vector2
    {
        T x, y;

        inline Vector2(T x, T y) : x(x), y(y) {}
        inline Vector2() : x(static_cast<T>(0)), y(static_cast<T>(0)) {}
    };

    template<Numeric T>
    struct Vector3
    {
        T x, y, z;

        inline Vector3(T x, T y, T z) : x(x), y(y), z(z) {}
        inline Vector3() : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(z)) {}
    };

    template<Numeric T>
    struct Vector4
    {
        T x, y, z, w;

        inline Vector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
        inline Vector4() : x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)), w(static_cast<T>(0)) {}
    };


}
