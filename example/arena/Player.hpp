/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** The controllable robot: movement, jumping, collisions and animation
*/
#pragma once

#include "Level.hpp"
#include <kronk3d/Kronk3d.hpp>
#include <filesystem>
#include <memory>
#include <optional>

struct Input
{
    float right = 0.f, forward = 0.f;       // [-1, 1], relative to the camera
    bool  run = false;
    bool  jump = false;
};

class Player
{
    public:
        static constexpr float HEIGHT = 1.8f;
        static constexpr float RADIUS = 0.35f;

        // Empty when the model cannot be loaded.
        static std::optional<Player> create(const std::filesystem::path& assets);

        // `cameraYaw` gives "forward": the character moves where the camera looks.
        void update(float dt, const Input& input, float cameraYaw, const Level& level);
        void draw(k3::IRasterizer& rasterizer);

        [[nodiscard]] const k3::Math::Vector3f& position() const noexcept { return m_position; }
        [[nodiscard]] std::string_view clip() const noexcept { return m_animator.clip(); }

    private:
        explicit Player(std::shared_ptr<const k3::AnimatedModel> model);

        void collide(const Level& level);
        [[nodiscard]] float groundHeight(const Level& level, float previousFeet) const;
        void animate();

        std::shared_ptr<const k3::AnimatedModel> m_model;
        k3::Animator                             m_animator;
        k3::Math::Matrix4                        m_fit;         // Model space -> 1.8 m tall, feet at the origin

        k3::Math::Vector3f m_position{0.f, 0.f, 0.f};           // Feet
        k3::Math::Vector3f m_velocity{0.f, 0.f, 0.f};
        float              m_facing = 0.f;                      // Yaw around +y
        bool               m_grounded = true;
        bool               m_running = false;
};
