#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_skips_draws_outside_the_view)
{
    if (!openGLAvailable())
        return;

    auto gpu = hardware(4, 4);
    K3_REQUIRE(gpu);

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    auto material = std::make_shared<k3::Material>();
    material->diffuse = k3::Math::Color::Red;
    material->unlit = true;
    k3::Environment black;
    black.clearColor = k3::Math::Color::Black;
    auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });

    gpu->beginFrame(camera, black);
    gpu->draw(mesh, material, k3::Math::Matrix4::identity());
    gpu->draw(mesh, material, k3::Math::Matrix4::translate({10.f, 0.f, 0.f}));
    gpu->endFrame();

    K3_CHECK(gpu->stats().drawCalls == 2 && gpu->stats().culledDraws == 1);
    K3_CHECK(gpu->readPixels().texel(2, 2)[0] == 255);
}
