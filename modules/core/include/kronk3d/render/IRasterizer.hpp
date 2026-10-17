/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Backend-agnostic rasterizer interface
*/
#pragma once

#include <kronk3d/math/Color.hpp>
#include <kronk3d/math/Matrix.hpp>
#include <kronk3d/render/Environment.hpp>
#include <kronk3d/scene/Camera.hpp>
#include <kronk3d/scene/Image.hpp>
#include <kronk3d/scene/Material.hpp>
#include <kronk3d/scene/Mesh.hpp>
#include <kronk3d/scene/Model.hpp>
#include <kronk3d/utils/Result.hpp>
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

    // Applied by every backend with the same algorithm, so they keep producing the same image.
    enum class AntiAliasing {
        None,
        FXAA,       // Post-process edge smoothing on the final image: cheap, slightly blurry
        SSAA        // 2x2 supersampling: renders 4x the pixels and averages them (in linear space): exact, costly
    };

    // A rectangle of the output image, in pixels: origin at the top-left, y down.
    struct Rect
    {
        float x = 0.f, y = 0.f, width = 0.f, height = 0.f;
    };

    struct FrameStats
    {
        std::size_t drawCalls = 0;
        std::size_t triangles = 0;      // Submitted, before culling / clipping
        std::size_t culledDraws = 0;    // Draws outside the view, not rendered by the main pass (they can still cast shadows)
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
        AntiAliasing  antiAliasing = AntiAliasing::None;
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

            virtual void setAntiAliasing(AntiAliasing mode) = 0;
            [[nodiscard]] virtual AntiAliasing antiAliasing() const noexcept = 0;

            virtual void beginFrame(const Camera& camera, const Environment& environment) = 0;

            // Starts a layer: what is drawn from now on goes over everything recorded before, seen by `camera`
            // with a cleared depth buffer, so it never goes through the scene. For a first-person weapon:
            // the player's camera with another `fovY` and `nearPlane`, the weapon placed in the world in front
            // of it. The layer shares the frame's environment: the lights (in world space), the fog, and the
            // shadows, which it receives but does not cast. The skybox only shows behind the first layer.
            // Layers can be stacked; a layer's draws are culled with its own camera.
            virtual void beginLayer(const Camera& camera) = 0;

            // A null material draws with a default (white, lit) one.
            virtual void draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material, const Math::Matrix4& transform) = 0;

            // Draws every primitive of the model with the same transform.
            void draw(const Model& model, const Math::Matrix4& transform = Math::Matrix4::identity());

            // 2D overlay (HUD, menus): recorded like draw(), then rendered by endFrame() over the finished 3D
            // frame, after anti-aliasing so it stays sharp, in submission order. It is alpha-blended in linear
            // space and works the same on every backend. Positions are in output pixels, whatever the
            // anti-aliasing mode.
            //
            // `source` is the part of the texture to show, in texels of its first level; an empty one means all
            // of it (a negative width or height flips the image). `tint` multiplies the texels, and the whole
            // rectangle is filled with it when there is no texture. Textures are sampled with their own
            // Sampler (nearest for pixel art) and, like on meshes, their mipmaps when shown smaller.
            virtual void draw2D(std::shared_ptr<Texture> texture, const Rect& destination, const Rect& source, const Math::Color& tint) = 0;

            void drawRect(const Rect& destination, const Math::Color& color);
            void drawImage(std::shared_ptr<Texture> texture, const Rect& destination, const Rect& source = {}, const Math::Color& tint = Math::Color::White);

            virtual void endFrame() = 0;

            // Last rendered frame, sRGB-encoded RGBA8, top row first. A GPU readback on hardware backends: slow.
            [[nodiscard]] virtual Image readPixels() = 0;

            [[nodiscard]] virtual const FrameStats& stats() const noexcept = 0;
    };

    Result<std::unique_ptr<IRasterizer>> createRasterizer(Backend backend, const RasterizerConfig& config = {});

}
