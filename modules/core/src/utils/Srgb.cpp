#include "Srgb.hpp"
#include "Color.hpp"
#include <cmath>
#include <vector>

namespace
{

    struct Tables
    {
        float decode[256];
        std::vector<std::uint8_t> encode;

        Tables() : encode(k3::srgb::ENCODE_SIZE)
        {
            for (int i = 0; i < 256; ++i)
                decode[i] = k3::Math::srgbToLinear(i / 255.f);
            for (std::size_t i = 0; i < k3::srgb::ENCODE_SIZE; ++i)
                encode[i] = static_cast<std::uint8_t>(std::lround(k3::Math::linearToSrgb(i / float(k3::srgb::ENCODE_SIZE - 1)) * 255.f));
        }
    };

    // Function-local: safe to use from other static initializers (e.g. a global Texture).
    const Tables& tables()
    {
        static const Tables instance;
        return instance;
    }

}

const float* k3::srgb::decodeTable() noexcept
{
    return tables().decode;
}

const std::uint8_t* k3::srgb::encodeTable() noexcept
{
    return tables().encode.data();
}
