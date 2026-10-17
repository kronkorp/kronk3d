#include "Test.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
#include <cmath>

namespace
{

    constexpr std::uint32_t SIZE = 16;

    // Orthographic camera seeing exactly the [-1, 1] square of the z = 0 plane.
    k3::Camera orthoCamera()
    {
        k3::Camera camera;

        camera.position = {0.f, 0.f, 1.f};
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 2.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 10.f;
        return camera;
    }

    k3::Environment blackEnvironment()
    {
        k3::Environment environment;

        environment.clearColor = k3::Math::Color::Black;
        return environment;
    }

    std::shared_ptr<k3::Mesh> quad(float z, float half = 1.f, float x = 0.f)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{x - half, -half, z}, {x + half, -half, z}, {x + half, half, z}, {x - half, half, z}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::shared_ptr<k3::Material> solid(const k3::Math::Color& color)
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = color;
        material->unlit = true;
        return material;
    }

    const k3::Math::Color& at(const k3::SoftwareRasterizer& rasterizer, std::uint32_t x, std::uint32_t y, std::uint32_t width = SIZE)
    {
        return rasterizer.colorBuffer()[y * width + x];
    }

}

K3_TEST(layer_is_drawn_over_nearer_geometry)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    // Without a layer the near red quad hides the far green one.
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    rasterizer.draw(quad(-5.f), solid(k3::Math::Color::Green), k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(at(rasterizer, 8, 8).r == 1.f && at(rasterizer, 8, 8).g == 0.f);

    // In a layer, it is drawn over whatever is below, whatever its depth.
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    rasterizer.beginLayer(orthoCamera());
    rasterizer.draw(quad(-5.f), solid(k3::Math::Color::Green), k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(at(rasterizer, 8, 8).g == 1.f && at(rasterizer, 8, 8).r == 0.f);
}

K3_TEST(layer_has_its_own_field_of_view)
{
    constexpr std::uint32_t BIG = 32;
    auto render = [&](float layerFov) {
        k3::SoftwareRasterizer rasterizer(BIG, BIG);
        k3::Camera camera;
        k3::Camera layer;

        camera.position = layer.position = {0.f, 0.f, 0.f};
        camera.fovY = 1.0471976f;               // 60 degrees
        layer.fovY = layerFov;
        rasterizer.beginFrame(camera, blackEnvironment());
        rasterizer.beginLayer(layer);
        rasterizer.draw(quad(-5.f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());     // 2 x 2 at 5 units
        rasterizer.endFrame();
        return at(rasterizer, 24, 16, BIG).r;       // Half way between the center and the right edge
    };

    K3_CHECK(render(0.5235988f) == 1.f);            // 30 degrees: the quad fills three quarters of the view
    K3_CHECK(render(1.0471976f) == 0.f);            // 60 degrees: a third
}

K3_TEST(layer_receives_shadows_but_does_not_cast_them)
{
    // Looking down at a floor lit by a sun shining at 45 degrees: something 1 unit above the floor shades the
    // floor 1 unit further along +x, but not when it is in a layer.
    constexpr float SUN = 0.70710678f;
    constexpr std::size_t SHADOWED = SIZE / 2 + 5;      // x = +1.25 on a 4-unit wide view
    auto topCamera = [] {
        k3::Camera camera;

        camera.position = {0.f, 5.f, 0.f};
        camera.pitch = -1.5707963f;
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 4.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 10.f;
        return camera;
    };
    auto plane = [](float half, float y) {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-half, y, half}, {half, y, half}, {half, y, -half}, {-half, y, -half}},
            .normals = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    };
    auto render = [&](bool occluderInLayer) {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
        k3::Environment environment;

        environment.ambient = k3::Math::Color::Black;
        environment.lights = {k3::Light::directional({1.f, -1.f, 0.f})};
        environment.lights[0].castShadows = true;
        rasterizer.beginFrame(topCamera(), environment);
        rasterizer.draw(plane(2.f, 0.f), std::make_shared<k3::Material>(), k3::Math::Matrix4::identity());
        if (occluderInLayer)
            rasterizer.beginLayer(topCamera());
        rasterizer.draw(plane(0.5f, 1.f), std::make_shared<k3::Material>(), k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer()[SIZE / 2 * SIZE + SHADOWED].r;
    };

    K3_CHECK(render(false) == 0.f);                     // Control: the occluder shades the floor
    K3_CHECK_NEAR(render(true), SUN, 1e-4f);            // In a layer it casts nothing
}

K3_TEST(layer_is_culled_with_its_own_camera)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    auto shifted = orthoCamera();

    shifted.position.x = 10.f;
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.beginLayer(shifted);
    rasterizer.draw(quad(0.f, 1.f, 10.f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());     // Far to the right, in front of the layer's camera
    rasterizer.endFrame();

    K3_CHECK(rasterizer.stats().culledDraws == 0);
    K3_CHECK(at(rasterizer, 8, 8).r == 1.f);
}

K3_TEST(layers_stack_over_the_sky_and_are_forgotten_after_the_frame)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    k3::Environment environment = blackEnvironment();

    environment.skybox = k3::Skybox::gradient(k3::Math::Color::Blue, k3::Math::Color::Blue, k3::Math::Color::Blue, 8);
    rasterizer.beginFrame(orthoCamera(), environment);
    rasterizer.draw(quad(0.f, 0.5f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());              // Center, 8 x 8 pixels
    rasterizer.beginLayer(orthoCamera());
    rasterizer.draw(quad(-1.f, 0.25f), solid(k3::Math::Color::Green), k3::Math::Matrix4::identity());          // Smaller, on top
    rasterizer.beginLayer(orthoCamera());                                                                       // An empty layer changes nothing
    rasterizer.endFrame();

    K3_CHECK(at(rasterizer, 8, 8).g == 1.f);                                        // Layer
    K3_CHECK(at(rasterizer, 5, 8).r == 1.f && at(rasterizer, 5, 8).g == 0.f);       // First layer
    K3_CHECK(at(rasterizer, 1, 1).b > 0.9f && at(rasterizer, 1, 1).r < 0.1f);       // Sky: only behind the first layer

    // The next frame starts with a single layer again.
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    rasterizer.draw(quad(-5.f), solid(k3::Math::Color::Green), k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(at(rasterizer, 8, 8).r == 1.f && at(rasterizer, 8, 8).g == 0.f);
}
