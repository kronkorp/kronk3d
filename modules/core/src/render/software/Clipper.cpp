#include "Clipper.hpp"
#include <algorithm>
#include <iterator>

namespace
{

    constexpr float GUARD_BAND = 4.f;

    // A vertex is inside a plane when dot(plane, position) >= 0.
    constexpr k3::Math::Vector4f PLANES[] = {
        {0.f, 0.f,  1.f, 1.f},              // z > -w (near)
        {0.f, 0.f, -1.f, 1.f},              // z <  w (far)
        { 1.f, 0.f, 0.f, GUARD_BAND},       // x > -G w
        {-1.f, 0.f, 0.f, GUARD_BAND},       // x <  G w
        {0.f,  1.f, 0.f, GUARD_BAND},       // y > -G w
        {0.f, -1.f, 0.f, GUARD_BAND},       // y <  G w
    };

    k3::sw::ClipVertex lerp(const k3::sw::ClipVertex& a, const k3::sw::ClipVertex& b, float t)
    {
        k3::sw::ClipVertex v;

        v.position = (1.f - t) * a.position + t * b.position;
        for (int i = 0; i < k3::sw::VARYING_COUNT; ++i)
            v.varyings[i] = a.varyings[i] + t * (b.varyings[i] - a.varyings[i]);
        return v;
    }

    // Bit p is set when the vertex is outside plane p.
    unsigned outcode(const k3::sw::ClipVertex& v)
    {
        unsigned code = 0;

        for (std::size_t p = 0; p < std::size(PLANES); ++p)
            if (k3::Math::Vector4f::dot(v.position, PLANES[p]) < 0.f)
                code |= 1u << p;
        return code;
    }

}

std::size_t k3::sw::clipTriangle(const ClipVertex& v0, const ClipVertex& v1, const ClipVertex& v2, ClipVertex out[MAX_CLIPPED_VERTICES])
{
    const unsigned c0 = outcode(v0), c1 = outcode(v1), c2 = outcode(v2);

    // All three vertices outside the same plane: nothing left.
    if (c0 & c1 & c2)
        return 0;

    out[0] = v0;
    out[1] = v1;
    out[2] = v2;

    const unsigned crossed = c0 | c1 | c2;
    if (crossed == 0)
        return 3;

    // Sutherland-Hodgman, only against the planes something crosses.
    ClipVertex scratch[MAX_CLIPPED_VERTICES];
    ClipVertex* input = out;
    ClipVertex* output = scratch;
    std::size_t count = 3;

    for (std::size_t p = 0; p < std::size(PLANES) && count > 0; ++p) {
        if (!(crossed & (1u << p)))
            continue;

        std::size_t written = 0;
        for (std::size_t i = 0; i < count; ++i) {
            const ClipVertex& a = input[i];
            const ClipVertex& b = input[(i + 1) % count];
            const float da = Math::Vector4f::dot(a.position, PLANES[p]);
            const float db = Math::Vector4f::dot(b.position, PLANES[p]);

            if (da >= 0.f)
                output[written++] = a;
            if ((da >= 0.f) != (db >= 0.f))
                output[written++] = lerp(a, b, da / (da - db));
        }
        count = written;
        std::swap(input, output);
    }

    if (input != out)
        std::copy(input, input + count, out);
    return count;
}
