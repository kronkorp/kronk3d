
#pragma once

#include <fstream>
#include <string_view>
#include <system_error>

namespace k3
{

    class Rasterizer
    {
        public:
            Rasterizer() = default;
            ~Rasterizer() = default;

            void saveImage(const std::ifstream& file) const;
            const Rasterizer& operator>>(const std::ifstream& file) const;

        private:
            // Private members
    };

}
