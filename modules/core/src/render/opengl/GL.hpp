/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Minimal OpenGL 3.3 core loader (no system GL header needed, works the same on every platform)
*/
#pragma once

#include "render/IRasterizer.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32) && !defined(_WIN64)
    #define K3_GL_APIENTRY __stdcall
#else
    #define K3_GL_APIENTRY
#endif

namespace k3::gl
{

    using GLenum     = unsigned int;
    using GLboolean  = unsigned char;
    using GLbitfield = unsigned int;
    using GLint      = int;
    using GLuint     = unsigned int;
    using GLsizei    = int;
    using GLfloat    = float;
    using GLdouble   = double;
    using GLchar     = char;
    using GLubyte    = unsigned char;
    using GLsizeiptr = std::ptrdiff_t;
    using GLintptr   = std::ptrdiff_t;

    // Only the constants kronk3d uses (named without "GL_", and avoiding <windows.h> macros).
    inline constexpr GLboolean FALSE_               = 0;
    inline constexpr GLboolean TRUE_                = 1;
    inline constexpr GLenum VERSION                 = 0x1F02;
    inline constexpr GLenum RENDERER                = 0x1F01;
    inline constexpr GLenum DEPTH_BUFFER_BIT        = 0x00000100;
    inline constexpr GLenum COLOR_BUFFER_BIT        = 0x00004000;
    inline constexpr GLenum TRIANGLES               = 0x0004;
    inline constexpr GLenum LESS                    = 0x0201;
    inline constexpr GLenum SRC_ALPHA               = 0x0302;
    inline constexpr GLenum ONE_MINUS_SRC_ALPHA     = 0x0303;
    inline constexpr GLenum ONE                     = 1;
    inline constexpr GLenum FRONT                   = 0x0404;
    inline constexpr GLenum BACK                    = 0x0405;
    inline constexpr GLenum CCW                     = 0x0901;
    inline constexpr GLenum CULL_FACE               = 0x0B44;
    inline constexpr GLenum DEPTH_TEST              = 0x0B71;
    inline constexpr GLenum BLEND                   = 0x0BE2;
    inline constexpr GLenum SCISSOR_TEST            = 0x0C11;
    inline constexpr GLenum STENCIL_TEST            = 0x0B90;
    inline constexpr GLenum PACK_ALIGNMENT          = 0x0D05;
    inline constexpr GLenum UNPACK_ALIGNMENT        = 0x0CF5;
    inline constexpr GLenum TEXTURE_2D              = 0x0DE1;
    inline constexpr GLenum UNSIGNED_BYTE           = 0x1401;
    inline constexpr GLenum UNSIGNED_INT            = 0x1405;
    inline constexpr GLenum FLOAT                   = 0x1406;
    inline constexpr GLenum DEPTH_COMPONENT         = 0x1902;
    inline constexpr GLenum RED                     = 0x1903;
    inline constexpr GLenum RGBA                    = 0x1908;
    inline constexpr GLenum NEAREST                 = 0x2600;
    inline constexpr GLenum LINEAR                  = 0x2601;
    inline constexpr GLenum NEAREST_MIPMAP_NEAREST  = 0x2700;
    inline constexpr GLenum LINEAR_MIPMAP_NEAREST   = 0x2701;
    inline constexpr GLenum NEAREST_MIPMAP_LINEAR   = 0x2702;
    inline constexpr GLenum LINEAR_MIPMAP_LINEAR    = 0x2703;
    inline constexpr GLenum TEXTURE_MAG_FILTER      = 0x2800;
    inline constexpr GLenum TEXTURE_MIN_FILTER      = 0x2801;
    inline constexpr GLenum TEXTURE_WRAP_S          = 0x2802;
    inline constexpr GLenum TEXTURE_WRAP_T          = 0x2803;
    inline constexpr GLenum REPEAT                  = 0x2901;
    inline constexpr GLenum CLAMP_TO_EDGE           = 0x812F;
    inline constexpr GLenum MIRRORED_REPEAT         = 0x8370;
    inline constexpr GLenum TEXTURE_BASE_LEVEL      = 0x813C;
    inline constexpr GLenum TEXTURE_MAX_LEVEL       = 0x813D;
    inline constexpr GLenum TEXTURE0                = 0x84C0;
    inline constexpr GLenum RGBA8                   = 0x8058;
    inline constexpr GLenum SRGB8_ALPHA8            = 0x8C43;
    inline constexpr GLenum DEPTH_COMPONENT24       = 0x81A6;
    inline constexpr GLenum DEPTH_COMPONENT32F      = 0x8CAC;
    inline constexpr GLenum TEXTURE_COMPARE_MODE    = 0x884C;
    inline constexpr GLenum ARRAY_BUFFER            = 0x8892;
    inline constexpr GLenum ELEMENT_ARRAY_BUFFER    = 0x8893;
    inline constexpr GLenum STATIC_DRAW             = 0x88E4;
    inline constexpr GLenum FRAGMENT_SHADER         = 0x8B30;
    inline constexpr GLenum VERTEX_SHADER           = 0x8B31;
    inline constexpr GLenum COMPILE_STATUS          = 0x8B81;
    inline constexpr GLenum LINK_STATUS             = 0x8B82;
    inline constexpr GLenum INFO_LOG_LENGTH         = 0x8B84;
    inline constexpr GLenum FRAMEBUFFER_SRGB        = 0x8DB9;
    inline constexpr GLenum READ_FRAMEBUFFER        = 0x8CA8;
    inline constexpr GLenum DRAW_FRAMEBUFFER        = 0x8CA9;
    inline constexpr GLenum FRAMEBUFFER             = 0x8D40;
    inline constexpr GLenum RENDERBUFFER            = 0x8D41;
    inline constexpr GLenum FRAMEBUFFER_COMPLETE    = 0x8CD5;
    inline constexpr GLenum COLOR_ATTACHMENT0       = 0x8CE0;
    inline constexpr GLenum DEPTH_ATTACHMENT        = 0x8D00;
    inline constexpr GLenum NONE                    = 0;
    inline constexpr GLenum DRAW_FRAMEBUFFER_BINDING = 0x8CA6;

    // name, return type, parameter list. Every function is reachable as k3::gl::<name> (without "gl").
    #define K3_GL_FUNCTIONS(X) \
        X(GetError,                 GLenum,         (void)) \
        X(GetString,                const GLubyte*, (GLenum)) \
        X(GetIntegerv,              void,           (GLenum, GLint*)) \
        X(Enable,                   void,           (GLenum)) \
        X(Disable,                  void,           (GLenum)) \
        X(Viewport,                 void,           (GLint, GLint, GLsizei, GLsizei)) \
        X(ClearColor,               void,           (GLfloat, GLfloat, GLfloat, GLfloat)) \
        X(ClearDepth,               void,           (GLdouble)) \
        X(Clear,                    void,           (GLbitfield)) \
        X(DepthFunc,                void,           (GLenum)) \
        X(DepthMask,                void,           (GLboolean)) \
        X(ColorMask,                void,           (GLboolean, GLboolean, GLboolean, GLboolean)) \
        X(CullFace,                 void,           (GLenum)) \
        X(FrontFace,                void,           (GLenum)) \
        X(BlendFuncSeparate,        void,           (GLenum, GLenum, GLenum, GLenum)) \
        X(PixelStorei,              void,           (GLenum, GLint)) \
        X(ReadPixels,               void,           (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
        X(Finish,                   void,           (void)) \
        X(GenTextures,              void,           (GLsizei, GLuint*)) \
        X(DeleteTextures,           void,           (GLsizei, const GLuint*)) \
        X(BindTexture,              void,           (GLenum, GLuint)) \
        X(ActiveTexture,            void,           (GLenum)) \
        X(TexImage2D,               void,           (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
        X(TexParameteri,            void,           (GLenum, GLenum, GLint)) \
        X(GenBuffers,               void,           (GLsizei, GLuint*)) \
        X(DeleteBuffers,            void,           (GLsizei, const GLuint*)) \
        X(BindBuffer,               void,           (GLenum, GLuint)) \
        X(BufferData,               void,           (GLenum, GLsizeiptr, const void*, GLenum)) \
        X(BufferSubData,            void,           (GLenum, GLintptr, GLsizeiptr, const void*)) \
        X(GenVertexArrays,          void,           (GLsizei, GLuint*)) \
        X(DeleteVertexArrays,       void,           (GLsizei, const GLuint*)) \
        X(BindVertexArray,          void,           (GLuint)) \
        X(EnableVertexAttribArray,  void,           (GLuint)) \
        X(DisableVertexAttribArray, void,           (GLuint)) \
        X(VertexAttribPointer,      void,           (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
        X(VertexAttrib4f,           void,           (GLuint, GLfloat, GLfloat, GLfloat, GLfloat)) \
        X(DrawArrays,               void,           (GLenum, GLint, GLsizei)) \
        X(DrawElements,             void,           (GLenum, GLsizei, GLenum, const void*)) \
        X(CreateShader,             GLuint,         (GLenum)) \
        X(ShaderSource,             void,           (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
        X(CompileShader,            void,           (GLuint)) \
        X(GetShaderiv,              void,           (GLuint, GLenum, GLint*)) \
        X(GetShaderInfoLog,         void,           (GLuint, GLsizei, GLsizei*, GLchar*)) \
        X(DeleteShader,             void,           (GLuint)) \
        X(CreateProgram,            GLuint,         (void)) \
        X(AttachShader,             void,           (GLuint, GLuint)) \
        X(BindAttribLocation,       void,           (GLuint, GLuint, const GLchar*)) \
        X(BindFragDataLocation,     void,           (GLuint, GLuint, const GLchar*)) \
        X(LinkProgram,              void,           (GLuint)) \
        X(GetProgramiv,             void,           (GLuint, GLenum, GLint*)) \
        X(GetProgramInfoLog,        void,           (GLuint, GLsizei, GLsizei*, GLchar*)) \
        X(DeleteProgram,            void,           (GLuint)) \
        X(UseProgram,               void,           (GLuint)) \
        X(GetUniformLocation,       GLint,          (GLuint, const GLchar*)) \
        X(Uniform1i,                void,           (GLint, GLint)) \
        X(Uniform1f,                void,           (GLint, GLfloat)) \
        X(Uniform3f,                void,           (GLint, GLfloat, GLfloat, GLfloat)) \
        X(Uniform4f,                void,           (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
        X(Uniform1iv,               void,           (GLint, GLsizei, const GLint*)) \
        X(Uniform1fv,               void,           (GLint, GLsizei, const GLfloat*)) \
        X(Uniform3fv,               void,           (GLint, GLsizei, const GLfloat*)) \
        X(UniformMatrix4fv,         void,           (GLint, GLsizei, GLboolean, const GLfloat*)) \
        X(GenFramebuffers,          void,           (GLsizei, GLuint*)) \
        X(DeleteFramebuffers,       void,           (GLsizei, const GLuint*)) \
        X(BindFramebuffer,          void,           (GLenum, GLuint)) \
        X(FramebufferTexture2D,     void,           (GLenum, GLenum, GLenum, GLuint, GLint)) \
        X(CheckFramebufferStatus,   GLenum,         (GLenum)) \
        X(BlitFramebuffer,          void,           (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
        X(DrawBuffer,               void,           (GLenum)) \
        X(ReadBuffer,               void,           (GLenum))

    #define K3_GL_DECLARE(name, ret, params) \
        using name##Proc = ret (K3_GL_APIENTRY*) params; \
        extern name##Proc name;
    K3_GL_FUNCTIONS(K3_GL_DECLARE)
    #undef K3_GL_DECLARE

    // Resolves every function through `loader` (the context must be current). Returns the names that
    // could not be found: empty means success. Process-wide, like any GL loader.
    std::vector<std::string> load(const GLLoader& loader);

}
