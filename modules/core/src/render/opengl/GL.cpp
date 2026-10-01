#include "GL.hpp"

#ifdef _WIN32
    #include <windows.h>
#endif

#define K3_GL_DEFINE(name, ret, params) k3::gl::name##Proc k3::gl::name = nullptr;
K3_GL_FUNCTIONS(K3_GL_DEFINE)
#undef K3_GL_DEFINE

namespace
{

    k3::GLProc resolve(const k3::GLLoader& loader, const char* name)
    {
        k3::GLProc proc = loader ? loader(name) : nullptr;

#ifdef _WIN32
        // wglGetProcAddress (behind most loaders) signals failure with small values too, and never
        // returns OpenGL 1.1 entry points: those are exported by opengl32.dll itself.
        const auto value = reinterpret_cast<std::intptr_t>(proc);
        if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1) {
            static HMODULE opengl32 = LoadLibraryA("opengl32.dll");
            proc = opengl32 ? reinterpret_cast<k3::GLProc>(GetProcAddress(opengl32, name)) : nullptr;
        }
#endif
        return proc;
    }

}

std::vector<std::string> k3::gl::load(const GLLoader& loader)
{
    std::vector<std::string> missing;

#define K3_GL_LOAD(name, ret, params) \
    name = reinterpret_cast<name##Proc>(resolve(loader, "gl" #name)); \
    if (!name) \
        missing.emplace_back("gl" #name);
    K3_GL_FUNCTIONS(K3_GL_LOAD)
#undef K3_GL_LOAD

    return missing;
}
