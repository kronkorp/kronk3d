/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Fast table-driven sRGB <-> linear conversions for 8-bit channels
*/
#pragma once

#include <cstddef>
#include <cstdint>

namespace k3::srgb
{

    // Linear values are quantized to 16 bits before encoding: fine enough that no sRGB level is skipped.
    inline constexpr std::size_t ENCODE_SIZE = 65536;

    // Hot loops should fetch these once and index them directly.
    const float*        decodeTable() noexcept;     // 256 entries: sRGB byte -> linear value in [0, 1]
    const std::uint8_t* encodeTable() noexcept;     // ENCODE_SIZE entries: quantized linear value -> sRGB byte

    inline float decode(std::uint8_t value, const float* table = decodeTable()) noexcept
    {
        return table[value];
    }

    // Linear value (clamped to [0, 1], NaN -> 0) -> sRGB-encoded byte.
    inline std::uint8_t encode(float value, const std::uint8_t* table = encodeTable()) noexcept
    {
        // Written so that NaN fails both comparisons and maps to 0.
        const float clamped = value > 0.f ? (value < 1.f ? value : 1.f) : 0.f;
        return table[static_cast<std::size_t>(clamped * (ENCODE_SIZE - 1) + 0.5f)];
    }

}
