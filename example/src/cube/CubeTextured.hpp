#pragma once

#include "scene/Model.hpp"
#include <filesystem>

k3::Model makeTexturedCube(const std::filesystem::path& texturePath);
