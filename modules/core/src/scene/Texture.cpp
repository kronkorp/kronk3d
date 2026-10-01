#include "Texture.hpp"
#include "Logger.hpp"
#include <utility>

k3::Texture::Texture(Image image, ColorSpace colorSpace)
    : m_colorSpace(colorSpace)
{
    setImage(std::move(image));
}

std::shared_ptr<k3::Texture> k3::Texture::load(const std::filesystem::path& path, ColorSpace colorSpace)
{
    auto image = Image::load(path);

    if (!image) {
        Logger::logger().error("Failed to load texture {}", image.error());
        return nullptr;
    }
    return std::make_shared<Texture>(std::move(*image), colorSpace);
}

void k3::Texture::setImage(Image image)
{
    m_image = std::move(image);
    m_hasTransparency = m_image.hasTransparency();
    ++m_version;
}
