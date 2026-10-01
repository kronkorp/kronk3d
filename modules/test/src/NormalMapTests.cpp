#include "Test.hpp"
#include "io/ObjLoader.hpp"
#include "render/software/SoftwareRasterizer.hpp"
#include "scene/Mesh.hpp"
#include "scene/Texture.hpp"
#include <cmath>
#include <map>
#include <sstream>

namespace
{

    // Quad in the z = 0 plane facing +z, uvs with (0, 0) at the top-left: u along +x, image-up along +y.
    k3::Mesh quad()
    {
        return k3::Mesh{
            .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
            .normals = {{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}},
            .uvs = {{0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {0.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        };
    }

    // 1x1 tangent-space normal map holding `n` (normalized, OpenGL convention).
    std::shared_ptr<k3::Texture> constantNormalMap(k3::Math::Vector3f n)
    {
        n = k3::Math::Vector3f::normalize(n);
        k3::Image image(1, 1);
        image.texel(0, 0)[0] = static_cast<std::uint8_t>(std::lround((n.x * 0.5f + 0.5f) * 255.f));
        image.texel(0, 0)[1] = static_cast<std::uint8_t>(std::lround((n.y * 0.5f + 0.5f) * 255.f));
        image.texel(0, 0)[2] = static_cast<std::uint8_t>(std::lround((n.z * 0.5f + 0.5f) * 255.f));
        return std::make_shared<k3::Texture>(image, k3::ColorSpace::Linear);
    }

    // Center pixel of the quad, unlit except by one directional light shining along `direction`.
    float litQuad(std::shared_ptr<k3::Mesh> mesh, std::shared_ptr<k3::Texture> normalMap, const k3::Math::Vector3f& direction)
    {
        k3::Camera camera;
        camera.position = {0.f, 0.f, 1.f};
        camera.projection = k3::Projection::Orthographic;
        k3::Environment environment;
        environment.ambient = k3::Math::Color::Black;
        environment.lights = {k3::Light::directional(direction)};
        auto material = std::make_shared<k3::Material>();
        material->normalMap = std::move(normalMap);

        k3::SoftwareRasterizer rasterizer(16, 16);
        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(mesh, material, k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer()[8 * 16 + 8].r;
    }

}

K3_TEST(mesh_tangents_follow_u_and_image_up)
{
    k3::Mesh mesh = quad();
    mesh.computeTangents();

    K3_REQUIRE(mesh.hasTangents());
    for (const auto& t : mesh.tangents) {
        K3_CHECK_NEAR(t.x, 1.f, 1e-5f);
        K3_CHECK_NEAR(t.y, 0.f, 1e-5f);
        K3_CHECK(t.w == 1.f);
    }

    // Mirrored uvs (u decreasing along +x): the tangent flips, and so does the handedness.
    for (auto& uv : mesh.uvs)
        uv.x = 1.f - uv.x;
    mesh.computeTangents();
    K3_CHECK_NEAR(mesh.tangents[0].x, -1.f, 1e-5f);
    K3_CHECK(mesh.tangents[0].w == -1.f);
}

K3_TEST(mesh_tangents_need_normals_and_uvs)
{
    k3::Mesh mesh = quad();
    mesh.uvs.clear();
    mesh.computeTangents();
    K3_CHECK(!mesh.hasTangents());
}

K3_TEST(height_map_becomes_a_normal_map)
{
    // Flat: straight up.
    const auto flat = k3::Texture::normalMapFromHeight(k3::Image(4, 4, {0.5f, 0.5f, 0.5f, 1.f}));
    K3_CHECK(flat->colorSpace() == k3::ColorSpace::Linear);
    K3_CHECK(flat->image().texel(1, 1)[0] == 128 && flat->image().texel(1, 1)[2] == 255);

    // Rising toward +x (to the right): the normal leans toward -x.
    k3::Image ramp(8, 1);
    for (std::uint32_t x = 0; x < 8; ++x)
        ramp.texel(x, 0)[0] = static_cast<std::uint8_t>(x * 16);
    const auto sloped = k3::Texture::normalMapFromHeight(ramp, 4.f);
    K3_CHECK(sloped->image().texel(3, 0)[0] < 128);
    K3_CHECK_NEAR(sloped->image().texel(3, 0)[1], 128.f, 1.f);

    // Rising toward the bottom of the image: the normal leans away from the slope, toward image-up (+y).
    k3::Image down(1, 8);
    for (std::uint32_t y = 0; y < 8; ++y)
        down.texel(0, y)[0] = static_cast<std::uint8_t>(y * 16);
    K3_CHECK(k3::Texture::normalMapFromHeight(down, 4.f)->image().texel(0, 3)[1] > 128);
}

K3_TEST(normal_map_tilts_the_lighting)
{
    auto mesh = std::make_shared<k3::Mesh>(quad());
    mesh->computeTangents();
    // Light grazing from +x: a flat surface gets nothing, one tilted toward +x gets cos(angle).
    const k3::Math::Vector3f grazing{-1.f, 0.f, 0.f};

    // (8-bit maps cannot encode exactly 0: 128 decodes to 1/255.)
    K3_CHECK_NEAR(litQuad(mesh, constantNormalMap({0.f, 0.f, 1.f}), grazing), 0.f, 0.005f);
    K3_CHECK_NEAR(litQuad(mesh, constantNormalMap({0.6f, 0.f, 0.8f}), grazing), 0.6f, 0.01f);
    // Tilted toward image-up (+y in tangent space) = world +y here: lit by a light from +y.
    K3_CHECK_NEAR(litQuad(mesh, constantNormalMap({0.f, 0.6f, 0.8f}), {0.f, -1.f, 0.f}), 0.6f, 0.01f);

    // Without tangents the normal map is ignored.
    auto noTangents = std::make_shared<k3::Mesh>(quad());
    K3_CHECK_NEAR(litQuad(noTangents, constantNormalMap({0.6f, 0.f, 0.8f}), grazing), 0.f, 1e-6f);
}

K3_TEST(obj_bump_and_norm_maps)
{
    std::map<std::string, k3::Image> images;
    images["height.png"] = k3::Image(4, 4, {0.3f, 0.3f, 0.3f, 1.f});
    images["normals.png"] = k3::Image(4, 4, {0.5f, 0.5f, 1.f, 1.f});

    k3::ObjLoadOptions options;
    options.openFile = [](const std::filesystem::path& path) -> std::unique_ptr<std::istream> {
        if (path.generic_string() != "scene.mtl")
            return nullptr;
        return std::make_unique<std::istringstream>("newmtl bumpy\nmap_Bump -bm 2 height.png\n\nnewmtl mapped\nnorm normals.png\n");
    };
    options.loadTexture = [&](const std::filesystem::path& path, k3::ColorSpace colorSpace) {
        return std::make_shared<k3::Texture>(images.at(path.generic_string()), colorSpace);
    };

    std::istringstream obj(
        "mtllib scene.mtl\n"
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl bumpy\nf 1/1 2/2 3/3\n"
        "usemtl mapped\nf 1/1 2/2 3/3\n"
    );
    auto model = k3::ObjLoader::loadFromStream(obj, "", options);

    K3_REQUIRE(model);
    K3_REQUIRE(model->primitives.size() == 2);
    for (const auto& primitive : model->primitives) {
        K3_REQUIRE(primitive.material->normalMap);
        K3_CHECK(primitive.material->normalMap->colorSpace() == k3::ColorSpace::Linear);
        K3_CHECK(primitive.mesh->hasTangents());
    }
    // The grey height map was converted (flat: straight up); the colored one kept as is.
    K3_CHECK(model->primitives[0].material->normalMap->image().texel(0, 0)[2] == 255);
    K3_CHECK(model->primitives[1].material->normalMap->image().texel(0, 0)[0] == 128);
}
