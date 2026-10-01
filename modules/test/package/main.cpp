// Renders a red triangle with the installed library and checks a pixel.
#include "Kronk3d.hpp"
#include <iostream>
#include <memory>

int main()
{
    auto rasterizer = k3::createRasterizer(k3::Backend::Software, {.width = 32, .height = 32});
    if (!rasterizer)
        return 1;

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;

    auto triangle = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {0.f, 1.f, 0.f}},
    });
    auto red = std::make_shared<k3::Material>();
    red->diffuse = k3::Math::Color::Red;
    red->unlit = true;

    (*rasterizer)->beginFrame(camera, k3::Environment{});
    (*rasterizer)->draw(triangle, red, k3::Math::Matrix4::identity());
    (*rasterizer)->endFrame();

    const k3::Image image = (*rasterizer)->readPixels();
    const bool ok = image.texel(16, 20)[0] == 255 && image.texel(16, 20)[1] == 0;
    std::cout << (ok ? "kronk3d package OK" : "kronk3d package: wrong pixel") << std::endl;
    return ok ? 0 : 1;
}
