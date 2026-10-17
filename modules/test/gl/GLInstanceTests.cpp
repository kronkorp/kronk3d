#include "GLFixture.hpp"

using namespace k3gl;

namespace
{

    struct Difference
    {
        double mean = 0.0;
        int worst = 0;
        double over16 = 0.0;
    };

    Difference compare(const k3::Image& a, const k3::Image& b)
    {
        Difference d;
        std::size_t over = 0;

        for (std::size_t i = 0; i < a.pixels.size(); i += 4) {
            int worst = 0;
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(int(a.pixels[i + c]) - int(b.pixels[i + c])));
            d.mean += worst;
            d.worst = std::max(d.worst, worst);
            over += worst > 16;
        }
        const double pixels = static_cast<double>(a.pixels.size() / 4);
        d.mean /= pixels;
        d.over16 = static_cast<double>(over) / pixels;
        return d;
    }

}

K3_TEST(gl_instances_match_single_draws_and_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 200, H = 150;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    auto floor = std::make_shared<k3::Material>();
    floor->diffuseMap = checker(90, 200);
    auto stone = std::make_shared<k3::Material>();
    stone->diffuseMap = checker(40, 230);
    stone->specular = {0.5f, 0.5f, 0.5f, 1.f};
    auto ball = sphere(12, 24);

    // A grid of balls, some behind the camera or far to the side (culled one by one), with a few of different sizes.
    std::vector<k3::Math::Matrix4> places;
    for (int i = 0; i < 60; ++i) {
        const float x = static_cast<float>(i % 10) * 1.4f - 6.3f, z = static_cast<float>(i / 10) * 1.6f - 6.f;
        const float size = 0.35f + 0.1f * static_cast<float>(i % 3);

        places.push_back(k3::Math::Matrix4::translate({x, -1.f + size, z}) * k3::Math::Matrix4::rotateZX(0.3f * static_cast<float>(i)) * k3::Math::Matrix4::scale(size));
    }
    places.push_back(k3::Math::Matrix4::translate({40.f, 0.f, 0.f}));         // Far outside the view
    places.push_back(k3::Math::Matrix4::translate({0.f, 0.f, 50.f}));         // Behind the camera

    auto draw = [&](k3::IRasterizer& rasterizer, bool instanced) {
        k3::Camera camera;
        camera.position = {0.f, 3.f, 9.f};
        camera.lookAt({0.f, -1.f, 0.f});

        k3::Environment environment;
        environment.ambient = {0.12f, 0.12f, 0.15f, 1.f};
        environment.lights = {k3::Light::directional({-0.4f, -1.f, -0.5f}), k3::Light::point({2.f, 2.f, 1.f}, {1.f, 0.6f, 0.3f, 1.f}, 6.f, 9.f)};
        environment.lights[0].castShadows = true;

        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(quad(10.f, -1.f), floor, k3::Math::Matrix4::identity());
        if (instanced) {
            rasterizer.drawInstances(ball, stone, places);
        } else {
            for (const auto& place : places)
                rasterizer.draw(ball, stone, place);
        }
        // The same mesh drawn on its own afterwards: the instanced path must leave the mesh's state as it was.
        rasterizer.draw(ball, stone, k3::Math::Matrix4::translate({0.f, 1.f, 3.f}));
        rasterizer.endFrame();
    };

    draw(*gpu, true);
    const auto instanced = gpu->readPixels();
    const std::size_t culled = gpu->stats().culledDraws;
    K3_CHECK(gpu->stats().drawCalls == 64);                    // The floor, 62 instances and the single ball: counted one by one
    K3_CHECK(culled >= 2);                                      // The two instances out of view, at least

    draw(*gpu, false);
    const auto single = gpu->readPixels();
    draw(cpu, true);
    const auto software = cpu.readPixels();

    const Difference vsSingle = compare(instanced, single);
    const Difference vsSoftware = compare(instanced, software);
    std::cout << "    instances vs single draws: worst " << vsSingle.worst << ", mean " << vsSingle.mean << "; vs software: mean " << vsSoftware.mean
              << ", > 16: " << 100.0 * vsSoftware.over16 << "%" << std::endl;

    K3_CHECK(vsSingle.worst <= 2);                              // Same pixels as one draw each (the normal matrix is computed on the GPU)
    K3_CHECK(vsSoftware.mean < 1.0);
    K3_CHECK(vsSoftware.over16 < 0.01);
}

K3_TEST(gl_instances_fall_back_for_mirrored_and_blended_draws)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 120, H = 90;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);

    auto ball = sphere(10, 20);
    auto opaque = std::make_shared<k3::Material>();
    auto glass = std::make_shared<k3::Material>();
    glass->diffuse = {0.3f, 0.6f, 1.f, 0.4f};
    glass->alphaMode = k3::AlphaMode::Blend;

    // Mirrored (negative scale on x) and non-mirrored placements together; and overlapping blended ones, which
    // must be sorted one by one.
    const std::vector<k3::Math::Matrix4> mixed = {
        k3::Math::Matrix4::translate({-1.5f, 0.f, 0.f}),
        k3::Math::Matrix4::translate({1.5f, 0.f, 0.f}) * k3::Math::Matrix4::scale({-1.f, 1.f, 1.f}),
    };
    const std::vector<k3::Math::Matrix4> overlapping = {
        k3::Math::Matrix4::translate({0.f, 0.f, 0.f}), k3::Math::Matrix4::translate({0.3f, 0.2f, 1.f}), k3::Math::Matrix4::translate({-0.3f, -0.2f, -1.f}),
    };

    auto draw = [&](bool instanced) {
        k3::Camera camera;
        camera.position = {0.f, 0.f, 7.f};
        camera.lookAt({0.f, 0.f, 0.f});
        k3::Environment environment;
        environment.lights = {k3::Light::directional({-0.4f, -1.f, -0.5f})};

        gpu->beginFrame(camera, environment);
        if (instanced) {
            gpu->drawInstances(ball, opaque, mixed);
            gpu->drawInstances(ball, glass, overlapping);
        } else {
            for (const auto& place : mixed)
                gpu->draw(ball, opaque, place);
            for (const auto& place : overlapping)
                gpu->draw(ball, glass, place);
        }
        gpu->endFrame();
        return gpu->readPixels();
    };
    const auto instanced = draw(true);
    const auto single = draw(false);

    K3_CHECK(compare(instanced, single).worst == 0);            // Both go through the plain path: identical
}
