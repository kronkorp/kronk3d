#include "IRasterizer.hpp"
#include "render/opengl/HardwareRasterizer.hpp"
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
        case Backend::Software: {
            auto rasterizer = std::make_unique<SoftwareRasterizer>(config.width, config.height, config.threads);
            rasterizer->setAntiAliasing(config.antiAliasing);
            return std::unique_ptr<IRasterizer>(std::move(rasterizer));
        }
        case Backend::OpenGL: {
            auto rasterizer = HardwareRasterizer::create(config.width, config.height, config.glLoader);
            if (!rasterizer)
                return Error{rasterizer.error()};
            (*rasterizer)->setAntiAliasing(config.antiAliasing);
            return std::unique_ptr<IRasterizer>(std::move(*rasterizer));
        }
    }
    return Error{"unknown backend"};
}
