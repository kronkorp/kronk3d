#include <kronk3d/render/Skybox.hpp>
#include "render/CubeMap.hpp"
#include "utils/Srgb.hpp"
#include <algorithm>
#include <cmath>

bool k3::Skybox::valid() const noexcept
{
    if (!faces[0] || faces[0]->image().empty())
        return false;

    const std::uint32_t size = faces[0]->width();

    return std::all_of(faces.begin(), faces.end(), [size](const auto& face) {
        return face && !face->image().empty() && face->width() == size && face->height() == size;
    });
}

k3::Skybox k3::Skybox::gradient(const Math::Color& zenith, const Math::Color& horizon, const Math::Color& ground, std::uint32_t faceSize)
{
    Skybox sky;
    const std::uint8_t* encode = srgb::encodeTable();
    const std::uint32_t size = std::max(faceSize, 1u);

    for (int face = 0; face < 6; ++face) {
        Image image(size, size);

        for (std::uint32_t y = 0; y < size; ++y) {
            for (std::uint32_t x = 0; x < size; ++x) {
                const auto direction = Math::Vector3f::normalize(cube::direction(face, (x + 0.5f) / size, (y + 0.5f) / size));
                const float up = std::clamp(direction.y, -1.f, 1.f);
                const Math::Color toward = up >= 0.f ? zenith : ground;
                const float amount = std::abs(up);
                std::uint8_t* texel = image.texel(x, y);

                texel[0] = srgb::encode(horizon.r + (toward.r - horizon.r) * amount, encode);
                texel[1] = srgb::encode(horizon.g + (toward.g - horizon.g) * amount, encode);
                texel[2] = srgb::encode(horizon.b + (toward.b - horizon.b) * amount, encode);
                texel[3] = 255;
            }
        }
        sky.faces[static_cast<std::size_t>(face)] = std::make_shared<Texture>(std::move(image));
        sky.faces[static_cast<std::size_t>(face)]->sampler.filter = TextureFilter::Linear;
    }
    return sky;
}
