// Renders an OBJ scene with each backend and reports load and frame times.
//
//     kronk3d_bench <scene.obj> [--size WxH] [--frames N] [--threads N] [--no-shadows] [--cascades N]
//                   [--shadow-resolution N] [--aa none|fxaa|ssaa] [--screenshot prefix]
//
// The camera stands inside the scene's bounds, near the floor, looking along its longest horizontal axis
// (a good default for architectural scenes such as Sponza).
#include <SFML/Window/Context.hpp>
#include "Kronk3d.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{

    using Clock = std::chrono::steady_clock;

    struct Options
    {
        std::string   scene;
        std::uint32_t width = 1280, height = 720;
        int           frames = 20;
        unsigned      threads = 0;
        bool          shadows = true;
        std::string   screenshot;
        k3::AntiAliasing antiAliasing = k3::AntiAliasing::None;
        k3::ShadowSettings shadowSettings{};
    };

    bool parse(int argc, char** argv, Options& options)
    {
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            const bool hasValue = i + 1 < argc;

            if (arg == "--size" && hasValue) {
                if (std::sscanf(argv[++i], "%ux%u", &options.width, &options.height) != 2)
                    return false;
            } else if (arg == "--frames" && hasValue) {
                options.frames = std::max(1, std::atoi(argv[++i]));
            } else if (arg == "--threads" && hasValue) {
                options.threads = static_cast<unsigned>(std::atoi(argv[++i]));
            } else if (arg == "--no-shadows") {
                options.shadows = false;
            } else if (arg == "--cascades" && hasValue) {
                options.shadowSettings.cascades = std::atoi(argv[++i]);
            } else if (arg == "--shadow-resolution" && hasValue) {
                options.shadowSettings.resolution = static_cast<std::uint32_t>(std::atoi(argv[++i]));
            } else if (arg == "--screenshot" && hasValue) {
                options.screenshot = argv[++i];
            } else if (arg == "--aa" && hasValue) {
                const std::string_view value = argv[++i];
                if (value != "none" && value != "fxaa" && value != "ssaa")
                    return false;
                options.antiAliasing = value == "fxaa" ? k3::AntiAliasing::FXAA : value == "ssaa" ? k3::AntiAliasing::SSAA : k3::AntiAliasing::None;
            } else if (!arg.starts_with("--") && options.scene.empty()) {
                options.scene = arg;
            } else {
                return false;
            }
        }
        return !options.scene.empty();
    }

    double since(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    void run(k3::IRasterizer& rasterizer, const k3::Model& model, const k3::Camera& camera, const k3::Environment& environment, const Options& options)
    {
        std::vector<double> frames;

        for (int i = 0; i < options.frames + 2; ++i) {
            const auto start = Clock::now();
            rasterizer.beginFrame(camera, environment);
            rasterizer.draw(model);
            rasterizer.endFrame();
            if (rasterizer.backend() == k3::Backend::OpenGL)
                (void)rasterizer.readPixels();      // Waits for the GPU to finish
            if (i >= 2)                             // Warm-up: uploads, caches
                frames.push_back(since(start));
        }

        std::sort(frames.begin(), frames.end());
        double sum = 0.0;
        for (double f : frames)
            sum += f;
        std::printf("%-9s %4ux%-4u  min %7.2f ms  median %7.2f ms  mean %7.2f ms  (%zu draws, %zu triangles)\n",
            std::string(rasterizer.name()).c_str(), options.width, options.height,
            frames.front(), frames[frames.size() / 2], sum / frames.size(),
            rasterizer.stats().drawCalls, rasterizer.stats().triangles);

        if (!options.screenshot.empty())
            rasterizer.readPixels().savePNG(options.screenshot + "_" + std::string(rasterizer.name()) + ".png");
    }

}

int main(int argc, char** argv)
{
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: " << argv[0] << " <scene.obj> [--size WxH] [--frames N] [--threads N] [--no-shadows] [--cascades N] [--shadow-resolution N] [--aa none|fxaa|ssaa] [--screenshot prefix]" << std::endl;
        return 2;
    }

    const auto loadStart = Clock::now();
    auto model = k3::ObjLoader::load(options.scene);
    if (!model) {
        std::cerr << model.error() << std::endl;
        return 1;
    }
    std::printf("load      %.0f ms\n", since(loadStart));

    const auto bounds = model->bounds();
    const auto size = bounds.size();
    const bool alongX = size.x >= size.z;
    k3::Camera camera;
    camera.position = bounds.center() + k3::Math::Vector3f{alongX ? -size.x * 0.35f : 0.f, -size.y * 0.3f, alongX ? 0.f : -size.z * 0.35f};
    camera.lookAt(camera.position + k3::Math::Vector3f{alongX ? 1.f : 0.f, 0.1f, alongX ? 0.f : 1.f});
    camera.nearPlane = bounds.radius() * 0.001f;
    camera.farPlane = bounds.radius() * 3.f;

    k3::Environment environment;
    environment.shadows = options.shadowSettings;
    environment.ambient = {0.15f, 0.15f, 0.17f, 1.f};
    environment.lights.push_back(k3::Light::directional({-0.3f, -1.f, -0.2f}, {1.f, 0.95f, 0.85f, 1.f}, 2.f));
    environment.lights.back().castShadows = options.shadows;
    environment.lights.push_back(k3::Light::point(bounds.center(), {1.f, 0.6f, 0.3f, 1.f}, 30.f, bounds.radius() * 0.5f));

    auto software = k3::createRasterizer(k3::Backend::Software, {.width = options.width, .height = options.height, .threads = options.threads, .antiAliasing = options.antiAliasing});
    run(**software, *model, camera, environment, options);

    // OpenGL, when a 3.3 context can be created here.
#if defined(__linux__)
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY"))
        return 0;
#endif
    const sf::ContextSettings settings(24, 8, 0, 3, 3, sf::ContextSettings::Core);
    sf::Context context(settings, 1, 1);
    context.setActive(true);
    auto opengl = k3::createRasterizer(k3::Backend::OpenGL, {
        .width = options.width, .height = options.height,
        .glLoader = [](const char* name) { return sf::Context::getFunction(name); },
        .antiAliasing = options.antiAliasing,
    });
    if (opengl)
        run(**opengl, *model, camera, environment, options);
    else
        std::cerr << "OpenGL: " << opengl.error() << std::endl;
    return 0;
}
