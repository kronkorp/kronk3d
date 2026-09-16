
#pragma once

#include <string_view>

namespace k3
{

    class Image
    {
        public:
            virtual ~Image() = default;

            virtual void save(std::string_view filename) const = 0;
    };

}
