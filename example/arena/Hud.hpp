/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** 2D overlay: counters drawn with a pixel font, and a minimap (drawRect / drawImage only)
*/
#pragma once

#include "Level.hpp"
#include <kronk3d/Kronk3d.hpp>
#include <memory>

class Hud
{
    public:
        Hud();

        // `cameraYaw` is where the camera looks (radians); `fps` and `stats` feed the counters.
        void draw(k3::IRasterizer& rasterizer, const Level& level, const k3::Math::Vector3f& player, float cameraYaw, int fps, const k3::FrameStats& stats) const;

    private:
        void number(k3::IRasterizer& rasterizer, int value, float x, float y, const k3::Math::Color& tint) const;

        std::shared_ptr<k3::Texture> m_digits;      // 10 glyphs of 3x5 texels, side by side with a gap
};
