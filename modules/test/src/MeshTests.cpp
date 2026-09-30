#include "Test.hpp"
#include "scene/Image.hpp"
#include "scene/Mesh.hpp"

K3_TEST(mesh_compute_normals_on_ccw_quad_points_to_plus_z)
{
    k3::Mesh mesh{
        .positions = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    };

    mesh.computeNormals();
    K3_REQUIRE(mesh.hasNormals());
    for (const auto& n : mesh.normals)
        K3_CHECK_NEAR(n.z, 1.f, 1e-6f);
}

K3_TEST(mesh_valid_rejects_out_of_range_indices_and_partial_attributes)
{
    k3::Mesh mesh{.positions = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}}};

    K3_CHECK(mesh.valid());
    mesh.indices = {0, 1, 3};
    K3_CHECK(!mesh.valid());
    mesh.indices = {0, 1, 2};
    mesh.uvs = {{0.f, 0.f}};
    K3_CHECK(!mesh.valid());
}

K3_TEST(image_png_round_trip)
{
    k3::Image image(3, 2, {1.f, 0.5f, 0.f, 1.f});
    const auto path = std::filesystem::temp_directory_path() / "kronk3d_test_image.png";

    K3_REQUIRE(image.savePNG(path));
    auto loaded = k3::Image::load(path);
    std::filesystem::remove(path);

    K3_REQUIRE(loaded);
    K3_CHECK(loaded->width == 3 && loaded->height == 2);
    K3_CHECK(loaded->pixels == image.pixels);
    K3_CHECK(!loaded->hasTransparency());
}
