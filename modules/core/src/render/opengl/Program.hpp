/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Owned GLSL program
*/
#pragma once

#include "GL.hpp"
#include "utils/Result.hpp"
#include <utility>

namespace k3::gl
{

    class Program
    {
        public:
            // Compiles and links; the error carries the driver's log.
            static Result<Program> create(const char* vertexSource, const char* fragmentSource);

            Program() = default;
            ~Program();
            Program(Program&& other) noexcept;
            Program& operator=(Program&& other) noexcept;
            Program(const Program&) = delete;
            Program& operator=(const Program&) = delete;

            [[nodiscard]] GLuint id() const noexcept { return m_id; }

            // Gives up ownership: the caller deletes the program.
            [[nodiscard]] GLuint release() noexcept { return std::exchange(m_id, 0); }
            [[nodiscard]] GLint uniform(const char* name) const { return GetUniformLocation(m_id, name); }

        private:
            explicit Program(GLuint id) : m_id(id) {}

            GLuint m_id = 0;
    };

}
