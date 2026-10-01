#include "third_party/stb_image.h"
#include "third_party/stb_image_write.h"

#include "Image.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

static std::uint8_t toByte(float value)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.f, 1.f) * 255.f));
}

k3::Image::Image(std::uint32_t w, std::uint32_t h, const Math::Color& fill)
    : width(w), height(h), pixels(static_cast<std::size_t>(w) * h * 4)
{
    const std::uint8_t rgba[4] = {toByte(fill.r), toByte(fill.g), toByte(fill.b), toByte(fill.a)};

    for (std::size_t i = 0; i < pixels.size(); i += 4)
        std::copy(rgba, rgba + 4, pixels.begin() + i);
}

bool k3::Image::hasTransparency() const noexcept
{
    for (std::size_t i = 3; i < pixels.size(); i += 4)
        if (pixels[i] != 255)
            return true;
    return false;
}

bool k3::Image::isGrayscale(int tolerance) const noexcept
{
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        const int r = pixels[i], g = pixels[i + 1], b = pixels[i + 2];
        if (std::abs(r - g) > tolerance || std::abs(g - b) > tolerance)
            return false;
    }
    return true;
}

k3::Result<k3::Image> k3::Image::loadFromMemory(const void* data, std::size_t size)
{
    int w = 0, h = 0, channels = 0;
    stbi_uc* stb = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size), &w, &h, &channels, 4);

    if (!stb)
        return k3::Error(std::string(stbi_failure_reason()));

    Image image;
    image.width = static_cast<std::uint32_t>(w);
    image.height = static_cast<std::uint32_t>(h);
    image.pixels.assign(stb, stb + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(stb);
    return image;
}

// Reading through std::ifstream (instead of stbi_load(const char*)) keeps non-ASCII paths working on Windows.
k3::Result<k3::Image> k3::Image::load(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return k3::Error("cannot open " + path.string());

    const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto image = loadFromMemory(bytes.data(), bytes.size());

    if (!image)
        return k3::Error(path.string() + ": " + image.error());
    return image;
}

bool k3::Image::savePNG(const std::filesystem::path& path) const
{
    std::ofstream file(path, std::ios::binary);

    if (!file || empty())
        return false;

    auto write = [](void* context, void* data, int size) {
        static_cast<std::ofstream*>(context)->write(static_cast<const char*>(data), size);
    };
    return stbi_write_png_to_func(write, &file, static_cast<int>(width), static_cast<int>(height), 4, pixels.data(), static_cast<int>(width * 4)) != 0
        && file.good();
}
