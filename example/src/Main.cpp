#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Image.hpp>
#include "Color.hpp"
#include "Matrix.hpp"
#include "Rasterizer.hpp"
#include "utils/Mesh.hpp"
#include "utils/Viewport.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include "cube/Cube.hpp"

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
            {-0.5f, -0.5f, 0.f},
            {-0.5f, 0.5f, 0.f},
            {0.5f, -0.5f, 0.f},
            {0.5f, 0.5f, 0.f},
        },
        .colors = {k3::Math::Color::Red, k3::Math::Color::Blue, k3::Math::Color::Green, k3::Math::Color::Yellow},
        .indices = {0, 1, 2, 2, 1, 3},
        .count = 6  // 3 segment
    };

    k3::Viewport viewport{
        0,
        HEIGHT,
        0,
        HEIGHT
    };

    using Clock = std::chrono::steady_clock;

    static constexpr float ROTATION_SPEED_RAD_PER_SEC = 1.f;
    static constexpr float CAMERA_SPEED_UNITS_PER_SEC = 3.f;

    std::size_t frameCount = 0;
    double drawTimeAccumulatedMs = 0.0;
    auto statsTimer = Clock::now();
    auto lastFrameTime = Clock::now();
    const auto appStart = Clock::now();

    k3::Math::Vector3f cameraPosition{0.f, 0.f, 5.f};

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        const auto frameNow = Clock::now();
        const float deltaTime = std::chrono::duration<float>(frameNow - lastFrameTime).count();
        lastFrameTime = frameNow;

        // Camera displacement (world space, no rotation yet): ZQSD/WASD on the XZ plane, Space/Shift for height
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Z))
            cameraPosition.z -= CAMERA_SPEED_UNITS_PER_SEC * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
            cameraPosition.z += CAMERA_SPEED_UNITS_PER_SEC * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
            cameraPosition.x -= CAMERA_SPEED_UNITS_PER_SEC * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
            cameraPosition.x += CAMERA_SPEED_UNITS_PER_SEC * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
            cameraPosition.y += CAMERA_SPEED_UNITS_PER_SEC * deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift))
            cameraPosition.y -= CAMERA_SPEED_UNITS_PER_SEC * deltaTime;

        engine.clear(k3::Math::Color::Grey);

        const float elapsedSinceStart = std::chrono::duration<float>(Clock::now() - appStart).count();
        const float rotationAngle = elapsedSinceStart * ROTATION_SPEED_RAD_PER_SEC;

        // View = inverse of the camera's world transform, shared by every mesh in the scene.
        const auto view = k3::Math::Matrix4::translate({-cameraPosition.x, -cameraPosition.y, -cameraPosition.z});
        // Model = this mesh's own placement/animation in world space.
        const auto model = k3::Math::Matrix4::rotateZX(rotationAngle) * k3::Math::Matrix4::rotateYZ(rotationAngle);

        const auto drawStart = Clock::now();
        engine.draw(
            cube,
            viewport,
            k3::Math::Matrix4::perspective(0.01f, 10.f, M_PI / 3.f, WIDTH * 1.f / WIDTH) * view * model,
            k3::Rasterizer::Cull::CW
        );
        const auto drawEnd = Clock::now();
        drawTimeAccumulatedMs += std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();

        window.clear(sf::Color::Black);

        const auto& framebuffer = engine.framebuffer();
        if (framebuffer.size() == WIDTH * HEIGHT) {
            sf::Image image = toImage(framebuffer, WIDTH, HEIGHT);
            texture.loadFromImage(image);
            sprite.setTexture(texture, true);
            window.draw(sprite);
        }

        window.display();

        frameCount++;
        const auto now = Clock::now();
        const auto statsElapsed = now - statsTimer;
        if (statsElapsed >= std::chrono::seconds(1)) {
            const double elapsedSeconds = std::chrono::duration<double>(statsElapsed).count();
            const double fps = frameCount / elapsedSeconds;
            const double avgDrawMs = drawTimeAccumulatedMs / frameCount;

            window.setTitle(std::format("kronk3d — FPS: {:.1f} | draw: {:.3f} ms", fps, avgDrawMs));

            frameCount = 0;
            drawTimeAccumulatedMs = 0.0;
            statsTimer = now;
        }
    }

    return 0;
}
