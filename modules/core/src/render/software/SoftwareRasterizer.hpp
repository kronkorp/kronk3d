/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** CPU rasterizer
*/
#pragma once

#include "Color.hpp"
#include "render/IRasterizer.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace k3
{

    // Tile-based CPU rasterizer. Per tile, opaque geometry first fills a visibility buffer (depth +
    // triangle id), every visible pixel is then shaded exactly once, and blended geometry is drawn on
    // top, back to front. Vertex processing, triangle setup and tiles are spread over a thread pool.
    class SoftwareRasterizer final : public IRasterizer
    {
        public:
            // `threads` counts the calling thread; 0 means one per hardware thread, 1 renders single-threaded.
            SoftwareRasterizer(std::uint32_t width, std::uint32_t height, unsigned threads = 0);
            ~SoftwareRasterizer() override;

            SoftwareRasterizer(const SoftwareRasterizer&) = delete;
            SoftwareRasterizer& operator=(const SoftwareRasterizer&) = delete;

            [[nodiscard]] Backend backend() const noexcept override { return Backend::Software; }
            [[nodiscard]] std::string_view name() const noexcept override { return "Software"; }

            void resize(std::uint32_t width, std::uint32_t height) override;
            [[nodiscard]] std::uint32_t width() const noexcept override;
            [[nodiscard]] std::uint32_t height() const noexcept override;
            [[nodiscard]] unsigned threadCount() const noexcept;

            void beginFrame(const Camera& camera, const Environment& environment) override;
            using IRasterizer::draw;
            void draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material, const Math::Matrix4& transform) override;
            void endFrame() override;

            [[nodiscard]] Image readPixels() override;
            [[nodiscard]] const FrameStats& stats() const noexcept override;

            // Last rendered frame in linear space (not gamma-encoded), clamped to [0, 1], row-major, top row first.
            [[nodiscard]] const std::vector<Math::Color>& colorBuffer() const noexcept;

        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };

}
