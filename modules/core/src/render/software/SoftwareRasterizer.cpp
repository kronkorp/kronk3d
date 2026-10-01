#include "SoftwareRasterizer.hpp"
#include "Clipper.hpp"
#include "Pipeline.hpp"
#include "Shading.hpp"
#include "ThreadPool.hpp"
#include "utils/Srgb.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace
{

    // Tiles are rendered independently (and in parallel): a tile's buffers stay in cache while all its
    // triangles are drawn, and no two threads ever touch the same pixel.
    constexpr std::int32_t TILE_SIZE = 64;

    // Work granularity of the parallel geometry stages.
    constexpr std::size_t VERTEX_CHUNK = 4096;
    constexpr std::size_t TRIANGLE_CHUNK = 2048;

    struct DrawCommand
    {
        std::shared_ptr<const k3::Mesh>     mesh;
        std::shared_ptr<const k3::Material> material;
        k3::Math::Matrix4                   transform;
        float                               viewDepth = 0.f;    // Blended draws are sorted on it
    };

    const k3::Material& defaultMaterial()
    {
        static const k3::Material material{};
        return material;
    }

    // Which faces a geometry pass keeps.
    enum class Faces {
        All,
        Front,
        Back
    };

    bool isBlended(const DrawCommand& command)
    {
        return command.material && command.material->alphaMode == k3::AlphaMode::Blend;
    }

    // Per-draw matrices, computed once per frame.
    struct DrawTransforms
    {
        k3::Math::Matrix4 modelViewProjection;
        k3::Math::Matrix4 model;
        k3::Math::Matrix4 normal;
    };

    struct VertexJob
    {
        std::uint32_t draw;
        std::size_t   first, last;          // Vertex range
    };

    struct GeometryJob
    {
        std::uint32_t draw;
        Faces         faces;
        bool          blended;
        std::size_t   first, last;          // Index range, multiple of 3
    };

    // Triangles overlapping a tile, in submission order.
    struct TileBin
    {
        std::vector<const k3::sw::Triangle*> opaque;
        std::vector<const k3::sw::Triangle*> blended;
    };

    // Calls fn(x, y, edges) for every pixel of `t` inside the [x0, x1] x [y0, y1] rectangle.
    template<typename Fn>
    void forEachCoveredPixel(const k3::sw::Triangle& t, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, Fn&& fn)
    {
        const std::int32_t xs = std::max(t.minX, x0), xe = std::min(t.maxX, x1);
        const std::int32_t ys = std::max(t.minY, y0), ye = std::min(t.maxY, y1);

        if (xs > xe || ys > ye)
            return;

        std::int64_t row[3];
        t.edgesAt(xs, ys, row);

        const std::int64_t stepX[3] = {t.edgeA[0] * k3::sw::SUBPIXEL_ONE, t.edgeA[1] * k3::sw::SUBPIXEL_ONE, t.edgeA[2] * k3::sw::SUBPIXEL_ONE};
        const std::int64_t stepY[3] = {t.edgeB[0] * k3::sw::SUBPIXEL_ONE, t.edgeB[1] * k3::sw::SUBPIXEL_ONE, t.edgeB[2] * k3::sw::SUBPIXEL_ONE};

        for (std::int32_t y = ys; y <= ye; ++y) {
            std::int64_t e[3] = {row[0], row[1], row[2]};

            for (std::int32_t x = xs; x <= xe; ++x) {
                // Inside when no edge function is negative: a single sign-bit test.
                if ((e[0] | e[1] | e[2]) >= 0)
                    fn(x, y, e);
                e[0] += stepX[0];
                e[1] += stepX[1];
                e[2] += stepX[2];
            }
            row[0] += stepY[0];
            row[1] += stepY[1];
            row[2] += stepY[2];
        }
    }

}

struct k3::SoftwareRasterizer::Impl
{
    explicit Impl(unsigned threads) : pool(threads) {}

    std::uint32_t width = 0, height = 0;
    std::uint32_t tilesX = 0, tilesY = 0;
    sw::ThreadPool pool;

    std::vector<Math::Color>          color;
    std::vector<float>                depth;
    std::vector<const sw::Triangle*>  visibility;       // Nearest opaque triangle of each pixel

    Camera             camera{};
    Environment        environment{};
    Math::Matrix4      view = Math::Matrix4::identity();
    Math::Matrix4      viewProjection = Math::Matrix4::identity();
    sw::ShadingContext shading{};

    // Frame data, indexed by draw (position in `commands` once sorted).
    std::vector<DrawCommand>                 commands;
    std::vector<sw::DrawState>               drawStates;
    std::vector<DrawTransforms>              transforms;
    std::vector<std::vector<sw::ClipVertex>> vertices;

    std::vector<VertexJob>                   vertexJobs;
    std::vector<GeometryJob>                 geometryJobs;
    std::vector<std::vector<sw::Triangle>>   jobTriangles;   // Output of each geometry job (stable during the frame)
    std::vector<TileBin>                     bins;

    FrameStats stats{};

    void resize(std::uint32_t w, std::uint32_t h)
    {
        width = w;
        height = h;
        tilesX = (w + TILE_SIZE - 1) / TILE_SIZE;
        tilesY = (h + TILE_SIZE - 1) / TILE_SIZE;
        color.assign(static_cast<std::size_t>(w) * h, Math::Color::Black);
        depth.assign(static_cast<std::size_t>(w) * h, 1.f);
        visibility.assign(static_cast<std::size_t>(w) * h, nullptr);
        bins.assign(static_cast<std::size_t>(tilesX) * tilesY, {});
    }

    /* Frame setup (sequential) */

    // Fills the per-draw data and splits the frame into vertex and geometry jobs. Geometry jobs are in
    // drawing order: opaque draws, then blended ones (`firstBlended` onward), back faces before front
    // faces for double-sided blended materials.
    void planJobs(std::size_t firstBlended)
    {
        drawStates.clear();
        transforms.clear();
        vertexJobs.clear();
        geometryJobs.clear();
        if (vertices.size() < commands.size())
            vertices.resize(commands.size());

        for (std::size_t d = 0; d < commands.size(); ++d) {
            const DrawCommand& command = commands[d];
            const Mesh& mesh = *command.mesh;
            const Material& material = command.material ? *command.material : defaultMaterial();
            const auto draw = static_cast<std::uint32_t>(d);

            drawStates.push_back({&material, material.alphaMode == AlphaMode::Mask, mesh.hasNormals(), mesh.hasColors()});
            transforms.push_back({viewProjection * command.transform, command.transform, command.transform.normalMatrix()});
            vertices[d].resize(mesh.vertexCount());
            for (std::size_t first = 0; first < mesh.vertexCount(); first += VERTEX_CHUNK)
                vertexJobs.push_back({draw, first, std::min(first + VERTEX_CHUNK, mesh.vertexCount())});
        }

        auto addGeometry = [&](std::size_t d, Faces faces, bool blended) {
            const std::size_t indexCount = commands[d].mesh->indexCount() / 3 * 3;

            for (std::size_t first = 0; first < indexCount; first += TRIANGLE_CHUNK * 3)
                geometryJobs.push_back({static_cast<std::uint32_t>(d), faces, blended, first, std::min(first + TRIANGLE_CHUNK * 3, indexCount)});
        };

        for (std::size_t d = 0; d < firstBlended; ++d)
            addGeometry(d, drawStates[d].material->doubleSided ? Faces::All : Faces::Front, false);
        // A double-sided blended mesh (a glass box) shows its inside through its outside: draw the faces
        // turned away from the camera first, so they end up behind whatever the triangle order is.
        for (std::size_t d = firstBlended; d < commands.size(); ++d) {
            if (drawStates[d].material->doubleSided)
                addGeometry(d, Faces::Back, true);
            addGeometry(d, Faces::Front, true);
        }

        if (jobTriangles.size() < geometryJobs.size())
            jobTriangles.resize(geometryJobs.size());
        for (std::size_t j = 0; j < geometryJobs.size(); ++j)
            jobTriangles[j].clear();
    }

    /* Geometry (parallel): vertex transform, then clipping and triangle setup */

    void transformVertices(const VertexJob& job)
    {
        const Mesh& mesh = *commands[job.draw].mesh;
        const sw::DrawState& state = drawStates[job.draw];
        const DrawTransforms& m = transforms[job.draw];
        const bool hasUVs = mesh.hasUVs();
        auto& out = vertices[job.draw];

        for (std::size_t i = job.first; i < job.last; ++i) {
            const auto& p = mesh.positions[i];
            const auto world = m.model.transformPoint(p);
            const auto normal = state.hasNormals ? m.normal.transformDirection(mesh.normals[i]) : Math::Vector3f{};
            const auto uv = hasUVs ? mesh.uvs[i] : Math::Vector2f{};
            const auto c = state.hasColors ? mesh.colors[i] : Math::Color::White;
            auto& v = out[i];

            v.position = m.modelViewProjection * p.asPoint();
            v.varyings[sw::WorldX] = world.x;
            v.varyings[sw::WorldY] = world.y;
            v.varyings[sw::WorldZ] = world.z;
            v.varyings[sw::NormalX] = normal.x;
            v.varyings[sw::NormalY] = normal.y;
            v.varyings[sw::NormalZ] = normal.z;
            v.varyings[sw::TexU] = uv.x;
            v.varyings[sw::TexV] = uv.y;
            v.varyings[sw::ColorR] = c.r;
            v.varyings[sw::ColorG] = c.g;
            v.varyings[sw::ColorB] = c.b;
            v.varyings[sw::ColorA] = c.a;
        }
    }

    void assemble(const GeometryJob& job, std::vector<sw::Triangle>& out) const
    {
        const Mesh& mesh = *commands[job.draw].mesh;
        const auto& transformed = vertices[job.draw];
        const bool hasNormals = drawStates[job.draw].hasNormals;
        const std::size_t vertexCount = mesh.vertexCount();
        sw::ClipVertex polygon[sw::MAX_CLIPPED_VERTICES];

        for (std::size_t t = job.first; t < job.last; t += 3) {
            const auto i0 = mesh.index(t), i1 = mesh.index(t + 1), i2 = mesh.index(t + 2);

            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                continue;

            sw::ClipVertex a = transformed[i0], b = transformed[i1], c = transformed[i2];

            if (!hasNormals)
                setFaceNormal(a, b, c);

            const std::size_t count = sw::clipTriangle(a, b, c, polygon);
            for (std::size_t k = 1; k + 1 < count; ++k)
                setupTriangle(polygon[0], polygon[k], polygon[k + 1], job.draw, job.faces, out);
        }
    }

    static void setFaceNormal(sw::ClipVertex& a, sw::ClipVertex& b, sw::ClipVertex& c)
    {
        auto world = [](const sw::ClipVertex& v) {
            return Math::Vector3f{v.varyings[sw::WorldX], v.varyings[sw::WorldY], v.varyings[sw::WorldZ]};
        };
        const auto n = Math::Vector3f::cross(world(b) - world(a), world(c) - world(a));

        for (auto* v : {&a, &b, &c}) {
            v->varyings[sw::NormalX] = n.x;
            v->varyings[sw::NormalY] = n.y;
            v->varyings[sw::NormalZ] = n.z;
        }
    }

    void setupTriangle(
        const sw::ClipVertex& v0, const sw::ClipVertex& v1, const sw::ClipVertex& v2,
        std::uint32_t draw, Faces faces, std::vector<sw::Triangle>& out
    ) const
    {
        const sw::ClipVertex* in[3] = {&v0, &v1, &v2};
        std::int64_t x[3], y[3];
        float z[3], invW[3];

        for (int i = 0; i < 3; ++i) {
            const auto& p = in[i]->position;
            invW[i] = 1.f / p.w;
            x[i] = std::llround((p.x * invW[i] * 0.5f + 0.5f) * width * sw::SUBPIXEL_ONE);
            y[i] = std::llround((0.5f - p.y * invW[i] * 0.5f) * height * sw::SUBPIXEL_ONE);
            z[i] = p.z * invW[i] * 0.5f + 0.5f;
        }

        std::int64_t area = (x[1] - x[0]) * (y[2] - y[0]) - (y[1] - y[0]) * (x[2] - x[0]);
        if (area == 0)
            return;

        // y points down on screen: counter-clockwise in NDC (front facing) has a negative area here.
        const bool frontFacing = area < 0;
        if ((faces == Faces::Front && !frontFacing) || (faces == Faces::Back && frontFacing))
            return;

        int order[3] = {0, 1, 2};
        if (area < 0) {
            std::swap(order[1], order[2]);
            area = -area;
        }

        sw::Triangle t;
        std::int64_t X[3], Y[3];

        for (int k = 0; k < 3; ++k) {
            const int src = order[k];
            X[k] = x[src];
            Y[k] = y[src];
            t.z[k] = z[src];
            t.invW[k] = invW[src];
            std::copy(std::begin(in[src]->varyings), std::end(in[src]->varyings), t.varyings[k]);
        }

        for (int i = 0; i < 3; ++i) {
            const int a = (i + 1) % 3, b = (i + 2) % 3;
            const std::int64_t A = Y[a] - Y[b];
            const std::int64_t B = X[b] - X[a];
            // Top-left fill rule: a pixel center exactly on an edge belongs to the triangle only if the
            // edge is a top or a left one, so that triangles sharing an edge never both draw it.
            const bool topLeft = A > 0 || (A == 0 && B > 0);

            t.edgeA[i] = A;
            t.edgeB[i] = B;
            t.edgeC[i] = -(A * X[a] + B * Y[a]) - (topLeft ? 0 : 1);
        }

        for (int axis = 0; axis < 2; ++axis) {
            float q = 0.f, qu = 0.f, qv = 0.f;

            for (int i = 0; i < 3; ++i) {
                const float step = static_cast<float>((axis == 0 ? t.edgeA[i] : t.edgeB[i]) * sw::SUBPIXEL_ONE) * t.invW[i];
                q += step;
                qu += step * t.varyings[i][sw::TexU];
                qv += step * t.varyings[i][sw::TexV];
            }
            t.qStep[axis] = q;
            t.uStep[axis] = qu;
            t.vStep[axis] = qv;
        }

        t.invArea = 1.f / static_cast<float>(area);
        t.minX = static_cast<std::int32_t>(std::max<std::int64_t>(0, std::min({X[0], X[1], X[2]}) >> sw::SUBPIXEL_BITS));
        t.minY = static_cast<std::int32_t>(std::max<std::int64_t>(0, std::min({Y[0], Y[1], Y[2]}) >> sw::SUBPIXEL_BITS));
        t.maxX = static_cast<std::int32_t>(std::min<std::int64_t>(width - 1, std::max({X[0], X[1], X[2]}) >> sw::SUBPIXEL_BITS));
        t.maxY = static_cast<std::int32_t>(std::min<std::int64_t>(height - 1, std::max({Y[0], Y[1], Y[2]}) >> sw::SUBPIXEL_BITS));
        if (t.minX > t.maxX || t.minY > t.maxY)
            return;

        t.draw = draw;
        t.frontFacing = frontFacing;
        out.push_back(t);
    }

    // Sequential, walking the jobs in drawing order: bins keep the submission order blending relies on.
    void bin()
    {
        for (auto& tile : bins) {
            tile.opaque.clear();
            tile.blended.clear();
        }

        for (std::size_t j = 0; j < geometryJobs.size(); ++j) {
            const bool blended = geometryJobs[j].blended;

            for (const sw::Triangle& t : jobTriangles[j]) {
                for (std::int32_t ty = t.minY / TILE_SIZE; ty <= t.maxY / TILE_SIZE; ++ty) {
                    for (std::int32_t tx = t.minX / TILE_SIZE; tx <= t.maxX / TILE_SIZE; ++tx) {
                        TileBin& tile = bins[static_cast<std::size_t>(ty) * tilesX + tx];
                        (blended ? tile.blended : tile.opaque).push_back(&t);
                    }
                }
            }
        }
    }

    /* Rasterization (parallel over tiles) */

    sw::Fragment fragmentAt(const sw::Triangle& t, const std::int64_t e[3]) const noexcept
    {
        // Perspective-correct weights: screen-space barycentrics divided by w, renormalized
        // (which also cancels the 1 / area factor).
        float l0 = static_cast<float>(e[0]) * t.invW[0];
        float l1 = static_cast<float>(e[1]) * t.invW[1];
        float l2 = static_cast<float>(e[2]) * t.invW[2];
        const float inv = 1.f / (l0 + l1 + l2);     // 1 / q

        l0 *= inv;
        l1 *= inv;
        l2 *= inv;

        const bool hasColors = drawStates[t.draw].hasColors;
        const int count = hasColors ? sw::VARYING_COUNT : sw::ColorR;
        float v[sw::VARYING_COUNT];

        for (int k = 0; k < count; ++k)
            v[k] = l0 * t.varyings[0][k] + l1 * t.varyings[1][k] + l2 * t.varyings[2][k];

        const float u = v[sw::TexU], tv = v[sw::TexV];

        return {
            {v[sw::WorldX], v[sw::WorldY], v[sw::WorldZ]},
            {v[sw::NormalX], v[sw::NormalY], v[sw::NormalZ]},
            {u, tv},
            {(t.uStep[0] - u * t.qStep[0]) * inv, (t.vStep[0] - tv * t.qStep[0]) * inv},
            {(t.uStep[1] - u * t.qStep[1]) * inv, (t.vStep[1] - tv * t.qStep[1]) * inv},
            hasColors ? Math::Color{v[sw::ColorR], v[sw::ColorG], v[sw::ColorB], v[sw::ColorA]} : Math::Color::White,
            t.frontFacing,
        };
    }

    static float depthAt(const sw::Triangle& t, const std::int64_t e[3]) noexcept
    {
        // Depth is affine in screen space: plain (not perspective-corrected) barycentrics.
        return (static_cast<float>(e[0]) * t.z[0] + static_cast<float>(e[1]) * t.z[1] + static_cast<float>(e[2]) * t.z[2]) * t.invArea;
    }

    void renderTile(std::uint32_t tx, std::uint32_t ty)
    {
        const std::int32_t x0 = static_cast<std::int32_t>(tx) * TILE_SIZE;
        const std::int32_t y0 = static_cast<std::int32_t>(ty) * TILE_SIZE;
        const std::int32_t x1 = std::min<std::int32_t>(x0 + TILE_SIZE, width) - 1;
        const std::int32_t y1 = std::min<std::int32_t>(y0 + TILE_SIZE, height) - 1;

        for (std::int32_t y = y0; y <= y1; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * width;
            std::fill(color.begin() + row + x0, color.begin() + row + x1 + 1, environment.clearColor);
            std::fill(depth.begin() + row + x0, depth.begin() + row + x1 + 1, 1.f);
            std::fill(visibility.begin() + row + x0, visibility.begin() + row + x1 + 1, nullptr);
        }

        const TileBin& tile = bins[static_cast<std::size_t>(ty) * tilesX + tx];

        // 1. Opaque and alpha-tested geometry: keep the nearest triangle of every pixel.
        for (const sw::Triangle* triangle : tile.opaque) {
            const sw::Triangle& t = *triangle;
            const sw::DrawState& state = drawStates[t.draw];

            forEachCoveredPixel(t, x0, y0, x1, y1, [&](std::int32_t x, std::int32_t y, const std::int64_t e[3]) {
                const std::size_t index = static_cast<std::size_t>(y) * width + x;
                const float z = depthAt(t, e);

                if (!(z < depth[index]))
                    return;
                if (state.alphaTest && sw::coverage(*state.material, fragmentAt(t, e)) < state.material->alphaCutoff)
                    return;
                depth[index] = z;
                visibility[index] = triangle;
            });
        }

        // 2. Shade each visible pixel once.
        for (std::int32_t y = y0; y <= y1; ++y) {
            for (std::int32_t x = x0; x <= x1; ++x) {
                const std::size_t index = static_cast<std::size_t>(y) * width + x;
                const sw::Triangle* t = visibility[index];

                if (!t)
                    continue;

                std::int64_t e[3];
                t->edgesAt(x, y, e);

                Math::Color c = sw::shade(shading, *drawStates[t->draw].material, fragmentAt(*t, e));
                c.a = 1.f;
                color[index] = c;
            }
        }

        // 3. Blended geometry, already sorted back to front: depth-tested against the opaque result, not written.
        for (const sw::Triangle* triangle : tile.blended) {
            const sw::Triangle& t = *triangle;
            const Material& material = *drawStates[t.draw].material;

            forEachCoveredPixel(t, x0, y0, x1, y1, [&](std::int32_t x, std::int32_t y, const std::int64_t e[3]) {
                const std::size_t index = static_cast<std::size_t>(y) * width + x;

                if (!(depthAt(t, e) < depth[index]))
                    return;

                const Math::Color src = sw::shade(shading, material, fragmentAt(t, e));
                const float a = std::clamp(src.a, 0.f, 1.f);
                Math::Color& dst = color[index];

                dst.r = src.r * a + dst.r * (1.f - a);
                dst.g = src.g * a + dst.g * (1.f - a);
                dst.b = src.b * a + dst.b * (1.f - a);
                dst.a = a + dst.a * (1.f - a);
            });
        }
    }
};

k3::SoftwareRasterizer::SoftwareRasterizer(std::uint32_t width, std::uint32_t height, unsigned threads)
    : m_impl(std::make_unique<Impl>(threads))
{
    m_impl->resize(width, height);
}

k3::SoftwareRasterizer::~SoftwareRasterizer() = default;

void k3::SoftwareRasterizer::resize(std::uint32_t width, std::uint32_t height)
{
    m_impl->resize(width, height);
}

std::uint32_t k3::SoftwareRasterizer::width() const noexcept
{
    return m_impl->width;
}

std::uint32_t k3::SoftwareRasterizer::height() const noexcept
{
    return m_impl->height;
}

unsigned k3::SoftwareRasterizer::threadCount() const noexcept
{
    return m_impl->pool.size();
}

void k3::SoftwareRasterizer::beginFrame(const Camera& camera, const Environment& environment)
{
    auto& impl = *m_impl;
    const float aspect = impl.height ? static_cast<float>(impl.width) / impl.height : 1.f;

    impl.camera = camera;
    impl.environment = environment;
    impl.view = camera.view();
    impl.viewProjection = camera.projectionMatrix(aspect) * impl.view;
    impl.shading = sw::ShadingContext::prepare(camera, environment);
    impl.commands.clear();
    impl.stats = {};
}

void k3::SoftwareRasterizer::draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material, const Math::Matrix4& transform)
{
    if (!mesh)
        return;
    m_impl->stats.drawCalls++;
    m_impl->stats.triangles += mesh->triangleCount();
    m_impl->commands.push_back({std::move(mesh), std::move(material), transform});
}

void k3::SoftwareRasterizer::endFrame()
{
    auto& impl = *m_impl;
    const auto start = std::chrono::steady_clock::now();

    // Opaque and alpha-tested draws keep their order, blended ones go last, farthest first.
    const auto firstBlended = std::stable_partition(impl.commands.begin(), impl.commands.end(), [](const DrawCommand& c) { return !isBlended(c); });
    for (auto it = firstBlended; it != impl.commands.end(); ++it)
        it->viewDepth = -impl.view.transformPoint(it->transform.transformPoint(it->mesh->bounds().center())).z;
    std::stable_sort(firstBlended, impl.commands.end(), [](const DrawCommand& a, const DrawCommand& b) { return a.viewDepth > b.viewDepth; });

    impl.planJobs(static_cast<std::size_t>(firstBlended - impl.commands.begin()));
    impl.pool.parallelFor(impl.vertexJobs.size(), [&](std::size_t j) {
        impl.transformVertices(impl.vertexJobs[j]);
    });
    impl.pool.parallelFor(impl.geometryJobs.size(), [&](std::size_t j) {
        impl.assemble(impl.geometryJobs[j], impl.jobTriangles[j]);
    });
    impl.bin();
    impl.pool.parallelFor(static_cast<std::size_t>(impl.tilesX) * impl.tilesY, [&](std::size_t tile) {
        impl.renderTile(static_cast<std::uint32_t>(tile % impl.tilesX), static_cast<std::uint32_t>(tile / impl.tilesX));
    });

    // Draw states point into the recorded materials: drop both together.
    impl.drawStates.clear();
    impl.commands.clear();
    impl.stats.frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

k3::Image k3::SoftwareRasterizer::readPixels()
{
    auto& impl = *m_impl;
    Image image;

    image.width = impl.width;
    image.height = impl.height;
    image.pixels.resize(impl.color.size() * 4);

    const std::uint8_t* encode = srgb::encodeTable();
    impl.pool.parallelFor(impl.height, [&](std::size_t y) {
        for (std::size_t i = y * impl.width; i < (y + 1) * impl.width; ++i) {
            const Math::Color& c = impl.color[i];

            image.pixels[i * 4 + 0] = srgb::encode(c.r, encode);
            image.pixels[i * 4 + 1] = srgb::encode(c.g, encode);
            image.pixels[i * 4 + 2] = srgb::encode(c.b, encode);
            image.pixels[i * 4 + 3] = static_cast<std::uint8_t>(std::clamp(c.a, 0.f, 1.f) * 255.f + 0.5f);
        }
    });
    return image;
}

const k3::FrameStats& k3::SoftwareRasterizer::stats() const noexcept
{
    return m_impl->stats;
}

const std::vector<k3::Math::Color>& k3::SoftwareRasterizer::colorBuffer() const noexcept
{
    return m_impl->color;
}
