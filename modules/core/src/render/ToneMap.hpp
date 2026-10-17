/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Tone mapping curves (the OpenGL tone-mapping shader mirrors them)
*/
#pragma once

#include <kronk3d/render/Environment.hpp>
#include <algorithm>

namespace k3
{

    // Maps a linear value of any size to [0, 1] after the exposure. None clamps.
    inline float toneMap(ToneMapping mode, float value, float exposure) noexcept
    {
        const float x = std::max(value, 0.f) * exposure;

        switch (mode) {
            case ToneMapping::Reinhard:
                return x / (1.f + x);
            case ToneMapping::ACES:     // Narkowicz's fit of the ACES filmic curve
                return std::clamp((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f), 0.f, 1.f);
            case ToneMapping::None:
                break;
        }
        return std::clamp(x, 0.f, 1.f);
    }

}
