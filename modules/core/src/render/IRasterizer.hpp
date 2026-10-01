/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Backend-agnostic rasterizer interface
*/
#pragma once

#include "Matrix.hpp"
#include "render/Environment.hpp"
#include "scene/Camera.hpp"
#include "scene/Image.hpp"
#include "scene/Material.hpp"
#include "scene/Mesh.hpp"
#include "scene/Model.hpp"
#include "utils/Result.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace k3
{

    enum class Backend {
        Software,   // CPU, renders into memory: works anywhere, no window or GPU needed
        OpenGL      // OpenGL 3.3 core, renders into the GL context current on the calling thread
    };

    struct FrameStats
    {
        std::size_t drawCalls = 0;
        std::size_t triangles = 0;      // Submitted, before culling / clipping
        double      frameMs   = 0.0;    // CPU time spent in endFrame()
    };

    // OpenGL function loader, e.g. glfwGetProcAddress, sf::Context::getFunction or a lambda around SDL_GL_GetProcAddress.
    using GLProc   = void (*)();
    using GLLoader = std::function<GLProc(const char* name)>;

    struct RasterizerConfig
    {
        std::uint32_t width   = 800;
        std::uint32_t height  = 600;
        unsigned      threads = 0;          // Software: worker threads, 0 = one per hardware thread
        GLLoader      glLoader{};           // OpenGL: required, and the context must be current
    };

    // A frame is:
    //     beginFrame(camera, environment);
    //     draw(...);   // any number of times
    //     endFrame();
    // Draws are only recorded: everything is rendered in endFrame(), which is what lets the backends
    // sort transparent geometry and render shadow maps before shading.
    //
    // Meshes, materials and textures are shared_ptrs so that GPU backends can cache their uploads and
    // free them once the last reference is gone. In-place edits are not detected: change a Texture
    // through setImage() (which bumps its version()), and replace a Mesh rather than editing it.
    class IRasterizer
    {
        public:
            virtual ~IRasterizer() = default;

            [[nodiscard]] virtual Backend backend() const noexcept = 0;
            [[nodiscard]] virtual std::string_view name() const noexcept = 0;

            virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
            [[nodiscard]] virtual std::uint32_t width() const noexcept = 0;
            [[nodiscard]] virtual std::uint32_t height() const noexcept = 0;

            virtual void beginFrame(const Camera& camera, const Environment& environment) = 0;

            // A null material draws with a default (white, lit) one.
            virtual void draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material, const Math::Matrix4& transform) = 0;

            // Draws every primitive of the model with the same transform.
            void draw(const Model& model, const Math::Matrix4& transform = Math::Matrix4::identity());

            virtual void endFrame() = 0;

            // Last rendered frame, sRGB-encoded RGBA8, top row first. A GPU readback on hardware backends: slow.
            [[nodiscard]] virtual Image readPixels() = 0;

            [[nodiscard]] virtual const FrameStats& stats() const noexcept = 0;
    };

    Result<std::unique_ptr<IRasterizer>> createRasterizer(Backend backend, const RasterizerConfig& config = {});

}
