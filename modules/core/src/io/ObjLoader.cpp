#include "ObjLoader.hpp"
#include "render/software/ThreadPool.hpp"
#include "utils/Log.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

    using MaterialMap = std::unordered_map<std::string, std::shared_ptr<k3::Material>>;

    // Normal "index" of a face corner: >= 0 is an OBJ vn, SMOOTH is generated from the adjacent
    // faces sharing the position, and <= FLAT_BASE encodes a per-face (flat) generated normal.
    constexpr int SMOOTH = -1;
    constexpr int FLAT_BASE = -2;

    /* Tokenizing */

    bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
    }

    std::string_view trim(std::string_view s)
    {
        while (!s.empty() && isSpace(s.front()))
            s.remove_prefix(1);
        while (!s.empty() && isSpace(s.back()))
            s.remove_suffix(1);
        return s;
    }

    // Pops the next whitespace-separated token out of `s`.
    std::string_view nextToken(std::string_view& s)
    {
        s = trim(s);
        std::size_t end = 0;

        while (end < s.size() && !isSpace(s[end]))
            ++end;

        std::string_view token = s.substr(0, end);
        s.remove_prefix(end);
        return token;
    }

    bool parseFloat(std::string_view token, float& out)
    {
        if (!token.empty() && token.front() == '+')
            token.remove_prefix(1);
        auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out);
        return ec == std::errc{} && ptr == token.data() + token.size();
    }

    bool parseInt(std::string_view token, int& out)
    {
        auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out);
        return ec == std::errc{} && ptr == token.data() + token.size();
    }

    // Reads up to `max` floats, returns how many were read.
    std::size_t parseFloats(std::string_view& rest, float* out, std::size_t max)
    {
        std::size_t count = 0;

        while (count < max) {
            std::string_view copy = rest;
            std::string_view token = nextToken(copy);

            if (token.empty() || !parseFloat(token, out[count]))
                break;
            rest = copy;
            ++count;
        }
        return count;
    }

    k3::Math::Color parseColor(std::string_view rest)
    {
        float rgb[3] = {0.f, 0.f, 0.f};
        std::size_t count = parseFloats(rest, rgb, 3);

        // "Kd 0.5" is a legal shorthand for a grey.
        if (count == 1)
            rgb[1] = rgb[2] = rgb[0];
        return {rgb[0], rgb[1], rgb[2], 1.f};
    }

    std::string lowercase(std::string s)
    {
        std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    /* Files */

    std::string readAll(std::istream& stream)
    {
        std::ostringstream buffer;

        buffer << stream.rdbuf();
        return std::move(buffer).str();
    }

    std::unique_ptr<std::istream> openFile(const std::filesystem::path& path, const k3::ObjLoadOptions& options)
    {
        if (options.openFile)
            return options.openFile(path);

        auto file = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (!*file)
            return nullptr;
        return file;
    }

    // Paths written on Windows use '\', and assets made there often disagree on case with the disk.
    std::filesystem::path resolvePath(const std::filesystem::path& baseDir, std::string raw)
    {
        std::ranges::replace(raw, '\\', '/');

        std::filesystem::path path = baseDir / std::filesystem::path(raw).make_preferred();
        std::error_code ec;

        if (std::filesystem::exists(path, ec))
            return path;

        const auto parent = path.parent_path();
        const auto wanted = lowercase(path.filename().string());

        for (const auto& entry : std::filesystem::directory_iterator(parent, ec))
            if (lowercase(entry.path().filename().string()) == wanted)
                return entry.path();
        return path;
    }

    /* MTL */

    struct TextureStatement
    {
        std::string path;
        bool clamp = false;
        float bumpMultiplier = 1.f;     // -bm
    };

    // "map_Kd -s 1 1 1 -clamp on my texture.png": options first, the rest of the line is the file name.
    TextureStatement parseTextureStatement(std::string_view rest)
    {
        static const std::unordered_map<std::string_view, std::size_t> OPTION_ARGS = {
            {"-blendu", 1}, {"-blendv", 1}, {"-bm", 1}, {"-boost", 1}, {"-cc", 1}, {"-clamp", 1},
            {"-imfchan", 1}, {"-mm", 2}, {"-o", 3}, {"-s", 3}, {"-t", 3}, {"-texres", 1}, {"-type", 1},
        };
        TextureStatement statement;

        for (;;) {
            std::string_view copy = rest;
            std::string_view option = nextToken(copy);
            auto it = OPTION_ARGS.find(option);

            if (it == OPTION_ARGS.end())
                break;
            rest = copy;
            if (option == "-clamp") {
                statement.clamp = nextToken(rest) == "on";
            } else if (option == "-bm") {
                parseFloats(rest, &statement.bumpMultiplier, 1);
            } else if (option == "-o" || option == "-s" || option == "-t") {
                float ignored[3];
                parseFloats(rest, ignored, 3);
            } else {
                for (std::size_t i = 0; i < it->second; ++i)
                    nextToken(rest);
            }
        }
        statement.path = std::string(trim(rest));
        return statement;
    }

    using TextureSlot = std::shared_ptr<k3::Texture> k3::Material::*;

    enum class TextureKind {
        Color,      // As is (sRGB)
        Opacity,    // Normalized to "red channel = opacity" (linear)
        Normal      // Tangent-space normal map (linear); height maps are converted
    };

    // Height maps converted to normal maps: rising over the whole height range across 1/64 of the
    // texture's width tilts the surface by 45 degrees (times -bm), whatever the texture resolution.
    constexpr float HEIGHT_MAP_SLOPE = 1.f / 64.f;

    // Textures are requested while MTL files are read, then decoded all at once, in parallel (decoding
    // PNG/JPEG is most of the loading time of a textured scene). Each file is loaded once per usage.
    class TextureCache
    {
        public:
            TextureCache(const k3::ObjLoadOptions& options) : m_options(options) {}

            void request(const std::shared_ptr<k3::Material>& material, TextureSlot slot, const std::filesystem::path& path, TextureKind kind, const TextureStatement& statement)
            {
                const std::string key = std::format("{}|{}|{}|{}", path.string(), static_cast<int>(kind), statement.clamp, statement.bumpMultiplier);
                auto [it, inserted] = m_indices.try_emplace(key, m_entries.size());

                if (inserted)
                    m_entries.push_back({path, kind, statement.clamp, statement.bumpMultiplier});

                Entry& entry = m_entries[it->second];
                if (entry.loaded)
                    (*material).*slot = entry.texture;
                else
                    entry.users.emplace_back(material, slot);
            }

            // Loads every texture requested since the last call and hands them to their materials.
            void resolve()
            {
                std::vector<Entry*> pending;
                for (auto& entry : m_entries)
                    if (!entry.loaded)
                        pending.push_back(&entry);

                auto load = [&](std::size_t i) { pending[i]->texture = loadOne(*pending[i]); };
                if (m_options.loadTexture) {
                    // A user hook may not be thread-safe.
                    for (std::size_t i = 0; i < pending.size(); ++i)
                        load(i);
                } else {
                    k3::sw::ThreadPool(0).parallelFor(pending.size(), load);
                }

                for (Entry* entry : pending) {
                    for (auto& [material, slot] : entry->users)
                        (*material).*slot = entry->texture;
                    entry->users.clear();
                    entry->loaded = true;
                }
            }

        private:
            struct Entry
            {
                std::filesystem::path        path;
                TextureKind                  kind;
                bool                         clamp;
                float                        bumpMultiplier;
                std::shared_ptr<k3::Texture> texture{};
                bool                         loaded = false;
                std::vector<std::pair<std::shared_ptr<k3::Material>, TextureSlot>> users{};
            };

            std::shared_ptr<k3::Texture> loadOne(const Entry& entry) const
            {
                const k3::ColorSpace colorSpace = entry.kind == TextureKind::Color ? k3::ColorSpace::Srgb : k3::ColorSpace::Linear;
                std::shared_ptr<k3::Texture> texture;

                if (m_options.loadTexture) {
                    texture = m_options.loadTexture(entry.path, colorSpace);
                } else if (auto image = k3::Image::load(entry.path)) {
                    texture = std::make_shared<k3::Texture>(std::move(*image), colorSpace);
                } else {
                    k3::log(k3::LogLevel::Error, "Failed to load texture {}", image.error());
                }

                if (texture && entry.kind == TextureKind::Opacity)
                    texture = toOpacityMask(*texture);
                if (texture && entry.kind == TextureKind::Normal)
                    texture = toNormalMap(*texture, entry.bumpMultiplier);
                if (texture && entry.clamp) {
                    // Copy first: a loader hook may share its textures between requests.
                    texture = std::make_shared<k3::Texture>(*texture);
                    texture->sampler.wrapU = texture->sampler.wrapV = k3::TextureWrap::ClampToEdge;
                }
                return texture;
            }

            // MTL's bump maps are heights, but many exporters write normal maps there: grey-scale images
            // are heights (converted), colored ones normal maps. -bm scales the bumpiness of both.
            static std::shared_ptr<k3::Texture> toNormalMap(const k3::Texture& texture, float bumpMultiplier)
            {
                const k3::Image& image = texture.image();

                if (image.isGrayscale())
                    return k3::Texture::normalMapFromHeight(image, static_cast<float>(image.width) * HEIGHT_MAP_SLOPE * bumpMultiplier);

                k3::Image normals = image;
                if (bumpMultiplier != 1.f) {
                    for (std::size_t i = 0; i < normals.pixels.size(); i += 4) {
                        auto decode = [&](std::size_t c) { return normals.pixels[i + c] / 127.5f - 1.f; };
                        const auto n = k3::Math::Vector3f::normalize({decode(0) * bumpMultiplier, decode(1) * bumpMultiplier, decode(2)});
                        normals.pixels[i + 0] = static_cast<std::uint8_t>(std::lround((n.x * 0.5f + 0.5f) * 255.f));
                        normals.pixels[i + 1] = static_cast<std::uint8_t>(std::lround((n.y * 0.5f + 0.5f) * 255.f));
                        normals.pixels[i + 2] = static_cast<std::uint8_t>(std::lround((n.z * 0.5f + 0.5f) * 255.f));
                    }
                }
                return std::make_shared<k3::Texture>(std::move(normals), k3::ColorSpace::Linear);
            }

            // map_d is usually a grey-scale image, but some exporters store the mask in the alpha
            // channel instead: normalize to "red channel = opacity".
            static std::shared_ptr<k3::Texture> toOpacityMask(const k3::Texture& texture)
            {
                auto result = std::make_shared<k3::Texture>(texture);

                if (texture.hasTransparency()) {
                    k3::Image image = texture.image();

                    for (std::size_t i = 0; i < image.pixels.size(); i += 4) {
                        image.pixels[i] = image.pixels[i + 1] = image.pixels[i + 2] = image.pixels[i + 3];
                        image.pixels[i + 3] = 255;
                    }
                    result->setImage(std::move(image));
                }
                return result;
            }

            const k3::ObjLoadOptions&                    m_options;
            std::vector<Entry>                           m_entries;     // In request order
            std::unordered_map<std::string, std::size_t> m_indices;
    };

    void parseMtl(
        const std::string& source,
        const std::filesystem::path& baseDir,
        const k3::ObjLoadOptions& options,
        TextureCache& textures,
        MaterialMap& materials
    )
    {
        std::shared_ptr<k3::Material> current;
        std::istringstream lines(source);
        std::string line;

        auto requestMap = [&](std::string_view rest, TextureSlot slot, TextureKind kind) {
            auto statement = parseTextureStatement(rest);

            (*current).*slot = nullptr;
            if (options.loadTextures && !statement.path.empty())
                textures.request(current, slot, resolvePath(baseDir, statement.path), kind, statement);
        };

        while (std::getline(lines, line)) {
            std::string_view rest = line;
            std::string_view keyword = nextToken(rest);

            if (keyword == "newmtl") {
                current = std::make_shared<k3::Material>();
                current->name = std::string(trim(rest));
                // MTL defaults: no specular unless Ks says so.
                current->specular = {0.f, 0.f, 0.f, 1.f};
                materials[current->name] = current;
                continue;
            }
            if (!current || keyword.empty() || keyword.front() == '#')
                continue;

            if (keyword == "Kd") {
                const float alpha = current->diffuse.a;
                current->diffuse = parseColor(rest);
                current->diffuse.a = alpha;
            } else if (keyword == "Ks") {
                current->specular = parseColor(rest);
            } else if (keyword == "Ke") {
                current->emissive = parseColor(rest);
            } else if (keyword == "Ns") {
                parseFloats(rest, &current->shininess, 1);
            } else if (keyword == "d") {
                parseFloats(rest, &current->diffuse.a, 1);
            } else if (keyword == "Tr") {
                float transparency = 0.f;
                if (parseFloats(rest, &transparency, 1) == 1)
                    current->diffuse.a = 1.f - transparency;
            } else if (keyword == "illum") {
                int illum = 2;
                parseInt(nextToken(rest), illum);
                if (illum == 0)
                    current->unlit = true;
                else if (illum == 1)
                    current->specular = {0.f, 0.f, 0.f, 1.f};
            } else if (keyword == "map_Kd") {
                requestMap(rest, &k3::Material::diffuseMap, TextureKind::Color);
            } else if (keyword == "map_Ks") {
                requestMap(rest, &k3::Material::specularMap, TextureKind::Color);
            } else if (keyword == "map_d") {
                requestMap(rest, &k3::Material::opacityMap, TextureKind::Opacity);
            } else if (keyword == "norm" || keyword == "map_Bump" || keyword == "map_bump" || keyword == "bump") {
                requestMap(rest, &k3::Material::normalMap, TextureKind::Normal);
            }
        }
    }

    // `mtllib a.mtl b.mtl` lists several files, but a single file name may also contain spaces.
    void loadMaterialLibraries(
        std::string_view rest,
        const std::filesystem::path& baseDir,
        const k3::ObjLoadOptions& options,
        TextureCache& textures,
        MaterialMap& materials
    )
    {
        auto tryLoad = [&](std::string_view name) {
            const auto path = resolvePath(baseDir, std::string(name));
            auto stream = openFile(path, options);

            if (!stream)
                return false;
            parseMtl(readAll(*stream), path.parent_path(), options, textures, materials);
            return true;
        };

        rest = trim(rest);
        if (!tryLoad(rest)) {
            while (!rest.empty()) {
                std::string_view name = nextToken(rest);
                if (!name.empty() && !tryLoad(name))
                    k3::log(k3::LogLevel::Warning, "OBJ: cannot open material library {}", name);
            }
        }
        textures.resolve();
    }

    /* OBJ */

    struct Corner
    {
        int v, vt, vn;

        bool operator==(const Corner&) const = default;
    };

    struct CornerHash
    {
        std::size_t operator()(const Corner& c) const noexcept
        {
            std::size_t h = static_cast<std::uint32_t>(c.v);
            h = h * 1000003u ^ static_cast<std::uint32_t>(c.vt);
            h = h * 1000003u ^ static_cast<std::uint32_t>(c.vn);
            return h;
        }
    };

    struct PrimitiveBuilder
    {
        std::shared_ptr<k3::Material> material;
        k3::Mesh mesh;
        std::unordered_map<Corner, std::uint32_t, CornerHash> lookup;
        std::vector<std::pair<std::uint32_t, int>> smoothVertices;   // (mesh vertex, OBJ position)
        bool hasUVs = false;
    };

    class ObjParser
    {
        public:
            ObjParser(const std::filesystem::path& baseDir, const k3::ObjLoadOptions& options)
                : m_baseDir(baseDir), m_options(options), m_textures(options) {}

            k3::Result<k3::Model> parse(const std::string& source)
            {
                std::istringstream lines(source);
                std::string line;
                std::size_t lineNumber = 0;

                while (std::getline(lines, line)) {
                    ++lineNumber;
                    if (auto error = parseLine(line); !error.empty())
                        return k3::Error(std::format("line {}: {}", lineNumber, error));
                }
                return finish();
            }

        private:
            std::string parseLine(std::string_view rest)
            {
                std::string_view keyword = nextToken(rest);

                if (keyword == "v") {
                    float values[7] = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
                    std::size_t count = parseFloats(rest, values, 7);

                    if (count < 3)
                        return "vertex needs at least 3 coordinates";
                    m_positions.push_back({values[0], values[1], values[2]});
                    // 6 values: "x y z r g b" (common extension). 4 values: "x y z w", w ignored.
                    if (count >= 6) {
                        m_colors.resize(m_positions.size() - 1, k3::Math::Color::White);
                        m_colors.push_back({values[3], values[4], values[5], 1.f});
                    }
                } else if (keyword == "vt") {
                    float uv[2] = {0.f, 0.f};
                    if (parseFloats(rest, uv, 2) < 1)
                        return "texture coordinate needs at least 1 value";
                    m_uvs.push_back({uv[0], m_options.flipV ? 1.f - uv[1] : uv[1]});
                } else if (keyword == "vn") {
                    float n[3] = {0.f, 0.f, 0.f};
                    if (parseFloats(rest, n, 3) < 3)
                        return "normal needs 3 values";
                    m_normals.push_back(k3::Math::Vector3f::normalize({n[0], n[1], n[2]}));
                } else if (keyword == "f") {
                    return parseFace(rest);
                } else if (keyword == "usemtl") {
                    m_current = &builder(std::string(trim(rest)));
                } else if (keyword == "mtllib") {
                    loadMaterialLibraries(rest, m_baseDir, m_options, m_textures, m_materials);
                } else if (keyword == "s") {
                    std::string_view group = nextToken(rest);
                    m_flat = group == "off" || group == "0";
                }
                // o, g, l, p, comments and unknown statements are ignored.
                return {};
            }

            // Resolves a 1-based (or negative, relative) OBJ index. -1 when absent.
            static bool resolveIndex(std::string_view token, std::size_t count, int& out)
            {
                if (token.empty()) {
                    out = -1;
                    return true;
                }

                int index = 0;
                if (!parseInt(token, index) || index == 0)
                    return false;
                out = index > 0 ? index - 1 : static_cast<int>(count) + index;
                return out >= 0 && static_cast<std::size_t>(out) < count;
            }

            std::string parseFace(std::string_view rest)
            {
                m_corners.clear();

                for (std::string_view token = nextToken(rest); !token.empty(); token = nextToken(rest)) {
                    std::string_view parts[3];
                    std::size_t part = 0;

                    for (std::size_t slash; part < 2 && (slash = token.find('/')) != std::string_view::npos; ++part) {
                        parts[part] = token.substr(0, slash);
                        token.remove_prefix(slash + 1);
                    }
                    parts[part] = token;

                    Corner corner{};
                    if (!resolveIndex(parts[0], m_positions.size(), corner.v) || corner.v < 0)
                        return std::format("invalid vertex index in face ({})", parts[0]);
                    if (!resolveIndex(parts[1], m_uvs.size(), corner.vt))
                        return std::format("invalid texture coordinate index in face ({})", parts[1]);
                    if (!resolveIndex(parts[2], m_normals.size(), corner.vn))
                        return std::format("invalid normal index in face ({})", parts[2]);
                    m_corners.push_back(corner);
                }
                if (m_corners.size() < 3)
                    return {};   // Degenerate face: skip, as other loaders do.

                // Newell's method: robust polygon normal, its length is twice the polygon area.
                k3::Math::Vector3f faceNormal{};
                for (std::size_t i = 0; i < m_corners.size(); ++i) {
                    const auto& a = m_positions[m_corners[i].v];
                    const auto& b = m_positions[m_corners[(i + 1) % m_corners.size()].v];
                    faceNormal += {(a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)};
                }

                int generated = SMOOTH;
                if (m_flat) {
                    generated = FLAT_BASE - static_cast<int>(m_flatNormals.size());
                    m_flatNormals.push_back(k3::Math::Vector3f::normalize(faceNormal));
                }

                for (auto& corner : m_corners) {
                    if (corner.vn < 0) {
                        corner.vn = generated;
                        if (generated == SMOOTH) {
                            m_smoothNormals.resize(m_positions.size());
                            m_smoothNormals[corner.v] += faceNormal;
                        }
                    }
                }

                PrimitiveBuilder& target = current();
                const std::uint32_t first = emit(target, m_corners[0]);
                for (std::size_t i = 1; i + 1 < m_corners.size(); ++i) {
                    target.mesh.indices.push_back(first);
                    target.mesh.indices.push_back(emit(target, m_corners[i]));
                    target.mesh.indices.push_back(emit(target, m_corners[i + 1]));
                }
                return {};
            }

            std::uint32_t emit(PrimitiveBuilder& target, const Corner& corner)
            {
                if (auto it = target.lookup.find(corner); it != target.lookup.end())
                    return it->second;

                auto& mesh = target.mesh;
                const auto index = static_cast<std::uint32_t>(mesh.positions.size());

                mesh.positions.push_back(m_positions[corner.v]);
                mesh.uvs.push_back(corner.vt >= 0 ? m_uvs[corner.vt] : k3::Math::Vector2f{});
                target.hasUVs |= corner.vt >= 0;

                if (corner.vn >= 0) {
                    mesh.normals.push_back(m_normals[corner.vn]);
                } else if (corner.vn == SMOOTH) {
                    mesh.normals.emplace_back();
                    target.smoothVertices.emplace_back(index, corner.v);
                } else {
                    mesh.normals.push_back(m_flatNormals[FLAT_BASE - corner.vn]);
                }

                target.lookup.emplace(corner, index);
                return index;
            }

            PrimitiveBuilder& builder(const std::string& materialName)
            {
                for (auto& b : m_builders)
                    if (b.material->name == materialName)
                        return b;

                auto& b = m_builders.emplace_back();
                if (auto it = m_materials.find(materialName); it != m_materials.end()) {
                    b.material = it->second;
                } else {
                    if (!materialName.empty())
                        k3::log(k3::LogLevel::Warning, "OBJ: unknown material {}, using a default one", materialName);
                    b.material = std::make_shared<k3::Material>();
                    b.material->name = materialName;
                }
                return b;
            }

            PrimitiveBuilder& current()
            {
                if (!m_current)
                    m_current = &builder("");
                return *m_current;
            }

            k3::Result<k3::Model> finish()
            {
                k3::Model model;
                const bool hasColors = !m_colors.empty();

                m_colors.resize(m_positions.size(), k3::Math::Color::White);
                for (auto& n : m_smoothNormals)
                    n = k3::Math::Vector3f::normalize(n);

                for (auto& b : m_builders) {
                    if (b.mesh.indices.empty())
                        continue;

                    for (auto [vertex, position] : b.smoothVertices)
                        b.mesh.normals[vertex] = m_smoothNormals[position];
                    if (!b.hasUVs)
                        b.mesh.uvs.clear();
                    if (hasColors) {
                        b.mesh.colors.resize(b.mesh.positions.size());
                        for (const auto& [corner, index] : b.lookup)
                            b.mesh.colors[index] = m_colors[corner.v];
                    }

                    if (b.material->normalMap)
                        b.mesh.computeTangents();
                    chooseAlphaMode(*b.material);
                    model.primitives.push_back({
                        std::make_shared<k3::Mesh>(std::move(b.mesh)),
                        b.material,
                    });
                }
                return model;
            }

            static void chooseAlphaMode(k3::Material& material)
            {
                if (material.alphaMode != k3::AlphaMode::Opaque)
                    return;
                if (material.diffuse.a < 1.f)
                    material.alphaMode = k3::AlphaMode::Blend;
                else if (material.opacityMap || (material.diffuseMap && material.diffuseMap->hasTransparency()))
                    material.alphaMode = k3::AlphaMode::Mask;
            }

            const std::filesystem::path& m_baseDir;
            const k3::ObjLoadOptions& m_options;
            TextureCache m_textures;
            MaterialMap m_materials;

            std::vector<k3::Math::Vector3f> m_positions;
            std::vector<k3::Math::Vector2f> m_uvs;
            std::vector<k3::Math::Vector3f> m_normals;
            std::vector<k3::Math::Color>    m_colors;
            std::vector<k3::Math::Vector3f> m_smoothNormals;
            std::vector<k3::Math::Vector3f> m_flatNormals;

            std::vector<Corner> m_corners;
            // m_current is re-assigned right after every builder insertion, so vector reallocation is safe.
            std::vector<PrimitiveBuilder> m_builders;
            PrimitiveBuilder* m_current = nullptr;
            bool m_flat = false;
    };

}

k3::Result<k3::Model> k3::ObjLoader::loadFromStream(
    std::istream& obj,
    const std::filesystem::path& baseDir,
    const ObjLoadOptions& options
)
{
    ObjParser parser(baseDir, options);
    return parser.parse(readAll(obj));
}

k3::Result<k3::Model> k3::ObjLoader::load(const std::filesystem::path& path, const ObjLoadOptions& options)
{
    auto stream = openFile(path, options);

    if (!stream)
        return k3::Error("cannot open " + path.string());

    auto model = loadFromStream(*stream, path.parent_path(), options);
    if (!model)
        return k3::Error(path.string() + ": " + model.error());

    std::size_t triangles = 0;
    for (const auto& primitive : model->primitives)
        triangles += primitive.mesh->triangleCount();
    model->name = path.stem().string();
    log(LogLevel::Info, "Loaded {}: {} primitive(s), {} triangles", path.string(), model->primitives.size(), triangles);
    return model;
}
