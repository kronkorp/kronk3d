
#include "core/Core.hpp"
#include <ios>
int main(void)
{
    k3::Rasterizer core;

    std::ifstream out;

    out.open("out.bmp", std::ios::app);

    core >> out;

    return 0;
}
