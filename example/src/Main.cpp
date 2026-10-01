#include <SFML/Graphics.hpp>
#include "Color.hpp"
#include "Matrix.hpp"
#include "Rasterizer.hpp"
#include "io/ObjLoader.hpp"
#include "scene/Model.hpp"
#include "utils/Viewport.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <numbers>
#include <vector>
#include "cube/Cube.hpp"
#include "cube/CubeTextured.hpp"

#ifndef K3_EXAMPLE_ASSETS_DIR
    #define K3_EXAMPLE_ASSETS_DIR "example/assets"
#endif

static void toRGBA8(const std::vector<k3::Math::Color>& pixels, std::vector<std::uint8_t>& out)
{
    out.resize(pixels.size() * 4);

    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const k3::Math::Color& color = pixels[i];

        out[i * 4 + 0] = static_cast<std::uint8_t>(std::clamp(color.r, 0.f, 1.f) * 255.f);
        out[i * 4 + 1] = static_cast<std::uint8_t>(std::clamp(color.g, 0.f, 1.f) * 255.f);
        out[i * 4 + 2] = static_cast<std::uint8_t>(std::clamp(color.b, 0.f, 1.f) * 255.f);
        out[i * 4 + 3] = 255;
    }
}

int main(void)
{
    static constexpr unsigned WIDTH = 800;
    static constexpr unsigned HEIGHT = 600;

    const std::filesystem::path assets = K3_EXAMPLE_ASSETS_DIR;

    // 1: textured cube, 2: vertex-colored cube, 3: Spot (OBJ + MTL + texture)
    const k3::Model texturedCube = makeTexturedCube(assets / "stone.png");
    const k3::Model coloredCube{"colored cube", {{std::make_shared<k3::Mesh>(cube), std::make_shared<k3::Material>()}}};
    auto spot = k3::ObjLoader::load(assets / "spot" / "spot.obj");

    if (!spot) {
        std::cerr << "Cannot load Spot: " << spot.error() << std::endl;
        return 1;
    }

    const k3::Model* scenes[] = {&texturedCube, &coloredCube, &*spot};
    std::size_t currentScene = 2;

    k3::Rasterizer engine(WIDTH, HEIGHT);

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "kronk3d");
    sf::Texture texture;
    sf::Sprite sprite;
    std::vector<std::uint8_t> rgba;

    texture.create(WIDTH, HEIGHT);
    sprite.setTexture(texture, true);

    k3::Viewport viewport{
        0,
        WIDTH,
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
            if (event.type == sf::Event::KeyPressed && event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num3)
                currentScene = static_cast<std::size_t>(event.key.code - sf::Keyboard::Num1);
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

        engine.clear(k3::Math::Color::fromRGB(22, 22, 22));

        const float elapsedSinceStart = std::chrono::duration<float>(Clock::now() - appStart).count();
        const float rotationAngle = elapsedSinceStart * ROTATION_SPEED_RAD_PER_SEC;

        // View = inverse of the camera's world transform, shared by every mesh in the scene.
        const auto view = k3::Math::Matrix4::translate({-cameraPosition.x, -cameraPosition.y, -cameraPosition.z});
        // Model = this mesh's own placement/animation in world space, centered and scaled to fit a 2-unit cube.
        const auto bounds = scenes[currentScene]->bounds();
        const auto fit = k3::Math::Matrix4::scale(1.5f / bounds.radius())
            * k3::Math::Matrix4::translate(-bounds.center());
        const auto model = k3::Math::Matrix4::rotateZX(rotationAngle) * k3::Math::Matrix4::rotateYZ(rotationAngle * 0.5f) * fit;
        const auto projection = k3::Math::Matrix4::perspective(0.01f, 100.f, std::numbers::pi_v<float> / 3.f, static_cast<float>(WIDTH) / HEIGHT);

        const auto drawStart = Clock::now();
        engine.draw(*scenes[currentScene], viewport, projection * view * model);

        const auto drawEnd = Clock::now();
        drawTimeAccumulatedMs += std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();

        toRGBA8(engine.framebuffer(), rgba);
        texture.update(rgba.data());

        window.clear(sf::Color::Black);
        window.draw(sprite);
        window.display();

        frameCount++;
        const auto now = Clock::now();
        const auto statsElapsed = now - statsTimer;
        if (statsElapsed >= std::chrono::seconds(1)) {
            const double elapsedSeconds = std::chrono::duration<double>(statsElapsed).count();
            const double fps = frameCount / elapsedSeconds;
            const double avgDrawMs = drawTimeAccumulatedMs / frameCount;

            window.setTitle(std::format("kronk3d — {} — FPS: {:.1f} | draw: {:.3f} ms [1-3: scene]", scenes[currentScene]->name, fps, avgDrawMs));

            frameCount = 0;
            drawTimeAccumulatedMs = 0.0;
            statsTimer = now;
        }
    }

    return 0;
}
