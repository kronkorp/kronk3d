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
    concept Numeric = std::is_arithmetic_v<T>() && !std::is_same_v<T, bool>;

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

}
