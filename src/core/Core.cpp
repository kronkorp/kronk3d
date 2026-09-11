#include <core/Core.hpp>
#include <mutex>
#include <string_view>

void k3::Rasterizer::saveImage(std::string_view file) const
{
    
}

const k3::Rasterizer& k3::Rasterizer::operator>>(std::string_view file) const
{
    this->saveImage(file);
    return *this;
}
