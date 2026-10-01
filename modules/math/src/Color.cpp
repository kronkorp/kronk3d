#include "Color.hpp"

namespace k3::Math
{
    const Color Color::White       = Color(1.0, 1.0, 1.0, 1.0);
    const Color Color::Black       = Color(0, 0, 0, 1.0);
    const Color Color::Red         = Color(1.0, 0, 0, 1.0);
    const Color Color::Green       = Color(0, 1.0, 0, 1.0);
    const Color Color::Blue        = Color(0, 0, 1.0, 1.0);
    const Color Color::Yellow      = Color(1.0, 1.0, 0, 1.0);
    const Color Color::Cyan        = Color(0, 1.0, 1.0, 1.0);
    const Color Color::Magenta     = Color(1.0, 0, 1.0, 1.0);
    const Color Color::Transparent = Color(0,   0,   0,   0.0);
    const Color Color::Grey        = Color::fromRGB(128, 128, 128);
}
