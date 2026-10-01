#include "Floor.hpp"
#include "scene/Image.hpp"
#include "scene/Texture.hpp"
#include <cstdint>
#include <memory>

k3::Model makeFloor(float halfSize, float y)
{
    // 2x2 checker cells; the uvs repeat it once per world unit.
    k3::Image checker(64, 64);
    for (std::uint32_t py = 0; py < checker.height; ++py) {
        for (std::uint32_t px = 0; px < checker.width; ++px) {
            const std::uint8_t value = ((px / 32 + py / 32) % 2) ? 170 : 110;
            std::uint8_t* texel = checker.texel(px, py);
            texel[0] = texel[1] = texel[2] = value;
            texel[3] = 255;
        }
    }

    const float repeat = halfSize;
    auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-halfSize, y, halfSize}, {halfSize, y, halfSize}, {halfSize, y, -halfSize}, {-halfSize, y, -halfSize}},
        .normals = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
        .uvs = {{0.f, repeat}, {repeat, repeat}, {repeat, 0.f}, {0.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });
    auto material = std::make_shared<k3::Material>();
    material->name = "floor";
    material->diffuseMap = std::make_shared<k3::Texture>(checker);

    return k3::Model{"floor", {{mesh, material}}};
}
