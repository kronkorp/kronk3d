#include "Test.hpp"
#include "io/ObjLoader.hpp"
#include <map>
#include <sstream>
#include <string>

namespace
{

    // In-memory filesystem for mtllib, and a texture loader that records what was asked.
    struct VirtualFiles
    {
        std::map<std::string, std::string> files;
        std::vector<std::string> requestedTextures;
        bool texturesHaveAlpha = false;

        k3::ObjLoadOptions options()
        {
            k3::ObjLoadOptions options;

            options.openFile = [this](const std::filesystem::path& path) -> std::unique_ptr<std::istream> {
                auto it = files.find(path.generic_string());
                if (it == files.end())
                    return nullptr;
                return std::make_unique<std::istringstream>(it->second);
            };
            options.loadTexture = [this](const std::filesystem::path& path, k3::ColorSpace colorSpace) {
                requestedTextures.push_back(path.generic_string());
                return std::make_shared<k3::Texture>(k3::Image(2, 2, {1.f, 1.f, 1.f, texturesHaveAlpha ? 0.5f : 1.f}), colorSpace);
            };
            return options;
        }
    };

    k3::Result<k3::Model> parse(const std::string& obj, const k3::ObjLoadOptions& options = {})
    {
        std::istringstream stream(obj);
        return k3::ObjLoader::loadFromStream(stream, "", options);
    }

}

K3_TEST(obj_quad_is_triangulated_with_all_attributes)
{
    auto model = parse(
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
    );

    K3_REQUIRE(model);
    K3_REQUIRE(model->primitives.size() == 1);

    const auto& mesh = *model->primitives[0].mesh;
    K3_CHECK(mesh.valid());
    K3_CHECK(mesh.triangleCount() == 2);
    K3_CHECK(mesh.vertexCount() == 4);
    K3_CHECK(mesh.hasUVs() && mesh.hasNormals() && !mesh.hasColors());
    // OBJ v = 0 is the bottom of the image, kronk3d's v = 0 the top.
    K3_CHECK_NEAR(mesh.uvs[0].y, 1.f, 1e-6f);
    K3_CHECK_NEAR(mesh.normals[0].z, 1.f, 1e-6f);
}

K3_TEST(obj_negative_indices_and_pentagon)
{
    auto model = parse(
        "v 0 0 0\nv 1 0 0\nv 1.5 1 0\nv 0.5 2 0\nv -0.5 1 0\n"
        "f -5 -4 -3 -2 -1\n"
    );

    K3_REQUIRE(model);
    const auto& mesh = *model->primitives[0].mesh;
    K3_CHECK(mesh.triangleCount() == 3);
    K3_CHECK(mesh.vertexCount() == 5);
    K3_CHECK(!mesh.hasUVs());
}

K3_TEST(obj_missing_normals_are_smoothed_across_faces)
{
    // Two triangles folded at 90 degrees along the x axis: the shared edge gets the averaged normal.
    auto model = parse(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 2 3\nf 1 4 2\n"
    );

    K3_REQUIRE(model);
    const auto& mesh = *model->primitives[0].mesh;
    K3_REQUIRE(mesh.hasNormals());
    K3_CHECK(mesh.vertexCount() == 4);
    // Vertex 0 is shared by (0,0,1) and (0,1,0) faces.
    K3_CHECK_NEAR(mesh.normals[0].y, 0.70710f, 1e-4f);
    K3_CHECK_NEAR(mesh.normals[0].z, 0.70710f, 1e-4f);
}

K3_TEST(obj_smoothing_off_gives_flat_normals)
{
    auto model = parse(
        "s off\n"
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 2 3\nf 1 4 2\n"
    );

    K3_REQUIRE(model);
    const auto& mesh = *model->primitives[0].mesh;
    // Shared positions are split, since each face has its own normal.
    K3_CHECK(mesh.vertexCount() == 6);
    K3_CHECK_NEAR(mesh.normals[0].z, 1.f, 1e-6f);
    K3_CHECK_NEAR(mesh.normals[3].y, 1.f, 1e-6f);
}

K3_TEST(obj_vertex_colors)
{
    auto model = parse("v 0 0 0 1 0 0\nv 1 0 0 0 1 0\nv 0 1 0 0 0 1\nf 1 2 3\n");

    K3_REQUIRE(model);
    const auto& mesh = *model->primitives[0].mesh;
    K3_REQUIRE(mesh.hasColors());
    K3_CHECK(mesh.colors[0].r == 1.f && mesh.colors[1].g == 1.f && mesh.colors[2].b == 1.f);
}

K3_TEST(obj_invalid_index_reports_line)
{
    auto model = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\n\nf 1 2 7\n");

    K3_REQUIRE(!model);
    K3_CHECK(model.error().find("line 5") != std::string::npos);
}

K3_TEST(obj_materials_and_mtl_statements)
{
    VirtualFiles fs;
    fs.files["scene.mtl"] =
        "newmtl glass\n"
        "Kd 0.1 0.2 0.3\n"
        "Ks 1\n"
        "Ns 250\n"
        "d 0.25\n"
        "\n"
        "newmtl leaves\n"
        "map_Kd -s 1 1 1 -clamp on textures\\leaf color.png\n"
        "map_d leaf_mask.png\n"
        "\n"
        "newmtl flat\n"
        "illum 0\n";

    auto model = parse(
        "mtllib scene.mtl\n"
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "usemtl glass\nf 1 2 3\n"
        "usemtl leaves\nf 1 2 3\n"
        "usemtl flat\nf 1 2 3\n"
        "usemtl glass\nf 3 2 1\n",
        fs.options()
    );

    K3_REQUIRE(model);
    K3_REQUIRE(model->primitives.size() == 3);

    const auto& glass = *model->primitives[0].material;
    K3_CHECK(glass.name == "glass");
    K3_CHECK_NEAR(glass.diffuse.g, 0.2f, 1e-6f);
    K3_CHECK_NEAR(glass.specular.b, 1.f, 1e-6f);
    K3_CHECK_NEAR(glass.shininess, 250.f, 1e-6f);
    K3_CHECK(glass.alphaMode == k3::AlphaMode::Blend);
    // Both "usemtl glass" blocks end up in the same primitive.
    K3_CHECK(model->primitives[0].mesh->triangleCount() == 2);

    const auto& leaves = *model->primitives[1].material;
    K3_REQUIRE(leaves.diffuseMap && leaves.opacityMap);
    K3_CHECK(leaves.diffuseMap->sampler.wrapU == k3::TextureWrap::ClampToEdge);
    K3_CHECK(leaves.diffuseMap->colorSpace() == k3::ColorSpace::Srgb);
    K3_CHECK(leaves.opacityMap->colorSpace() == k3::ColorSpace::Linear);
    K3_CHECK(leaves.alphaMode == k3::AlphaMode::Mask);
    K3_REQUIRE(fs.requestedTextures.size() == 2);
    K3_CHECK(fs.requestedTextures[0] == "textures/leaf color.png");

    K3_CHECK(model->primitives[2].material->unlit);
}

K3_TEST(obj_unknown_material_falls_back_to_default)
{
    auto model = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl missing\nf 1 2 3\n");

    K3_REQUIRE(model);
    K3_REQUIRE(model->primitives.size() == 1);
    K3_CHECK(model->primitives[0].material->name == "missing");
    K3_CHECK(model->primitives[0].material->alphaMode == k3::AlphaMode::Opaque);
}
