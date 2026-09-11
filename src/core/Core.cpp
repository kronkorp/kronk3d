#include "core/CoreException.hpp"
#include <core/Core.hpp>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string_view>

void k3::Rasterizer::saveImage(const std::ifstream& file) const
{
    if (!file.is_open() || file.bad()) {
        throw FileNotOpen();
    }
    std::cout << "Saving" << std::endl;
}

const k3::Rasterizer& k3::Rasterizer::operator>>(const std::ifstream& file) const
{
    this->saveImage(file);
    return *this;
}
