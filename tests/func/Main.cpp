
#include "core/Core.hpp"
#include "image/BmpImage.hpp"
#include "utils/Color.hpp"

int main(void)
{
    k3::Rasterizer core(800, 600);

    core.clear(k3::Color::Green);

    k3::BmpImage image(core.framebuffer(), core.viewWidth(), core.viewHeight());
    image.save("out.bmp");

    return 0;
}
