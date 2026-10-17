/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Umbrella header: the whole public API
*/
#pragma once

#include <kronk3d/math/Bounds.hpp>
#include <kronk3d/math/Color.hpp>
#include <kronk3d/math/Matrix.hpp>
#include <kronk3d/math/Vector.hpp>

#include <kronk3d/scene/Animation.hpp>
#include <kronk3d/scene/Animator.hpp>
#include <kronk3d/scene/BillboardBatch.hpp>
#include <kronk3d/scene/Camera.hpp>
#include <kronk3d/scene/Image.hpp>
#include <kronk3d/scene/Light.hpp>
#include <kronk3d/scene/Material.hpp>
#include <kronk3d/scene/Mesh.hpp>
#include <kronk3d/scene/Model.hpp>
#include <kronk3d/scene/Texture.hpp>

#include <kronk3d/io/GltfLoader.hpp>
#include <kronk3d/io/ObjLoader.hpp>

#include <kronk3d/render/Environment.hpp>
#include <kronk3d/render/IRasterizer.hpp>
#include <kronk3d/render/Skybox.hpp>
#include <kronk3d/render/opengl/HardwareRasterizer.hpp>
#include <kronk3d/render/opengl/ImagePresenter.hpp>
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
