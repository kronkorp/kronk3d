#include <kronk3d/render/IRasterizer.hpp>
#include <kronk3d/render/opengl/HardwareRasterizer.hpp>
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
#include <vector>

void k3::IRasterizer::draw(const Model& model, const Math::Matrix4& transform)
{
    for (const auto& primitive : model.primitives)
        if (primitive.mesh)
            this->draw(primitive.mesh, primitive.material, transform);
}

void k3::IRasterizer::drawLine(const Math::Vector3f& from, const Math::Vector3f& to, const Math::Color& color, bool depthTested)
{
    const DebugLine line{from, to, color};

    drawLines({&line, 1}, depthTested);
}

void k3::IRasterizer::drawBounds(const Math::Bounds3f& bounds, const Math::Color& color, const Math::Matrix4& transform, bool depthTested)
{
    if (bounds.empty())
        return;

    Math::Vector3f corner[8];
    for (int i = 0; i < 8; ++i) {
        corner[i] = transform.transformPoint({
            (i & 1) ? bounds.max.x : bounds.min.x,
            (i & 2) ? bounds.max.y : bounds.min.y,
            (i & 4) ? bounds.max.z : bounds.min.z,
        });
    }

    // Two corners are joined when they differ by one bit: the edges of the box.
    std::vector<DebugLine> lines;
    for (int i = 0; i < 8; ++i) {
        for (int bit = 1; bit < 8; bit <<= 1) {
            if (!(i & bit))
                lines.push_back({corner[i], corner[i | bit], color});
        }
    }
    drawLines(lines, depthTested);
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
