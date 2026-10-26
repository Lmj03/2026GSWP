#include "stdafx.h"
#include "ModelAsset.h"
#include "Dependencies/cgltf.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace Models
{
namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::vector<float> Floats(const cgltf_accessor* accessor, cgltf_type type)
{
    Require(accessor && accessor->type == type && accessor->count > 0 && accessor->count <= 4000000,
            "invalid accessor shape or count");
    const size_t count = accessor->count * cgltf_num_components(type);
    std::vector<float> values(count);
    Require(cgltf_accessor_unpack_floats(accessor, values.data(), count) == count,
            "unreadable accessor");
    for (float value : values)
    {
        Require(std::isfinite(value), "non-finite accessor value");
    }
    return values;
}

std::vector<char> ReadBinary(const std::wstring& file)
{
    std::ifstream stream(file.c_str(), std::ios::binary | std::ios::ate);
    Require(bool(stream), "cannot open model file");
    const auto size = stream.tellg();
    Require(size > 0 && size <= 128 * 1024 * 1024, "model size outside 1..128 MiB");
    std::vector<char> bytes(static_cast<size_t>(size));
    stream.seekg(0);
    Require(bool(stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))),
            "incomplete model read");
    return bytes;
}

void Hierarchy(Asset& asset)
{
    std::vector<int> state(asset.nodes.size());
    std::function<void(size_t)> visit = [&](size_t i)
    {
        Require(state[i] != 1, "cyclic node hierarchy");
        if (state[i] == 2)
        {
            return;
        }
        state[i] = 1;
        if (asset.nodes[i].parent >= 0)
        {
            visit(static_cast<size_t>(asset.nodes[i].parent));
        }
        state[i] = 2;
        asset.hierarchy.push_back(i);
    };
    for (size_t i = 0; i < asset.nodes.size(); ++i)
    {
        visit(i);
    }
}

void CopyFinite(const float* source, float* target, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        Require(std::isfinite(source[i]), "non-finite transform or material");
        target[i] = source[i];
    }
}

void MeasureBounds(Asset& asset)
{
    std::copy_n(asset.vertices.front().position, 3, asset.minimum.data());
    asset.maximum = asset.minimum;
    for (const auto& vertex : asset.vertices)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            asset.minimum[i] = std::min(asset.minimum[i], vertex.position[i]);
            asset.maximum[i] = std::max(asset.maximum[i], vertex.position[i]);
        }
    }
}
}

bool LoadGlb(const std::wstring& file, Asset& output, std::string& error)
{
    try
    {
        const auto bytes = ReadBinary(file);
        cgltf_options options = {};
        options.type = cgltf_file_type_glb;
        cgltf_data* raw = nullptr;
        Require(cgltf_parse(&options, bytes.data(), bytes.size(), &raw) == cgltf_result_success,
                "GLB parse failed");
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
        Require(raw->buffers_count == 1 && !raw->buffers[0].uri,
                "only self-contained GLB buffers are supported");
        Require(cgltf_load_buffers(&options, raw, nullptr) == cgltf_result_success,
                "GLB buffer load failed");
        Require(cgltf_validate(raw) == cgltf_result_success, "GLB validation failed");
        Require(raw->extensions_required_count == 0, "required glTF extensions are unsupported");
        Require(raw->nodes_count > 0 && raw->nodes_count <= 1024 && raw->animations_count <= 256,
                "too many nodes or animations");
        Asset asset;
        asset.nodes.resize(raw->nodes_count);
        for (size_t i = 0; i < raw->nodes_count; ++i)
        {
            const auto& source = raw->nodes[i];
            auto& node = asset.nodes[i];
            node.name = source.name ? source.name : "";
            node.parent = source.parent ? static_cast<int>(source.parent - raw->nodes) : -1;
            node.matrixAuthored = source.has_matrix != 0;
            if (source.has_matrix)
            {
                CopyFinite(source.matrix, node.matrix.data(), 16);
            }
            if (source.has_translation)
            {
                CopyFinite(source.translation, node.bind.translation.data(), 3);
            }
            if (source.has_rotation)
            {
                CopyFinite(source.rotation, node.bind.rotation.data(), 4);
            }
            if (source.has_scale)
            {
                CopyFinite(source.scale, node.bind.scale.data(), 3);
            }
        }
        Hierarchy(asset);
        std::map<const cgltf_skin*, size_t> skinOffsets;
        for (size_t i = 0; i < raw->skins_count; ++i)
        {
            const auto& skin = raw->skins[i];
            skinOffsets[&skin] = asset.palette.size();
            Require(skin.joints_count > 0 && asset.palette.size() + skin.joints_count <= MaxPalette,
                    "skin exceeds 48 palette matrices");
            const auto matrices = skin.inverse_bind_matrices
                                      ? Floats(skin.inverse_bind_matrices, cgltf_type_mat4)
                                      : std::vector<float>();
            Require(matrices.empty() || matrices.size() == skin.joints_count * 16,
                    "inverse bind count mismatch");
            for (size_t j = 0; j < skin.joints_count; ++j)
            {
                Joint joint;
                joint.node = static_cast<size_t>(skin.joints[j] - raw->nodes);
                if (!matrices.empty())
                {
                    std::copy_n(matrices.data() + j * 16, 16, joint.inverseBind.data());
                }
                asset.palette.push_back(joint);
            }
        }
        // Respect the selected scene: inactive mesh nodes are not accidentally drawn.
        std::vector<bool> active(raw->nodes_count, false);
        std::function<void(const cgltf_node*)> activate = [&](const cgltf_node* node)
        {
            active[static_cast<size_t>(node - raw->nodes)] = true;
            for (size_t i = 0; i < node->children_count; ++i)
            {
                activate(node->children[i]);
            }
        };
        const cgltf_scene* scene = raw->scene          ? raw->scene
                                   : raw->scenes_count ? &raw->scenes[0]
                                                       : nullptr;
        Require(scene != nullptr, "GLB needs a scene");
        for (size_t i = 0; i < scene->nodes_count; ++i)
        {
            activate(scene->nodes[i]);
        }
        for (size_t n = 0; n < raw->nodes_count; ++n)
        {
            const auto& node = raw->nodes[n];
            if (!active[n] || !node.mesh)
            {
                continue;
            }
            size_t rigidPalette = 0;
            if (!node.skin)
            {
                Require(asset.palette.size() < MaxPalette, "rigid mesh exceeds palette limit");
                rigidPalette = asset.palette.size();
                Joint joint;
                joint.node = n;
                asset.palette.push_back(joint);
            }
            for (size_t p = 0; p < node.mesh->primitives_count; ++p)
            {
                const auto& primitive = node.mesh->primitives[p];
                Require(primitive.type == cgltf_primitive_type_triangles &&
                            !primitive.targets_count && !primitive.has_draco_mesh_compression,
                        "only uncompressed triangle meshes without morphs are supported");
                const cgltf_accessor *positions = nullptr, *normals = nullptr, *joints = nullptr,
                                     *weights = nullptr, *colors = nullptr;
                for (size_t a = 0; a < primitive.attributes_count; ++a)
                {
                    const auto& attribute = primitive.attributes[a];
                    Require(!((attribute.type == cgltf_attribute_type_joints ||
                               attribute.type == cgltf_attribute_type_weights) &&
                              attribute.index != 0),
                            "more than four skin influences are unsupported");
                    if (attribute.type == cgltf_attribute_type_position)
                    {
                        positions = attribute.data;
                    }
                    else if (attribute.type == cgltf_attribute_type_normal)
                    {
                        normals = attribute.data;
                    }
                    else if (attribute.type == cgltf_attribute_type_joints)
                    {
                        joints = attribute.data;
                    }
                    else if (attribute.type == cgltf_attribute_type_weights)
                    {
                        weights = attribute.data;
                    }
                    else if (attribute.type == cgltf_attribute_type_color && attribute.index == 0)
                    {
                        colors = attribute.data;
                    }
                }
                const auto positionData = Floats(positions, cgltf_type_vec3);
                const auto normalData = Floats(normals, cgltf_type_vec3);
                Require(normals->count == positions->count, "normal count mismatch");
                const auto jointData =
                    node.skin ? Floats(joints, cgltf_type_vec4) : std::vector<float>();
                const auto weightData =
                    node.skin ? Floats(weights, cgltf_type_vec4) : std::vector<float>();
                Require(!node.skin || (joints->count == positions->count &&
                                       weights->count == positions->count),
                        "skin attribute count mismatch");
                const auto colorData = colors ? Floats(colors, colors->type) : std::vector<float>();
                Require(!colors ||
                            ((colors->type == cgltf_type_vec3 || colors->type == cgltf_type_vec4) &&
                             colors->count == positions->count),
                        "invalid vertex colors");
                Vertex material;
                if (primitive.material)
                {
                    const auto& mat = *primitive.material;
                    Require(mat.alpha_mode == cgltf_alpha_mode_opaque &&
                                !mat.pbr_metallic_roughness.base_color_texture.texture &&
                                !mat.normal_texture.texture && !mat.emissive_texture.texture &&
                                !mat.occlusion_texture.texture &&
                                !mat.pbr_metallic_roughness.metallic_roughness_texture.texture &&
                                !mat.has_pbr_specular_glossiness && !mat.has_transmission,
                            "loader supports opaque palette materials without textures only");
                    CopyFinite(mat.pbr_metallic_roughness.base_color_factor, material.color, 4);
                    CopyFinite(mat.emissive_factor, material.emission, 3);
                    const float strength =
                        mat.has_emissive_strength ? mat.emissive_strength.emissive_strength : 1;
                    Require(std::isfinite(strength) && strength >= 0, "invalid emissive strength");
                    for (float& value : material.emission)
                    {
                        value *= strength;
                    }
                }
                Require(asset.vertices.size() + positions->count <= 2000000,
                        "model vertex budget exceeded");
                const auto base = static_cast<std::uint32_t>(asset.vertices.size());
                for (size_t v = 0; v < positions->count; ++v)
                {
                    Vertex vertex = material;
                    std::copy_n(positionData.data() + v * 3, 3, vertex.position);
                    std::copy_n(normalData.data() + v * 3, 3, vertex.normal);
                    if (colors)
                    {
                        const size_t components = cgltf_num_components(colors->type);
                        for (size_t i = 0; i < components; ++i)
                        {
                            vertex.color[i] *= colorData[v * components + i];
                        }
                    }
                    if (node.skin)
                    {
                        float total = 0;
                        for (size_t i = 0; i < 4; ++i)
                        {
                            const float index = jointData[v * 4 + i],
                                        weight = weightData[v * 4 + i];
                            Require(index >= 0 && std::floor(index) == index &&
                                        index < node.skin->joints_count && weight >= 0,
                                    "joint index or weight out of range");
                            vertex.joints[i] = static_cast<std::uint32_t>(
                                skinOffsets.at(node.skin) + static_cast<size_t>(index));
                            vertex.weights[i] = weight;
                            total += weight;
                        }
                        Require(std::isfinite(total) && total > 1e-6f, "invalid total skin weight");
                        for (float& weight : vertex.weights)
                        {
                            weight /= total;
                        }
                    }
                    else
                    {
                        vertex.joints[0] = static_cast<std::uint32_t>(rigidPalette);
                    }
                    asset.vertices.push_back(vertex);
                }
                const size_t count =
                    primitive.indices ? primitive.indices->count : positions->count;
                Require(count % 3 == 0 && asset.indices.size() + count <= 6000000,
                        "invalid triangle index count");
                Require(!primitive.indices || (!primitive.indices->is_sparse &&
                                               primitive.indices->type == cgltf_type_scalar),
                        "sparse or nonscalar indices unsupported");
                for (size_t i = 0; i < count; ++i)
                {
                    const size_t index =
                        primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
                    Require(index < positions->count, "triangle index out of range");
                    asset.indices.push_back(base + static_cast<std::uint32_t>(index));
                }
            }
        }
        for (size_t a = 0; a < raw->animations_count; ++a)
        {
            const auto& source = raw->animations[a];
            Clip clip;
            Require(source.name && *source.name, "animation needs a stable name");
            clip.name = source.name;
            Require(!asset.FindClip(clip.name), "duplicate animation name");
            for (size_t c = 0; c < source.channels_count; ++c)
            {
                const auto& input = source.channels[c];
                Require(input.target_node && input.sampler && !input.target_node->has_matrix,
                        "animation target must use TRS");
                Channel channel;
                channel.node = static_cast<size_t>(input.target_node - raw->nodes);
                Require(input.target_path == cgltf_animation_path_type_translation ||
                            input.target_path == cgltf_animation_path_type_rotation ||
                            input.target_path == cgltf_animation_path_type_scale,
                        "morph animation unsupported");
                channel.path =
                    input.target_path == cgltf_animation_path_type_rotation ? Path::Rotation
                    : input.target_path == cgltf_animation_path_type_scale  ? Path::Scale
                                                                            : Path::Translation;
                const auto& sampler = *input.sampler;
                Require(sampler.interpolation == cgltf_interpolation_type_step ||
                            sampler.interpolation == cgltf_interpolation_type_linear ||
                            sampler.interpolation == cgltf_interpolation_type_cubic_spline,
                        "unsupported key interpolation");
                channel.interpolation =
                    sampler.interpolation == cgltf_interpolation_type_step ? Interpolation::Step
                    : sampler.interpolation == cgltf_interpolation_type_cubic_spline
                        ? Interpolation::Cubic
                        : Interpolation::Linear;
                channel.times = Floats(sampler.input, cgltf_type_scalar);
                Require(channel.times.front() >= 0, "negative animation time");
                for (size_t i = 1; i < channel.times.size(); ++i)
                {
                    Require(channel.times[i] > channel.times[i - 1],
                            "key times must strictly increase");
                }
                const size_t components = channel.path == Path::Rotation ? 4 : 3;
                const auto values =
                    Floats(sampler.output, components == 4 ? cgltf_type_vec4 : cgltf_type_vec3);
                const size_t count =
                    channel.times.size() * (channel.interpolation == Interpolation::Cubic ? 3 : 1);
                Require(values.size() == count * components, "animation key count mismatch");
                channel.values.resize(count);
                for (size_t i = 0; i < count; ++i)
                {
                    std::copy_n(values.data() + i * components,
                                components,
                                channel.values[i].data());
                }
                clip.duration = std::max(clip.duration, channel.times.back());
                clip.channels.push_back(std::move(channel));
            }
            asset.clips.push_back(std::move(clip));
        }
        Require(!asset.indices.empty(), "model has no visible triangles");
        MeasureBounds(asset);
        output = std::move(asset);
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

// OBJ palette data uses Blender Z-up/+Y-forward; convert once to glTF Y-up/-Z-forward.
bool LoadObj(const std::wstring& file, Asset& output, std::string& error)
{
    try
    {
        const auto bytes = ReadBinary(file);
        std::istringstream lines(std::string(bytes.begin(), bytes.end()));
        std::vector<std::array<float, 3>> positions, normals;
        std::map<std::string, Vertex> materials;
        Vertex material;
        Asset asset;
        asset.nodes.emplace_back();
        asset.hierarchy.push_back(0);
        asset.palette.emplace_back();
        auto vector = [](std::istringstream& input)
        {
            std::array<float, 3> source;
            Require(bool(input >> source[0] >> source[1] >> source[2]), "invalid OBJ vector");
            for (float value : source)
            {
                Require(std::isfinite(value), "non-finite OBJ value");
            }
            return std::array<float, 3>{{source[0], source[2], -source[1]}};
        };
        auto index = [](const std::string& token, size_t count)
        {
            size_t consumed = 0;
            const long value = std::stol(token, &consumed);
            Require(consumed == token.size() && value != 0, "invalid OBJ index");
            const long resolved = value < 0 ? static_cast<long>(count) + value : value - 1;
            Require(resolved >= 0 && static_cast<size_t>(resolved) < count,
                    "OBJ index out of range");
            return static_cast<size_t>(resolved);
        };
        std::string line;
        while (std::getline(lines, line))
        {
            line = line.substr(0, line.find('#'));
            std::istringstream input(line);
            std::string command;
            input >> command;
            if (command == "v")
            {
                Require(positions.size() < 2000000, "OBJ position budget exceeded");
                positions.push_back(vector(input));
            }
            else if (command == "vn")
            {
                Require(normals.size() < 2000000, "OBJ normal budget exceeded");
                normals.push_back(vector(input));
            }
            else if (command == "mtllib")
            {
                std::string path;
                input >> path;
                Require(!path.empty() && path.find(':') == std::string::npos && path[0] != '/' &&
                            path[0] != '\\',
                        "invalid MTL path");
                const std::wstring materialFile = file.substr(0, file.find_last_of(L"/\\") + 1) +
                                                  std::wstring(path.begin(), path.end());
                const auto materialBytes = ReadBinary(materialFile);
                std::istringstream materialLines(
                    std::string(materialBytes.begin(), materialBytes.end()));
                std::string name;
                while (std::getline(materialLines, line))
                {
                    std::istringstream fields(line);
                    fields >> command;
                    if (command == "newmtl")
                    {
                        fields >> name;
                        materials[name] = Vertex();
                    }
                    else if (command == "Kd" || command == "Ke")
                    {
                        Require(!name.empty(), "MTL color before material");
                        float* target =
                            command == "Kd" ? materials[name].color : materials[name].emission;
                        Require(bool(fields >> target[0] >> target[1] >> target[2]),
                                "invalid MTL color");
                        for (size_t i = 0; i < 3; ++i)
                        {
                            Require(std::isfinite(target[i]) && target[i] >= 0,
                                    "invalid MTL color value");
                        }
                    }
                    else if (command == "d")
                    {
                        float alpha = 0;
                        Require(bool(fields >> alpha) && alpha == 1,
                                "transparent OBJ materials unsupported");
                    }
                    else if (command.compare(0, 4, "map_") == 0)
                    {
                        throw std::runtime_error("textured OBJ materials unsupported");
                    }
                }
            }
            else if (command == "usemtl")
            {
                std::string name;
                input >> name;
                Require(materials.count(name) != 0, "unknown OBJ material");
                material = materials.at(name);
            }
            else if (command == "f")
            {
                std::vector<Vertex> face;
                std::string token;
                while (input >> token)
                {
                    const size_t first = token.find('/'), last = token.find_last_of('/');
                    Require(first != std::string::npos && last != first && last + 1 < token.size(),
                            "OBJ faces require explicit normals");
                    const auto& position =
                        positions.at(index(token.substr(0, first), positions.size()));
                    const auto& normal = normals.at(index(token.substr(last + 1), normals.size()));
                    Vertex vertex = material;
                    std::copy_n(position.data(), 3, vertex.position);
                    std::copy_n(normal.data(), 3, vertex.normal);
                    face.push_back(vertex);
                    Require(face.size() <= 3, "OBJ delivery must be triangulated");
                }
                Require(face.size() == 3 && asset.vertices.size() + 3 <= 2000000,
                        "invalid OBJ triangle or vertex budget");
                for (const auto& vertex : face)
                {
                    asset.indices.push_back(static_cast<std::uint32_t>(asset.vertices.size()));
                    asset.vertices.push_back(vertex);
                }
            }
        }
        Require(!asset.indices.empty(), "OBJ has no triangles");
        MeasureBounds(asset);
        output = std::move(asset);
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}
}
