/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Software pipeline internals: vertex / triangle formats shared by its stages
*/
#pragma once

#include "Vector.hpp"
#include "scene/Material.hpp"
#include <cstddef>
#include <cstdint>

namespace k3::sw
{

    // Per-vertex values interpolated across triangles, in this order. Colors and tangents come last:
    // they are only interpolated by the draws that need them.
    enum Varying : int {
        WorldX, WorldY, WorldZ,
        NormalX, NormalY, NormalZ,
        TexU, TexV,
        ColorR, ColorG, ColorB, ColorA,
        TangentX, TangentY, TangentZ, TangentW,
        VARYING_COUNT
    };

    struct ClipVertex
    {
        Math::Vector4f position;            // Clip space
        float          varyings[VARYING_COUNT];
    };

    // Screen positions are snapped to 1/256 pixel: edge functions are then exact integers, which makes
    // rasterization watertight and lets the fill rule settle pixels lying exactly on an edge.
    inline constexpr int          SUBPIXEL_BITS = 8;
    inline constexpr std::int64_t SUBPIXEL_ONE  = std::int64_t{1} << SUBPIXEL_BITS;
    inline constexpr std::int64_t SUBPIXEL_HALF = SUBPIXEL_ONE / 2;

    // Everything a draw's triangles share.
    struct DrawState
    {
        const Material* material;   // Kept alive by the recorded draw command
        bool            alphaTest;
        bool            hasNormals; // Without normals, triangles are flat-shaded with their face normal
        bool            hasColors;  // Without vertex colors, fragments get exactly white (no interpolation rounding)
        bool            normalMapped; // Normal map + normals + uvs + tangents
    };

    // A triangle after clipping, projection and setup, ready to be rasterized.
    struct Triangle
    {
        // E_i(x, y) = edgeA[i] * x + edgeB[i] * y + edgeC[i], with x/y in subpixels. E_i is proportional
        // to the barycentric weight of vertex i and >= 0 inside (fill-rule bias already folded into C).
        std::int64_t  edgeA[3], edgeB[3], edgeC[3];
        float         invArea;                      // 1 / (E_0 + E_1 + E_2)
        float         z[3];                         // Depth in [0, 1]
        float         invW[3];                      // 1 / clip w, for perspective-correct interpolation
        float         varyings[3][VARYING_COUNT];
        // Per-pixel steps (along x, then y) of q = sum(E_i / w_i), q * u and q * v. Since u = (q * u) / q,
        // they give exact screen-space uv derivatives, which select the texture mip levels.
        float         qStep[2], uStep[2], vStep[2];
        std::int32_t  minX, minY, maxX, maxY;       // Pixel bounds, clamped to the target
        std::uint32_t draw;
        bool          frontFacing;

        // Edge functions at the center of pixel (x, y).
        void edgesAt(std::int32_t x, std::int32_t y, std::int64_t e[3]) const noexcept
        {
            const std::int64_t px = x * SUBPIXEL_ONE + SUBPIXEL_HALF;
            const std::int64_t py = y * SUBPIXEL_ONE + SUBPIXEL_HALF;

            for (int i = 0; i < 3; ++i)
                e[i] = edgeA[i] * px + edgeB[i] * py + edgeC[i];
        }
    };

}
