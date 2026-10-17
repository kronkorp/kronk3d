#pragma once

#include <kronk3d/scene/Model.hpp>

// Large checkered floor at height `y`, to receive shadows (texture generated in code).
k3::Model makeFloor(float halfSize, float y);
