#pragma once

#include "scene/Model.hpp"

// Vertex-colored cube inside a double-sided blue glass box (AlphaMode::Blend), in front of an
// alpha-tested lattice (AlphaMode::Mask) whose texture is generated in code.
k3::Model makeTransparencyScene();
