/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Shows a CPU image in an OpenGL context
*/
#pragma once

#include "render/IRasterizer.hpp"
#include "scene/Image.hpp"
#include "utils/Result.hpp"
#include <cstdint>
#include <memory>

namespace k3
{

    // Draws an Image (e.g. SoftwareRasterizer::readPixels()) over the currently bound framebuffer of an
    // OpenGL 3.3 core context: the way to display the software backend in a GL window, side by side
    // with the hardware one. The image is copied as is (it is already sRGB-encoded).
    class ImagePresenter
    {
        public:
            static Result<std::unique_ptr<ImagePresenter>> create(const GLLoader& loader);
            ~ImagePresenter();

            ImagePresenter(const ImagePresenter&) = delete;
            ImagePresenter& operator=(const ImagePresenter&) = delete;

            // Stretches `image` over a viewport of the given size, at (0, 0).
            void present(const Image& image, std::uint32_t viewportWidth, std::uint32_t viewportHeight);

        private:
            ImagePresenter() = default;

            unsigned      m_program = 0;
            unsigned      m_texture = 0;
            unsigned      m_vao = 0;
            int           m_imageLocation = -1;
    };

}
