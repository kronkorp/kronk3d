/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Shared by the OpenGL tests: an offscreen context, the hardware rasterizer and a few scenes
*/
#pragma once

#include "Test.hpp"
#include <kronk3d/Kronk3d.hpp>
#include "render/opengl/GL.hpp"
#include <SFML/Window/Context.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>

namespace k3gl
{


    // One offscreen OpenGL 3.3 core context for the whole run.
    struct GLEnvironment
    {
        std::unique_ptr<sf::Context> context;
        bool available = false;

        GLEnvironment()
        {
#if defined(__linux__)
            // Without a display SFML aborts instead of failing: do not even try.
            if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY"))
                return;
#endif
            const sf::ContextSettings settings(24, 8, 0, 3, 3, sf::ContextSettings::Core);
            context = std::make_unique<sf::Context>(settings, 1, 1);
            context->setActive(true);

            const auto actual = context->getSettings();
            available = actual.majorVersion > 3 || (actual.majorVersion == 3 && actual.minorVersion >= 3);
        }
    };

    inline GLEnvironment& environment()
    {
        static GLEnvironment instance;
        return instance;
    }

    inline const k3::GLLoader LOADER = [](const char* name) { return sf::Context::getFunction(name); };

    // False when the test cannot run. That is a failure only when K3_REQUIRE_GL is set (CI).
    inline bool openGLAvailable()
    {
        if (environment().available)
            return true;
        if (std::getenv("K3_REQUIRE_GL")) {
            k3test::fail(__FILE__, __LINE__, "no OpenGL 3.3 core context (K3_REQUIRE_GL is set)");
            return false;
        }
        std::cout << "    skipped: no OpenGL 3.3 core context" << std::endl;
        return false;
    }

    inline std::unique_ptr<k3::IRasterizer> hardware(std::uint32_t width, std::uint32_t height)
    {
        auto rasterizer = k3::createRasterizer(k3::Backend::OpenGL, {.width = width, .height = height, .glLoader = LOADER});

        if (!rasterizer) {
            std::cerr << "    " << rasterizer.error() << std::endl;
            return nullptr;
        }
        return std::move(*rasterizer);
    }

    inline std::shared_ptr<k3::Mesh> sphere(unsigned rings, unsigned segments)
    {
        auto mesh = std::make_shared<k3::Mesh>();
        const float pi = std::numbers::pi_v<float>;

        for (unsigned r = 0; r <= rings; ++r) {
            for (unsigned s = 0; s <= segments; ++s) {
                const float theta = pi * r / rings, phi = 2.f * pi * s / segments;
                const k3::Math::Vector3f n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
                mesh->positions.push_back(n);
                mesh->normals.push_back(n);
                mesh->uvs.push_back({static_cast<float>(s) / segments * 4.f, static_cast<float>(r) / rings * 2.f});
            }
        }
        for (unsigned r = 0; r < rings; ++r) {
            for (unsigned s = 0; s < segments; ++s) {
                const std::uint32_t a = r * (segments + 1) + s, b = a + segments + 1;
                mesh->indices.insert(mesh->indices.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
        }
        return mesh;
    }

    inline std::shared_ptr<k3::Mesh> quad(float half, float y)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-half, y, half}, {half, y, half}, {half, y, -half}, {-half, y, -half}},
            .normals = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
            .uvs = {{0.f, half}, {half, half}, {half, 0.f}, {0.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    inline std::shared_ptr<k3::Texture> checker(std::uint8_t dark, std::uint8_t light)
    {
        k3::Image image(64, 64);

        for (std::uint32_t y = 0; y < 64; ++y)
            for (std::uint32_t x = 0; x < 64; ++x)
                for (int c = 0; c < 3; ++c)
                    image.texel(x, y)[c] = ((x / 8 + y / 8) % 2) ? light : dark;
        return std::make_shared<k3::Texture>(image);
    }

    // Everything at once: textures, normal map, every light type, shadows, alpha test, double-sided blending.
    inline void drawScene(k3::IRasterizer& rasterizer)
    {
        k3::Camera camera;
        camera.position = {0.f, 1.5f, 4.f};
        camera.lookAt({0.f, 0.f, 0.f});

        k3::Environment environment;
        environment.ambient = {0.1f, 0.1f, 0.12f, 1.f};
        environment.lights = {
            k3::Light::directional({-0.4f, -1.f, -0.5f}, {1.f, 0.95f, 0.9f, 1.f}, 1.4f),
            k3::Light::point({1.5f, 0.8f, 1.f}, {1.f, 0.5f, 0.2f, 1.f}, 5.f, 6.f),
            k3::Light::spot({-2.f, 2.f, 1.f}, {1.f, -1.f, -0.5f}, {0.3f, 0.5f, 1.f, 1.f}, 8.f, 8.f, 0.3f, 0.45f),
        };
        environment.lights[0].castShadows = true;

        auto floor = std::make_shared<k3::Material>();
        floor->diffuseMap = checker(90, 200);
        auto shiny = std::make_shared<k3::Material>();
        shiny->diffuseMap = checker(40, 230);
        shiny->normalMap = k3::Texture::normalMapFromHeight(checker(0, 255)->image(), 2.f);
        shiny->specular = {0.6f, 0.6f, 0.6f, 1.f};
        shiny->shininess = 48.f;
        auto bumpySphere = sphere(32, 64);
        bumpySphere->computeTangents();
        auto glass = std::make_shared<k3::Material>();
        glass->diffuse = {0.3f, 0.6f, 1.f, 0.35f};
        glass->specular = {0.8f, 0.8f, 0.8f, 1.f};
        glass->alphaMode = k3::AlphaMode::Blend;
        glass->doubleSided = true;

        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(quad(4.f, -1.f), floor, k3::Math::Matrix4::identity());
        rasterizer.draw(bumpySphere, shiny, k3::Math::Matrix4::translate({-0.6f, -0.2f, 0.f}) * k3::Math::Matrix4::scale(0.8f));
        rasterizer.draw(sphere(16, 32), glass, k3::Math::Matrix4::translate({0.9f, 0.f, 0.6f}) * k3::Math::Matrix4::scale(0.6f));
        rasterizer.endFrame();
    }

}
