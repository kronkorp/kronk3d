#include "SoftwareRasterizer.hpp"
#include "Clipper.hpp"
#include "Pipeline.hpp"
#include "Shading.hpp"
#include "ThreadPool.hpp"
#include "render/ShadowFit.hpp"
#include "utils/Srgb.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
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
        k3::Math::Bounds3f                  bounds{};           // World space, computed in endFrame()
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

    // Per-draw matrices, computed once per frame and shared by every pass.
    struct DrawTransforms
    {
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

    // Pixel bounds of a triangle, stored apart so binning streams 16 bytes per triangle instead of the
    // whole triangle.
    struct TriangleBounds
    {
        std::int32_t minX, minY, maxX, maxY;
    };

    // Triangles overlapping a tile, in submission order.
    struct TileBin
    {
        std::vector<const k3::sw::Triangle*> opaque;
        std::vector<const k3::sw::Triangle*> blended;
    };

    // One rasterization of (some of) the frame's draws into a target: the main view or the shadow map.
    struct Pass
    {
        k3::Math::Matrix4                            viewProjection = k3::Math::Matrix4::identity();
        std::uint32_t                                width = 0, height = 0;
        std::uint32_t                                tilesX = 0, tilesY = 0;
        std::vector<std::vector<k3::sw::ClipVertex>> vertices;      // Per draw
        std::vector<VertexJob>                       vertexJobs;
        std::vector<GeometryJob>                     geometryJobs;
        std::vector<std::vector<k3::sw::Triangle>>   jobTriangles;  // Output of each geometry job (stable during the frame)
        std::vector<std::vector<TriangleBounds>>     jobBounds;     // Same order as jobTriangles
        std::vector<TileBin>                         bins;

        void setSize(std::uint32_t w, std::uint32_t h)
        {
            if (w == width && h == height)
                return;
            width = w;
            height = h;
            tilesX = (w + TILE_SIZE - 1) / TILE_SIZE;
            tilesY = (h + TILE_SIZE - 1) / TILE_SIZE;
            bins.assign(static_cast<std::size_t>(tilesX) * tilesY, {});
        }

        void begin(std::size_t drawCount)
        {
            vertexJobs.clear();
            geometryJobs.clear();
            if (vertices.size() < drawCount)
                vertices.resize(drawCount);
        }

        [[nodiscard]] std::size_t tileCount() const noexcept
        {
            return static_cast<std::size_t>(tilesX) * tilesY;
        }

        // Pixel rectangle [x0, x1] x [y0, y1] of a tile.
        void tileRect(std::size_t tile, std::int32_t& x0, std::int32_t& y0, std::int32_t& x1, std::int32_t& y1) const noexcept
        {
            x0 = static_cast<std::int32_t>(tile % tilesX) * TILE_SIZE;
            y0 = static_cast<std::int32_t>(tile / tilesX) * TILE_SIZE;
            x1 = std::min<std::int32_t>(x0 + TILE_SIZE, width) - 1;
            y1 = std::min<std::int32_t>(y0 + TILE_SIZE, height) - 1;
        }
    };

    // The color buffer behaves like an 8-bit (UNORM) render target: what is written, or blended, is
    // clamped to [0, 1] first. GPU backends do exactly that, so overbright surfaces composite the same.
    k3::Math::Color saturate(const k3::Math::Color& c)
    {
        return {std::clamp(c.r, 0.f, 1.f), std::clamp(c.g, 0.f, 1.f), std::clamp(c.b, 0.f, 1.f), std::clamp(c.a, 0.f, 1.f)};
    }

    // Calls fn(x, y, edges) for every pixel of `t` inside the [x0, x1] x [y0, y1] rectangle.
    //
    // Rather than testing every pixel of the bounding box (mostly empty for long thin triangles), each
    // row solves E_i(x) >= 0 for x: edge functions are affine, so the covered pixels form one exact span.
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
            // Offsets from xs of the first and last covered pixels.
            std::int64_t first = 0, last = xe - xs;

            for (int i = 0; i < 3 && first <= last; ++i) {
                if (stepX[i] > 0) {
                    // Increasing edge: covered from the first x where it becomes >= 0.
                    if (row[i] < 0)
                        first = std::max(first, (-row[i] + stepX[i] - 1) / stepX[i]);
                } else if (stepX[i] < 0) {
                    // Decreasing edge: covered until it becomes < 0.
                    last = row[i] < 0 ? -1 : std::min(last, row[i] / -stepX[i]);
                } else if (row[i] < 0) {
                    last = -1;
                }
            }

            if (first <= last) {
                std::int64_t e[3] = {row[0] + first * stepX[0], row[1] + first * stepX[1], row[2] + first * stepX[2]};

                for (std::int64_t x = xs + first; x <= xs + last; ++x) {
                    fn(static_cast<std::int32_t>(x), y, e);
                    e[0] += stepX[0];
                    e[1] += stepX[1];
                    e[2] += stepX[2];
                }
            }
            row[0] += stepY[0];
            row[1] += stepY[1];
            row[2] += stepY[2];
        }
    }

    // Round half away from zero, inline (std::llround is an out-of-line libm call).
    std::int64_t roundToInt(float value) noexcept
    {
        return static_cast<std::int64_t>(value + (value >= 0.f ? 0.5f : -0.5f));
    }

}

struct k3::SoftwareRasterizer::Impl
{
    explicit Impl(unsigned threads) : pool(threads) {}

    sw::ThreadPool pool;

    std::vector<Math::Color>          color;
    std::vector<float>                depth;
    std::vector<const sw::Triangle*>  visibility;       // Nearest opaque triangle of each pixel
    std::vector<float>                shadowDepth;

    Camera             camera{};
    Environment        environment{};
    Math::Matrix4      view = Math::Matrix4::identity();
    sw::ShadingContext shading{};

    // Frame data, indexed by draw (position in `commands` once sorted).
    std::vector<DrawCommand>    commands;
    std::vector<sw::DrawState>  drawStates;
    std::vector<DrawTransforms> transforms;

    Pass mainPass;
    Pass shadowPass;

    // Local bounds of every mesh drawn recently: computing them walks all the vertices, so it is done
    // once per mesh, not once per frame. Meshes are immutable once drawn, as for the GPU backends.
    struct CachedBounds
    {
        std::weak_ptr<const Mesh> source;
        Math::Bounds3f            bounds{};
        bool                      valid = false;
    };
    std::unordered_map<const Mesh*, CachedBounds> boundsCache;     // Node addresses are stable
    std::vector<CachedBounds*>                    boundsToCompute;
    std::vector<const CachedBounds*>              drawBounds;          // Per draw

    FrameStats stats{};

    void resize(std::uint32_t w, std::uint32_t h)
    {
        mainPass.setSize(w, h);
        color.assign(static_cast<std::size_t>(w) * h, Math::Color::Black);
        depth.assign(static_cast<std::size_t>(w) * h, 1.f);
        visibility.assign(static_cast<std::size_t>(w) * h, nullptr);
    }

    /* Frame planning (sequential) */

    void prepareDraws()
    {
        drawStates.clear();
        transforms.clear();
        for (const DrawCommand& command : commands) {
            const Mesh& mesh = *command.mesh;
            const Material& material = command.material ? *command.material : defaultMaterial();

            drawStates.push_back({&material, material.alphaMode == AlphaMode::Mask, mesh.hasNormals(), mesh.hasColors()});
            transforms.push_back({command.transform, command.transform.normalMatrix()});
        }
    }

    void addVertices(Pass& pass, std::size_t d)
    {
        const std::size_t count = commands[d].mesh->vertexCount();

        pass.vertices[d].resize(count);
        for (std::size_t first = 0; first < count; first += VERTEX_CHUNK)
            pass.vertexJobs.push_back({static_cast<std::uint32_t>(d), first, std::min(first + VERTEX_CHUNK, count)});
    }

    void addGeometry(Pass& pass, std::size_t d, Faces faces, bool blended)
    {
        const std::size_t indexCount = commands[d].mesh->indexCount() / 3 * 3;

        for (std::size_t first = 0; first < indexCount; first += TRIANGLE_CHUNK * 3)
            pass.geometryJobs.push_back({static_cast<std::uint32_t>(d), faces, blended, first, std::min(first + TRIANGLE_CHUNK * 3, indexCount)});
    }

    // Opaque draws, then blended ones (`firstBlended` onward), back faces before front faces for
    // double-sided blended materials.
    void planMainPass(std::size_t firstBlended)
    {
        mainPass.begin(commands.size());
        for (std::size_t d = 0; d < commands.size(); ++d)
            addVertices(mainPass, d);
        for (std::size_t d = 0; d < firstBlended; ++d)
            addGeometry(mainPass, d, drawStates[d].material->doubleSided ? Faces::All : Faces::Front, false);
        // A double-sided blended mesh (a glass box) shows its inside through its outside: draw the faces
        // turned away from the camera first, so they end up behind whatever the triangle order is.
        for (std::size_t d = firstBlended; d < commands.size(); ++d) {
            if (drawStates[d].material->doubleSided)
                addGeometry(mainPass, d, Faces::Back, true);
            addGeometry(mainPass, d, Faces::Front, true);
        }
    }

    // Opaque and alpha-tested casters, both faces (thin single-sided geometry still casts).
    void planShadowPass(std::size_t firstBlended)
    {
        shadowPass.begin(commands.size());
        for (std::size_t d = 0; d < firstBlended; ++d) {
            if (!drawStates[d].material->castShadows)
                continue;
            addVertices(shadowPass, d);
            addGeometry(shadowPass, d, Faces::All, false);
        }
    }

    /* Geometry (parallel): vertex transform, then clipping and triangle setup */

    void runGeometry(Pass& pass)
    {
        if (pass.jobTriangles.size() < pass.geometryJobs.size()) {
            pass.jobTriangles.resize(pass.geometryJobs.size());
            pass.jobBounds.resize(pass.geometryJobs.size());
        }
        for (std::size_t j = 0; j < pass.geometryJobs.size(); ++j) {
            pass.jobTriangles[j].clear();
            pass.jobBounds[j].clear();
        }

        pool.parallelFor(pass.vertexJobs.size(), [&](std::size_t j) {
            transformVertices(pass, pass.vertexJobs[j]);
        });
        pool.parallelFor(pass.geometryJobs.size(), [&](std::size_t j) {
            assemble(pass, pass.geometryJobs[j], pass.jobTriangles[j]);

            pass.jobBounds[j].reserve(pass.jobTriangles[j].size());
            for (const sw::Triangle& t : pass.jobTriangles[j])
                pass.jobBounds[j].push_back({t.minX, t.minY, t.maxX, t.maxY});
        });
        bin(pass);
    }

    void transformVertices(Pass& pass, const VertexJob& job)
    {
        const Mesh& mesh = *commands[job.draw].mesh;
        const sw::DrawState& state = drawStates[job.draw];
        const DrawTransforms& m = transforms[job.draw];
        const Math::Matrix4 mvp = pass.viewProjection * m.model;
        const bool hasUVs = mesh.hasUVs();
        auto& out = pass.vertices[job.draw];

        for (std::size_t i = job.first; i < job.last; ++i) {
            const auto& p = mesh.positions[i];
            const auto world = m.model.transformPoint(p);
            const auto normal = state.hasNormals ? m.normal.transformDirection(mesh.normals[i]) : Math::Vector3f{};
            const auto uv = hasUVs ? mesh.uvs[i] : Math::Vector2f{};
            const auto c = state.hasColors ? mesh.colors[i] : Math::Color::White;
            auto& v = out[i];

            v.position = mvp * p.asPoint();
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

    void assemble(const Pass& pass, const GeometryJob& job, std::vector<sw::Triangle>& out) const
    {
        const Mesh& mesh = *commands[job.draw].mesh;
        const auto& transformed = pass.vertices[job.draw];
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
                setupTriangle(pass, polygon[0], polygon[k], polygon[k + 1], job.draw, job.faces, out);
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

    static void setupTriangle(
        const Pass& pass,
        const sw::ClipVertex& v0, const sw::ClipVertex& v1, const sw::ClipVertex& v2,
        std::uint32_t draw, Faces faces, std::vector<sw::Triangle>& out
    )
    {
        const sw::ClipVertex* in[3] = {&v0, &v1, &v2};
        std::int64_t x[3], y[3];
        float z[3], invW[3];

        for (int i = 0; i < 3; ++i) {
            const auto& p = in[i]->position;
            invW[i] = 1.f / p.w;
            x[i] = roundToInt((p.x * invW[i] * 0.5f + 0.5f) * pass.width * sw::SUBPIXEL_ONE);
            y[i] = roundToInt((0.5f - p.y * invW[i] * 0.5f) * pass.height * sw::SUBPIXEL_ONE);
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
        t.maxX = static_cast<std::int32_t>(std::min<std::int64_t>(pass.width - 1, std::max({X[0], X[1], X[2]}) >> sw::SUBPIXEL_BITS));
        t.maxY = static_cast<std::int32_t>(std::min<std::int64_t>(pass.height - 1, std::max({Y[0], Y[1], Y[2]}) >> sw::SUBPIXEL_BITS));
        if (t.minX > t.maxX || t.minY > t.maxY)
            return;

        t.draw = draw;
        t.frontFacing = frontFacing;
        out.push_back(t);
    }

    // Each worker fills one row of tiles, walking the jobs in drawing order: bins keep the submission
    // order blending relies on, and no two workers write to the same bin.
    void bin(Pass& pass)
    {
        pool.parallelFor(pass.tilesY, [&](std::size_t tileRow) {
            const auto ty = static_cast<std::int32_t>(tileRow);
            const std::int32_t rowTop = ty * TILE_SIZE, rowBottom = rowTop + TILE_SIZE - 1;
            TileBin* row = &pass.bins[tileRow * pass.tilesX];

            for (std::uint32_t tx = 0; tx < pass.tilesX; ++tx) {
                row[tx].opaque.clear();
                row[tx].blended.clear();
            }

            for (std::size_t j = 0; j < pass.geometryJobs.size(); ++j) {
                const bool blended = pass.geometryJobs[j].blended;
                const auto& bounds = pass.jobBounds[j];
                const sw::Triangle* triangles = pass.jobTriangles[j].data();

                for (std::size_t k = 0; k < bounds.size(); ++k) {
                    const TriangleBounds& b = bounds[k];

                    if (b.maxY < rowTop || b.minY > rowBottom)
                        continue;
                    for (std::int32_t tx = b.minX / TILE_SIZE; tx <= b.maxX / TILE_SIZE; ++tx)
                        (blended ? row[tx].blended : row[tx].opaque).push_back(triangles + k);
                }
            }
        });
    }

    /* Bounds */

    void computeBounds()
    {
        boundsToCompute.clear();
        drawBounds.clear();
        for (const DrawCommand& command : commands) {
            CachedBounds& cached = boundsCache[command.mesh.get()];

            drawBounds.push_back(&cached);

            // Stale entry: its mesh is gone and this one reuses the address.
            if (cached.valid && cached.source.expired())
                cached.valid = false;
            if (!cached.valid && cached.source.lock() != command.mesh) {
                cached.source = command.mesh;
                boundsToCompute.push_back(&cached);
            }
        }

        pool.parallelFor(boundsToCompute.size(), [&](std::size_t i) {
            CachedBounds& cached = *boundsToCompute[i];
            cached.bounds = cached.source.lock()->bounds();
            cached.valid = true;
        });
        pool.parallelFor(commands.size(), [&](std::size_t d) {
            DrawCommand& command = commands[d];
            command.bounds = drawBounds[d]->bounds.transformed(command.transform);
        });
    }

    void collectBounds()
    {
        std::erase_if(boundsCache, [](const auto& entry) { return entry.second.source.expired(); });
    }

    /* Shadows */

    // Renders the shadow map of the shadow-casting light (if any) and hands it to the shading context.
    void renderShadows(std::size_t firstBlended)
    {
        const ShadowSettings& settings = environment.shadows;

        shading.shadow = {};
        if (shading.shadowLight < 0 || settings.resolution == 0)
            return;

        Math::Bounds3f bounds = settings.bounds;
        if (bounds.empty())
            for (const DrawCommand& command : commands)
                bounds.merge(command.bounds);

        const auto projection = fitShadowProjection(bounds, shading.lights[shading.shadowLight].direction);
        if (!projection)
            return;

        shadowPass.viewProjection = projection->viewProjection;
        shadowPass.setSize(settings.resolution, settings.resolution);
        shadowDepth.resize(static_cast<std::size_t>(settings.resolution) * settings.resolution);

        planShadowPass(firstBlended);
        if (shadowPass.geometryJobs.empty())
            return;
        runGeometry(shadowPass);
        pool.parallelFor(shadowPass.tileCount(), [&](std::size_t tile) { renderShadowTile(tile); });

        shading.shadow = {
            shadowDepth.data(),
            settings.resolution,
            shadowPass.viewProjection,
            settings.depthBias,
            settings.normalBias * 2.f * projection->radius / static_cast<float>(settings.resolution),
            std::max(settings.pcfRadius, 0),
        };
    }

    void renderShadowTile(std::size_t tile)
    {
        std::int32_t x0, y0, x1, y1;
        shadowPass.tileRect(tile, x0, y0, x1, y1);

        const std::uint32_t size = shadowPass.width;
        for (std::int32_t y = y0; y <= y1; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * size;
            std::fill(shadowDepth.begin() + row + x0, shadowDepth.begin() + row + x1 + 1, 1.f);
        }

        for (const sw::Triangle* triangle : shadowPass.bins[tile].opaque) {
            const sw::Triangle& t = *triangle;
            const sw::DrawState& state = drawStates[t.draw];

            forEachCoveredPixel(t, x0, y0, x1, y1, [&](std::int32_t x, std::int32_t y, const std::int64_t e[3]) {
                float& stored = shadowDepth[static_cast<std::size_t>(y) * size + x];
                const float z = depthAt(t, e);

                if (!(z < stored))
                    return;
                if (state.alphaTest && sw::coverage(*state.material, fragmentAt(t, e)) < state.material->alphaCutoff)
                    return;
                stored = z;
            });
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

    void renderTile(std::size_t tileIndex)
    {
        std::int32_t x0, y0, x1, y1;
        mainPass.tileRect(tileIndex, x0, y0, x1, y1);

        const std::uint32_t width = mainPass.width;
        for (std::int32_t y = y0; y <= y1; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * width;
            std::fill(color.begin() + row + x0, color.begin() + row + x1 + 1, environment.clearColor);
            std::fill(depth.begin() + row + x0, depth.begin() + row + x1 + 1, 1.f);
            std::fill(visibility.begin() + row + x0, visibility.begin() + row + x1 + 1, nullptr);
        }

        const TileBin& tile = mainPass.bins[tileIndex];

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

                Math::Color c = saturate(sw::shade(shading, *drawStates[t->draw].material, fragmentAt(*t, e)));
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

                const Math::Color src = saturate(sw::shade(shading, material, fragmentAt(t, e)));
                const float a = src.a;
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
    return m_impl->mainPass.width;
}

std::uint32_t k3::SoftwareRasterizer::height() const noexcept
{
    return m_impl->mainPass.height;
}

unsigned k3::SoftwareRasterizer::threadCount() const noexcept
{
    return m_impl->pool.size();
}

void k3::SoftwareRasterizer::beginFrame(const Camera& camera, const Environment& environment)
{
    auto& impl = *m_impl;
    const float aspect = impl.mainPass.height ? static_cast<float>(impl.mainPass.width) / impl.mainPass.height : 1.f;

    impl.camera = camera;
    impl.environment = environment;
    impl.view = camera.view();
    impl.mainPass.viewProjection = camera.projectionMatrix(aspect) * impl.view;
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

    impl.computeBounds();

    // Opaque and alpha-tested draws keep their order, blended ones go last, farthest first.
    const auto firstBlended = std::stable_partition(impl.commands.begin(), impl.commands.end(), [](const DrawCommand& c) { return !isBlended(c); });
    for (auto it = firstBlended; it != impl.commands.end(); ++it)
        it->viewDepth = -impl.view.transformPoint(it->bounds.center()).z;
    std::stable_sort(firstBlended, impl.commands.end(), [](const DrawCommand& a, const DrawCommand& b) { return a.viewDepth > b.viewDepth; });

    const auto blendedStart = static_cast<std::size_t>(firstBlended - impl.commands.begin());

    impl.prepareDraws();
    impl.renderShadows(blendedStart);
    impl.planMainPass(blendedStart);
    impl.runGeometry(impl.mainPass);
    impl.pool.parallelFor(impl.mainPass.tileCount(), [&](std::size_t tile) { impl.renderTile(tile); });

    // Draw states point into the recorded materials: drop both together.
    impl.drawStates.clear();
    impl.commands.clear();
    impl.shading.shadow = {};
    impl.collectBounds();
    impl.stats.frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

k3::Image k3::SoftwareRasterizer::readPixels()
{
    auto& impl = *m_impl;
    const std::uint32_t width = impl.mainPass.width;
    Image image;

    image.width = width;
    image.height = impl.mainPass.height;
    image.pixels.resize(impl.color.size() * 4);

    const std::uint8_t* encode = srgb::encodeTable();
    impl.pool.parallelFor(image.height, [&](std::size_t y) {
        for (std::size_t i = y * width; i < (y + 1) * width; ++i) {
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
