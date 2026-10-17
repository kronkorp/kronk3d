/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** glTF 2.0 loader (.gltf + .bin, .glb): meshes, materials, skins and animations
*/
#pragma once

#include <kronk3d/scene/Animation.hpp>
#include <kronk3d/utils/Result.hpp>
#include <cstddef>
#include <filesystem>

namespace k3
{

    struct GltfLoadOptions
    {
        bool loadTextures = true;
    };

    // glTF is what Blender (File > Export > glTF 2.0), Maya, Mixamo and most asset stores export for games.
    //
    // Supported: triangle meshes (POSITION, NORMAL, TANGENT, TEXCOORD_0, COLOR_0, JOINTS_0, WEIGHTS_0),
    // node hierarchies (TRS or matrix), skins, animation clips (translation / rotation / scale channels,
    // STEP / LINEAR / CUBICSPLINE), metallic-roughness materials converted to Blinn-Phong (base color,
    // normal map, alpha modes, double sided, KHR_materials_unlit), textures embedded or external with
    // their samplers. Missing normals are generated, and tangents too when the material has a normal map.
    // Ignored: morph targets, cameras, lights, emissive / occlusion / metallic-roughness textures, texture
    // coordinate sets other than 0, KHR_texture_transform, more than 4 joints per vertex.
    class GltfLoader
    {
        public:
            static k3::Result<AnimatedModel> load(const std::filesystem::path& path, const GltfLoadOptions& options = {});

            // `baseDir` is where external .bin buffers and images are looked up.
            static k3::Result<AnimatedModel> loadFromMemory(const void* data, std::size_t size, const std::filesystem::path& baseDir, const GltfLoadOptions& options = {});
    };

}
