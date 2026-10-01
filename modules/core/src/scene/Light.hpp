/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Punctual lights
*/
#pragma once

#include "Color.hpp"
#include "Vector.hpp"

namespace k3
{

    enum class LightType {
        Directional,    // Sun: parallel rays along `direction`
        Point,          // Bulb: radiates from `position`, fades out at `range`
        Spot            // Point light restricted to a cone around `direction`
    };

    // Colors are linear. Point and spot lights use inverse-square falloff windowed to reach 0 at `range`.
    struct Light
    {
        LightType      type      = LightType::Directional;
        Math::Vector3f position  = {0.f, 0.f, 0.f};
        Math::Vector3f direction = {0.f, -1.f, 0.f};        // Direction the light travels (normalized by the renderer)
        Math::Color    color     = Math::Color::White;
        float          intensity = 1.f;
        float          range     = 10.f;
        float          innerCone = 0.35f;                   // Spot: half-angle (radians) of full intensity
        float          outerCone = 0.5f;                    // Spot: half-angle (radians) where it reaches 0
        bool           castShadows = false;                 // Directional lights only

        static Light directional(const Math::Vector3f& direction, const Math::Color& color = Math::Color::White, float intensity = 1.f)
        {
            Light light;
            light.type = LightType::Directional;
            light.direction = direction;
            light.color = color;
            light.intensity = intensity;
            return light;
        }

        static Light point(const Math::Vector3f& position, const Math::Color& color = Math::Color::White, float intensity = 1.f, float range = 10.f)
        {
            Light light;
            light.type = LightType::Point;
            light.position = position;
            light.color = color;
            light.intensity = intensity;
            light.range = range;
            return light;
        }

        static Light spot(
            const Math::Vector3f& position, const Math::Vector3f& direction,
            const Math::Color& color = Math::Color::White, float intensity = 1.f, float range = 10.f,
            float innerCone = 0.35f, float outerCone = 0.5f
        )
        {
            Light light = point(position, color, intensity, range);
            light.type = LightType::Spot;
            light.direction = direction;
            light.innerCone = innerCone;
            light.outerCone = outerCone;
            return light;
        }
    };

}
