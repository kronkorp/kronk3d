/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Wavefront OBJ + MTL loader
*/
#pragma once

#include "scene/Model.hpp"
#include "scene/Texture.hpp"
#include "utils/Result.hpp"
#include <filesystem>
#include <functional>
#include <istream>
#include <memory>
#include <string>

namespace k3
{

    struct ObjLoadOptions
    {
        // OBJ puts v = 0 at the bottom of the image, kronk3d at the top.
        bool flipV = true;
        bool loadTextures = true;

        // Hooks for virtual filesystems / archives / tests. Left empty, the disk is used.
        std::function<std::unique_ptr<std::istream>(const std::filesystem::path&)> openFile{};
        std::function<std::shared_ptr<Texture>(const std::filesystem::path&, ColorSpace)> loadTexture{};
    };

    // Supported:
    //   OBJ: v (+ optional vertex colors "v x y z r g b"), vt, vn, f (any polygon, fan-triangulated,
    //        negative indices, v | v/vt | v//vn | v/vt/vn), usemtl, mtllib, s (flat normals when off)
    //   MTL: newmtl, Kd, Ks, Ke, Ns, d, Tr, illum, map_Kd, map_Ks, map_d, norm, map_Bump / bump (grey-scale
    //        height maps are converted to normal maps; -bm and -clamp honored, other options skipped)
    // One primitive is produced per material. Missing normals are generated, and tangents too when the
    // material has a normal map.
    class ObjLoader
    {
        public:
            static k3::Result<Model> load(const std::filesystem::path& path, const ObjLoadOptions& options = {});

            // `baseDir` is where mtllib files are looked up.
            static k3::Result<Model> loadFromStream(std::istream& obj, const std::filesystem::path& baseDir, const ObjLoadOptions& options = {});
    };

}
