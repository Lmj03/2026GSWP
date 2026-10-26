#pragma once
#include "ModelAsset.h"
#include <map>
#include <memory>
#include <set>

// One library per scene; CPU assets are loaded lazily and shared by all instances.
class ModelLibrary
{
public:
    // Reinitialization affects future imports; existing visuals keep their shared assets.
    bool Initialize(const std::wstring& root = {});
    std::shared_ptr<const Models::Asset> Get(const std::string& id);
    std::string PartId(const std::string& slot, int index) const;
    const std::wstring& Root() const;
    bool Enabled() const;

private:
    struct Entry
    {
        std::wstring file;
        bool glb = false;
        std::string metadata;
    };

    bool m_enabled = false;
    std::wstring m_root;
    std::map<std::string, Entry> m_entries;
    std::map<std::string, std::shared_ptr<const Models::Asset>> m_cache;
    std::map<std::string, std::map<int, std::string>> m_parts;
    std::set<std::string> m_failed;
};
