#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Image.hpp>
#include "Color.hpp"
#include "Rasterizer.hpp"
#include "utils/Mesh.hpp"
#include "utils/Viewport.hpp"
#include <algorithm>
#include <cstdint>

static sf::Image toImage(const std::vector<k3::Math::Color>& pixels, size_t width, size_t height)
{
    sf::Image image;
    image.create(width, height, sf::Color::Black);

    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            const k3::Math::Color& color = pixels[x + y * width];

            image.setPixel(x, y, sf::Color(
                static_cast<std::uint8_t>(std::clamp(color.r, 0.f, 1.f) * 255.f),
                static_cast<std::uint8_t>(std::clamp(color.g, 0.f, 1.f) * 255.f),
                static_cast<std::uint8_t>(std::clamp(color.b, 0.f, 1.f) * 255.f),
                static_cast<std::uint8_t>(std::clamp(color.a, 0.f, 1.f) * 255.f)
            ));
        }
    }

    return image;
}

int main(void)
{
    static constexpr size_t WIDTH = 800;
    static constexpr size_t HEIGHT = 600;

    k3::Rasterizer engine(WIDTH, HEIGHT);

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "kronk3d");
    sf::Texture texture;
    sf::Sprite sprite;

    k3::Mesh mesh{
        .vertices = {
            {-0.5f, 0.5f, 0.f},
            {0.5f, 0.5f},
            {0.f, -0.5f}
        },
        .colors = {k3::Math::Color::Blue, k3::Math::Color::Red, k3::Math::Color::Green},
        .indices = {1, 2, 3},
        .count = 3  // 3 segment
    };

    k3::Viewport viewport{
        0,
        WIDTH,
        0,
        HEIGHT
    };

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        engine.clear(k3::Math::Color::fromRGB(255, 0, 255));

        engine.draw(mesh, viewport);

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
