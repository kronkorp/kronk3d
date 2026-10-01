#include <SFML/Window.hpp>
#include "Kronk3d.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "cube/Cube.hpp"
#include "cube/CubeTextured.hpp"
#include "scenes/Floor.hpp"
#include "scenes/Transparency.hpp"

#ifndef K3_EXAMPLE_ASSETS_DIR
    #define K3_EXAMPLE_ASSETS_DIR "example/assets"
#endif

namespace
{

    constexpr float MOVE_SPEED = 3.f;          // Units per second
    constexpr float TURN_SPEED = 1.5f;         // Radians per second (arrow keys)
    constexpr float MOUSE_SENSITIVITY = 0.004f; // Radians per pixel
    constexpr float MAX_PITCH = 1.55f;

    bool pressed(sf::Keyboard::Key key)
    {
        return sf::Keyboard::isKeyPressed(key);
    }

    // ZQSD / WASD to move, Space / Shift for height, arrows or right mouse drag to look around.
    void updateCamera(k3::Camera& camera, float dt, const sf::Vector2i& mouseDelta, bool mouseLook)
    {
        k3::Math::Vector3f move{};
        const auto forward = camera.forward();
        const auto right = camera.right();

        if (pressed(sf::Keyboard::W) || pressed(sf::Keyboard::Z))
            move += forward;
        if (pressed(sf::Keyboard::S))
            move += -forward;
        if (pressed(sf::Keyboard::D))
            move += right;
        if (pressed(sf::Keyboard::A) || pressed(sf::Keyboard::Q))
            move += -right;
        if (pressed(sf::Keyboard::Space))
            move += {0.f, 1.f, 0.f};
        if (pressed(sf::Keyboard::LShift))
            move += {0.f, -1.f, 0.f};
        camera.position += k3::Math::Vector3f::normalize(move) * (MOVE_SPEED * dt);

        if (pressed(sf::Keyboard::Left))
            camera.yaw -= TURN_SPEED * dt;
        if (pressed(sf::Keyboard::Right))
            camera.yaw += TURN_SPEED * dt;
        if (pressed(sf::Keyboard::Up))
            camera.pitch += TURN_SPEED * dt;
        if (pressed(sf::Keyboard::Down))
            camera.pitch -= TURN_SPEED * dt;
        if (mouseLook) {
            camera.yaw += mouseDelta.x * MOUSE_SENSITIVITY;
            camera.pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
        }
        camera.pitch = std::clamp(camera.pitch, -MAX_PITCH, MAX_PITCH);
    }

    struct Options
    {
        k3::Backend backend = k3::Backend::Software;
        std::size_t scene = 2;
        std::string screenshot{};   // Render a few frames, save the last one there and quit
    };

    std::optional<Options> parseOptions(int argc, char** argv)
    {
        Options options;

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            const bool hasValue = i + 1 < argc;

            if (arg == "--backend" && hasValue) {
                const std::string_view value = argv[++i];
                if (value != "software" && value != "opengl")
                    return std::nullopt;
                options.backend = value == "opengl" ? k3::Backend::OpenGL : k3::Backend::Software;
            } else if (arg == "--scene" && hasValue) {
                options.scene = static_cast<std::size_t>(std::clamp(std::atoi(argv[++i]), 1, 4) - 1);
            } else if (arg == "--screenshot" && hasValue) {
                options.screenshot = argv[++i];
            } else {
                return std::nullopt;
            }
        }
        return options;
    }

}

int main(int argc, char** argv)
{
    static constexpr unsigned WIDTH = 800;
    static constexpr unsigned HEIGHT = 600;

    const auto options = parseOptions(argc, argv);
    if (!options) {
        std::cerr << "usage: " << argv[0] << " [--backend software|opengl] [--scene 1-4] [--screenshot file.png]" << std::endl;
        return 2;
    }

    const std::filesystem::path assets = K3_EXAMPLE_ASSETS_DIR;

    // 1: textured cube, 2: vertex-colored cube, 3: Spot (OBJ + MTL + texture), 4: transparency
    const k3::Model texturedCube = makeTexturedCube(assets / "stone.png");
    const k3::Model coloredCube{"colored cube", {{std::make_shared<k3::Mesh>(cube), std::make_shared<k3::Material>()}}};
    auto spot = k3::ObjLoader::load(assets / "spot" / "spot.obj");

    if (!spot) {
        std::cerr << "Cannot load Spot: " << spot.error() << std::endl;
        return 1;
    }

    const k3::Model transparency = makeTransparencyScene();
    const k3::Model* scenes[] = {&texturedCube, &coloredCube, &*spot, &transparency};
    // Models are scaled to a 1.5 radius around the origin: the floor sits right under them.
    const k3::Model floor = makeFloor(8.f, -1.55f);
    std::size_t currentScene = options->scene;

    // An OpenGL 3.3 core context: the hardware backend renders into it, and the software backend's
    // images are shown through it (ImagePresenter).
    const sf::ContextSettings settings(24, 8, 0, 3, 3, sf::ContextSettings::Core);
    sf::Window window(sf::VideoMode(WIDTH, HEIGHT), "kronk3d", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(false);
    window.setActive(true);

    const k3::GLLoader loader = [](const char* name) { return sf::Context::getFunction(name); };
    auto presenter = k3::ImagePresenter::create(loader);
    if (!presenter) {
        std::cerr << "Cannot initialize OpenGL: " << presenter.error() << std::endl;
        return 1;
    }

    // Tab switches between the software and the OpenGL backend.
    k3::Backend backend = options->backend;
    std::unique_ptr<k3::IRasterizer> rasterizer;
    auto switchTo = [&](k3::Backend wanted) {
        const sf::Vector2u size = window.getSize();
        auto created = k3::createRasterizer(wanted, {.width = size.x, .height = size.y, .glLoader = loader});

        if (!created) {
            std::cerr << "Cannot create the rasterizer: " << created.error() << std::endl;
            return false;
        }
        rasterizer = std::move(*created);
        backend = wanted;
        return true;
    };
    if (!switchTo(backend))
        return 1;

    k3::Camera camera;
    camera.position = {0.f, 0.5f, 4.f};
    camera.lookAt({0.f, 0.f, 0.f});

    k3::Environment environment;
    environment.clearColor = k3::Math::Color::fromSRGB(22, 22, 26);
    environment.ambient = {0.15f, 0.15f, 0.18f, 1.f};
    environment.lights.push_back(k3::Light::directional({-0.5f, -1.f, -0.6f}, {1.f, 0.95f, 0.9f, 1.f}, 1.5f));
    environment.lights.back().castShadows = true;
    // Warm bulb orbiting the model (position updated every frame).
    environment.lights.push_back(k3::Light::point({}, {1.f, 0.45f, 0.2f, 1.f}, 6.f, 6.f));

    using Clock = std::chrono::steady_clock;

    static constexpr float ROTATION_SPEED_RAD_PER_SEC = 0.5f;

    std::size_t frameCount = 0;
    double frameTimeAccumulatedMs = 0.0;
    auto statsTimer = Clock::now();
    auto lastFrameTime = Clock::now();
    const auto appStart = Clock::now();
    sf::Vector2i lastMouse = sf::Mouse::getPosition(window);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::Resized)
                rasterizer->resize(event.size.width, event.size.height);
            if (event.type == sf::Event::KeyPressed && event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num4)
                currentScene = static_cast<std::size_t>(event.key.code - sf::Keyboard::Num1);
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab)
                switchTo(backend == k3::Backend::Software ? k3::Backend::OpenGL : k3::Backend::Software);
        }

        const auto frameNow = Clock::now();
        const float deltaTime = std::chrono::duration<float>(frameNow - lastFrameTime).count();
        lastFrameTime = frameNow;

        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        const bool mouseLook = window.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Right);
        if (window.hasFocus())
            updateCamera(camera, deltaTime, mouse - lastMouse, mouseLook);
        lastMouse = mouse;

        const float elapsedSinceStart = std::chrono::duration<float>(Clock::now() - appStart).count();
        const float rotationAngle = elapsedSinceStart * ROTATION_SPEED_RAD_PER_SEC;

        // Center the model on the origin and scale it to a ~3 unit wide box, then spin it.
        const k3::Model& model = *scenes[currentScene];
        const auto bounds = model.bounds();
        const auto fit = k3::Math::Matrix4::scale(1.5f / bounds.radius()) * k3::Math::Matrix4::translate(-bounds.center());
        const auto transform = k3::Math::Matrix4::rotateZX(rotationAngle) * fit;

        environment.lights[1].position = {2.f * std::cos(elapsedSinceStart), 1.f, 2.f * std::sin(elapsedSinceStart)};

        rasterizer->beginFrame(camera, environment);
        rasterizer->draw(model, transform);
        rasterizer->draw(floor);
        rasterizer->endFrame();
        frameTimeAccumulatedMs += rasterizer->stats().frameMs;

        if (!options->screenshot.empty() && frameCount == 3) {
            const bool saved = rasterizer->readPixels().savePNG(options->screenshot);
            std::cout << (saved ? "Saved " : "Cannot save ") << options->screenshot << std::endl;
            return saved ? 0 : 1;
        }

        // The OpenGL backend already copied its frame to the window; the software one hands an image over.
        if (backend == k3::Backend::Software)
            (*presenter)->present(rasterizer->readPixels(), rasterizer->width(), rasterizer->height());
        window.display();

        frameCount++;
        const auto now = Clock::now();
        const auto statsElapsed = now - statsTimer;
        if (statsElapsed >= std::chrono::seconds(1)) {
            const double elapsedSeconds = std::chrono::duration<double>(statsElapsed).count();
            const double fps = frameCount / elapsedSeconds;
            const double avgFrameMs = frameTimeAccumulatedMs / frameCount;

            window.setTitle(std::format(
                "kronk3d [{}] — {} — FPS: {:.1f} | render: {:.2f} ms | {} triangles [1-4: scene, Tab: backend]",
                rasterizer->name(), model.name, fps, avgFrameMs, rasterizer->stats().triangles
            ));

            frameCount = 0;
            frameTimeAccumulatedMs = 0.0;
            statsTimer = now;
        }
    }

    return 0;
}
