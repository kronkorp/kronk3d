#include "Test.hpp"
#include "render/software/SoftwareRasterizer.hpp"
#include "render/software/ThreadPool.hpp"
#include <atomic>
#include <cmath>
#include <numbers>
#include <vector>

namespace
{

    // UV sphere: many small triangles crossing tile borders, with normals and uvs.
    std::shared_ptr<k3::Mesh> sphere(unsigned rings, unsigned segments)
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
                mesh->indices.insert(mesh->indices.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        }
        return mesh;
    }

    std::vector<std::uint8_t> render(unsigned threads, k3::AntiAliasing antiAliasing = k3::AntiAliasing::None)
    {
        k3::Image checker(8, 8);
        for (std::uint32_t y = 0; y < 8; ++y)
            for (std::uint32_t x = 0; x < 8; ++x)
                checker.texel(x, y)[1] = ((x + y) % 2) ? 255 : 30;

        auto opaque = std::make_shared<k3::Material>();
        opaque->diffuseMap = std::make_shared<k3::Texture>(checker);
        opaque->specular = {0.5f, 0.5f, 0.5f, 1.f};
        auto glass = std::make_shared<k3::Material>();
        glass->diffuse = {1.f, 0.2f, 0.2f, 0.4f};
        glass->alphaMode = k3::AlphaMode::Blend;
        glass->doubleSided = true;

        k3::Camera camera;
        camera.position = {0.f, 0.5f, 3.f};
        camera.lookAt({0.f, 0.f, 0.f});
        k3::Environment environment;
        environment.lights = {k3::Light::directional({-1.f, -1.f, -1.f}), k3::Light::point({1.f, 1.f, 1.f}, k3::Math::Color::White, 3.f)};
        environment.lights[0].castShadows = true;
        environment.shadows.resolution = 512;

        k3::SoftwareRasterizer rasterizer(320, 240, threads);
        rasterizer.setAntiAliasing(antiAliasing);
        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(sphere(48, 96), opaque, k3::Math::Matrix4::scale(0.6f));
        rasterizer.draw(sphere(24, 48), glass, k3::Math::Matrix4::translate({0.4f, 0.f, 0.6f}) * k3::Math::Matrix4::scale(0.5f));
        rasterizer.draw(sphere(24, 48), glass, k3::Math::Matrix4::translate({-0.5f, 0.2f, 0.8f}) * k3::Math::Matrix4::scale(0.4f));
        rasterizer.endFrame();
        return rasterizer.readPixels().pixels;
    }

}

K3_TEST(thread_pool_runs_every_index_once)
{
    k3::sw::ThreadPool pool(4);
    std::vector<std::atomic<int>> hits(10000);

    for (int round = 0; round < 3; ++round)
        pool.parallelFor(hits.size(), [&](std::size_t i) { hits[i].fetch_add(1); });

    for (const auto& hit : hits)
        K3_REQUIRE(hit.load() == 3);
    K3_CHECK(pool.size() == 4);
    K3_CHECK(k3::sw::ThreadPool(0).size() >= 1);
}

K3_TEST(software_output_does_not_depend_on_thread_count)
{
    for (auto mode : {k3::AntiAliasing::None, k3::AntiAliasing::FXAA, k3::AntiAliasing::SSAA}) {
        const auto reference = render(1, mode);

        K3_CHECK(render(2, mode) == reference);
        K3_CHECK(render(7, mode) == reference);
    }
}
