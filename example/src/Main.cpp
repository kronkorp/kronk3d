#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Image.hpp>

#include "Rasterizer.hpp"
#include "Vector.hpp"

#include <algorithm>
#include <cstdint>

static sf::Image toImage(const std::vector<k3::Math::Vector4f>& pixels, size_t width, size_t height)
{
    sf::Image image;
    image.create(width, height, sf::Color::Black);

    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            const k3::Math::Vector4f& color = pixels[x + y * width];

            image.setPixel(x, y, sf::Color(
                static_cast<std::uint8_t>(std::clamp(color.x, 0.f, 1.f) * 255.f),
                static_cast<std::uint8_t>(std::clamp(color.y, 0.f, 1.f) * 255.f),
                static_cast<std::uint8_t>(std::clamp(color.z, 0.f, 1.f) * 255.f),
                static_cast<std::uint8_t>(std::clamp(color.w, 0.f, 1.f) * 255.f)
            ));
        }
    }

    return image;
}

int main(void)
{
    static constexpr size_t WIDTH = 800;
    static constexpr size_t HEIGHT = 600;

    k3::Rasterizer engine(800, 600);

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "kronk3d");
    sf::Texture texture;
    sf::Sprite sprite;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        engine.clear(k3::Math::Vector4f(1.f, 1.f, 1.f, 1.f));

        // engine.draw();

        window.clear(sf::Color::Black);

        const auto& framebuffer = engine.framebuffer();
        if (framebuffer.size() == WIDTH * HEIGHT) {
            sf::Image image = toImage(framebuffer, WIDTH, HEIGHT);
            texture.loadFromImage(image);
            sprite.setTexture(texture, true);
            window.draw(sprite);
        }

        window.display();
    }

    return 0;
}
