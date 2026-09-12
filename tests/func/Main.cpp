
#include "kronk3d/core/Core.hpp"
#include "kronk3d/image/BmpImage.hpp"
#include "kronk3d/mesh/Mesh.hpp"
#include "kronk3d/utils/Color.hpp"
#include "kronk3d/utils/Matrix.hpp"
#include "kronk3d/utils/Vector.hpp"
#include "kronk3d/viewport/ViewPort.hpp"
#include <chrono>
#include <iostream>
#include <ostream>
#include <vector>

int main(void)
{
    k3::Rasterizer core(800, 600);

    core.clear(k3::Color::White);

    k3::Vector3f vertices[] = {
        {0.f, 0.5f, 0.f},
        {-0.5f, -0.5f, 0.f},
        {0.5f, -0.5f, 0.f},
    };

    k3::Color colors[] = {
        k3::Color::Red,
        k3::Color::Green,
        k3::Color::Blue,
    };

    auto point = std::chrono::high_resolution_clock::now().time_since_epoch();

    core.draw(k3::Mesh{
            {vertices},
            {colors},
            3
        },
        k3::ViewPort{
            0, 800, 0, 600
        }
    );

    std::chrono::high_resolution_clock::duration a = std::chrono::high_resolution_clock::now().time_since_epoch() - point;
    std::cout << "Took " << a << " ms to draw" << std::endl;

    k3::BmpImage image(core.framebuffer(), core.viewWidth(), core.viewHeight());
    image.save("out.bmp");

    return 0;
}
