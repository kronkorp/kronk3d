
#pragma once

#include <string_view>
#include <system_error>

namespace k3
{

    class Rasterizer
    {
        public:
            Rasterizer() = default;
            ~Rasterizer() = default;

            void saveImage(std::string_view file) const;
            const Rasterizer& operator>>(std::string_view file) const;

        private:
            // Private members
    };

}
