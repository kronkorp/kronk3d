/*
** KRONK CORP, 2026
** KRONK3D
** File decription:
** Rasterizer class impl
*/

#include "Rasterizer.hpp"
#include "Vector.hpp"
#include <algorithm>
#include <cstddef>
#include <iterator>

k3::Rasterizer::Rasterizer(std::size_t width, std::size_t height) : m_frameBuffer(width * height), m_depthBuffer(width * height)
{
    // this->m_frameBuffer.reserve(width * height);
}

void k3::Rasterizer::draw()
{
    // To implement
}

[[nodiscard]] const k3::Rasterizer::FrameBuffer& k3::Rasterizer::framebuffer(void) const noexcept
{
    return m_frameBuffer;
}

void k3::Rasterizer::clear(const k3::Math::Vector4f& color) noexcept
{
    std::fill_n(this->m_frameBuffer.begin(), this->m_frameBuffer.size(), color);
}
