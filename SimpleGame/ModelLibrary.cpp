#include "stdafx.h"
#include "ModelLibrary.h"
#include "Dependencies/json.hpp"
#include "FrameProfiler.h"
#include "Resources.h"
#include "RuntimeFiles.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using Json = nlohmann::json;

std::wstring Wide(const std::string& text)
{
    const int count = MultiByteToWideChar(CP_UTF8,
                                          MB_ERR_INVALID_CHARS,
                                          text.data(),
                                          static_cast<int>(text.size()),
                                          nullptr,
                                          0);
    if (!count)
    {
        throw std::runtime_error("invalid UTF-8 asset path");
    }
    std::wstring value(count, L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        &value[0],
                        count);
    return value;
}

Json ReadJson(const std::wstring& file)
{
    std::ifstream input(file.c_str(), std::ios::binary | std::ios::ate);
    if (!input || input.tellg() > 16 * 1024 * 1024)
    {
        throw std::runtime_error("missing or oversized asset catalog");
    }
    input.seekg(0);
    return Json::parse(input);
}

std::wstring Relative(const std::string& text)
{
    if (text.empty() || text[0] == '/' || text.find('\\') != std::string::npos ||
        text.find(':') != std::string::npos)
    {
        throw std::runtime_error("catalog path must be relative");
    }
    std::istringstream parts(text);
    std::string part;
    while (std::getline(parts, part, '/'))
    {
        if (part.empty() || part == "." || part == "..")
        {
            throw std::runtime_error("catalog path contains traversal");
        }
    }
    return Wide(text);
}

bool Exists(const std::wstring& path)
{
    const DWORD flags = GetFileAttributesW(path.c_str());
    return flags != INVALID_FILE_ATTRIBUTES && !(flags & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring FindRoot()
{
    wchar_t environment[32768] = {};
    const DWORD count = GetEnvironmentVariableW(L"NEON_ART_ROOT", environment, 32768);
    if (count > 0 && count < 32768)
    {
        return environment;
    }
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        return {};
    }
    std::wstring folder(executable, length);
    folder = folder.substr(0, folder.find_last_of(L"/\\"));
    for (int level = 0; level < 5; ++level)
    {
        for (const auto* suffix : {L"/Art", L"/Data/Art", L"/.modeling-dev/Art"})
        {
            const std::wstring root = folder + suffix;
            if (Exists(root + L"/Characters/manifest.json") ||
                Exists(root + L"/LowPoly/catalog.json"))
            {
                return root;
            }
        }
        const size_t slash = folder.find_last_of(L"/\\");
        if (slash == std::wstring::npos)
        {
            break;
        }
        folder.resize(slash);
    }
    return {};
}

void Failure(const std::string& id, const std::string& error)
{
    FrameProfiler::Instance().Event("model_error id=" + id + " reason=" + error);
    std::cerr << "[Model] " << id << ": " << error << '\n';
}
}

bool ModelLibrary::Initialize(const std::wstring& root)
{
    m_enabled = false;
    m_entries.clear();
    m_cache.clear();
    m_parts.clear();
    m_failed.clear();
    m_buildingVariants.clear();
    std::string config;
    if (ReadResource(L"Data/Rendering/models.cfg", config))
    {
        std::istringstream lines(config);
        std::string line;
        while (std::getline(lines, line))
        {
            std::istringstream fields(line.substr(0, line.find('#')));
            std::string key;
            int value = 1;
            if (fields >> key >> value && key == "enabled" && !value)
            {
                FrameProfiler::Instance().Event("models disabled_by_config=1");
                return false;
            }
        }
    }
    m_root = root.empty() ? FindRoot() : root;
    if (m_root.empty())
    {
        Failure("catalog", "Art folder not found; procedural visuals retained");
        return false;
    }
    try
    {
        if (Exists(m_root + L"/Characters/manifest.json"))
        {
            const auto manifest = ReadJson(m_root + L"/Characters/manifest.json");
            for (const auto& item : manifest.at("assets"))
            {
                const auto id = item.at("id").get<std::string>();
                Entry entry;
                entry.file = m_root + L"/Characters/" + Relative(item.at("glb").get<std::string>());
                if (entry.file.size() < 4 || entry.file.substr(entry.file.size() - 4) != L".glb")
                {
                    throw std::runtime_error("character must reference a GLB");
                }
                entry.glb = true;
                entry.metadata = item.dump();
                if (id.empty() || !m_entries.emplace(id, std::move(entry)).second)
                {
                    throw std::runtime_error("duplicate or empty asset ID");
                }
            }
        }
        if (Exists(m_root + L"/LowPoly/catalog.json"))
        {
            const auto catalog = ReadJson(m_root + L"/LowPoly/catalog.json");
            if (catalog.at("version") != 1 || catalog.at("axis") != "Z_UP_Y_FORWARD")
            {
                throw std::runtime_error("unsupported static catalog contract");
            }
            auto add = [&](const Json& item)
            {
                const auto id = item.at("name").get<std::string>();
                Entry entry;
                entry.file = m_root + L"/LowPoly/" + Relative(item.at("file").get<std::string>());
                if (entry.file.size() < 4 || entry.file.substr(entry.file.size() - 4) != L".obj")
                {
                    throw std::runtime_error("static catalog must reference an OBJ");
                }
                if (id.empty() || !m_entries.emplace(id, std::move(entry)).second)
                {
                    throw std::runtime_error("duplicate static asset ID");
                }
                return id;
            };
            for (auto slot = catalog.at("slots").begin(); slot != catalog.at("slots").end(); ++slot)
            {
                for (const auto& item : slot.value())
                {
                    const int index = item.at("index").get<int>();
                    if (index < 0 || !m_parts[slot.key()].emplace(index, add(item)).second)
                    {
                        throw std::runtime_error("duplicate or negative part index");
                    }
                }
            }
            for (const auto& item : catalog.at("environment"))
            {
                const auto id = add(item);
                if (id.compare(0, 13, "ENV_building_") == 0 && id != "ENV_building_arcadia" &&
                    id != "ENV_building_tower")
                {
                    m_buildingVariants.push_back(id);
                }
            }
            std::sort(m_buildingVariants.begin(), m_buildingVariants.end());
        }
        m_enabled = !m_entries.empty();
        FrameProfiler::Instance().Event("model_catalog root=" + PathUtf8(m_root) +
                                        " entries=" + std::to_string(m_entries.size()));
        return m_enabled;
    }
    catch (const std::exception& error)
    {
        m_entries.clear();
        m_parts.clear();
        m_buildingVariants.clear();
        Failure("catalog", error.what());
        return false;
    }
}

std::shared_ptr<const Models::Asset> ModelLibrary::Get(const std::string& id)
{
    if (!m_enabled || m_failed.count(id))
    {
        return {};
    }
    const auto cached = m_cache.find(id);
    if (cached != m_cache.end())
    {
        return cached->second;
    }
    const auto entry = m_entries.find(id);
    if (entry == m_entries.end())
    {
        m_failed.insert(id);
        Failure(id, "unknown catalog ID");
        return {};
    }
    const auto start = FrameProfiler::Clock::now();
    auto asset = std::make_shared<Models::Asset>();
    std::string error;
    bool loaded = entry->second.glb ? Models::LoadGlb(entry->second.file, *asset, error)
                                    : Models::LoadObj(entry->second.file, *asset, error);
    if (loaded && entry->second.glb)
    {
        try
        {
            const auto metadata = Json::parse(entry->second.metadata);
            const std::wstring sidecar =
                entry->second.file.substr(0, entry->second.file.size() - 4) + L".animations.json";
            const auto description = Exists(sidecar) ? ReadJson(sidecar) : metadata;
            if (description.at("id").get<std::string>() != id)
            {
                throw std::runtime_error("animation metadata asset ID mismatch");
            }
            std::set<std::string> described;
            for (const auto& info : description.at("clips"))
            {
                const auto name = info.at("name").get<std::string>();
                auto clip = std::find_if(asset->clips.begin(),
                                         asset->clips.end(),
                                         [&](const Models::Clip& value)
                                         {
                                             return value.name == name;
                                         });
                if (clip == asset->clips.end() || !described.insert(name).second)
                {
                    throw std::runtime_error("metadata references missing or duplicate animation");
                }
                clip->loop = info.value("loop", false);
                clip->rootMotion = info.value("root_motion", false);
                clip->blendSeconds = info.value("blend_in_seconds", .12f);
                const float duration = info.at("duration").get<float>();
                if (!std::isfinite(duration) || std::abs(duration - clip->duration) > .01f)
                {
                    throw std::runtime_error("animation metadata duration mismatch");
                }
                if (!std::isfinite(clip->blendSeconds) || clip->blendSeconds < 0 ||
                    clip->blendSeconds > 10)
                {
                    throw std::runtime_error("invalid animation blend duration");
                }
                if (info.contains("contact_frames"))
                {
                    const auto& contact = info.at("contact_frames");
                    clip->contactFrames[0] = contact.value("L", std::vector<int>());
                    clip->contactFrames[1] = contact.value("R", std::vector<int>());
                }
            }
            if (described.size() != asset->clips.size())
            {
                throw std::runtime_error("animation metadata must describe every clip");
            }
        }
        catch (const std::exception& exception)
        {
            loaded = false;
            error = exception.what();
        }
    }
    if (!loaded)
    {
        m_failed.insert(id);
        Failure(id, error);
        return {};
    }
    asset->id = id;
    FrameProfiler::Instance().Event(
        "model_import id=" + id + " vertices=" + std::to_string(asset->vertices.size()) +
        " triangles=" + std::to_string(asset->indices.size() / 3) + " palette=" +
        std::to_string(asset->palette.size()) + " clips=" + std::to_string(asset->clips.size()) +
        " cpu_ms=" + std::to_string(FrameProfiler::Milliseconds(start)));
    m_cache[id] = asset;
    return asset;
}

std::string ModelLibrary::PartId(const std::string& slot, int index) const
{
    const auto found = m_parts.find(slot);
    if (found == m_parts.end())
    {
        return {};
    }
    const auto part = found->second.find(index);
    return part == found->second.end() ? std::string() : part->second;
}

std::string ModelLibrary::BuildingId(const std::string& districtId, int palette) const
{
    if (!m_enabled)
    {
        return {};
    }
    const std::string authored = "ENV_building_" + districtId;
    if (m_entries.count(authored))
    {
        return authored;
    }
    if (districtId.compare(0, 10, "generated/") != 0 || m_buildingVariants.empty())
    {
        return {};
    }
    // Fixed FNV-1a; std::hash is not a persistent coordinate/appearance contract.
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : districtId)
    {
        hash = (hash ^ byte) * 1099511628211ULL;
    }
    hash = (hash ^ static_cast<std::uint32_t>(palette)) * 1099511628211ULL;
    return m_buildingVariants[static_cast<size_t>(hash % m_buildingVariants.size())];
}

const std::wstring& ModelLibrary::Root() const
{
    return m_root;
}

bool ModelLibrary::Enabled() const
{
    return m_enabled;
}
