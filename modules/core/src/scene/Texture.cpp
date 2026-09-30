// Compiles the stb_image implementation exactly once for the whole project.
// Every other translation unit only sees the declarations via Texture.hpp.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Texture.hpp"
#include "Logger.hpp"

k3::Texture k3::Texture::fromImage(const std::string& filename)
{
    Texture t;
    unsigned char* stb = stbi_load(filename.c_str(), &t.width, &t.height, &t.channels, 4);

    if (!stb) {
        Logger::logger().error("Failed to load texture {}", filename);
        return Texture{};
    }

    for (auto y = 0; y < t.height; ++y) {
        for (auto x = 0; x < t.width; ++x) {
            int index = (y * t.width + x) * 4;
            t.pixels.push_back(Math::Color::fromRGB(stb[index], stb[index + 1], stb[index + 2], stb[index + 3]));
        }
    }

    stbi_image_free(stb);

    return t;
}
