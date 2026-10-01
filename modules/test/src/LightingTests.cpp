#include "Test.hpp"
#include "render/software/SoftwareRasterizer.hpp"

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

    // Quad in the z = 0 plane, normal +z (toward the camera).
    std::shared_ptr<k3::Mesh> quad()
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
            .normals = {{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    k3::Environment darkEnvironment()
    {
        k3::Environment environment;
        environment.clearColor = k3::Math::Color::Black;
        environment.ambient = k3::Math::Color::Black;
        return environment;
    }

    // Renders the quad with `material` and returns the linear color of the center pixel.
    k3::Math::Color render(const k3::Environment& environment, std::shared_ptr<k3::Material> material = std::make_shared<k3::Material>(), std::shared_ptr<k3::Mesh> mesh = quad())
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.beginFrame(orthoCamera(), environment);
        rasterizer.draw(mesh, material, k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer()[SIZE / 2 * SIZE + SIZE / 2];
    }

}

K3_TEST(lighting_ambient_only)
{
    auto environment = darkEnvironment();
    environment.ambient = {0.25f, 0.25f, 0.25f, 1.f};

    K3_CHECK_NEAR(render(environment).r, 0.25f, 1e-5f);
}

K3_TEST(lighting_directional_follows_lambert)
{
    auto environment = darkEnvironment();

    environment.lights = {k3::Light::directional({0.f, 0.f, -1.f})};
    K3_CHECK_NEAR(render(environment).r, 1.f, 1e-5f);

    // 60 degrees off the normal: cos = 0.5
    environment.lights = {k3::Light::directional({0.f, -0.866025f, -0.5f}, k3::Math::Color::White, 2.f)};
    K3_CHECK_NEAR(render(environment).g, 1.f, 1e-4f);

    // From behind: nothing.
    environment.lights = {k3::Light::directional({0.f, 0.f, 1.f})};
    K3_CHECK(render(environment).r == 0.f);
}

K3_TEST(lighting_point_light_fades_out_at_range)
{
    auto environment = darkEnvironment();

    // The center pixel of a 16x16 view of [-1, 1]^2 is at (1/16, -1/16, 0).
    const k3::Math::Vector3f pixel{0.0625f, -0.0625f, 0.f};
    const k3::Math::Vector3f light{0.f, 0.f, 1.f};
    const float d = k3::Math::Vector3f::length(light - pixel);
    const float window = 1.f - std::pow(d / 10.f, 4.f);
    const float expected = window * window / (d * d + 1.f) * (1.f / d);    // * N.L

    environment.lights = {k3::Light::point(light, k3::Math::Color::White, 1.f, 10.f)};
    K3_CHECK_NEAR(render(environment).r, expected, 1e-5f);

    environment.lights = {k3::Light::point({0.f, 0.f, 1.f}, k3::Math::Color::White, 1.f, 0.9f)};
    K3_CHECK(render(environment).r == 0.f);
}

K3_TEST(lighting_spot_cone)
{
    auto environment = darkEnvironment();

    // Pointing at the quad: center inside the inner cone.
    environment.lights = {k3::Light::spot({0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}, k3::Math::Color::White, 2.f, 10.f, 0.2f, 0.3f)};
    K3_CHECK(render(environment).r > 0.9f);

    // Pointing away from the center: outside the outer cone.
    environment.lights = {k3::Light::spot({0.f, 0.f, 1.f}, {1.f, 0.f, -1.f}, k3::Math::Color::White, 2.f, 10.f, 0.2f, 0.3f)};
    K3_CHECK(render(environment).r == 0.f);
}

K3_TEST(lighting_specular_highlight_adds_to_diffuse)
{
    auto environment = darkEnvironment();
    auto material = std::make_shared<k3::Material>();

    environment.lights = {k3::Light::directional({0.f, 0.f, -1.f})};
    material->diffuse = {0.5f, 0.5f, 0.5f, 1.f};
    material->specular = {0.25f, 0.25f, 0.25f, 1.f};
    // Light, normal and view aligned: N.H = 1, so the full specular is added.
    K3_CHECK_NEAR(render(environment, material).r, 0.75f, 1e-5f);
}

K3_TEST(lighting_emissive_and_unlit)
{
    auto environment = darkEnvironment();
    auto material = std::make_shared<k3::Material>();

    material->emissive = {0.f, 0.3f, 0.f, 1.f};
    K3_CHECK_NEAR(render(environment, material).g, 0.3f, 1e-6f);

    material->unlit = true;
    material->diffuse = {0.4f, 0.4f, 0.4f, 1.f};
    K3_CHECK_NEAR(render(environment, material).r, 0.4f, 1e-6f);
}

K3_TEST(lighting_double_sided_back_face_is_lit_from_behind)
{
    auto environment = darkEnvironment();
    auto material = std::make_shared<k3::Material>();
    auto back = quad();

    // Turn the quad around: its normal now points away from the camera.
    std::swap(back->indices[1], back->indices[2]);
    std::swap(back->indices[4], back->indices[5]);
    for (auto& n : back->normals)
        n = {0.f, 0.f, -1.f};

    material->doubleSided = true;
    environment.lights = {k3::Light::directional({0.f, 0.f, -1.f})};
    K3_CHECK_NEAR(render(environment, material, back).r, 1.f, 1e-5f);
}

K3_TEST(lighting_missing_normals_use_the_face_normal)
{
    auto environment = darkEnvironment();
    auto mesh = quad();

    mesh->normals.clear();
    environment.lights = {k3::Light::directional({0.f, 0.f, -1.f})};
    K3_CHECK_NEAR(render(environment, std::make_shared<k3::Material>(), mesh).r, 1.f, 1e-5f);
}
