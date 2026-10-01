/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** OpenGL 3.3 core rasterizer
*/
#pragma once

#include "render/IRasterizer.hpp"
#include "utils/Result.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace k3
{

    // GPU rasterizer on OpenGL 3.3 core: same features and same shading as SoftwareRasterizer.
    //
    // kronk3d does not own any window: create a 3.3+ core context with your windowing library (SFML,
    // GLFW, SDL, Qt...), make it current, and pass its function loader. Every call (destructor
    // included) must happen with that context current.
    //
    // The frame is rendered into an internal sRGB framebuffer, then copied at the end of endFrame() to
    // the present framebuffer (0 by default: the window's back buffer, ready to be swapped).
    // Meshes and textures are uploaded on first use and freed once nothing references them anymore.
    class HardwareRasterizer final : public IRasterizer
    {
        public:
            static Result<std::unique_ptr<HardwareRasterizer>> create(std::uint32_t width, std::uint32_t height, const GLLoader& loader);
            ~HardwareRasterizer() override;

            HardwareRasterizer(const HardwareRasterizer&) = delete;
            HardwareRasterizer& operator=(const HardwareRasterizer&) = delete;

            [[nodiscard]] Backend backend() const noexcept override { return Backend::OpenGL; }
            [[nodiscard]] std::string_view name() const noexcept override { return "OpenGL"; }

            void resize(std::uint32_t width, std::uint32_t height) override;
            [[nodiscard]] std::uint32_t width() const noexcept override;
            [[nodiscard]] std::uint32_t height() const noexcept override;

            void beginFrame(const Camera& camera, const Environment& environment) override;
            using IRasterizer::draw;
            void draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material, const Math::Matrix4& transform) override;
            void endFrame() override;

            [[nodiscard]] Image readPixels() override;
            [[nodiscard]] const FrameStats& stats() const noexcept override;

            // Framebuffer object the frame is copied to by endFrame(); nullopt keeps it internal only
            // (readPixels() still works).
            void setPresentFramebuffer(std::optional<std::uint32_t> framebuffer) noexcept;

            // Driver description, e.g. "4.6 (Core Profile) Mesa 25.2 / AMD Radeon 610M".
            [[nodiscard]] const std::string& driver() const noexcept;

        private:
            struct Impl;

            explicit HardwareRasterizer(std::unique_ptr<Impl> impl);

            std::unique_ptr<Impl> m_impl;
    };

}
