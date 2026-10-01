/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Blinn-Phong material (maps 1:1 to Wavefront MTL)
*/
#pragma once

#include "Color.hpp"
#include "Texture.hpp"
#include <memory>
#include <string>

namespace k3
{

    enum class AlphaMode {
        Opaque,   // Alpha ignored
        Mask,     // Fragments with alpha < alphaCutoff are discarded (foliage, fences)
        Blend     // Alpha blended, drawn after opaque geometry (glass, smoke)
    };

    struct Material
    {
        std::string name{};

        Math::Color diffuse  = Math::Color::White;          // Kd, alpha = d
        Math::Color specular = {0.f, 0.f, 0.f, 1.f};        // Ks
        Math::Color emissive = {0.f, 0.f, 0.f, 1.f};        // Ke
        float       shininess = 32.f;                       // Ns

        std::shared_ptr<Texture> diffuseMap{};              // map_Kd, multiplies diffuse
        std::shared_ptr<Texture> specularMap{};             // map_Ks, multiplies specular
        std::shared_ptr<Texture> opacityMap{};              // map_d, red channel multiplies alpha
        // Tangent-space normal map (norm, or map_Bump height maps converted on load), linear color space,
        // OpenGL convention: green points toward the top of the image. Needs mesh tangents.
        std::shared_ptr<Texture> normalMap{};
        float                    normalScale = 1.f;         // Bumpiness: scales the map's x/y

        AlphaMode alphaMode   = AlphaMode::Opaque;
        float     alphaCutoff = 0.5f;
        bool      doubleSided = false;
        bool      unlit       = false;                      // Output diffuse * textures * vertex colors, no lighting
        bool      castShadows = true;
    };

}
