#pragma once

#include <kronk3d/scene/Model.hpp>
#include <filesystem>

k3::Model makeTexturedCube(const std::filesystem::path& texturePath);
