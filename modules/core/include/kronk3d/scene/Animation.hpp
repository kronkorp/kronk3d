/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Skeletal animation data: node hierarchy, skins, clips (glTF model)
*/
#pragma once

#include <kronk3d/math/Bounds.hpp>
#include <kronk3d/scene/Material.hpp>
#include <kronk3d/math/Matrix.hpp>
#include <kronk3d/scene/Mesh.hpp>
#include <kronk3d/math/Vector.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace k3
{

    struct Transform
    {
        Math::Vector3f translation{};
        Math::Vector4f rotation{0.f, 0.f, 0.f, 1.f};    // Unit quaternion (x, y, z, w)
        Math::Vector3f scale{1.f, 1.f, 1.f};

        [[nodiscard]] Math::Matrix4 matrix() const;

        // Shear is dropped.
        static Transform fromMatrix(const Math::Matrix4& matrix);

        // Linear blend of translations and scales, shortest-path slerp of rotations.
        static Transform mix(const Transform& a, const Transform& b, float t);
    };

    struct Node
    {
        std::string name{};
        int         parent = -1;    // Always lower than the node's own index: parents come first
        Transform   local{};        // Rest pose
    };

    // Joints are indices into AnimatedModel::nodes. Vertex joint `j` is moved by nodes[joints[j]].
    struct Skin
    {
        std::vector<int>          joints{};
        std::vector<Math::Matrix4> inverseBind{};   // Same size as joints: model space -> joint space at bind time
    };

    struct AnimationChannel
    {
        enum class Path { Translation, Rotation, Scale };
        enum class Interpolation { Step, Linear, CubicSpline };

        int           node = -1;
        Path          path = Path::Translation;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float>          times{};    // Seconds, strictly increasing
        // xyz (translation, scale) or xyzw (rotation) per key; CubicSpline: in-tangent, value, out-tangent per key.
        std::vector<Math::Vector4f> values{};
    };

    struct Animation
    {
        std::string                   name{};
        float                         duration = 0.f;
        std::vector<AnimationChannel> channels{};

        // Writes the animated nodes of `pose` (one Transform per node, initially the rest pose) at `time`,
        // clamped to [0, duration].
        void sample(float time, std::vector<Transform>& pose) const;
    };

    // A whole glTF scene: meshes attached to a node hierarchy, optionally skinned, and its animation clips.
    // Play it with an Animator; with no clip, the Animator draws the rest pose (so this also loads static
    // scenes made of several nodes).
    struct AnimatedModel
    {
        struct Primitive
        {
            std::shared_ptr<Mesh>     mesh{};
            std::shared_ptr<Material> material{};
            int                       node = 0;     // Rigid primitive: follows this node
            int                       skin = -1;    // >= 0: vertices are skinned (the node's transform is ignored)
        };

        std::string            name{};
        std::vector<Node>      nodes{};
        std::vector<Skin>      skins{};
        std::vector<Primitive> primitives{};
        std::vector<Animation> animations{};

        // -1 when there is no such clip.
        [[nodiscard]] int findAnimation(std::string_view clip) const noexcept;

        // Object-space transform of every node for the given local transforms.
        [[nodiscard]] std::vector<Math::Matrix4> globalTransforms(const std::vector<Transform>& pose) const;

        // Rest-pose bounds.
        [[nodiscard]] Math::Bounds3f bounds() const;
    };

}
