#pragma once

#include "kronk3d/utils/Vector.hpp"
#include "kronk3d/utils/Color.hpp"
#include <cstddef>
#include <vector>

namespace k3
{

    template<typename T>
    struct Box
    {
        void const *ptr = nullptr;
        size_t typeSize = sizeof(T);

        const T& operator[](size_t idx) const {
            return *reinterpret_cast<const T*>(static_cast<const char*>(ptr) + idx * typeSize);
        }
    };

    struct Mesh
    {
        // NOTE: Vertex count is vertices.size() / 3.
        Box<Vector3f> vertices     = {};
        Box<Color>    colors       = {};
        size_t*       indices      = nullptr;
        size_t        count        = 0;
    };

}
