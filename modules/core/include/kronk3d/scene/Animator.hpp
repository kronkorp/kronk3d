/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Animator: plays the clips of an AnimatedModel, with crossfades
*/
#pragma once

#include <kronk3d/scene/Animation.hpp>
#include <kronk3d/math/Matrix.hpp>
#include <memory>
#include <string_view>
#include <vector>

namespace k3
{

    class IRasterizer;

    struct PlayOptions
    {
        bool  loop  = true;
        float fade  = 0.2f;     // Seconds spent blending from the clip that was playing
        float speed = 1.f;      // Negative plays backward
    };

    // One Animator per animated instance (several can share the same AnimatedModel):
    //     Animator hero(model);
    //     hero.play("Walk");
    //     each frame:  hero.update(dt);  hero.draw(*rasterizer, transform);
    // Skinned meshes are deformed on the CPU into one Mesh per primitive, edited in place (and its revision
    // bumped) at each update, so both backends draw them unchanged and the GPU buffers are reused.
    class Animator
    {
        public:
            explicit Animator(std::shared_ptr<const AnimatedModel> model);

            // A copy would share, and so deform, the same meshes.
            Animator(const Animator&) = delete;
            Animator& operator=(const Animator&) = delete;
            Animator(Animator&&) = default;
            Animator& operator=(Animator&&) = default;

            // Starts `clip`, fading from the current one. Asking for the clip that is already playing (and
            // has not finished) only updates the options, so it is safe to call every frame from game
            // logic. False when the model has no such clip.
            bool play(std::string_view clip, const PlayOptions& options = {});

            // Back to the rest pose.
            void stop();

            void update(float deltaSeconds);

            // A clip played with loop = false reached its end (and stays on its last pose).
            [[nodiscard]] bool finished() const noexcept;

            // Name of the clip playing, empty when stopped.
            [[nodiscard]] std::string_view clip() const noexcept;
            [[nodiscard]] float time() const noexcept { return m_current.time; }

            [[nodiscard]] const AnimatedModel& model() const noexcept { return *m_model; }

            // Records every primitive for the current pose; `transform` places the model in the world.
            void draw(IRasterizer& rasterizer, const Math::Matrix4& transform = Math::Matrix4::identity());

        private:
            struct Track
            {
                int   clip  = -1;
                float time  = 0.f;
                float speed = 1.f;
                bool  loop  = true;
            };

            void advance(Track& track, float deltaSeconds) const;
            void evaluate();

            std::shared_ptr<const AnimatedModel> m_model;
            Track  m_current{};
            Track  m_previous{};    // Fading out while clip >= 0
            float  m_fadeElapsed  = 0.f;
            float  m_fadeDuration = 0.f;
            bool   m_dirty = true;

            std::vector<Math::Matrix4>         m_globals{};
            std::vector<std::shared_ptr<Mesh>> m_posed{};   // Per primitive, null when rigid
    };

}
