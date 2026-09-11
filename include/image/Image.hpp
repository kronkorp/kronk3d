
#pragma once

#include "pixel/Pixel.hpp"
#include <cstddef>
#include <string_view>
#include <vector>

namespace k3
{

    class Image
    {
        public:
            virtual ~Image() = default;

            virtual void save(std::string_view filename) const = 0;
    };

}
