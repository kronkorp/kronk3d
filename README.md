# kronk3d

A small C++23 3D rasterization library with two interchangeable backends behind a single interface:

- **Software**: a multithreaded, tile-based CPU rasterizer that renders into memory and needs no GPU or window.
- **OpenGL 3.3 core**: renders into the OpenGL context your application already has.

Both backends produce the same image: the shading code is mirrored, and tests compare their outputs pixel by pixel.

## Features

- **Rendering API**
  - A frame is `beginFrame(camera, environment)`, any number of `draw(...)` calls, then `endFrame()`.
  - Backends are created at runtime with `createRasterizer(Backend::Software | Backend::OpenGL)`.
- **Assets**
  - Wavefront **OBJ + MTL** loading: polygons with any number of sides, negative indices, vertex colors.
  - Normals are generated when the file has none (smooth, or flat with `s off`).
  - Material properties `Kd Ks Ke Ns d Tr illum` and textures `map_Kd map_Ks map_d`.
- **Lighting**
  - Per-pixel Blinn-Phong, computed in linear space with sRGB output.
  - Ambient, directional, point and spot lights (up to 8).
  - Shadow maps for a directional light, with PCF soft edges and the alpha test applied to casters.
- **Textures**
  - Mipmaps built in linear space for sRGB textures.
  - Nearest / bilinear / trilinear filtering; repeat, mirrored and clamp wrap modes, with OpenGL sampler semantics.
- **Transparency**
  - Alpha test (`AlphaMode::Mask`).
  - Alpha blending (`AlphaMode::Blend`), sorted back to front. Double-sided blended meshes draw their back faces first.
- **Software backend**
  - Exact rasterization in 24.8 fixed point with the top-left fill rule.
  - Clipping against the near/far planes and a guard band.
  - A visibility buffer, so each opaque pixel is shaded exactly once.
  - Every stage runs on all cores, and the output does not depend on the thread count.
- **Portability**: Linux (GCC, Clang) and Windows (MSVC, MinGW), all checked in CI.

## Building

Requirements: CMake 3.20+ and a C++23 compiler (GCC 13+, Clang 18+, or MSVC from Visual Studio 2022 or newer).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/bin/kronk3d_example
```

On Windows (Visual Studio), add `--config Release` to the build and `-C Release` to ctest.

The library itself has no dependency: stb_image and stb_image_write are vendored. SFML 2.6 is only used
by the example and the OpenGL tests, to open a window. CMake uses the system SFML when there is one,
and otherwise downloads and builds it, so nothing has to be installed by hand on Windows.

| Option | Default | Effect |
|---|---|---|
| `K3_BUILD_EXAMPLES` | ON when top-level | Builds `kronk3d_example` |
| `K3_BUILD_TESTS` | ON when top-level | Builds `kronk3d_tests` |
| `K3_BUILD_GL_TESTS` | `K3_BUILD_TESTS` | Builds `kronk3d_gl_tests`. These tests skip when no OpenGL 3.3 context is available, unless `K3_REQUIRE_GL` is set |

The CMake targets are `kronk3d_static` and `kronk3d_shared`.

## Quick start

```cpp
#include "Kronk3d.hpp"

// Assets
auto model = k3::ObjLoader::load("assets/spot/spot.obj");     // k3::Result<k3::Model>
if (!model)
    return std::cerr << model.error() << '\n', 1;

// Backend. Software needs nothing. OpenGL needs a 3.3 core context to be current, and its function
// loader (SFML here, glfwGetProcAddress with GLFW...); the software backend ignores the loader.
auto rasterizer = k3::createRasterizer(k3::Backend::OpenGL, {
    .width = 800, .height = 600,
    .glLoader = [](const char* name) { return sf::Context::getFunction(name); },
});
if (!rasterizer)
    return std::cerr << rasterizer.error() << '\n', 1;

// Scene
k3::Camera camera;
camera.position = {0.f, 1.f, 4.f};
camera.lookAt({0.f, 0.f, 0.f});

k3::Environment environment;
environment.lights.push_back(k3::Light::directional({-0.5f, -1.f, -0.6f}));
environment.lights.back().castShadows = true;

// Frame
(*rasterizer)->beginFrame(camera, environment);
(*rasterizer)->draw(*model, k3::Math::Matrix4::rotateZX(angle));
(*rasterizer)->endFrame();

k3::Image frame = (*rasterizer)->readPixels();   // sRGB RGBA8, top row first
frame.savePNG("frame.png");
```

The OpenGL backend copies each frame to the window's framebuffer during `endFrame()`; you only need to
swap buffers. To show the software backend's frames in an OpenGL window, use `k3::ImagePresenter`
(this is how the example switches backends at runtime).

Meshes, materials and textures are passed as `shared_ptr`. The OpenGL backend caches what it uploads
and frees it once nothing references it anymore. To change a texture, call `Texture::setImage` (which
bumps its version); to change a mesh, replace it with a new one.

## Example

`kronk3d_example [--backend software|opengl] [--scene 1-4] [--screenshot file.png]`

| Key | Action |
|---|---|
| `1`-`4` | Textured cube, vertex-colored cube, Spot (OBJ), transparency |
| `Tab` | Switch between the software and OpenGL backends |
| `ZQSD` / `WASD`, `Space`, `Shift` | Move |
| Arrow keys, right mouse drag | Look around |

## Conventions

- **Coordinates**: right-handed, y up; the camera looks toward -Z. Front faces are counter-clockwise (as in OpenGL and OBJ).
- **Matrices**: row-major (`Matrix4::values`), column vectors (`M * v`); the depth range is OpenGL's.
- **UVs**: `(0, 0)` is the top-left of the image. The OBJ loader flips `v` (`ObjLoadOptions::flipV`).
- **Colors**: every `Math::Color` is linear. Use `Color::fromSRGB` for colors taken from an image editor. Textures declare their color space (sRGB for colors, linear for masks).
- **Attenuation**: point and spot lights fall off as `(1 - (d / range)^4)^2 / (d^2 + 1)`.

## Layout

```
modules/
  math/    Vectors, matrices, bounds, colors (header-only)
  logger/  Logger
  core/src/
    scene/     Image, Texture, Mesh, Material, Model, Camera, Light
    io/        ObjLoader
    render/    IRasterizer, Environment, ShadowFit
      software/  SoftwareRasterizer: pipeline, clipper, sampling, shading, thread pool
      opengl/    HardwareRasterizer, ImagePresenter, GL loader, shaders
  test/
    src/  CPU unit tests
    gl/   OpenGL tests (including a pixel comparison with the software backend)
example/  Interactive demo (SFML window)
```
