#include "ImagePresenter.hpp"
#include "GL.hpp"
#include "Program.hpp"
#include "Shaders.hpp"

k3::Result<std::unique_ptr<k3::ImagePresenter>> k3::ImagePresenter::create(const GLLoader& loader)
{
    if (!loader)
        return Error{"OpenGL: no function loader given"};
    if (const auto missing = gl::load(loader); !missing.empty())
        return Error{"OpenGL: missing " + missing.front() + " (is a 3.3 core context current?)"};

    auto program = gl::Program::create(gl::shaders::PRESENT_VERTEX, gl::shaders::PRESENT_FRAGMENT);
    if (!program)
        return Error{"OpenGL: present shader: " + program.error()};

    std::unique_ptr<ImagePresenter> presenter(new ImagePresenter());
    presenter->m_imageLocation = program->uniform("uImage");
    presenter->m_program = program->release();
    gl::GenTextures(1, &presenter->m_texture);
    gl::GenVertexArrays(1, &presenter->m_vao);

    gl::BindTexture(gl::TEXTURE_2D, presenter->m_texture);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAX_LEVEL, 0);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE);
    gl::BindTexture(gl::TEXTURE_2D, 0);
    return presenter;
}

k3::ImagePresenter::~ImagePresenter()
{
    gl::DeleteVertexArrays(1, &m_vao);
    gl::DeleteTextures(1, &m_texture);
    gl::DeleteProgram(m_program);
}

void k3::ImagePresenter::present(const Image& image, std::uint32_t viewportWidth, std::uint32_t viewportHeight)
{
    if (image.empty())
        return;

    gl::Disable(gl::FRAMEBUFFER_SRGB);
    gl::Disable(gl::DEPTH_TEST);
    gl::Disable(gl::CULL_FACE);
    gl::Disable(gl::BLEND);
    gl::Viewport(0, 0, static_cast<gl::GLsizei>(viewportWidth), static_cast<gl::GLsizei>(viewportHeight));

    gl::ActiveTexture(gl::TEXTURE0);
    gl::BindTexture(gl::TEXTURE_2D, m_texture);
    gl::PixelStorei(gl::UNPACK_ALIGNMENT, 1);
    gl::TexImage2D(gl::TEXTURE_2D, 0, static_cast<gl::GLint>(gl::RGBA8), static_cast<gl::GLsizei>(image.width), static_cast<gl::GLsizei>(image.height), 0, gl::RGBA, gl::UNSIGNED_BYTE, image.pixels.data());

    gl::UseProgram(m_program);
    gl::Uniform1i(m_imageLocation, 0);
    gl::BindVertexArray(m_vao);
    gl::DrawArrays(gl::TRIANGLES, 0, 3);

    gl::BindVertexArray(0);
    gl::UseProgram(0);
    gl::BindTexture(gl::TEXTURE_2D, 0);
}
