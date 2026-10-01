#include "Program.hpp"
#include <string>
#include <utility>

namespace
{

    k3::Result<k3::gl::GLuint> compile(k3::gl::GLenum type, const char* source)
    {
        using namespace k3::gl;

        const GLuint shader = CreateShader(type);
        ShaderSource(shader, 1, &source, nullptr);
        CompileShader(shader);

        GLint ok = 0;
        GetShaderiv(shader, COMPILE_STATUS, &ok);
        if (ok)
            return shader;

        GLint length = 0;
        GetShaderiv(shader, INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
        GetShaderInfoLog(shader, length, nullptr, log.data());
        DeleteShader(shader);
        return k3::Error{std::string(type == VERTEX_SHADER ? "vertex" : "fragment") + " shader: " + log};
    }

}

k3::Result<k3::gl::Program> k3::gl::Program::create(const char* vertexSource, const char* fragmentSource)
{
    auto vertex = compile(VERTEX_SHADER, vertexSource);
    if (!vertex)
        return Error{vertex.error()};

    auto fragment = compile(FRAGMENT_SHADER, fragmentSource);
    if (!fragment) {
        DeleteShader(*vertex);
        return Error{fragment.error()};
    }

    const GLuint program = CreateProgram();
    AttachShader(program, *vertex);
    AttachShader(program, *fragment);
    LinkProgram(program);
    DeleteShader(*vertex);
    DeleteShader(*fragment);

    GLint ok = 0;
    GetProgramiv(program, LINK_STATUS, &ok);
    if (ok)
        return Program(program);

    GLint length = 0;
    GetProgramiv(program, INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
    GetProgramInfoLog(program, length, nullptr, log.data());
    DeleteProgram(program);
    return Error{"link: " + log};
}

k3::gl::Program::~Program()
{
    if (m_id)
        DeleteProgram(m_id);
}

k3::gl::Program::Program(Program&& other) noexcept
    : m_id(std::exchange(other.m_id, 0))
{
}

k3::gl::Program& k3::gl::Program::operator=(Program&& other) noexcept
{
    if (this != &other) {
        if (m_id)
            DeleteProgram(m_id);
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}
