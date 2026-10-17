#include "Animator.hpp"
#include "render/IRasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{

    // Weighted sum of the (affine) joint matrices of a vertex.
    k3::Math::Matrix4 blend(const std::vector<k3::Math::Matrix4>& joints, const std::array<std::uint16_t, 4>& indices, const k3::Math::Vector4f& weights)
    {
        k3::Math::Matrix4 result{};
        const float w[4] = {weights.x, weights.y, weights.z, weights.w};

        for (int k = 0; k < 4; ++k) {
            if (w[k] <= 0.f || indices[static_cast<std::size_t>(k)] >= joints.size())
                continue;
            const float* m = joints[indices[static_cast<std::size_t>(k)]].values;

            for (int i = 0; i < 12; ++i)
                result.values[i] += w[k] * m[i];
        }
        result.values[15] = 1.f;
        return result;
    }

    // ponytail: single-threaded, and copies the whole mesh (uvs, indices...) every update. Fine for
    // character-sized meshes; move to a worker pool or GPU skinning if profiling points here.
    // Normals follow the joints' rotation only, so non-uniform joint scales light slightly wrong.
    void skin(const k3::Mesh& source, k3::Mesh& out, const std::vector<k3::Math::Matrix4>& joints)
    {
        for (std::size_t v = 0; v < source.positions.size(); ++v) {
            const auto matrix = blend(joints, source.joints[v], source.weights[v]);

            out.positions[v] = matrix.transformPoint(source.positions[v]);
            if (source.hasNormals())
                out.normals[v] = k3::Math::Vector3f::normalize(matrix.transformDirection(source.normals[v]));
            if (source.hasTangents()) {
                const auto& t = source.tangents[v];
                const auto direction = k3::Math::Vector3f::normalize(matrix.transformDirection({t.x, t.y, t.z}));

                out.tangents[v] = {direction.x, direction.y, direction.z, t.w};
            }
        }
    }

}

k3::Animator::Animator(std::shared_ptr<const AnimatedModel> model)
    : m_model(std::move(model))
{
}

bool k3::Animator::play(std::string_view clip, const PlayOptions& options)
{
    const int index = m_model->findAnimation(clip);

    if (index < 0)
        return false;
    if (index == m_current.clip && !finished()) {
        m_current.loop = options.loop;
        m_current.speed = options.speed;
        return true;
    }
    if (m_current.clip >= 0 && options.fade > 0.f) {
        m_previous = m_current;
        m_fadeElapsed = 0.f;
        m_fadeDuration = options.fade;
    } else {
        m_previous.clip = -1;
    }
    m_current = {index, 0.f, options.speed, options.loop};
    m_dirty = true;
    return true;
}

void k3::Animator::stop()
{
    m_current.clip = m_previous.clip = -1;
    m_dirty = true;
}

void k3::Animator::advance(Track& track, float deltaSeconds) const
{
    const float duration = m_model->animations[static_cast<std::size_t>(track.clip)].duration;

    track.time += deltaSeconds * track.speed;
    if (track.loop && duration > 0.f) {
        track.time = std::fmod(track.time, duration);
        if (track.time < 0.f)
            track.time += duration;
    } else {
        track.time = std::clamp(track.time, 0.f, duration);
    }
}

void k3::Animator::update(float deltaSeconds)
{
    if (m_current.clip >= 0)
        advance(m_current, deltaSeconds);
    if (m_previous.clip >= 0) {
        advance(m_previous, deltaSeconds);
        m_fadeElapsed += deltaSeconds;
        if (m_fadeElapsed >= m_fadeDuration)
            m_previous.clip = -1;
    }
    m_dirty = true;
}

bool k3::Animator::finished() const noexcept
{
    if (m_current.clip < 0 || m_current.loop)
        return false;

    const float duration = m_model->animations[static_cast<std::size_t>(m_current.clip)].duration;

    return m_current.speed >= 0.f ? m_current.time >= duration : m_current.time <= 0.f;
}

std::string_view k3::Animator::clip() const noexcept
{
    return m_current.clip < 0 ? std::string_view{} : std::string_view(m_model->animations[static_cast<std::size_t>(m_current.clip)].name);
}

void k3::Animator::evaluate()
{
    const AnimatedModel& model = *m_model;
    std::vector<Transform> pose(model.nodes.size());

    for (std::size_t i = 0; i < pose.size(); ++i)
        pose[i] = model.nodes[i].local;

    auto sample = [&](const Track& track, std::vector<Transform>& into) {
        model.animations[static_cast<std::size_t>(track.clip)].sample(track.time, into);
    };

    if (m_current.clip >= 0)
        sample(m_current, pose);
    if (m_previous.clip >= 0) {
        std::vector<Transform> from(model.nodes.size());

        for (std::size_t i = 0; i < from.size(); ++i)
            from[i] = model.nodes[i].local;
        sample(m_previous, from);

        const float alpha = std::clamp(m_fadeElapsed / m_fadeDuration, 0.f, 1.f);

        for (std::size_t i = 0; i < pose.size(); ++i)
            pose[i] = Transform::mix(from[i], pose[i], alpha);
    }
    m_globals = model.globalTransforms(pose);

    // Joint matrices once per skin, then one deformed copy of each skinned mesh.
    std::vector<std::vector<Math::Matrix4>> joints(model.skins.size());

    for (std::size_t s = 0; s < joints.size(); ++s) {
        const Skin& skin = model.skins[s];

        joints[s].resize(skin.joints.size());
        for (std::size_t j = 0; j < skin.joints.size(); ++j)
            joints[s][j] = m_globals[static_cast<std::size_t>(skin.joints[j])] * skin.inverseBind[j];
    }
    m_posed.resize(model.primitives.size());
    for (std::size_t p = 0; p < model.primitives.size(); ++p) {
        const auto& primitive = model.primitives[p];

        if (primitive.skin < 0 || !primitive.mesh || !primitive.mesh->hasSkin())
            continue;
        if (!m_posed[p])
            m_posed[p] = std::make_shared<Mesh>(*primitive.mesh);
        skin(*primitive.mesh, *m_posed[p], joints[static_cast<std::size_t>(primitive.skin)]);
        ++m_posed[p]->revision;
    }
    m_dirty = false;
}

void k3::Animator::draw(IRasterizer& rasterizer, const Math::Matrix4& transform)
{
    if (m_dirty)
        evaluate();
    for (std::size_t p = 0; p < m_model->primitives.size(); ++p) {
        const auto& primitive = m_model->primitives[p];

        if (m_posed[p])
            rasterizer.draw(m_posed[p], primitive.material, transform);
        else if (primitive.mesh && primitive.skin < 0)
            rasterizer.draw(primitive.mesh, primitive.material, transform * m_globals[static_cast<std::size_t>(primitive.node)]);
    }
}
