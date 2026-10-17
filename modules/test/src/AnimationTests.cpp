#include "Test.hpp"
#include <kronk3d/io/GltfLoader.hpp>
#include <kronk3d/render/IRasterizer.hpp>
#include <kronk3d/scene/Animator.hpp>
#include <cmath>
#include <cstring>
#include <format>
#include <numbers>
#include <string>

namespace
{

    using namespace k3;
    using Math::Matrix4;
    using Math::Vector3f;
    using Math::Vector4f;

    // Records what is drawn, instead of rendering it.
    class RecordingRasterizer : public IRasterizer
    {
        public:
            struct Call
            {
                std::shared_ptr<const Mesh> mesh;
                Matrix4                     transform;
            };

            Backend backend() const noexcept override { return Backend::Software; }
            std::string_view name() const noexcept override { return "recording"; }
            void resize(std::uint32_t, std::uint32_t) override {}
            std::uint32_t width() const noexcept override { return 1; }
            std::uint32_t height() const noexcept override { return 1; }
            void setAntiAliasing(AntiAliasing) override {}
            AntiAliasing antiAliasing() const noexcept override { return AntiAliasing::None; }
            void beginFrame(const Camera&, const Environment&) override { calls.clear(); }
            void beginLayer(const Camera&) override {}
            void draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material>, const Matrix4& transform) override
            {
                calls.push_back({std::move(mesh), transform});
            }
            void drawLines(std::span<const DebugLine>, bool) override {}
            void draw2D(std::shared_ptr<Texture>, const Rect&, const Rect&, const Math::Color&) override {}
            void endFrame() override {}
            Image readPixels() override { return {}; }
            const FrameStats& stats() const noexcept override { return m_stats; }

            std::vector<Call> calls{};

        private:
            FrameStats m_stats{};
    };

    AnimationChannel channel(int node, AnimationChannel::Path path, AnimationChannel::Interpolation interpolation,
                             std::vector<float> times, std::vector<Vector4f> values)
    {
        return {node, path, interpolation, std::move(times), std::move(values)};
    }

    // Root node 0, child node 1 ("tip") at (0, 1, 0). A quad whose lower vertices (0, 1) follow node 0 and
    // upper ones (2, 3) node 1. Clips: "Lift" moves the tip up to y = 3 in one second, "Slide" moves it to x = 4.
    AnimatedModel twoBones()
    {
        AnimatedModel model;
        auto mesh = std::make_shared<Mesh>();

        model.nodes = {{"root", -1, {}}, {"tip", 0, {{0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}}}};
        mesh->positions = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f}};
        mesh->normals.assign(4, {0.f, 0.f, 1.f});
        mesh->joints = {{{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{1, 0, 0, 0}}, {{1, 0, 0, 0}}};
        mesh->weights.assign(4, {1.f, 0.f, 0.f, 0.f});
        mesh->indices = {0, 1, 2, 1, 3, 2};

        Skin skin;
        skin.joints = {0, 1};
        skin.inverseBind = {Matrix4::identity(), Matrix4::translate({0.f, -1.f, 0.f})};
        model.skins.push_back(skin);
        model.primitives.push_back({mesh, nullptr, 0, 0});

        using Path = AnimationChannel::Path;
        using Interpolation = AnimationChannel::Interpolation;

        model.animations.push_back({"Lift", 1.f, {channel(1, Path::Translation, Interpolation::Linear, {0.f, 1.f}, {{0.f, 1.f, 0.f, 0.f}, {0.f, 3.f, 0.f, 0.f}})}});
        model.animations.push_back({"Slide", 1.f, {channel(1, Path::Translation, Interpolation::Linear, {0.f, 1.f}, {{0.f, 1.f, 0.f, 0.f}, {4.f, 1.f, 0.f, 0.f}})}});
        return model;
    }

    // The mesh the animator hands to the rasterizer for its first primitive.
    std::shared_ptr<const Mesh> drawn(Animator& animator, RecordingRasterizer& rasterizer)
    {
        rasterizer.beginFrame({}, {});
        animator.draw(rasterizer);
        return rasterizer.calls.empty() ? nullptr : rasterizer.calls[0].mesh;
    }

}

K3_TEST(transform_matrix_round_trips)
{
    const Transform source{{1.f, -2.f, 3.f}, {0.f, 0.f, std::sin(0.4f), std::cos(0.4f)}, {2.f, 3.f, 0.5f}};
    const Transform back = Transform::fromMatrix(source.matrix());

    K3_CHECK_NEAR(back.translation.y, -2.f, 1e-5f);
    K3_CHECK_NEAR(back.scale.x, 2.f, 1e-5f);
    K3_CHECK_NEAR(back.scale.z, 0.5f, 1e-5f);
    K3_CHECK_NEAR(std::abs(back.rotation.z), std::sin(0.4f), 1e-5f);
    K3_CHECK_NEAR(std::abs(back.rotation.w), std::cos(0.4f), 1e-5f);
}

K3_TEST(transform_rotation_acts_like_the_matrix_helpers)
{
    // 90 degrees around Z takes +X to +Y.
    const float half = std::numbers::pi_v<float> / 4.f;
    const Transform quarterTurn{{}, {0.f, 0.f, std::sin(half), std::cos(half)}, {1.f, 1.f, 1.f}};
    const Vector3f p = quarterTurn.matrix().transformPoint({1.f, 0.f, 0.f});

    K3_CHECK_NEAR(p.x, 0.f, 1e-5f);
    K3_CHECK_NEAR(p.y, 1.f, 1e-5f);
}

K3_TEST(transform_mix_takes_the_short_way_between_rotations)
{
    const float quarter = std::numbers::pi_v<float> / 4.f;
    const Transform a;
    const Transform b{{}, {0.f, 0.f, -std::sin(quarter), -std::cos(quarter)}, {1.f, 1.f, 1.f}};     // -q: same as a quarter turn
    const Vector3f p = Transform::mix(a, b, 0.5f).matrix().transformPoint({1.f, 0.f, 0.f});

    K3_CHECK_NEAR(p.x, std::cos(quarter), 1e-4f);       // 45 degrees, not 135
    K3_CHECK_NEAR(p.y, std::sin(quarter), 1e-4f);
}

K3_TEST(animation_interpolates_linearly_and_clamps)
{
    const AnimatedModel model = twoBones();
    std::vector<Transform> pose = {model.nodes[0].local, model.nodes[1].local};

    model.animations[0].sample(0.5f, pose);
    K3_CHECK_NEAR(pose[1].translation.y, 2.f, 1e-6f);
    model.animations[0].sample(9.f, pose);
    K3_CHECK_NEAR(pose[1].translation.y, 3.f, 1e-6f);
    model.animations[0].sample(-1.f, pose);
    K3_CHECK_NEAR(pose[1].translation.y, 1.f, 1e-6f);
}

K3_TEST(animation_step_holds_the_previous_key)
{
    Animation animation{"step", 2.f, {channel(0, AnimationChannel::Path::Scale, AnimationChannel::Interpolation::Step, {0.f, 1.f, 2.f},
                                              {{1.f, 1.f, 1.f, 0.f}, {2.f, 2.f, 2.f, 0.f}, {3.f, 3.f, 3.f, 0.f}})}};
    std::vector<Transform> pose(1);

    animation.sample(0.99f, pose);
    K3_CHECK_NEAR(pose[0].scale.x, 1.f, 1e-6f);
    animation.sample(1.f, pose);
    K3_CHECK_NEAR(pose[0].scale.x, 2.f, 1e-6f);
}

K3_TEST(animation_cubic_spline_hits_the_keys_and_follows_the_tangents)
{
    // Keys: (in-tangent, value, out-tangent). Flat tangents give a smoothstep: halfway is the midpoint.
    Animation flat{"flat", 1.f, {channel(0, AnimationChannel::Path::Translation, AnimationChannel::Interpolation::CubicSpline, {0.f, 1.f},
                                         {{}, {0.f, 0.f, 0.f, 0.f}, {}, {}, {2.f, 0.f, 0.f, 0.f}, {}})}};
    std::vector<Transform> pose(1);

    flat.sample(0.f, pose);
    K3_CHECK_NEAR(pose[0].translation.x, 0.f, 1e-6f);
    flat.sample(0.5f, pose);
    K3_CHECK_NEAR(pose[0].translation.x, 1.f, 1e-6f);
    flat.sample(1.f, pose);
    K3_CHECK_NEAR(pose[0].translation.x, 2.f, 1e-6f);

    // An out-tangent of 4 (units per second) on the first key pushes the curve above the straight line.
    Animation pushed{"pushed", 1.f, {channel(0, AnimationChannel::Path::Translation, AnimationChannel::Interpolation::CubicSpline, {0.f, 1.f},
                                             {{}, {0.f, 0.f, 0.f, 0.f}, {4.f, 0.f, 0.f, 0.f}, {}, {2.f, 0.f, 0.f, 0.f}, {}})}};

    pushed.sample(0.25f, pose);
    K3_CHECK(pose[0].translation.x > 0.5f);
}

K3_TEST(animation_ignores_channels_of_unknown_nodes)
{
    Animation animation{"bad", 1.f, {channel(7, AnimationChannel::Path::Translation, AnimationChannel::Interpolation::Linear, {0.f}, {{1.f, 0.f, 0.f, 0.f}})}};
    std::vector<Transform> pose(1);

    animation.sample(0.f, pose);
    K3_CHECK_NEAR(pose[0].translation.x, 0.f, 1e-6f);
}

K3_TEST(animator_rest_pose_is_the_bind_pose)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));
    RecordingRasterizer rasterizer;
    const auto mesh = drawn(animator, rasterizer);

    K3_REQUIRE(mesh);
    for (std::size_t v = 0; v < 4; ++v) {
        K3_CHECK_NEAR(mesh->positions[v].x, static_cast<float>(v % 2), 1e-6f);
        K3_CHECK_NEAR(mesh->positions[v].y, v < 2 ? 0.f : 1.f, 1e-6f);
    }
}

K3_TEST(animator_skins_vertices_with_their_joints)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));
    RecordingRasterizer rasterizer;

    K3_REQUIRE(animator.play("Lift"));
    animator.update(0.5f);

    const auto mesh = drawn(animator, rasterizer);

    K3_REQUIRE(mesh);
    K3_CHECK(mesh->valid());
    K3_CHECK_NEAR(mesh->positions[0].y, 0.f, 1e-6f);        // Follows the root: still
    K3_CHECK_NEAR(mesh->positions[2].y, 2.f, 1e-6f);        // Follows the tip: 1 -> 3, halfway
    K3_CHECK_NEAR(mesh->positions[3].x, 1.f, 1e-6f);
    K3_CHECK_NEAR(mesh->normals[2].z, 1.f, 1e-6f);
}

K3_TEST(animator_blends_joint_weights)
{
    auto model = twoBones();

    model.primitives[0].mesh->joints[2] = {{0, 1, 0, 0}};
    model.primitives[0].mesh->weights[2] = {0.5f, 0.5f, 0.f, 0.f};

    Animator animator(std::make_shared<AnimatedModel>(model));
    RecordingRasterizer rasterizer;

    animator.play("Lift", {.loop = false});
    animator.update(1.f);
    // Half pinned to the root (stays at y = 1), half carried by the tip (y = 3).
    K3_CHECK_NEAR(drawn(animator, rasterizer)->positions[2].y, 2.f, 1e-6f);
}

K3_TEST(animator_leaves_the_source_mesh_untouched)
{
    const auto model = std::make_shared<AnimatedModel>(twoBones());
    Animator animator(model);
    RecordingRasterizer rasterizer;

    animator.play("Lift");
    animator.update(1.f);
    K3_CHECK(drawn(animator, rasterizer) != model->primitives[0].mesh);
    K3_CHECK_NEAR(model->primitives[0].mesh->positions[2].y, 1.f, 1e-6f);
}

K3_TEST(animator_draws_rigid_primitives_at_their_node)
{
    AnimatedModel model = twoBones();

    model.primitives.clear();
    model.primitives.push_back({std::make_shared<Mesh>(), nullptr, 1, -1});
    model.primitives[0].mesh->positions = {{}, {}, {}};

    Animator animator(std::make_shared<AnimatedModel>(model));
    RecordingRasterizer rasterizer;

    animator.play("Lift", {.loop = false});
    animator.update(1.f);
    rasterizer.beginFrame({}, {});
    animator.draw(rasterizer, Matrix4::translate({10.f, 0.f, 0.f}));
    K3_REQUIRE(rasterizer.calls.size() == 1);
    K3_CHECK_NEAR(rasterizer.calls[0].transform.values[3], 10.f, 1e-6f);
    K3_CHECK_NEAR(rasterizer.calls[0].transform.values[7], 3.f, 1e-6f);
}

K3_TEST(animator_loops_and_stops_at_the_end)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));

    animator.play("Lift");
    animator.update(1.25f);
    K3_CHECK_NEAR(animator.time(), 0.25f, 1e-5f);
    K3_CHECK(!animator.finished());

    animator.play("Slide", {.loop = false, .fade = 0.f});
    K3_CHECK(animator.clip() == "Slide");
    animator.update(5.f);
    K3_CHECK(animator.finished());
    K3_CHECK_NEAR(animator.time(), 1.f, 1e-6f);
}

K3_TEST(animator_does_not_restart_the_clip_it_is_playing)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));

    animator.play("Lift");
    animator.update(0.4f);
    animator.play("Lift");                      // E.g. called every frame by game logic
    K3_CHECK_NEAR(animator.time(), 0.4f, 1e-6f);
    K3_CHECK(!animator.play("Nope"));
    K3_CHECK(animator.clip() == "Lift");
}

K3_TEST(animator_crossfades_between_clips)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));
    RecordingRasterizer rasterizer;

    animator.play("Slide");
    animator.update(0.75f);
    animator.play("Lift", {.loop = true, .fade = 1.f});
    animator.update(0.5f);      // Halfway through the fade. Slide has looped to 0.25: tip at (1, 1). Lift is at 0.5: tip at (0, 2).

    const auto mesh = drawn(animator, rasterizer);

    K3_REQUIRE(mesh);
    K3_CHECK_NEAR(mesh->positions[2].x, 0.5f, 1e-5f);       // Tip halfway between the two: (0.5, 1.5), the vertex sits 1 above the bind tip... minus the bind offset
    K3_CHECK_NEAR(mesh->positions[2].y, 1.5f, 1e-5f);
}

K3_TEST(animator_stop_returns_to_the_rest_pose)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));
    RecordingRasterizer rasterizer;

    animator.play("Lift");
    animator.update(1.f);
    animator.stop();
    K3_CHECK(animator.clip().empty());
    K3_CHECK_NEAR(drawn(animator, rasterizer)->positions[2].y, 1.f, 1e-6f);
}

K3_TEST(animator_reuses_its_posed_meshes)
{
    Animator animator(std::make_shared<AnimatedModel>(twoBones()));
    RecordingRasterizer rasterizer;

    animator.play("Lift");
    animator.update(0.25f);

    const auto first = drawn(animator, rasterizer);
    const auto revision = first->revision;

    animator.update(0.25f);
    K3_CHECK(drawn(animator, rasterizer) == first);         // Same mesh, so the GPU buffers are reused
    K3_CHECK(first->revision > revision);
    K3_CHECK_NEAR(first->positions[2].y, 2.f, 1e-6f);       // And it holds the new pose
}

/* glTF */

namespace
{

    struct Blob
    {
        std::vector<std::uint8_t> bytes{};

        std::size_t add(const void* data, std::size_t size)
        {
            const std::size_t offset = bytes.size();

            bytes.insert(bytes.end(), static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
            bytes.resize((bytes.size() + 3) & ~std::size_t{3});
            return offset;
        }
    };

    // The two-bone strip as a GLB. Node order in the file is child first, to check that parents end up before children.
    std::vector<std::uint8_t> twoBoneGlb(const char* interpolation = "LINEAR")
    {
        const float positions[] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0};
        const std::uint8_t joints[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        const float weights[] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        const std::uint16_t indices[] = {0, 1, 2, 1, 3, 2};
        const float times[] = {0, 1};
        const bool cubic = std::string_view(interpolation) == "CUBICSPLINE";
        // Cubic splines store an in-tangent, a value and an out-tangent per key (flat tangents here).
        const float lift[] = {0, 1, 0, 0, 3, 0};
        const float cubicLift[] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0};
        const float inverseBind[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                                     1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1};
        Blob blob;
        const std::size_t offsets[] = {
            blob.add(positions, sizeof positions), blob.add(joints, sizeof joints), blob.add(weights, sizeof weights),
            blob.add(indices, sizeof indices), blob.add(times, sizeof times), cubic ? blob.add(cubicLift, sizeof cubicLift) : blob.add(lift, sizeof lift),
            blob.add(inverseBind, sizeof inverseBind),
        };
        const std::size_t sizes[] = {sizeof positions, sizeof joints, sizeof weights, sizeof indices, sizeof times, cubic ? sizeof cubicLift : sizeof lift, sizeof inverseBind};
        const struct { int component; const char* type; int count; } accessors[] = {
            {5126, "VEC3", 4}, {5121, "VEC4", 4}, {5126, "VEC4", 4}, {5123, "SCALAR", 6}, {5126, "SCALAR", 2}, {5126, "VEC3", cubic ? 6 : 2}, {5126, "MAT4", 2},
        };
        std::string views, accessorJson;

        for (int i = 0; i < 7; ++i) {
            views += std::format("{}{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}}", i ? "," : "", offsets[i], sizes[i]);
            accessorJson += std::format("{}{{\"bufferView\":{},\"componentType\":{},\"type\":\"{}\",\"count\":{}}}", i ? "," : "", i,
                                        accessors[i].component, accessors[i].type, accessors[i].count);
        }

        const std::string json = std::format(
            "{{\"asset\":{{\"version\":\"2.0\"}},\"scene\":0,\"scenes\":[{{\"nodes\":[1,2]}}],"
            "\"nodes\":[{{\"name\":\"bone1\",\"translation\":[0,1,0]}},{{\"name\":\"bone0\",\"children\":[0]}},{{\"name\":\"body\",\"mesh\":0,\"skin\":0}}],"
            "\"skins\":[{{\"joints\":[1,0],\"inverseBindMatrices\":6}}],"
            "\"meshes\":[{{\"primitives\":[{{\"attributes\":{{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":2}},\"indices\":3}}]}}],"
            "\"animations\":[{{\"name\":\"Lift\",\"samplers\":[{{\"input\":4,\"output\":5,\"interpolation\":\"{}\"}}],"
            "\"channels\":[{{\"sampler\":0,\"target\":{{\"node\":0,\"path\":\"translation\"}}}}]}}],"
            "\"buffers\":[{{\"byteLength\":{}}}],\"bufferViews\":[{}],\"accessors\":[{}]}}",
            interpolation, blob.bytes.size(), views, accessorJson);

        std::vector<std::uint8_t> glb;
        auto put32 = [&glb](std::uint32_t value) { for (int i = 0; i < 4; ++i) glb.push_back(static_cast<std::uint8_t>(value >> (8 * i))); };
        std::string paddedJson = json;

        paddedJson.resize((paddedJson.size() + 3) & ~std::size_t{3}, ' ');
        put32(0x46546C67);
        put32(2);
        put32(static_cast<std::uint32_t>(12 + 8 + paddedJson.size() + 8 + blob.bytes.size()));
        put32(static_cast<std::uint32_t>(paddedJson.size()));
        put32(0x4E4F534A);
        glb.insert(glb.end(), paddedJson.begin(), paddedJson.end());
        put32(static_cast<std::uint32_t>(blob.bytes.size()));
        put32(0x004E4942);
        glb.insert(glb.end(), blob.bytes.begin(), blob.bytes.end());
        return glb;
    }

}

K3_TEST(gltf_loads_nodes_skins_meshes_and_clips)
{
    const auto glb = twoBoneGlb();
    auto loaded = GltfLoader::loadFromMemory(glb.data(), glb.size(), "");

    K3_REQUIRE(loaded);
    K3_REQUIRE(loaded->nodes.size() == 3);
    K3_CHECK(loaded->nodes[0].name == "bone0" && loaded->nodes[0].parent == -1);
    K3_CHECK(loaded->nodes[1].name == "bone1" && loaded->nodes[1].parent == 0);     // Parents first
    K3_CHECK(loaded->nodes[2].name == "body");
    K3_REQUIRE(loaded->skins.size() == 1);
    K3_CHECK(loaded->skins[0].joints == (std::vector<int>{0, 1}));
    K3_CHECK_NEAR(loaded->skins[0].inverseBind[1].values[7], -1.f, 1e-6f);        // Column-major file -> row-major matrix
    K3_REQUIRE(loaded->primitives.size() == 1);
    K3_CHECK(loaded->primitives[0].skin == 0);
    K3_CHECK(loaded->primitives[0].mesh->hasSkin());
    K3_CHECK(loaded->primitives[0].mesh->hasNormals());                           // Generated
    K3_CHECK(loaded->primitives[0].mesh->triangleCount() == 2);
    K3_REQUIRE(loaded->animations.size() == 1);
    K3_CHECK(loaded->animations[0].name == "Lift");
    K3_CHECK_NEAR(loaded->animations[0].duration, 1.f, 1e-6f);
    K3_CHECK(loaded->findAnimation("Lift") == 0);
    K3_CHECK(loaded->findAnimation("Run") == -1);
}

K3_TEST(gltf_clip_deforms_the_mesh_like_the_hand_built_model)
{
    const auto glb = twoBoneGlb();
    auto loaded = GltfLoader::loadFromMemory(glb.data(), glb.size(), "");

    K3_REQUIRE(loaded);

    Animator animator(std::make_shared<AnimatedModel>(std::move(*loaded)));
    RecordingRasterizer rasterizer;

    K3_REQUIRE(animator.play("Lift"));
    animator.update(0.5f);

    const auto mesh = drawn(animator, rasterizer);

    K3_REQUIRE(mesh);
    K3_CHECK_NEAR(mesh->positions[0].y, 0.f, 1e-6f);
    K3_CHECK_NEAR(mesh->positions[2].y, 2.f, 1e-6f);
}

K3_TEST(gltf_cubic_spline_clips_keep_their_tangents)
{
    const auto glb = twoBoneGlb("CUBICSPLINE");
    auto loaded = GltfLoader::loadFromMemory(glb.data(), glb.size(), "");

    K3_REQUIRE(loaded);
    K3_REQUIRE(loaded->animations.size() == 1 && loaded->animations[0].channels.size() == 1);

    const auto& channel = loaded->animations[0].channels[0];

    K3_CHECK(channel.interpolation == AnimationChannel::Interpolation::CubicSpline);
    K3_CHECK(channel.values.size() == 6);

    std::vector<Transform> pose(3);

    loaded->animations[0].sample(0.5f, pose);
    K3_CHECK_NEAR(pose[1].translation.y, 2.f, 1e-5f);       // Flat tangents: the midpoint
}

K3_TEST(gltf_rejects_garbage)
{
    const char garbage[] = "this is not a glTF file at all";
    const auto glb = twoBoneGlb();

    K3_CHECK(!GltfLoader::loadFromMemory(garbage, sizeof garbage, ""));
    K3_CHECK(!GltfLoader::loadFromMemory(glb.data(), glb.size() / 2, ""));
    K3_CHECK(!GltfLoader::load("/nonexistent/model.glb"));
}
