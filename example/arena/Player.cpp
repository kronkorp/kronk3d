#include "Player.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iostream>

namespace
{

    using k3::Math::Vector3f;

    constexpr float WALK_SPEED = 2.6f;
    constexpr float RUN_SPEED = 6.5f;
    constexpr float JUMP_SPEED = 9.5f;
    constexpr float GRAVITY = 28.f;
    constexpr float TURN_SPEED = 14.f;          // How fast the model turns toward where it goes (1 / s)
    constexpr float STEP_UP = 0.1f;             // Blocks lower than this are walked over

    // The model faces +z in its own space (checked on a render of the robot).
    constexpr float MODEL_YAW = 0.f;

    float approach(float value, float target, float maxChange)
    {
        return value + std::clamp(target - value, -maxChange, maxChange);
    }

    float shortestAngle(float from, float to)
    {
        constexpr float PI = 3.14159265f;
        float d = std::fmod(to - from, 2.f * PI);

        if (d > PI)
            d -= 2.f * PI;
        if (d < -PI)
            d += 2.f * PI;
        return d;
    }

}

Player::Player(std::shared_ptr<const k3::AnimatedModel> model)
    : m_model(std::move(model)), m_animator(m_model)
{
    // Scaled to HEIGHT, centered on its own footprint, feet on the ground.
    const auto bounds = m_model->bounds();
    const auto center = bounds.center();

    m_fit = k3::Math::Matrix4::scale(HEIGHT / (bounds.max.y - bounds.min.y)) * k3::Math::Matrix4::translate({-center.x, -bounds.min.y, -center.z});
    m_animator.play("idle");
}

std::optional<Player> Player::create(const std::filesystem::path& assets)
{
    auto loaded = k3::GltfLoader::load(assets / "robot" / "player.glb");

    if (!loaded) {
        std::cerr << "Cannot load the robot: " << loaded.error() << std::endl;
        return std::nullopt;
    }
    return Player(std::make_shared<const k3::AnimatedModel>(std::move(*loaded)));
}

void Player::update(float dt, const Input& input, float cameraYaw, const Level& level)
{
    // Where the camera looks, flattened; "right" is a quarter turn clockwise from above.
    const Vector3f forward{std::sin(cameraYaw), 0.f, -std::cos(cameraYaw)};
    const Vector3f right{std::cos(cameraYaw), 0.f, std::sin(cameraYaw)};
    Vector3f wish = forward * input.forward + right * input.right;

    if (Vector3f::length(wish) > 1.f)
        wish = Vector3f::normalize(wish);

    m_running = input.run;
    const float speed = input.run ? RUN_SPEED : WALK_SPEED;
    const float acceleration = (m_grounded ? 45.f : 14.f) * dt;

    m_velocity.x = approach(m_velocity.x, wish.x * speed, acceleration);
    m_velocity.z = approach(m_velocity.z, wish.z * speed, acceleration);

    if (m_grounded && input.jump) {
        m_velocity.y = JUMP_SPEED;
        m_grounded = false;
        m_animator.play("jump", {.loop = false, .fade = 0.08f});
    }
    m_velocity.y -= GRAVITY * dt;

    m_position.x += m_velocity.x * dt;
    m_position.z += m_velocity.z * dt;
    collide(level);

    const float previousFeet = m_position.y;
    m_position.y += m_velocity.y * dt;

    const float ground = groundHeight(level, previousFeet);
    if (m_position.y <= ground && m_velocity.y <= 0.f) {
        m_position.y = ground;
        m_velocity.y = 0.f;
        m_grounded = true;
    } else {
        m_grounded = false;
    }

    if (std::hypot(m_velocity.x, m_velocity.z) > 0.3f)
        m_facing += shortestAngle(m_facing, std::atan2(m_velocity.x, m_velocity.z)) * std::min(1.f, TURN_SPEED * dt);

    animate();
    m_animator.update(dt);
}

// Pushes the cylinder out of every block it overlaps on the ground plane (two passes for corners).
void Player::collide(const Level& level)
{
    for (int pass = 0; pass < 2; ++pass) {
        for (const Box& box : level.solids()) {
            // Blocks the feet are above (stairs, crates seen from their top) do not block.
            if (box.max.y <= m_position.y + STEP_UP || box.min.y >= m_position.y + HEIGHT)
                continue;

            const float nearestX = std::clamp(m_position.x, box.min.x, box.max.x);
            const float nearestZ = std::clamp(m_position.z, box.min.z, box.max.z);
            const float dx = m_position.x - nearestX, dz = m_position.z - nearestZ;
            const float distance = std::hypot(dx, dz);

            if (distance >= RADIUS)
                continue;
            if (distance > 1e-5f) {
                m_position.x = nearestX + dx / distance * RADIUS;
                m_position.z = nearestZ + dz / distance * RADIUS;
            } else {
                // The center is inside the block: leave through the nearest side.
                const float left = m_position.x - box.min.x, rightSide = box.max.x - m_position.x;
                const float back = m_position.z - box.min.z, front = box.max.z - m_position.z;
                const float smallest = std::min({left, rightSide, back, front});

                if (smallest == left)
                    m_position.x = box.min.x - RADIUS;
                else if (smallest == rightSide)
                    m_position.x = box.max.x + RADIUS;
                else if (smallest == back)
                    m_position.z = box.min.z - RADIUS;
                else
                    m_position.z = box.max.z + RADIUS;
            }
        }
    }
    const float limit = Level::HALF_SIZE - RADIUS;
    m_position.x = std::clamp(m_position.x, -limit, limit);
    m_position.z = std::clamp(m_position.z, -limit, limit);
}

// The highest surface under the feet that they came from above (the floor is 0).
float Player::groundHeight(const Level& level, float previousFeet) const
{
    constexpr float EDGE = 0.2f;        // The center only has to be this close to a block's edge to stand on it
    float ground = 0.f;

    for (const Box& box : level.solids()) {
        if (box.max.y > previousFeet + STEP_UP)
            continue;
        if (m_position.x > box.min.x - EDGE && m_position.x < box.max.x + EDGE && m_position.z > box.min.z - EDGE && m_position.z < box.max.z + EDGE)
            ground = std::max(ground, box.max.y);
    }
    return ground;
}

void Player::animate()
{
    const float planar = std::hypot(m_velocity.x, m_velocity.z);

    if (!m_grounded) {
        const bool takingOff = m_animator.clip() == "jump" && !m_animator.finished();

        if (!takingOff)
            m_animator.play("falling", {.fade = 0.12f});
    } else if (planar < 0.3f) {
        m_animator.play("idle", {.fade = 0.15f});
    } else {
        m_animator.play(m_running ? "run" : "walk", {.fade = 0.15f});
    }
}

void Player::draw(k3::IRasterizer& rasterizer)
{
    const auto placement = k3::Math::Matrix4::translate(m_position) * k3::Math::Matrix4::rotate({0.f, 1.f, 0.f}, m_facing + MODEL_YAW) * m_fit;

    m_animator.draw(rasterizer, placement);
}

void Player::toggleFlatShading()
{
    for (const auto& primitive : m_model->primitives) {
        if (primitive.material)
            primitive.material->flatShading = !primitive.material->flatShading;
    }
}
