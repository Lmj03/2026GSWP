#include "stdafx.h"
#include "CityScene.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace
{
std::uint64_t Mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
}

CityScene::ChunkContent CityScene::GenerateChunk(const ChunkKey& key) const
{
    ChunkContent result;
    const std::uint64_t seed = Mix(static_cast<std::uint64_t>(key.first)) ^
                               Mix(static_cast<std::uint64_t>(key.second) + 2026ULL);
    const std::string prefix =
        "generated/" + std::to_string(key.first) + "/" + std::to_string(key.second);
    // Occupy two or three of the four lots; retain one or two open spaces.
    // Coordinate-derived masks keep the same courtyards on every revisit.
    static const unsigned int lotMasks[] = {0x3, 0x5, 0x9, 0x6, 0xA, 0xC, 0x7, 0xB, 0xD, 0xE};
    const unsigned int occupiedLots = lotMasks[Mix(seed + 71ULL) % 10];
    for (int lot = 0; lot < 4; ++lot)
    {
        if ((occupiedLots & (1u << lot)) == 0)
        {
            continue;
        }
        const std::uint64_t value = Mix(seed + lot * 117ULL);
        Building building{};
        building.id = prefix + "/building/" + std::to_string(lot);
        building.x = lot % 2 == 0 ? .22f : 5.18f;
        building.y = lot / 2 == 0 ? .22f : 5.18f;
        building.w = 2.25f + static_cast<float>((value >> 8) % 4) * .1f;
        building.d = 2.25f + static_cast<float>((value >> 12) % 4) * .1f;
        building.height = 65.f + static_cast<float>((value >> 16) % 115);
        building.palette = static_cast<int>((value >> 24) % 3);
        building.sign = m_text.Get("generated.sign." + std::to_string((value >> 32) % 8));
        result.buildings.push_back(building);
    }
    Citizen citizen{};
    citizen.id = prefix + "/citizen";
    citizen.role = static_cast<int>((seed >> 8) % 3);
    citizen.name = m_text.Get("generated.name." + std::to_string(citizen.role));
    citizen.home = {3.5f, 4.4f};
    citizen.position = citizen.home;
    result.citizens.push_back(citizen);
    return result;
}

void CityScene::Recenter()
{
    const int dx = static_cast<int>(std::floor(m_player.x / ChunkSize));
    const int dy = static_cast<int>(std::floor(m_player.y / ChunkSize));
    if (!dx && !dy)
    {
        return;
    }
    // Keep arithmetic defined even at the representational limits of int64.
    const std::int64_t limit = std::numeric_limits<std::int64_t>::max() - 4096;
    if (m_originX > limit - dx || m_originX < -limit - dx || m_originY > limit - dy ||
        m_originY < -limit - dy)
    {
        return;
    }
    m_originX += dx;
    m_originY += dy;
    const float offsetX = static_cast<float>(dx * ChunkSize);
    const float offsetY = static_cast<float>(dy * ChunkSize);
    m_player.x -= offsetX;
    m_player.y -= offsetY;
    m_camera.x -= offsetX;
    m_camera.y -= offsetY;
    for (Citizen& citizen : m_citizens)
    {
        citizen.position.x -= offsetX;
        citizen.position.y -= offsetY;
        citizen.home.x -= offsetX;
        citizen.home.y -= offsetY;
    }
    RefreshWorld();
}

void CityScene::RefreshWorld()
{
    // Keep active actors' positions; never retain every visited chunk.
    std::unordered_map<std::string, Point> positions;
    for (const Citizen& citizen : m_citizens)
    {
        positions.emplace(citizen.id, citizen.position);
    }
    for (auto it = m_generated.begin(); it != m_generated.end();)
    {
        if (it->first.first < m_originX - StreamRadius ||
            it->first.first > m_originX + StreamRadius ||
            it->first.second < m_originY - StreamRadius ||
            it->first.second > m_originY + StreamRadius)
        {
            it = m_generated.erase(it);
        }
        else
        {
            ++it;
        }
    }
    m_chunks.clear();
    m_buildings.clear();
    m_citizens.clear();
    auto addCitizen = [&](Citizen citizen)
    {
        const auto old = positions.find(citizen.id);
        if (old != positions.end())
        {
            citizen.position = old->second;
        }
        citizen.remembersHelp = m_remembered.count(citizen.id) != 0;
        m_citizens.push_back(std::move(citizen));
    };
    for (int x = -StreamRadius; x <= StreamRadius; ++x)
    {
        for (int y = -StreamRadius; y <= StreamRadius; ++y)
        {
            m_chunks.insert({x, y});
            const ChunkKey key(m_originX + x, m_originY + y);
            const bool authored =
                key.first >= -256 && key.first <= 256 && key.second >= -256 && key.second <= 256 &&
                m_authoredChunks.count({static_cast<int>(key.first), static_cast<int>(key.second)});
            if (authored)
            {
                continue;
            }
            auto found = m_generated.find(key);
            if (found == m_generated.end())
            {
                found = m_generated.emplace(key, GenerateChunk(key)).first;
            }
            const float offsetX = static_cast<float>(x * ChunkSize);
            const float offsetY = static_cast<float>(y * ChunkSize);
            for (Building building : found->second.buildings)
            {
                building.x += offsetX;
                building.y += offsetY;
                m_buildings.push_back(std::move(building));
            }
            for (Citizen citizen : found->second.citizens)
            {
                citizen.home.x += offsetX;
                citizen.home.y += offsetY;
                citizen.position = citizen.home;
                addCitizen(std::move(citizen));
            }
        }
    }
    // Authored landmarks override generation and may span multiple chunks.
    for (Building building : m_authoredBuildings)
    {
        const auto cx = static_cast<std::int64_t>(std::floor(building.x / ChunkSize));
        const auto cy = static_cast<std::int64_t>(std::floor(building.y / ChunkSize));
        if (cx < m_originX - StreamRadius - 1 || cx > m_originX + StreamRadius ||
            cy < m_originY - StreamRadius - 1 || cy > m_originY + StreamRadius)
        {
            continue;
        }
        building.x = static_cast<float>((cx - m_originX) * ChunkSize) +
                     (building.x - static_cast<float>(cx * ChunkSize));
        building.y = static_cast<float>((cy - m_originY) * ChunkSize) +
                     (building.y - static_cast<float>(cy * ChunkSize));
        m_buildings.push_back(std::move(building));
    }
    for (Citizen citizen : m_authoredCitizens)
    {
        const auto cx = static_cast<std::int64_t>(std::floor(citizen.home.x / ChunkSize));
        const auto cy = static_cast<std::int64_t>(std::floor(citizen.home.y / ChunkSize));
        if (cx < m_originX - StreamRadius || cx > m_originX + StreamRadius ||
            cy < m_originY - StreamRadius || cy > m_originY + StreamRadius)
        {
            continue;
        }
        citizen.home.x = static_cast<float>((cx - m_originX) * ChunkSize) +
                         (citizen.home.x - static_cast<float>(cx * ChunkSize));
        citizen.home.y = static_cast<float>((cy - m_originY) * ChunkSize) +
                         (citizen.home.y - static_cast<float>(cy * ChunkSize));
        citizen.position = citizen.home;
        addCitizen(std::move(citizen));
    }
}
