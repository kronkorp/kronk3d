// Minimal animation demo: loads a glTF / GLB file and plays its clips.
//     kronk3d_animation_demo [file.glb] [--software]
// 1-9: play that clip (crossfaded) | Space: pause the spin | Escape: quit
#include <SFML/Window.hpp>
#include "Kronk3d.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#ifndef K3_EXAMPLE_ASSETS_DIR
    #define K3_EXAMPLE_ASSETS_DIR "example/assets"
#endif

int main(int argc, char** argv)
{
    std::filesystem::path path = std::filesystem::path(K3_EXAMPLE_ASSETS_DIR) / "fox" / "Fox.glb";
    k3::Backend backend = k3::Backend::OpenGL;

    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--software")
            backend = k3::Backend::Software;
        else
            path = argv[i];
    }

    auto loaded = k3::GltfLoader::load(path);
    if (!loaded) {
        std::cerr << "Cannot load " << path << ": " << loaded.error() << std::endl;
        return 1;
    }

    const auto model = std::make_shared<const k3::AnimatedModel>(std::move(*loaded));
    k3::Animator animator(model);

    std::cout << path.filename() << ": " << model->primitives.size() << " meshes, " << model->nodes.size() << " nodes, "
              << model->animations.size() << " animations" << std::endl;
    for (std::size_t i = 0; i < model->animations.size(); ++i)
        std::cout << "  [" << i + 1 << "] " << model->animations[i].name << " (" << model->animations[i].duration << " s)" << std::endl;
    if (!model->animations.empty())
        animator.play(model->animations[0].name);

    sf::Window window(sf::VideoMode(800, 600), "kronk3d animation", sf::Style::Default, sf::ContextSettings(24, 0, 0, 3, 3, sf::ContextSettings::Core));
    window.setActive(true);
    window.setVerticalSyncEnabled(true);    // The GL path re-uploads the posed meshes every frame: don't run it uncapped

    const k3::GLLoader loader = [](const char* name) { return sf::Context::getFunction(name); };
    auto presenter = k3::ImagePresenter::create(loader);     // Shows the software backend's images
    auto rasterizer = k3::createRasterizer(backend, {.width = 800, .height = 600, .glLoader = loader});

    if (!presenter || !rasterizer) {
        std::cerr << "Cannot initialize rendering: " << (presenter ? rasterizer.error() : presenter.error()) << std::endl;
        return 1;
    }

    // Whatever its size, the model is centered on the origin and scaled to a 1.5 radius.
    const auto bounds = model->bounds();
    const auto fit = k3::Math::Matrix4::scale(1.5f / bounds.radius()) * k3::Math::Matrix4::translate(-bounds.center());

    k3::Camera camera;
    camera.position = {0.f, 0.5f, 4.f};
    camera.lookAt({0.f, 0.f, 0.f});

    k3::Environment environment;
    environment.clearColor = k3::Math::Color::fromSRGB(22, 22, 26);
    environment.ambient = {0.25f, 0.25f, 0.28f, 1.f};
    environment.lights.push_back(k3::Light::directional({-0.5f, -1.f, -0.6f}, {1.f, 0.95f, 0.9f, 1.f}, 1.5f));

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    float angle = 0.f;
    bool spin = true;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed || (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
                window.close();
            if (event.type == sf::Event::Resized)
                (*rasterizer)->resize(event.size.width, event.size.height);
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space)
                spin = !spin;
            if (event.type == sf::Event::KeyPressed && event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num9) {
                const auto index = static_cast<std::size_t>(event.key.code - sf::Keyboard::Num1);

                if (index < model->animations.size())
                    animator.play(model->animations[index].name, {.fade = 0.25f});
            }
        }

        const auto now = Clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (spin)
            angle += 0.5f * dt;

        animator.update(dt);
        (*rasterizer)->beginFrame(camera, environment);
        animator.draw(**rasterizer, k3::Math::Matrix4::rotateZX(angle) * fit);
        (*rasterizer)->endFrame();

        if (backend == k3::Backend::Software)
            (*presenter)->present((*rasterizer)->readPixels(), (*rasterizer)->width(), (*rasterizer)->height());
        window.display();
        window.setTitle("kronk3d animation: " + std::string(animator.clip()) + " [1-9: clip, Space: spin]");
    }
    return 0;
}
