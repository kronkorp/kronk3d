/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Anti-aliasing passes of the software rasterizer
*/
#pragma once

#include "Color.hpp"
#include "ThreadPool.hpp"
#include <cstdint>
#include <vector>

namespace k3::sw
{

    // SSAA resolve: each output pixel is the average of a 2x2 block of `source` (linear colors).
    void downsample2x2(const std::vector<Math::Color>& source, std::uint32_t width, std::uint32_t height, std::vector<Math::Color>& destination, ThreadPool& pool);

    // FXAA (Lottes' compact variant): blurs along the local edge direction where luma contrast says
    // there is an edge. The GLSL in render/opengl/Shaders.hpp is the same algorithm, constant for constant.
    void fxaa(const std::vector<Math::Color>& source, std::uint32_t width, std::uint32_t height, std::vector<Math::Color>& destination, ThreadPool& pool);

}
