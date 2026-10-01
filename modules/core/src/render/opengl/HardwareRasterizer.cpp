#include "HardwareRasterizer.hpp"
#include "GL.hpp"
#include "utils/Log.hpp"
#include "Program.hpp"
#include "Shaders.hpp"
#include "render/ShadowFit.hpp"
#include "render/software/Shading.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

    using namespace k3::gl;

    enum TextureUnit : GLint {
        DIFFUSE_UNIT  = 0,
        SPECULAR_UNIT = 1,
        OPACITY_UNIT  = 2,
        SHADOW_UNIT   = 3,
    };

    struct GpuMesh
    {
        std::weak_ptr<const k3::Mesh> source;
        GLuint             vao = 0, vbo = 0, ebo = 0;
        GLsizei            count = 0;               // Indices (or vertices when not indexed)
        bool               indexed = false;
        bool               hasNormals = false, hasUVs = false, hasColors = false;
        k3::Math::Bounds3f bounds{};
    };

    struct GpuTexture
    {
        std::weak_ptr<const k3::Texture> source;
        GLuint        id = 0;
        std::uint64_t version = 0;
        k3::Sampler   sampler{};
    };

    struct DrawCommand
    {
        std::shared_ptr<const k3::Mesh>     mesh;
        std::shared_ptr<const k3::Material> material;
        k3::Math::Matrix4                   transform;
        const GpuMesh*                      gpu = nullptr;
        k3::Math::Bounds3f                  bounds{};
        float                               viewDepth = 0.f;
    };

    const k3::Material& defaultMaterial()
    {
        static const k3::Material material{};
        return material;
    }

    const k3::Material& materialOf(const DrawCommand& command)
    {
        return command.material ? *command.material : defaultMaterial();
    }

    // Parses the "major.minor" at the start of GL_VERSION.
    bool atLeast33(const char* version)
    {
        int major = 0, minor = 0;

        return version && std::sscanf(version, "%d.%d", &major, &minor) == 2 && (major > 3 || (major == 3 && minor >= 3));
    }

    GLint minFilter(const k3::Sampler& sampler)
    {
        const bool linear = sampler.filter == k3::TextureFilter::Linear;

        switch (sampler.mipmaps) {
            case k3::MipmapFilter::None:
                return linear ? LINEAR : NEAREST;
            case k3::MipmapFilter::Nearest:
                return linear ? LINEAR_MIPMAP_NEAREST : NEAREST_MIPMAP_NEAREST;
            case k3::MipmapFilter::Linear:
                break;
        }
        return linear ? LINEAR_MIPMAP_LINEAR : NEAREST_MIPMAP_LINEAR;
    }

    GLint wrapMode(k3::TextureWrap wrap)
    {
        switch (wrap) {
            case k3::TextureWrap::Repeat:
                return REPEAT;
            case k3::TextureWrap::MirroredRepeat:
                return MIRRORED_REPEAT;
            case k3::TextureWrap::ClampToEdge:
                break;
        }
        return CLAMP_TO_EDGE;
    }

    bool operator==(const k3::Sampler& a, const k3::Sampler& b)
    {
        return a.filter == b.filter && a.mipmaps == b.mipmaps && a.wrapU == b.wrapU && a.wrapV == b.wrapV;
    }

    void setMatrix(GLint location, const k3::Math::Matrix4& m)
    {
        // kronk3d matrices are row-major.
        UniformMatrix4fv(location, 1, TRUE_, m.values);
    }

    struct MeshUniforms
    {
        GLint viewProjection, model, normalMatrix;
        GLint cameraPosition, viewDirection, orthographic, ambient;
        GLint lightCount, lightType, lightPosition, lightToLight, lightDirection, lightRadiance, lightRange, lightCosInner, lightCosOuter;
        GLint shadowLight, shadowMap, shadowViewProjection, shadowSize, shadowDepthBias, shadowNormalOffset, shadowPcfRadius;
        GLint diffuse, specular, emissive, shininess, unlit, alphaMode, alphaCutoff;
        GLint hasNormals, hasDiffuseMap, hasSpecularMap, hasOpacityMap, diffuseMap, specularMap, opacityMap;

        explicit MeshUniforms(const Program& p)
            : viewProjection(p.uniform("uViewProjection")), model(p.uniform("uModel")), normalMatrix(p.uniform("uNormalMatrix")),
              cameraPosition(p.uniform("uCameraPosition")), viewDirection(p.uniform("uViewDirection")),
              orthographic(p.uniform("uOrthographic")), ambient(p.uniform("uAmbient")),
              lightCount(p.uniform("uLightCount")), lightType(p.uniform("uLightType")), lightPosition(p.uniform("uLightPosition")),
              lightToLight(p.uniform("uLightToLight")), lightDirection(p.uniform("uLightDirection")),
              lightRadiance(p.uniform("uLightRadiance")), lightRange(p.uniform("uLightRange")),
              lightCosInner(p.uniform("uLightCosInner")), lightCosOuter(p.uniform("uLightCosOuter")),
              shadowLight(p.uniform("uShadowLight")), shadowMap(p.uniform("uShadowMap")),
              shadowViewProjection(p.uniform("uShadowViewProjection")), shadowSize(p.uniform("uShadowSize")),
              shadowDepthBias(p.uniform("uShadowDepthBias")), shadowNormalOffset(p.uniform("uShadowNormalOffset")),
              shadowPcfRadius(p.uniform("uShadowPcfRadius")),
              diffuse(p.uniform("uDiffuse")), specular(p.uniform("uSpecular")), emissive(p.uniform("uEmissive")),
              shininess(p.uniform("uShininess")), unlit(p.uniform("uUnlit")), alphaMode(p.uniform("uAlphaMode")),
              alphaCutoff(p.uniform("uAlphaCutoff")), hasNormals(p.uniform("uHasNormals")),
              hasDiffuseMap(p.uniform("uHasDiffuseMap")), hasSpecularMap(p.uniform("uHasSpecularMap")),
              hasOpacityMap(p.uniform("uHasOpacityMap")), diffuseMap(p.uniform("uDiffuseMap")),
              specularMap(p.uniform("uSpecularMap")), opacityMap(p.uniform("uOpacityMap"))
        {
        }
    };

    struct ShadowUniforms
    {
        GLint viewProjection, model, alphaTest, diffuseAlpha, alphaCutoff, hasDiffuseMap, hasOpacityMap, diffuseMap, opacityMap;

        explicit ShadowUniforms(const Program& p)
            : viewProjection(p.uniform("uViewProjection")), model(p.uniform("uModel")), alphaTest(p.uniform("uAlphaTest")),
              diffuseAlpha(p.uniform("uDiffuseAlpha")), alphaCutoff(p.uniform("uAlphaCutoff")),
              hasDiffuseMap(p.uniform("uHasDiffuseMap")), hasOpacityMap(p.uniform("uHasOpacityMap")),
              diffuseMap(p.uniform("uDiffuseMap")), opacityMap(p.uniform("uOpacityMap"))
        {
        }
    };

}

struct k3::HardwareRasterizer::Impl
{
    Impl(Program meshProgram, Program shadowProgram)
        : mesh(std::move(meshProgram)), shadow(std::move(shadowProgram)), meshUniforms(mesh), shadowUniforms(shadow) {}

    ~Impl()
    {
        for (auto& [_, gpu] : meshes)
            release(gpu);
        for (auto& [_, gpu] : textures)
            DeleteTextures(1, &gpu.id);
        releaseTarget();
        if (shadowTexture)
            DeleteTextures(1, &shadowTexture);
        if (shadowFbo)
            DeleteFramebuffers(1, &shadowFbo);
    }

    Program        mesh;
    Program        shadow;
    MeshUniforms   meshUniforms;
    ShadowUniforms shadowUniforms;
    std::string    driver;

    std::uint32_t width = 0, height = 0;
    GLuint fbo = 0, colorTexture = 0, depthTexture = 0;
    GLuint shadowFbo = 0, shadowTexture = 0;
    std::uint32_t shadowSize = 0;
    std::optional<std::uint32_t> presentFramebuffer = 0u;

    std::unordered_map<const Mesh*, GpuMesh>       meshes;
    std::unordered_map<const Texture*, GpuTexture> textures;

    Camera             camera{};
    Environment        environment{};
    Math::Matrix4      view = Math::Matrix4::identity();
    Math::Matrix4      viewProjection = Math::Matrix4::identity();
    std::vector<DrawCommand> commands;
    FrameStats         stats{};

    /* Render target */

    void releaseTarget()
    {
        if (fbo)
            DeleteFramebuffers(1, &fbo);
        if (colorTexture)
            DeleteTextures(1, &colorTexture);
        if (depthTexture)
            DeleteTextures(1, &depthTexture);
        fbo = colorTexture = depthTexture = 0;
    }

    static GLuint makeTexture(GLint internalFormat, std::uint32_t w, std::uint32_t h, GLenum format, GLenum type)
    {
        GLuint id = 0;

        GenTextures(1, &id);
        BindTexture(TEXTURE_2D, id);
        TexImage2D(TEXTURE_2D, 0, internalFormat, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, format, type, nullptr);
        TexParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, NEAREST);
        TexParameteri(TEXTURE_2D, TEXTURE_MAG_FILTER, NEAREST);
        TexParameteri(TEXTURE_2D, TEXTURE_MAX_LEVEL, 0);
        TexParameteri(TEXTURE_2D, TEXTURE_WRAP_S, CLAMP_TO_EDGE);
        TexParameteri(TEXTURE_2D, TEXTURE_WRAP_T, CLAMP_TO_EDGE);
        return id;
    }

    void resize(std::uint32_t w, std::uint32_t h)
    {
        releaseTarget();
        width = w;
        height = h;
        if (w == 0 || h == 0)
            return;

        colorTexture = makeTexture(static_cast<GLint>(SRGB8_ALPHA8), w, h, RGBA, UNSIGNED_BYTE);
        depthTexture = makeTexture(static_cast<GLint>(DEPTH_COMPONENT24), w, h, DEPTH_COMPONENT, FLOAT);
        GenFramebuffers(1, &fbo);
        BindFramebuffer(FRAMEBUFFER, fbo);
        FramebufferTexture2D(FRAMEBUFFER, COLOR_ATTACHMENT0, TEXTURE_2D, colorTexture, 0);
        FramebufferTexture2D(FRAMEBUFFER, DEPTH_ATTACHMENT, TEXTURE_2D, depthTexture, 0);
        if (CheckFramebufferStatus(FRAMEBUFFER) != FRAMEBUFFER_COMPLETE)
            log(LogLevel::Error, "OpenGL: incomplete render target {}x{}", w, h);
        BindFramebuffer(FRAMEBUFFER, 0);
    }

    void ensureShadowTarget(std::uint32_t size)
    {
        if (size == shadowSize && shadowFbo)
            return;
        if (shadowTexture)
            DeleteTextures(1, &shadowTexture);
        if (!shadowFbo)
            GenFramebuffers(1, &shadowFbo);

        shadowSize = size;
        shadowTexture = makeTexture(static_cast<GLint>(DEPTH_COMPONENT32F), size, size, DEPTH_COMPONENT, FLOAT);
        BindFramebuffer(FRAMEBUFFER, shadowFbo);
        FramebufferTexture2D(FRAMEBUFFER, DEPTH_ATTACHMENT, TEXTURE_2D, shadowTexture, 0);
        DrawBuffer(NONE);
        ReadBuffer(NONE);
        if (CheckFramebufferStatus(FRAMEBUFFER) != FRAMEBUFFER_COMPLETE)
            log(LogLevel::Error, "OpenGL: incomplete shadow map {}x{}", size, size);
    }

    /* Resource cache */

    static void release(GpuMesh& gpu)
    {
        DeleteVertexArrays(1, &gpu.vao);
        DeleteBuffers(1, &gpu.vbo);
        if (gpu.ebo)
            DeleteBuffers(1, &gpu.ebo);
        gpu = {};
    }

    const GpuMesh& meshFor(const std::shared_ptr<const Mesh>& source)
    {
        GpuMesh& gpu = meshes[source.get()];

        // A stale entry: the mesh it was made from is gone and this one reuses its address.
        if (gpu.vao && gpu.source.expired())
            release(gpu);
        if (!gpu.vao)
            upload(*source, gpu);
        gpu.source = source;
        return gpu;
    }

    static void upload(const Mesh& mesh, GpuMesh& gpu)
    {
        const std::size_t n = mesh.vertexCount();

        gpu.hasNormals = mesh.hasNormals();
        gpu.hasUVs = mesh.hasUVs();
        gpu.hasColors = mesh.hasColors();
        gpu.bounds = mesh.bounds();

        // One buffer, attributes one after the other: positions, normals, uvs, colors.
        const std::size_t positionsSize = n * sizeof(Math::Vector3f);
        const std::size_t normalsSize = gpu.hasNormals ? n * sizeof(Math::Vector3f) : 0;
        const std::size_t uvsSize = gpu.hasUVs ? n * sizeof(Math::Vector2f) : 0;
        const std::size_t colorsSize = gpu.hasColors ? n * sizeof(Math::Color) : 0;

        GenVertexArrays(1, &gpu.vao);
        BindVertexArray(gpu.vao);
        GenBuffers(1, &gpu.vbo);
        BindBuffer(ARRAY_BUFFER, gpu.vbo);
        BufferData(ARRAY_BUFFER, static_cast<GLsizeiptr>(positionsSize + normalsSize + uvsSize + colorsSize), nullptr, STATIC_DRAW);

        std::size_t offset = 0;
        auto attribute = [&](GLuint location, GLint components, const void* data, std::size_t size) {
            if (size == 0) {
                DisableVertexAttribArray(location);
                return;
            }
            BufferSubData(ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
            EnableVertexAttribArray(location);
            VertexAttribPointer(location, components, FLOAT, FALSE_, 0, reinterpret_cast<const void*>(offset));
            offset += size;
        };
        attribute(shaders::POSITION, 3, mesh.positions.data(), positionsSize);
        attribute(shaders::NORMAL, 3, mesh.normals.data(), normalsSize);
        attribute(shaders::UV, 2, mesh.uvs.data(), uvsSize);
        attribute(shaders::COLOR, 4, mesh.colors.data(), colorsSize);

        // Out-of-range indices would read outside the buffer: drop those triangles, as the software backend does.
        const std::size_t indexCount = mesh.indexCount() / 3 * 3;
        std::vector<std::uint32_t> indices;
        indices.reserve(indexCount);
        for (std::size_t t = 0; t < indexCount; t += 3) {
            const std::uint32_t i0 = mesh.index(t), i1 = mesh.index(t + 1), i2 = mesh.index(t + 2);
            if (i0 < n && i1 < n && i2 < n)
                indices.insert(indices.end(), {i0, i1, i2});
        }

        gpu.indexed = true;
        gpu.count = static_cast<GLsizei>(indices.size());
        GenBuffers(1, &gpu.ebo);
        BindBuffer(ELEMENT_ARRAY_BUFFER, gpu.ebo);
        BufferData(ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data(), STATIC_DRAW);
        BindVertexArray(0);
    }

    // Binds the texture on `unit` (uploading or refreshing it first) and returns whether there is one.
    bool bindTexture(GLint unit, const std::shared_ptr<Texture>& source)
    {
        ActiveTexture(TEXTURE0 + static_cast<GLenum>(unit));
        if (!source || source->image().empty()) {
            BindTexture(TEXTURE_2D, 0);
            return false;
        }

        GpuTexture& gpu = textures[source.get()];
        const bool stale = gpu.id && (gpu.source.expired() || gpu.version != source->version());

        if (stale) {
            DeleteTextures(1, &gpu.id);
            gpu.id = 0;
        }
        if (!gpu.id) {
            GenTextures(1, &gpu.id);
            BindTexture(TEXTURE_2D, gpu.id);
            PixelStorei(UNPACK_ALIGNMENT, 1);

            const GLint format = static_cast<GLint>(source->colorSpace() == ColorSpace::Srgb ? SRGB8_ALPHA8 : RGBA8);
            for (std::size_t level = 0; level < source->levelCount(); ++level) {
                const Image& image = source->level(level);
                TexImage2D(TEXTURE_2D, static_cast<GLint>(level), format, static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height), 0, RGBA, UNSIGNED_BYTE, image.pixels.data());
            }
            TexParameteri(TEXTURE_2D, TEXTURE_BASE_LEVEL, 0);
            TexParameteri(TEXTURE_2D, TEXTURE_MAX_LEVEL, static_cast<GLint>(source->levelCount() - 1));
            gpu.version = source->version();
            gpu.sampler = source->sampler;
            applySampler(source->sampler);
        } else {
            BindTexture(TEXTURE_2D, gpu.id);
            if (!(gpu.sampler == source->sampler)) {
                gpu.sampler = source->sampler;
                applySampler(source->sampler);
            }
        }
        gpu.source = source;
        return true;
    }

    static void applySampler(const Sampler& sampler)
    {
        TexParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, minFilter(sampler));
        TexParameteri(TEXTURE_2D, TEXTURE_MAG_FILTER, sampler.filter == TextureFilter::Linear ? LINEAR : NEAREST);
        TexParameteri(TEXTURE_2D, TEXTURE_WRAP_S, wrapMode(sampler.wrapU));
        TexParameteri(TEXTURE_2D, TEXTURE_WRAP_T, wrapMode(sampler.wrapV));
    }

    // Frees what nothing references anymore.
    void collectGarbage()
    {
        for (auto it = meshes.begin(); it != meshes.end();) {
            if (it->second.source.expired()) {
                release(it->second);
                it = meshes.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = textures.begin(); it != textures.end();) {
            if (it->second.source.expired()) {
                DeleteTextures(1, &it->second.id);
                it = textures.erase(it);
            } else {
                ++it;
            }
        }
    }

    /* Drawing */

    static void drawMesh(const GpuMesh& gpu)
    {
        BindVertexArray(gpu.vao);
        // Disabled attributes read these constants: exactly white when a mesh has no vertex colors.
        if (!gpu.hasNormals)
            VertexAttrib4f(shaders::NORMAL, 0.f, 0.f, 0.f, 0.f);
        if (!gpu.hasUVs)
            VertexAttrib4f(shaders::UV, 0.f, 0.f, 0.f, 1.f);
        if (!gpu.hasColors)
            VertexAttrib4f(shaders::COLOR, 1.f, 1.f, 1.f, 1.f);
        DrawElements(TRIANGLES, gpu.count, UNSIGNED_INT, nullptr);
    }

    // Returns the index of the shadow-casting light if a shadow map was rendered, -1 otherwise.
    int renderShadows(const sw::ShadingContext& lights, std::size_t firstBlended, ShadowProjection& projection)
    {
        const ShadowSettings& settings = environment.shadows;

        if (lights.shadowLight < 0 || settings.resolution == 0)
            return -1;

        Math::Bounds3f bounds = settings.bounds;
        if (bounds.empty())
            for (const DrawCommand& command : commands)
                bounds.merge(command.bounds);

        const auto fitted = fitShadowProjection(bounds, lights.lights[lights.shadowLight].direction);
        if (!fitted)
            return -1;
        projection = *fitted;

        bool anyCaster = false;
        for (std::size_t d = 0; d < firstBlended; ++d)
            anyCaster |= materialOf(commands[d]).castShadows;
        if (!anyCaster)
            return -1;

        ensureShadowTarget(settings.resolution);
        BindFramebuffer(FRAMEBUFFER, shadowFbo);
        Viewport(0, 0, static_cast<GLsizei>(settings.resolution), static_cast<GLsizei>(settings.resolution));
        DepthMask(TRUE_);
        ClearDepth(1.0);
        Clear(DEPTH_BUFFER_BIT);
        // Both faces, like the software backend: thin single-sided geometry still casts.
        Disable(CULL_FACE);
        Disable(BLEND);
        Enable(DEPTH_TEST);
        DepthFunc(LESS);

        UseProgram(shadow.id());
        setMatrix(shadowUniforms.viewProjection, projection.viewProjection);
        Uniform1i(shadowUniforms.diffuseMap, DIFFUSE_UNIT);
        Uniform1i(shadowUniforms.opacityMap, OPACITY_UNIT);

        for (std::size_t d = 0; d < firstBlended; ++d) {
            const DrawCommand& command = commands[d];
            const Material& material = materialOf(command);

            if (!material.castShadows)
                continue;

            const bool alphaTest = material.alphaMode == AlphaMode::Mask;
            setMatrix(shadowUniforms.model, command.transform);
            Uniform1i(shadowUniforms.alphaTest, alphaTest);
            if (alphaTest) {
                Uniform1f(shadowUniforms.diffuseAlpha, material.diffuse.a);
                Uniform1f(shadowUniforms.alphaCutoff, material.alphaCutoff);
                Uniform1i(shadowUniforms.hasDiffuseMap, bindTexture(DIFFUSE_UNIT, material.diffuseMap));
                Uniform1i(shadowUniforms.hasOpacityMap, bindTexture(OPACITY_UNIT, material.opacityMap));
            }
            drawMesh(*command.gpu);
        }
        return lights.shadowLight;
    }

    void setFrameUniforms(const sw::ShadingContext& lights, int shadowLight, const ShadowProjection& projection)
    {
        const auto& u = meshUniforms;
        GLint types[MAX_LIGHTS]{};
        GLfloat positions[MAX_LIGHTS * 3]{}, toLights[MAX_LIGHTS * 3]{}, directions[MAX_LIGHTS * 3]{}, radiances[MAX_LIGHTS * 3]{};
        GLfloat ranges[MAX_LIGHTS]{}, cosInner[MAX_LIGHTS]{}, cosOuter[MAX_LIGHTS]{};

        for (std::size_t i = 0; i < lights.lightCount; ++i) {
            const auto& light = lights.lights[i];
            const Math::Vector3f* vectors[] = {&light.position, &light.toLight, &light.direction};
            GLfloat* targets[] = {positions, toLights, directions};

            types[i] = static_cast<GLint>(light.type);
            for (int k = 0; k < 3; ++k) {
                targets[k][i * 3 + 0] = vectors[k]->x;
                targets[k][i * 3 + 1] = vectors[k]->y;
                targets[k][i * 3 + 2] = vectors[k]->z;
            }
            radiances[i * 3 + 0] = light.radiance.r;
            radiances[i * 3 + 1] = light.radiance.g;
            radiances[i * 3 + 2] = light.radiance.b;
            ranges[i] = light.range;
            cosInner[i] = light.cosInner;
            cosOuter[i] = light.cosOuter;
        }

        setMatrix(u.viewProjection, viewProjection);
        Uniform3f(u.cameraPosition, lights.cameraPosition.x, lights.cameraPosition.y, lights.cameraPosition.z);
        Uniform3f(u.viewDirection, lights.viewDirection.x, lights.viewDirection.y, lights.viewDirection.z);
        Uniform1i(u.orthographic, lights.orthographic);
        Uniform3f(u.ambient, lights.ambient.r, lights.ambient.g, lights.ambient.b);
        Uniform1i(u.lightCount, static_cast<GLint>(lights.lightCount));
        Uniform1iv(u.lightType, MAX_LIGHTS, types);
        Uniform3fv(u.lightPosition, MAX_LIGHTS, positions);
        Uniform3fv(u.lightToLight, MAX_LIGHTS, toLights);
        Uniform3fv(u.lightDirection, MAX_LIGHTS, directions);
        Uniform3fv(u.lightRadiance, MAX_LIGHTS, radiances);
        Uniform1fv(u.lightRange, MAX_LIGHTS, ranges);
        Uniform1fv(u.lightCosInner, MAX_LIGHTS, cosInner);
        Uniform1fv(u.lightCosOuter, MAX_LIGHTS, cosOuter);

        Uniform1i(u.diffuseMap, DIFFUSE_UNIT);
        Uniform1i(u.specularMap, SPECULAR_UNIT);
        Uniform1i(u.opacityMap, OPACITY_UNIT);
        Uniform1i(u.shadowMap, SHADOW_UNIT);
        Uniform1i(u.shadowLight, shadowLight);
        if (shadowLight >= 0) {
            const ShadowSettings& settings = environment.shadows;

            ActiveTexture(TEXTURE0 + SHADOW_UNIT);
            BindTexture(TEXTURE_2D, shadowTexture);
            setMatrix(u.shadowViewProjection, projection.viewProjection);
            Uniform1i(u.shadowSize, static_cast<GLint>(settings.resolution));
            Uniform1f(u.shadowDepthBias, settings.depthBias);
            Uniform1f(u.shadowNormalOffset, settings.normalBias * 2.f * projection.radius / static_cast<float>(settings.resolution));
            Uniform1i(u.shadowPcfRadius, std::max(settings.pcfRadius, 0));
        }
    }

    void drawShaded(const DrawCommand& command)
    {
        const auto& u = meshUniforms;
        const Material& material = materialOf(command);

        setMatrix(u.model, command.transform);
        setMatrix(u.normalMatrix, command.transform.normalMatrix());
        Uniform4f(u.diffuse, material.diffuse.r, material.diffuse.g, material.diffuse.b, material.diffuse.a);
        Uniform3f(u.specular, material.specular.r, material.specular.g, material.specular.b);
        Uniform3f(u.emissive, material.emissive.r, material.emissive.g, material.emissive.b);
        Uniform1f(u.shininess, material.shininess);
        Uniform1i(u.unlit, material.unlit);
        Uniform1i(u.alphaMode, static_cast<GLint>(material.alphaMode));
        Uniform1f(u.alphaCutoff, material.alphaCutoff);
        Uniform1i(u.hasNormals, command.gpu->hasNormals);
        Uniform1i(u.hasDiffuseMap, bindTexture(DIFFUSE_UNIT, material.diffuseMap));
        Uniform1i(u.hasSpecularMap, bindTexture(SPECULAR_UNIT, material.specularMap));
        Uniform1i(u.hasOpacityMap, bindTexture(OPACITY_UNIT, material.opacityMap));
        drawMesh(*command.gpu);
    }

    void renderMain(std::size_t firstBlended)
    {
        BindFramebuffer(FRAMEBUFFER, fbo);
        Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        // Shading happens in linear space; the sRGB target encodes on write and blends in linear,
        // exactly like the software backend.
        Enable(FRAMEBUFFER_SRGB);
        ColorMask(TRUE_, TRUE_, TRUE_, TRUE_);
        DepthMask(TRUE_);
        ClearColor(environment.clearColor.r, environment.clearColor.g, environment.clearColor.b, environment.clearColor.a);
        ClearDepth(1.0);
        Clear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);
        Enable(DEPTH_TEST);
        DepthFunc(LESS);
        FrontFace(CCW);
        Disable(BLEND);

        for (std::size_t d = 0; d < firstBlended; ++d) {
            if (materialOf(commands[d]).doubleSided) {
                Disable(CULL_FACE);
            } else {
                Enable(CULL_FACE);
                CullFace(BACK);
            }
            drawShaded(commands[d]);
        }

        // Blended geometry, farthest first, depth-tested but not written. A double-sided blended mesh
        // draws its back faces first so they end up behind its front faces.
        DepthMask(FALSE_);
        Enable(BLEND);
        BlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA);
        Enable(CULL_FACE);
        for (std::size_t d = firstBlended; d < commands.size(); ++d) {
            if (materialOf(commands[d]).doubleSided) {
                CullFace(FRONT);
                drawShaded(commands[d]);
            }
            CullFace(BACK);
            drawShaded(commands[d]);
        }

        Disable(BLEND);
        DepthMask(TRUE_);
        Disable(FRAMEBUFFER_SRGB);
    }

    void present()
    {
        if (!presentFramebuffer || !fbo)
            return;
        BindFramebuffer(READ_FRAMEBUFFER, fbo);
        BindFramebuffer(DRAW_FRAMEBUFFER, *presentFramebuffer);
        const auto w = static_cast<GLint>(width), h = static_cast<GLint>(height);
        BlitFramebuffer(0, 0, w, h, 0, 0, w, h, COLOR_BUFFER_BIT, NEAREST);
    }

    // Leaves the context the way most callers expect it.
    void restoreState()
    {
        BindVertexArray(0);
        UseProgram(0);
        ActiveTexture(TEXTURE0);
        Disable(CULL_FACE);
        Disable(DEPTH_TEST);
        BindFramebuffer(FRAMEBUFFER, presentFramebuffer.value_or(0));
        Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
};

k3::Result<std::unique_ptr<k3::HardwareRasterizer>> k3::HardwareRasterizer::create(std::uint32_t width, std::uint32_t height, const GLLoader& loader)
{
    if (!loader)
        return Error{"OpenGL: no function loader given"};

    if (const auto missing = gl::load(loader); !missing.empty()) {
        std::string list;
        for (const auto& name : missing)
            list += (list.empty() ? "" : ", ") + name;
        return Error{"OpenGL: missing functions (is a 3.3 core context current?): " + list};
    }

    const char* version = reinterpret_cast<const char*>(gl::GetString(gl::VERSION));
    const char* renderer = reinterpret_cast<const char*>(gl::GetString(gl::RENDERER));
    if (!atLeast33(version))
        return Error{std::string("OpenGL 3.3 required, the current context is ") + (version ? version : "unknown")};

    auto mesh = gl::Program::create(gl::shaders::MESH_VERTEX, gl::shaders::MESH_FRAGMENT);
    if (!mesh)
        return Error{"OpenGL: mesh shader: " + mesh.error()};
    auto shadow = gl::Program::create(gl::shaders::SHADOW_VERTEX, gl::shaders::SHADOW_FRAGMENT);
    if (!shadow)
        return Error{"OpenGL: shadow shader: " + shadow.error()};

    auto impl = std::make_unique<Impl>(std::move(*mesh), std::move(*shadow));
    impl->driver = std::string(version) + " / " + (renderer ? renderer : "unknown");
    impl->resize(width, height);
    log(LogLevel::Info, "OpenGL rasterizer on {}", impl->driver);
    return std::unique_ptr<HardwareRasterizer>(new HardwareRasterizer(std::move(impl)));
}

k3::HardwareRasterizer::HardwareRasterizer(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

k3::HardwareRasterizer::~HardwareRasterizer() = default;

void k3::HardwareRasterizer::resize(std::uint32_t width, std::uint32_t height)
{
    m_impl->resize(width, height);
}

std::uint32_t k3::HardwareRasterizer::width() const noexcept
{
    return m_impl->width;
}

std::uint32_t k3::HardwareRasterizer::height() const noexcept
{
    return m_impl->height;
}

void k3::HardwareRasterizer::beginFrame(const Camera& camera, const Environment& environment)
{
    auto& impl = *m_impl;
    const float aspect = impl.height ? static_cast<float>(impl.width) / impl.height : 1.f;

    impl.camera = camera;
    impl.environment = environment;
    impl.view = camera.view();
    impl.viewProjection = camera.projectionMatrix(aspect) * impl.view;
    impl.commands.clear();
    impl.stats = {};
}

void k3::HardwareRasterizer::draw(std::shared_ptr<const Mesh> mesh, std::shared_ptr<const Material> material, const Math::Matrix4& transform)
{
    if (!mesh)
        return;
    m_impl->stats.drawCalls++;
    m_impl->stats.triangles += mesh->triangleCount();
    m_impl->commands.push_back({std::move(mesh), std::move(material), transform});
}

void k3::HardwareRasterizer::endFrame()
{
    auto& impl = *m_impl;
    const auto start = std::chrono::steady_clock::now();

    if (!impl.fbo) {
        impl.commands.clear();
        return;
    }

    for (DrawCommand& command : impl.commands) {
        command.gpu = &impl.meshFor(command.mesh);
        command.bounds = command.gpu->bounds.transformed(command.transform);
    }

    // Same ordering as the software backend: opaque in submission order, blended farthest first.
    const auto isBlended = [](const DrawCommand& c) { return materialOf(c).alphaMode == AlphaMode::Blend; };
    const auto firstBlended = std::stable_partition(impl.commands.begin(), impl.commands.end(), [&](const DrawCommand& c) { return !isBlended(c); });
    for (auto it = firstBlended; it != impl.commands.end(); ++it)
        it->viewDepth = -impl.view.transformPoint(it->bounds.center()).z;
    std::stable_sort(firstBlended, impl.commands.end(), [](const DrawCommand& a, const DrawCommand& b) { return a.viewDepth > b.viewDepth; });
    const auto blendedStart = static_cast<std::size_t>(firstBlended - impl.commands.begin());

    const sw::ShadingContext lights = sw::ShadingContext::prepare(impl.camera, impl.environment);
    ShadowProjection projection{Math::Matrix4::identity(), 0.f};
    const int shadowLight = impl.renderShadows(lights, blendedStart, projection);

    gl::UseProgram(impl.mesh.id());
    impl.setFrameUniforms(lights, shadowLight, projection);
    impl.renderMain(blendedStart);
    impl.present();
    impl.restoreState();

    impl.commands.clear();
    impl.collectGarbage();
    impl.stats.frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

k3::Image k3::HardwareRasterizer::readPixels()
{
    auto& impl = *m_impl;
    Image image;

    if (!impl.fbo)
        return image;

    image.width = impl.width;
    image.height = impl.height;
    image.pixels.resize(static_cast<std::size_t>(impl.width) * impl.height * 4);

    gl::BindFramebuffer(gl::READ_FRAMEBUFFER, impl.fbo);
    gl::ReadBuffer(gl::COLOR_ATTACHMENT0);
    gl::PixelStorei(gl::PACK_ALIGNMENT, 1);
    gl::ReadPixels(0, 0, static_cast<gl::GLsizei>(impl.width), static_cast<gl::GLsizei>(impl.height), gl::RGBA, gl::UNSIGNED_BYTE, image.pixels.data());
    gl::BindFramebuffer(gl::READ_FRAMEBUFFER, impl.presentFramebuffer.value_or(0));

    // OpenGL rows go bottom-up, Image rows top-down.
    const std::size_t stride = static_cast<std::size_t>(impl.width) * 4;
    for (std::uint32_t y = 0; y < impl.height / 2; ++y)
        std::swap_ranges(image.pixels.begin() + y * stride, image.pixels.begin() + (y + 1) * stride, image.pixels.begin() + (impl.height - 1 - y) * stride);
    return image;
}

const k3::FrameStats& k3::HardwareRasterizer::stats() const noexcept
{
    return m_impl->stats;
}

void k3::HardwareRasterizer::setPresentFramebuffer(std::optional<std::uint32_t> framebuffer) noexcept
{
    m_impl->presentFramebuffer = framebuffer;
}

const std::string& k3::HardwareRasterizer::driver() const noexcept
{
    return m_impl->driver;
}
