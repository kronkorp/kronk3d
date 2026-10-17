#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_texture_changes_are_uploaded)
{
    if (!openGLAvailable())
        return;

    auto gpu = hardware(4, 4);
    K3_REQUIRE(gpu);

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    auto texture = std::make_shared<k3::Texture>(k3::Image(1, 1, k3::Math::Color::Red));
    auto material = std::make_shared<k3::Material>();
    material->unlit = true;
    material->diffuseMap = texture;
    auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
        .uvs = {{0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {0.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });

    auto render = [&] {
        gpu->beginFrame(camera, k3::Environment{});
        gpu->draw(mesh, material, k3::Math::Matrix4::identity());
        gpu->endFrame();
        return gpu->readPixels();
    };

    K3_CHECK(render().texel(2, 2)[0] == 255);
    texture->setImage(k3::Image(1, 1, k3::Math::Color::Green));
    const k3::Image image = render();
    K3_CHECK(image.texel(2, 2)[0] == 0 && image.texel(2, 2)[1] == 255);
}

K3_TEST(gl_mesh_edits_are_uploaded)
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

    auto render = [&] {
        gpu->beginFrame(camera, black);
        gpu->draw(mesh, material, k3::Math::Matrix4::identity());
        gpu->endFrame();
        return gpu->readPixels();
    };
    auto shift = [&](float x) {
        for (auto& position : mesh->positions)
            position.x += x;
        ++mesh->revision;
    };

    K3_CHECK(render().texel(2, 2)[0] == 255);
    shift(10.f);                                        // Out of view
    K3_CHECK(render().texel(2, 2)[0] == 0);
    shift(-10.f);
    K3_CHECK(render().texel(2, 2)[0] == 255);
}
