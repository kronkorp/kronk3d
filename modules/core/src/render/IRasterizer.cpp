#include "IRasterizer.hpp"
#include "render/software/SoftwareRasterizer.hpp"

void k3::IRasterizer::draw(const Model& model, const Math::Matrix4& transform)
{
    for (const auto& primitive : model.primitives)
        if (primitive.mesh)
            this->draw(primitive.mesh, primitive.material, transform);
}

k3::Result<std::unique_ptr<k3::IRasterizer>> k3::createRasterizer(Backend backend, const RasterizerConfig& config)
{
    switch (backend) {
        case Backend::Software:
            return std::unique_ptr<IRasterizer>(std::make_unique<SoftwareRasterizer>(config.width, config.height, config.threads));
        case Backend::OpenGL:
            break;
    }
    return Error{"backend not available in this build"};
}
