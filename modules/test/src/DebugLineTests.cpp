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

    // World coordinates of the center of a pixel of the 16 x 16 view above.
    float worldX(float pixel) { return pixel / 8.f - 1.f; }
    float worldY(float pixel) { return 1.f - pixel / 8.f; }

    std::shared_ptr<k3::Mesh> wall(float z)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-2.f, -2.f, z}, {2.f, -2.f, z}, {2.f, 2.f, z}, {-2.f, 2.f, z}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::shared_ptr<k3::Material> green()
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = k3::Math::Color::Green;
        material->unlit = true;
        return material;
    }

    const k3::Math::Color& at(const k3::SoftwareRasterizer& rasterizer, std::uint32_t x, std::uint32_t y)
    {
        return rasterizer.colorBuffer()[y * SIZE + x];
    }

    bool isRed(const k3::Math::Color& c) { return c.r == 1.f && c.g == 0.f && c.b == 0.f; }

}

K3_TEST(debug_line_covers_the_pixels_between_its_ends_but_the_last)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.drawLine({worldX(2.5f), worldY(4.5f), 0.f}, {worldX(10.5f), worldY(4.5f), 0.f}, k3::Math::Color::Red);
    rasterizer.endFrame();

    for (std::uint32_t y = 0; y < SIZE; ++y)
        for (std::uint32_t x = 0; x < SIZE; ++x)
            K3_CHECK(isRed(at(rasterizer, x, y)) == (y == 4 && x >= 2 && x <= 9));
}

K3_TEST(debug_line_is_hidden_by_nearer_geometry_unless_it_ignores_depth)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    const k3::Math::Vector3f from{worldX(2.5f), worldY(4.5f), 0.f}, to{worldX(10.5f), worldY(4.5f), 0.f};

    // A wall between the camera (z = 1) and the line (z = 0).
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(wall(0.5f), green(), k3::Math::Matrix4::identity());
    rasterizer.drawLine(from, to, k3::Math::Color::Red);                                       // Depth tested
    rasterizer.drawLine({from.x, worldY(8.5f), 0.f}, {to.x, worldY(8.5f), 0.f}, k3::Math::Color::Red, false);
    rasterizer.drawLine({from.x, worldY(11.5f), 0.8f}, {to.x, worldY(11.5f), 0.8f}, k3::Math::Color::Red);       // In front of the wall
    rasterizer.endFrame();

    K3_CHECK(!isRed(at(rasterizer, 5, 4)) && at(rasterizer, 5, 4).g == 1.f);
    K3_CHECK(isRed(at(rasterizer, 5, 8)));
    K3_CHECK(isRed(at(rasterizer, 5, 11)));
}

K3_TEST(debug_line_blends_with_its_alpha)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.drawLine({worldX(2.5f), worldY(4.5f), 0.f}, {worldX(10.5f), worldY(4.5f), 0.f}, {1.f, 0.f, 0.f, 0.5f});
    rasterizer.endFrame();

    K3_CHECK_NEAR(at(rasterizer, 5, 4).r, 0.5f, 1e-6f);
}

K3_TEST(debug_line_is_clipped_to_the_view)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    k3::Camera camera;

    camera.position = {0.f, 0.f, 0.f};
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.f;

    auto count = [&](const k3::Math::Vector3f& from, const k3::Math::Vector3f& to, std::size_t& left) {
        rasterizer.beginFrame(camera, blackEnvironment());
        rasterizer.drawLine(from, to, k3::Math::Color::Red);
        rasterizer.endFrame();

        std::size_t red = 0;
        left = 0;
        for (std::uint32_t y = 0; y < SIZE; ++y) {
            for (std::uint32_t x = 0; x < SIZE; ++x) {
                red += isRed(at(rasterizer, x, y));
                left += isRed(at(rasterizer, x, y)) && x < SIZE / 2;
            }
        }
        return red;
    };
    std::size_t left = 0;

    // From in front of the camera to far behind it, off the axis: the part behind the near plane is cut,
    // and what is left runs from the projected start to the edge of the view.
    K3_CHECK(count({1.f, 0.f, -5.f}, {1.f, 0.f, 50.f}, left) > 3 && left == 0);
    K3_CHECK(count({1.f, 0.f, 5.f}, {1.f, 0.f, 50.f}, left) == 0);                    // Entirely behind
    K3_CHECK(count({-50.f, 3.f, -5.f}, {-40.f, 3.f, -5.f}, left) == 0);               // Entirely to the left of the view
    K3_CHECK(count({-500.f, 0.f, -5.f}, {500.f, 0.f, -5.f}, left) > 10);              // Crossing the whole view, far longer than it
}

K3_TEST(debug_bounds_draws_the_edges_of_a_box)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.drawBounds({{-0.55f, -0.55f, -0.5f}, {0.55f, 0.55f, 0.5f}}, k3::Math::Color::Red);
    rasterizer.drawBounds(k3::Math::Bounds3f{}, k3::Math::Color::Green);                // Empty: nothing
    rasterizer.endFrame();

    std::size_t red = 0;
    for (const auto& pixel : rasterizer.colorBuffer()) {
        red += isRed(pixel);
        K3_CHECK(pixel.g == 0.f);
    }
    K3_CHECK(red >= 25 && red <= 44);                       // A square outline of about 9 pixels a side
    K3_CHECK(!isRed(at(rasterizer, 8, 8)));                 // Hollow
    K3_CHECK(isRed(at(rasterizer, 8, 3)) || isRed(at(rasterizer, 8, 4)));     // Top edge
}

K3_TEST(debug_line_belongs_to_its_layer_and_the_frame)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    const k3::Math::Vector3f from{worldX(2.5f), worldY(4.5f), 0.f}, to{worldX(10.5f), worldY(4.5f), 0.f};

    // A line recorded in the first layer is hidden by the first layer's wall, one in the second layer is not.
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(wall(0.5f), green(), k3::Math::Matrix4::identity());
    rasterizer.drawLine(from, to, k3::Math::Color::Red);
    rasterizer.beginLayer(orthoCamera());
    rasterizer.drawLine({from.x, worldY(8.5f), 0.f}, {to.x, worldY(8.5f), 0.f}, k3::Math::Color::Red);
    rasterizer.endFrame();
    K3_CHECK(!isRed(at(rasterizer, 5, 4)));
    K3_CHECK(isRed(at(rasterizer, 5, 8)));

    // The next frame has no line.
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.endFrame();
    for (const auto& pixel : rasterizer.colorBuffer())
        K3_REQUIRE(pixel.r == 0.f);
}
