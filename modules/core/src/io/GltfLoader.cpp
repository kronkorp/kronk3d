#include <kronk3d/io/GltfLoader.hpp>
#include "third_party/cgltf.h"
#include "utils/Log.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace
{

    using namespace k3;

    const char* describe(cgltf_result result)
    {
        switch (result) {
            case cgltf_result_data_too_short:
                return "truncated file";
            case cgltf_result_unknown_format:
                return "not a glTF file";
            case cgltf_result_invalid_json:
                return "invalid JSON";
            case cgltf_result_invalid_gltf:
                return "invalid glTF";
            case cgltf_result_invalid_options:
                return "invalid options";
            case cgltf_result_file_not_found:
                return "file not found (external buffer or image)";
            case cgltf_result_io_error:
                return "read error";
            case cgltf_result_out_of_memory:
                return "out of memory";
            case cgltf_result_legacy_gltf:
                return "legacy glTF 1.0 is not supported";
            default:
                return "unknown error";
        }
    }

    Math::Matrix4 fromColumnMajor(const float* m)
    {
        Math::Matrix4 result;

        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                result.values[4 * row + column] = m[4 * column + row];
        return result;
    }

    Transform localTransform(const cgltf_node& node)
    {
        if (node.has_matrix)
            return Transform::fromMatrix(fromColumnMajor(node.matrix));

        Transform result;

        if (node.has_translation)
            result.translation = {node.translation[0], node.translation[1], node.translation[2]};
        if (node.has_rotation)
            result.rotation = {node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]};
        if (node.has_scale)
            result.scale = {node.scale[0], node.scale[1], node.scale[2]};
        return result;
    }

    // Raw bytes of an image: an embedded buffer view, a base64 data URI, or nothing (an external file).
    std::vector<std::uint8_t> embeddedBytes(const cgltf_image& image, const cgltf_options& options)
    {
        if (image.buffer_view) {
            const auto* bytes = static_cast<const std::uint8_t*>(cgltf_buffer_view_data(image.buffer_view));

            return bytes ? std::vector<std::uint8_t>(bytes, bytes + image.buffer_view->size) : std::vector<std::uint8_t>{};
        }
        if (!image.uri || std::strncmp(image.uri, "data:", 5) != 0)
            return {};

        const char* comma = std::strchr(image.uri, ',');
        if (!comma)
            return {};

        const std::string_view payload(comma + 1);
        const std::size_t padding = static_cast<std::size_t>(std::count(payload.begin(), payload.end(), '='));
        const std::size_t size = payload.size() / 4 * 3 - std::min(padding, payload.size() / 4 * 3);
        void* decoded = nullptr;

        if (cgltf_load_buffer_base64(&options, size, comma + 1, &decoded) != cgltf_result_success)
            return {};

        std::vector<std::uint8_t> bytes(static_cast<std::uint8_t*>(decoded), static_cast<std::uint8_t*>(decoded) + size);

        std::free(decoded);
        return bytes;
    }

    Sampler convert(const cgltf_sampler* sampler)
    {
        constexpr int NEAREST = 9728, NEAREST_MIPMAP_NEAREST = 9984, NEAREST_MIPMAP_LINEAR = 9986;
        constexpr int CLAMP_TO_EDGE = 33071, MIRRORED_REPEAT = 33648;
        Sampler result;

        auto wrap = [](int mode) {
            return mode == CLAMP_TO_EDGE ? TextureWrap::ClampToEdge : mode == MIRRORED_REPEAT ? TextureWrap::MirroredRepeat : TextureWrap::Repeat;
        };

        if (!sampler)
            return result;
        if (sampler->mag_filter == NEAREST)
            result.filter = TextureFilter::Nearest;
        if (sampler->min_filter == 9729 || sampler->min_filter == NEAREST)      // No mipmaps
            result.mipmaps = MipmapFilter::None;
        else if (sampler->min_filter == NEAREST_MIPMAP_NEAREST || sampler->min_filter == 9985)
            result.mipmaps = MipmapFilter::Nearest;
        else if (sampler->min_filter == NEAREST_MIPMAP_LINEAR || sampler->min_filter == 9987)
            result.mipmaps = MipmapFilter::Linear;
        result.wrapU = wrap(sampler->wrap_s);
        result.wrapV = wrap(sampler->wrap_t);
        return result;
    }

    class Converter
    {
        public:
            Converter(cgltf_data& data, const cgltf_options& options, const std::filesystem::path& baseDir, const GltfLoadOptions& loadOptions)
                : m_data(data), m_options(options), m_baseDir(baseDir), m_loadOptions(loadOptions)
            {
            }

            Result<AnimatedModel> run()
            {
                AnimatedModel& model = m_model;

                addNodes();
                addSkins();
                for (cgltf_size n = 0; n < m_data.nodes_count; ++n)
                    if (auto error = addMeshes(m_data.nodes[n]); !error.empty())
                        return Error{error};
                for (cgltf_size a = 0; a < m_data.animations_count; ++a)
                    addAnimation(m_data.animations[a], a);
                return std::move(model);
            }

        private:
            // Depth first, so that parents always come before their children.
            void addNodes()
            {
                m_nodeIndex.assign(m_data.nodes_count, -1);

                auto visit = [&](auto&& self, const cgltf_node& node, int parent) -> void {
                    const int index = static_cast<int>(m_model.nodes.size());

                    m_nodeIndex[static_cast<std::size_t>(&node - m_data.nodes)] = index;
                    m_model.nodes.push_back({node.name ? node.name : "", parent, localTransform(node)});
                    for (cgltf_size c = 0; c < node.children_count; ++c)
                        self(self, *node.children[c], index);
                };

                for (cgltf_size n = 0; n < m_data.nodes_count; ++n)
                    if (!m_data.nodes[n].parent)
                        visit(visit, m_data.nodes[n], -1);
            }

            void addSkins()
            {
                for (cgltf_size s = 0; s < m_data.skins_count; ++s) {
                    const cgltf_skin& source = m_data.skins[s];
                    Skin skin;

                    for (cgltf_size j = 0; j < source.joints_count; ++j)
                        skin.joints.push_back(m_nodeIndex[static_cast<std::size_t>(source.joints[j] - m_data.nodes)]);
                    skin.inverseBind.assign(source.joints_count, Math::Matrix4::identity());
                    if (source.inverse_bind_matrices && source.inverse_bind_matrices->count >= source.joints_count) {
                        for (cgltf_size j = 0; j < source.joints_count; ++j) {
                            float values[16];

                            cgltf_accessor_read_float(source.inverse_bind_matrices, j, values, 16);
                            skin.inverseBind[j] = fromColumnMajor(values);
                        }
                    }
                    m_model.skins.push_back(std::move(skin));
                }
            }

            // Returns an error message, empty on success.
            std::string addMeshes(const cgltf_node& node)
            {
                if (!node.mesh)
                    return {};
                for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
                    const cgltf_primitive& primitive = node.mesh->primitives[p];
                    auto material = materialFor(primitive.material);
                    auto mesh = meshFor(primitive, *material);

                    if (!mesh)
                        continue;
                    if (!mesh->valid())
                        return "invalid mesh \"" + std::string(node.mesh->name ? node.mesh->name : "") + "\"";

                    const bool skinned = node.skin && mesh->hasSkin();

                    m_model.primitives.push_back({
                        mesh, material,
                        m_nodeIndex[static_cast<std::size_t>(&node - m_data.nodes)],
                        skinned ? static_cast<int>(node.skin - m_data.skins) : -1,
                    });
                }
                return {};
            }

            std::shared_ptr<Mesh> meshFor(const cgltf_primitive& source, const Material& material)
            {
                if (source.type != cgltf_primitive_type_triangles) {
                    log(LogLevel::Warning, "glTF: skipping a primitive that is not a triangle list");
                    return nullptr;
                }

                // Shared by every node instancing the mesh (the material is per primitive, so the tangents too).
                auto& slot = m_meshes[&source];

                if (slot)
                    return slot;
                slot = std::make_shared<Mesh>();

                Mesh& mesh = *slot;

                for (cgltf_size a = 0; a < source.attributes_count; ++a) {
                    const cgltf_attribute& attribute = source.attributes[a];
                    const cgltf_accessor& accessor = *attribute.data;
                    const std::size_t count = accessor.count;
                    float v[16] = {};

                    if (attribute.index != 0)
                        continue;
                    switch (attribute.type) {
                        case cgltf_attribute_type_position:
                            for (std::size_t i = 0; i < count; ++i) {
                                cgltf_accessor_read_float(&accessor, i, v, 3);
                                mesh.positions.push_back({v[0], v[1], v[2]});
                            }
                            break;
                        case cgltf_attribute_type_normal:
                            for (std::size_t i = 0; i < count; ++i) {
                                cgltf_accessor_read_float(&accessor, i, v, 3);
                                mesh.normals.push_back({v[0], v[1], v[2]});
                            }
                            break;
                        case cgltf_attribute_type_tangent:
                            for (std::size_t i = 0; i < count; ++i) {
                                cgltf_accessor_read_float(&accessor, i, v, 4);
                                mesh.tangents.push_back({v[0], v[1], v[2], v[3]});
                            }
                            break;
                        case cgltf_attribute_type_texcoord:     // glTF's v = 0 is the top, like kronk3d's
                            for (std::size_t i = 0; i < count; ++i) {
                                cgltf_accessor_read_float(&accessor, i, v, 2);
                                mesh.uvs.push_back({v[0], v[1]});
                            }
                            break;
                        case cgltf_attribute_type_color: {
                            const std::size_t components = cgltf_num_components(accessor.type);

                            for (std::size_t i = 0; i < count; ++i) {
                                v[3] = 1.f;
                                cgltf_accessor_read_float(&accessor, i, v, components);
                                mesh.colors.push_back({v[0], v[1], v[2], v[3]});
                            }
                            break;
                        }
                        case cgltf_attribute_type_joints:
                            for (std::size_t i = 0; i < count; ++i) {
                                cgltf_uint j[4] = {};

                                cgltf_accessor_read_uint(&accessor, i, j, 4);
                                mesh.joints.push_back({static_cast<std::uint16_t>(j[0]), static_cast<std::uint16_t>(j[1]),
                                                       static_cast<std::uint16_t>(j[2]), static_cast<std::uint16_t>(j[3])});
                            }
                            break;
                        case cgltf_attribute_type_weights:
                            for (std::size_t i = 0; i < count; ++i) {
                                cgltf_accessor_read_float(&accessor, i, v, 4);

                                const float sum = v[0] + v[1] + v[2] + v[3];
                                const float scale = sum > 0.f ? 1.f / sum : 0.f;

                                mesh.weights.push_back({v[0] * scale, v[1] * scale, v[2] * scale, v[3] * scale});
                            }
                            break;
                        default:
                            break;
                    }
                }
                if (source.indices) {
                    mesh.indices.resize(source.indices->count);
                    for (std::size_t i = 0; i < mesh.indices.size(); ++i)
                        mesh.indices[i] = static_cast<std::uint32_t>(cgltf_accessor_read_index(source.indices, i));
                }
                if (!mesh.hasNormals() && !mesh.positions.empty() && mesh.valid())
                    mesh.computeNormals();
                if (material.normalMap && !mesh.hasTangents() && mesh.hasNormals() && mesh.hasUVs() && mesh.valid())
                    mesh.computeTangents();
                return slot;
            }

            std::shared_ptr<Material> materialFor(const cgltf_material* source)
            {
                auto& slot = m_materials[source];

                if (slot)
                    return slot;
                slot = std::make_shared<Material>();
                if (!source)
                    return slot;

                Material& material = *slot;
                const cgltf_pbr_metallic_roughness& pbr = source->pbr_metallic_roughness;
                // The spec's defaults when the block is absent: white, fully metallic, rough.
                const float base[4] = {
                    source->has_pbr_metallic_roughness ? pbr.base_color_factor[0] : 1.f, source->has_pbr_metallic_roughness ? pbr.base_color_factor[1] : 1.f,
                    source->has_pbr_metallic_roughness ? pbr.base_color_factor[2] : 1.f, source->has_pbr_metallic_roughness ? pbr.base_color_factor[3] : 1.f,
                };
                const float metallic = source->has_pbr_metallic_roughness ? pbr.metallic_factor : 1.f;
                const float roughness = source->has_pbr_metallic_roughness ? pbr.roughness_factor : 1.f;

                material.name = source->name ? source->name : "";
                material.diffuse = {base[0], base[1], base[2], base[3]};
                // Dielectrics reflect 4% white, metals their base color; roughness -> Blinn-Phong exponent (2 / alpha^2 - 2).
                const float alpha = std::max(roughness * roughness, 0.02f);
                const float f0 = 0.04f * (1.f - metallic);

                material.specular = {f0 + base[0] * metallic, f0 + base[1] * metallic, f0 + base[2] * metallic, 1.f};
                material.shininess = std::clamp(2.f / (alpha * alpha) - 2.f, 2.f, 512.f);
                material.emissive = {source->emissive_factor[0], source->emissive_factor[1], source->emissive_factor[2], 1.f};
                material.unlit = source->unlit;
                material.doubleSided = source->double_sided;
                material.alphaCutoff = source->alpha_cutoff;
                material.alphaMode = source->alpha_mode == cgltf_alpha_mode_blend ? AlphaMode::Blend
                                   : source->alpha_mode == cgltf_alpha_mode_mask ? AlphaMode::Mask : AlphaMode::Opaque;
                if (source->has_pbr_metallic_roughness)
                    material.diffuseMap = textureFor(pbr.base_color_texture.texture, ColorSpace::Srgb);
                material.normalMap = textureFor(source->normal_texture.texture, ColorSpace::Linear);
                material.normalScale = source->normal_texture.scale;
                return slot;
            }

            std::shared_ptr<Texture> textureFor(const cgltf_texture* source, ColorSpace colorSpace)
            {
                if (!source || !source->image || !m_loadOptions.loadTextures)
                    return nullptr;

                auto& slot = m_textures[{source, colorSpace}];

                if (slot)
                    return slot;

                const cgltf_image& image = *source->image;
                const auto bytes = embeddedBytes(image, m_options);
                Result<Image> decoded = Error{"no image data"};

                if (!bytes.empty()) {
                    decoded = Image::loadFromMemory(bytes.data(), bytes.size());
                } else if (image.uri && std::strncmp(image.uri, "data:", 5) != 0) {
                    std::string uri = image.uri;

                    cgltf_decode_uri(uri.data());
                    uri.resize(std::strlen(uri.c_str()));
                    decoded = Image::load(m_baseDir / uri);
                }
                if (!decoded) {
                    log(LogLevel::Warning, "glTF: cannot load texture \"{}\": {}", image.name ? image.name : (image.uri ? image.uri : ""), decoded.error());
                    return nullptr;
                }
                slot = std::make_shared<Texture>(std::move(*decoded), colorSpace);
                slot->sampler = convert(source->sampler);
                return slot;
            }

            void addAnimation(const cgltf_animation& source, cgltf_size index)
            {
                Animation animation;

                animation.name = source.name ? source.name : "animation_" + std::to_string(index);
                for (cgltf_size c = 0; c < source.channels_count; ++c) {
                    const cgltf_animation_channel& channel = source.channels[c];
                    const cgltf_animation_sampler& sampler = *channel.sampler;
                    AnimationChannel result;

                    if (!channel.target_node || channel.target_path == cgltf_animation_path_type_weights
                        || channel.target_path == cgltf_animation_path_type_invalid) {
                        if (channel.target_path == cgltf_animation_path_type_weights)
                            log(LogLevel::Warning, "glTF: morph target animations are not supported (clip \"{}\")", animation.name);
                        continue;
                    }
                    result.node = m_nodeIndex[static_cast<std::size_t>(channel.target_node - m_data.nodes)];
                    result.path = channel.target_path == cgltf_animation_path_type_translation ? AnimationChannel::Path::Translation
                                : channel.target_path == cgltf_animation_path_type_rotation ? AnimationChannel::Path::Rotation
                                : AnimationChannel::Path::Scale;
                    result.interpolation = sampler.interpolation == cgltf_interpolation_type_step ? AnimationChannel::Interpolation::Step
                                         : sampler.interpolation == cgltf_interpolation_type_cubic_spline ? AnimationChannel::Interpolation::CubicSpline
                                         : AnimationChannel::Interpolation::Linear;

                    const std::size_t keys = sampler.input->count;
                    const std::size_t components = result.path == AnimationChannel::Path::Rotation ? 4 : 3;
                    const std::size_t expected = result.interpolation == AnimationChannel::Interpolation::CubicSpline ? 3 * keys : keys;

                    if (keys == 0 || sampler.output->count != expected || cgltf_num_components(sampler.output->type) != components) {
                        log(LogLevel::Warning, "glTF: skipping a malformed channel in clip \"{}\"", animation.name);
                        continue;
                    }
                    result.times.resize(keys);
                    cgltf_accessor_unpack_floats(sampler.input, result.times.data(), keys);

                    std::vector<float> flat(expected * components);

                    cgltf_accessor_unpack_floats(sampler.output, flat.data(), flat.size());
                    for (std::size_t k = 0; k < expected; ++k)
                        result.values.push_back({flat[k * components], flat[k * components + 1], flat[k * components + 2], components == 4 ? flat[k * components + 3] : 0.f});
                    animation.duration = std::max(animation.duration, result.times.back());
                    animation.channels.push_back(std::move(result));
                }
                m_model.animations.push_back(std::move(animation));
            }

            cgltf_data&                  m_data;
            const cgltf_options&         m_options;
            const std::filesystem::path& m_baseDir;
            const GltfLoadOptions&       m_loadOptions;
            AnimatedModel                m_model{};
            std::vector<int>             m_nodeIndex{};     // cgltf node -> AnimatedModel::nodes
            std::unordered_map<const cgltf_primitive*, std::shared_ptr<Mesh>>     m_meshes{};
            std::unordered_map<const cgltf_material*, std::shared_ptr<Material>>  m_materials{};
            std::map<std::pair<const cgltf_texture*, ColorSpace>, std::shared_ptr<Texture>> m_textures{};
    };

}

k3::Result<k3::AnimatedModel> k3::GltfLoader::load(const std::filesystem::path& path, const GltfLoadOptions& options)
{
    // Read through std::ifstream so that non-ASCII paths work on Windows.
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return Error{"cannot open " + path.string()};

    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto result = loadFromMemory(bytes.data(), bytes.size(), path.parent_path(), options);

    if (result)
        result->name = path.stem().string();
    return result;
}

k3::Result<k3::AnimatedModel> k3::GltfLoader::loadFromMemory(const void* bytes, std::size_t size, const std::filesystem::path& baseDir, const GltfLoadOptions& loadOptions)
{
    cgltf_options options{};
    cgltf_data* raw = nullptr;

    if (const auto status = cgltf_parse(&options, bytes, size, &raw); status != cgltf_result_success)
        return Error{std::string("glTF: ") + describe(status)};

    const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
    // cgltf wants a file inside the directory, to resolve relative URIs against.
    const std::string anchor = (baseDir / "model.gltf").string();

    if (const auto status = cgltf_load_buffers(&options, raw, anchor.c_str()); status != cgltf_result_success)
        return Error{std::string("glTF: ") + describe(status)};
    if (const auto status = cgltf_validate(raw); status != cgltf_result_success)
        return Error{std::string("glTF: ") + describe(status)};
    return Converter(*raw, options, baseDir, loadOptions).run();
}
