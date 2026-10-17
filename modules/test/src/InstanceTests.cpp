#include "Test.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>

namespace
{

    constexpr std::uint32_t SIZE = 16;

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

    std::shared_ptr<k3::Mesh> smallQuad()
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-0.2f, -0.2f, 0.f}, {0.2f, -0.2f, 0.f}, {0.2f, 0.2f, 0.f}, {-0.2f, 0.2f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::shared_ptr<k3::Material> red()
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = k3::Math::Color::Red;
        material->unlit = true;
        return material;
    }

}

K3_TEST(draw_instances_draws_the_mesh_at_every_place)
{
    const std::vector<k3::Math::Matrix4> places = {
        k3::Math::Matrix4::translate({-0.6f, 0.f, 0.f}), k3::Math::Matrix4::translate({0.6f, 0.f, 0.f}), k3::Math::Matrix4::translate({0.f, 0.6f, 0.f}),
    };
    k3::Environment environment;
    environment.clearColor = k3::Math::Color::Black;

    k3::SoftwareRasterizer instanced(SIZE, SIZE), single(SIZE, SIZE);

    instanced.beginFrame(orthoCamera(), environment);
    instanced.drawInstances(smallQuad(), red(), places);
    instanced.endFrame();

    single.beginFrame(orthoCamera(), environment);
    for (const auto& place : places)
        single.draw(smallQuad(), red(), place);
    single.endFrame();

    K3_CHECK(instanced.stats().drawCalls == 3 && instanced.stats().triangles == 6);
    K3_REQUIRE(instanced.colorBuffer().size() == single.colorBuffer().size());
    std::size_t red_pixels = 0;
    for (std::size_t i = 0; i < single.colorBuffer().size(); ++i) {
        K3_REQUIRE(instanced.colorBuffer()[i].r == single.colorBuffer()[i].r);
        red_pixels += single.colorBuffer()[i].r > 0.5f;
    }
    K3_CHECK(red_pixels > 15);
}

K3_TEST(draw_instances_ignores_an_empty_list_and_a_null_mesh)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), {});
    rasterizer.drawInstances(smallQuad(), red(), {});
    rasterizer.drawInstances(nullptr, red(), std::vector<k3::Math::Matrix4>{k3::Math::Matrix4::identity()});
    rasterizer.endFrame();

    K3_CHECK(rasterizer.stats().drawCalls == 0);
}
