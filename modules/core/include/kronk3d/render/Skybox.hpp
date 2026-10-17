/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Sky drawn behind the geometry
*/
#pragma once

#include <kronk3d/math/Color.hpp>
#include <kronk3d/scene/Texture.hpp>
#include <array>
#include <cstdint>
#include <memory>

namespace k3
{

    // A background infinitely far away: it follows the camera's orientation but never its position, and
    // it ignores the near and far planes. Six square images of the same size, in the order of OpenGL
    // cube maps: +X, -X, +Y, -Y, +Z, -Z, each one upright as in the usual skybox assets (seen from the
    // inside, y up). Only the first level of the images is used, with the filter of the first face.
    struct Skybox
    {
        std::array<std::shared_ptr<Texture>, 6> faces{};
        Math::Color                             tint = Math::Color::White;      // Multiplies the sky: dim it at night

        // All six faces are set, square and of the same size.
        [[nodiscard]] bool valid() const noexcept;

        // A sky without any image: `horizon` at eye level blending up to `zenith` straight above and down
        // to `ground` straight below. The colors are linear.
        [[nodiscard]] static Skybox gradient(const Math::Color& zenith, const Math::Color& horizon, const Math::Color& ground, std::uint32_t faceSize = 64);
    };

}
