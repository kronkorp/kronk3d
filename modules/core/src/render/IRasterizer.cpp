#include <kronk3d/render/IRasterizer.hpp>
#include <kronk3d/render/opengl/HardwareRasterizer.hpp>
#include <kronk3d/render/software/SoftwareRasterizer.hpp>

void k3::IRasterizer::draw(const Model& model, const Math::Matrix4& transform)
{
    for (const auto& primitive : model.primitives)
        if (primitive.mesh)
            this->draw(primitive.mesh, primitive.material, transform);
}

void k3::IRasterizer::drawRect(const Rect& destination, const Math::Color& color)
{
    draw2D(nullptr, destination, {}, color);
}

void k3::IRasterizer::drawImage(std::shared_ptr<Texture> texture, const Rect& destination, const Rect& source, const Math::Color& tint)
{
    draw2D(std::move(texture), destination, source, tint);
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
