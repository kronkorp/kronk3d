
#include "cube/Cube.hpp"
#include "kronk3d/core/Core.hpp"
#include "kronk3d/mesh/Mesh.hpp"
#include "kronk3d/utils/Color.hpp"
#include "kronk3d/utils/Matrix.hpp"
#include "kronk3d/utils/Vector.hpp"
#include "kronk3d/viewport/ViewPort.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <vector>

static sf::Image toImage(const std::vector<k3::Color>& pixels, size_t width, size_t height)
{
    sf::Image image;
    image.create(width, height, sf::Color::Black);

    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            const k3::Color& color = pixels[x + y * width];

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
    k3::Rasterizer core(800, 600);

    const k3::ViewPort viewport{0, 800, 0, 600};

    sf::Texture texture;
    texture.create(core.viewWidth(), core.viewHeight());

    sf::Sprite sprite(texture);

    sf::RenderWindow window(sf::VideoMode(core.viewWidth(), core.viewHeight()), "kronk3d");

    sf::Clock clock;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        float angle = clock.getElapsedTime().asSeconds();

        auto point = std::chrono::high_resolution_clock::now().time_since_epoch();

        core.clear(k3::Color::White);
        core.draw(
            cube,
            viewport,
            k3::Matrix4::scale(0.5f)
                * k3::Matrix4::scale({600 * 1.f / 800, 1.f, 1.f})
                * k3::Matrix4::rotateZX(angle)
                * k3::Matrix4::rotateXY(angle * 1.61f),
            k3::Rasterizer::Cull::CW
        );

        std::chrono::high_resolution_clock::duration a = std::chrono::high_resolution_clock::now().time_since_epoch() - point;
        std::cout << "Took " << a << " to draw" << std::endl;

        sf::Image image = toImage(core.framebuffer(), core.viewWidth(), core.viewHeight());
        texture.update(image);

        window.clear();
        window.draw(sprite);
        window.display();
    }

    return 0;
}
