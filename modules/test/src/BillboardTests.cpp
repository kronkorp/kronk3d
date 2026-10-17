#include "Test.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
#include <kronk3d/scene/BillboardBatch.hpp>
#include <cmath>

namespace
{

    constexpr std::uint32_t SIZE = 32;

    // Looks at the origin from 5 units away, along (yaw, pitch).
    k3::Camera lookingAtOrigin(float yaw, float pitch)
    {
        k3::Camera camera;

        camera.yaw = yaw;
        camera.pitch = pitch;
        camera.position = -5.f * camera.forward();
        camera.nearPlane = 0.1f;
        camera.farPlane = 50.f;
        return camera;
    }

    k3::Environment blackEnvironment()
    {
        k3::Environment environment;

        environment.clearColor = k3::Math::Color::Black;
        return environment;
    }

    std::shared_ptr<k3::Material> unlit(const k3::Math::Color& color = k3::Math::Color::White, k3::AlphaMode mode = k3::AlphaMode::Opaque)
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = color;
        material->unlit = true;
        material->alphaMode = mode;
        return material;
    }

    using Frame = std::vector<k3::Math::Color>;

    Frame render(const k3::Camera& camera, k3::BillboardBatch& batch, const std::shared_ptr<k3::Material>& material, k3::Environment environment = blackEnvironment())
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.beginFrame(camera, environment);
        batch.draw(rasterizer, camera, material);
        rasterizer.endFrame();
        return rasterizer.colorBuffer();
    }

    std::size_t covered(const Frame& frame)
    {
        std::size_t count = 0;

        for (const auto& pixel : frame)
            count += pixel.r > 0.5f || pixel.g > 0.5f || pixel.b > 0.5f;
        return count;
    }

    const k3::Math::Color& at(const Frame& frame, int x, int y)
    {
        return frame[static_cast<std::size_t>(y) * SIZE + static_cast<std::size_t>(x)];
    }

}

K3_TEST(billboard_faces_the_camera_from_any_side)
{
    // 2 x 2 units seen from 5 units with a 60 degree field of view: 12 pixels a side out of 32.
    for (const auto& [yaw, pitch] : {std::pair{0.f, 0.f}, std::pair{0.9f, 0.4f}, std::pair{-2.2f, -0.7f}, std::pair{3.f, 1.2f}}) {
        k3::BillboardBatch batch;

        batch.add({.size = {2.f, 2.f}});

        const auto rasterizer = render(lookingAtOrigin(yaw, pitch), batch, unlit());

        K3_CHECK(covered(rasterizer) >= 128 && covered(rasterizer) <= 160);
        K3_CHECK(at(rasterizer, 16, 16).r > 0.99f);
    }
}

K3_TEST(billboard_rotation_turns_the_quad_around_the_view_axis)
{
    auto cornerAndSide = [](float rotation) {
        k3::BillboardBatch batch;

        batch.add({.size = {2.f, 2.f}, .rotation = rotation});

        const auto rasterizer = render(lookingAtOrigin(0.f, 0.f), batch, unlit());

        // The square reaches 5.5 pixels from the center: near its corner (4.5, 4.5) is in, (6.5, 0.5) is out.
        // Turned by 45 degrees it is a diamond reaching 7.8 pixels along the axes: (6.5, 0.5) is in, (4.5, 4.5) is out.
        return std::pair{at(rasterizer, 16 + 4, 16 + 4).r > 0.5f, at(rasterizer, 16 + 6, 16).r > 0.5f};
    };

    K3_CHECK(cornerAndSide(0.f) == std::pair(true, false));
    K3_CHECK(cornerAndSide(0.78539816f) == std::pair(false, true));
}

K3_TEST(billboard_color_and_alpha_come_from_each_quad)
{
    k3::BillboardBatch batch;

    batch.add({.position = {-1.5f, 0.f, 0.f}, .size = {1.5f, 1.5f}, .color = k3::Math::Color::Red});
    batch.add({.position = {1.5f, 0.f, 0.f}, .size = {1.5f, 1.5f}, .color = {0.f, 1.f, 0.f, 0.5f}});

    const auto rasterizer = render(lookingAtOrigin(0.f, 0.f), batch, unlit(k3::Math::Color::White, k3::AlphaMode::Blend));
    const auto& left = at(rasterizer, 16 - 8, 16);
    const auto& right = at(rasterizer, 16 + 8, 16);

    K3_CHECK_NEAR(left.r, 1.f, 1e-5f);              // Opaque red
    K3_CHECK_NEAR(left.g, 0.f, 1e-5f);
    K3_CHECK_NEAR(right.g, 0.5f, 1e-5f);            // Half green over black
    K3_CHECK_NEAR(right.r, 0.f, 1e-5f);
}

K3_TEST(billboard_uv_picks_a_cell_of_the_texture)
{
    k3::Image image(2, 1);

    image.texel(0, 0)[0] = 255;
    image.texel(0, 0)[3] = 255;
    image.texel(1, 0)[1] = 255;
    image.texel(1, 0)[3] = 255;

    auto material = std::make_shared<k3::Material>();
    material->unlit = true;
    material->diffuseMap = std::make_shared<k3::Texture>(image, k3::ColorSpace::Linear);
    material->diffuseMap->sampler.filter = k3::TextureFilter::Nearest;

    auto cell = [&](float u0, float u1) {
        k3::BillboardBatch batch;

        batch.add({.size = {2.f, 2.f}, .uv = {u0, 0.f, u1, 1.f}});
        return at(render(lookingAtOrigin(0.f, 0.f), batch, material), 16, 16);
    };

    K3_CHECK(cell(0.f, 0.5f).r > 0.99f && cell(0.f, 0.5f).g < 0.01f);          // Left cell: red
    K3_CHECK(cell(0.5f, 1.f).g > 0.99f && cell(0.5f, 1.f).r < 0.01f);          // Right cell: green

    // The whole texture, flipped by swapping u: green on the left.
    k3::BillboardBatch flipped;
    flipped.add({.size = {2.f, 2.f}, .uv = {1.f, 0.f, 0.f, 1.f}});
    const auto rasterizer = render(lookingAtOrigin(0.f, 0.f), flipped, material);
    K3_CHECK(at(rasterizer, 16 - 3, 16).g > 0.99f && at(rasterizer, 16 + 3, 16).r > 0.99f);
}

K3_TEST(billboards_are_sorted_back_to_front_whatever_the_order_added)
{
    auto image = [](bool farFirst) {
        k3::BillboardBatch batch;
        const k3::Billboard far{.position = {0.f, 0.f, -1.f}, .size = {3.f, 3.f}, .color = {1.f, 0.f, 0.f, 0.5f}};     // Behind the origin, as seen from +z
        const k3::Billboard near{.position = {0.f, 0.f, 1.f}, .size = {3.f, 3.f}, .color = {0.f, 1.f, 0.f, 0.5f}};

        if (farFirst) {
            batch.add(far);
            batch.add(near);
        } else {
            batch.add(near);
            batch.add(far);
        }
        return at(render(lookingAtOrigin(0.f, 0.f), batch, unlit(k3::Math::Color::White, k3::AlphaMode::Blend)), 16, 16);
    };
    const auto a = image(true);
    const auto b = image(false);

    K3_CHECK_NEAR(a.r, b.r, 1e-6f);
    K3_CHECK_NEAR(a.g, b.g, 1e-6f);
    K3_CHECK_NEAR(a.r, 0.25f, 1e-5f);               // Red at half over black, then half green over it
    K3_CHECK_NEAR(a.g, 0.5f, 1e-5f);
}

K3_TEST(billboard_batch_is_cleared_and_reused_frame_after_frame)
{
    k3::BillboardBatch batch;
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    const auto camera = lookingAtOrigin(0.f, 0.f);
    auto frame = [&] {
        rasterizer.beginFrame(camera, blackEnvironment());
        batch.draw(rasterizer, camera, unlit());
        rasterizer.endFrame();
        return covered(rasterizer.colorBuffer());
    };

    K3_CHECK(batch.size() == 0 && frame() == 0);        // Nothing to draw

    for (int i = 0; i < 3; ++i)
        batch.add({.position = {-2.f + 2.f * static_cast<float>(i), 0.f, 0.f}, .size = {1.5f, 1.5f}});
    K3_CHECK(batch.size() == 3);
    const std::size_t three = frame();

    batch.clear();
    batch.add({.size = {1.5f, 1.5f}});
    const std::size_t one = frame();

    K3_CHECK(one > 20 && three > 2 * one);              // Each frame shows what was in the batch then
    batch.clear();
    K3_CHECK(frame() == 0);
}

K3_TEST(lit_billboards_face_the_light_that_comes_from_the_camera)
{
    k3::BillboardBatch batch;
    k3::Environment environment;
    auto material = std::make_shared<k3::Material>();           // Lit

    environment.ambient = k3::Math::Color::Black;
    environment.clearColor = k3::Math::Color::Black;
    environment.lights = {k3::Light::directional({0.f, 0.f, -1.f})};    // Shines toward -z: on the faces turned toward +z
    batch.add({.size = {2.f, 2.f}});

    const auto rasterizer = render(lookingAtOrigin(0.f, 0.f), batch, material, environment);

    K3_CHECK_NEAR(at(rasterizer, 16, 16).r, 1.f, 1e-4f);        // Full light: the normal faces the camera
}
