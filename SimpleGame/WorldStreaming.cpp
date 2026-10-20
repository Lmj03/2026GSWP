#include "stdafx.h"
#include "CityScene.h"
#include "WorldLayout.h"
#include <algorithm>
#include <cmath>
#include <limits>

CityScene::ChunkContent CityScene::GenerateChunk(const ChunkKey& key) const
{
    constexpr float MinimumBuildingGap = 2.f;
    constexpr float BuildingInset = 1.1f;
    ChunkContent result;
    const int townIndex = WorldTownIndex(key.first, key.second);
    if (townIndex < 0)
    {
        return result;
    }
    const auto& town = WorldTowns()[static_cast<size_t>(townIndex)];
    const auto seed = WorldHash(key.first, key.second, m_populationSeed);
    const int region = WorldRegion(key.first, key.second);
    const std::string prefix =
        "generated/" + std::to_string(key.first) + "/" + std::to_string(key.second);
    const bool authored =
        key.first >= -256 && key.first <= 256 && key.second >= -256 && key.second <= 256 &&
        m_authoredChunks.count({static_cast<int>(key.first), static_cast<int>(key.second)});
    const int buildingLimit =
        region == 2 ? 1 : 1 + static_cast<int>(WorldHash(key.first, key.second, 17) % 2);
    for (int lot = 0;
         !authored && lot < 36 && static_cast<int>(result.buildings.size()) < buildingLimit;
         ++lot)
    {
        // Buildings depend on coordinates alone; population varies between new games.
        const auto value = WorldHash(key.first, key.second, lot * 137ULL + 71);
        Building b{};
        b.id = prefix + "/building/" + std::to_string(result.buildings.size());
        const bool useCompactFootprint = lot % 3 == 0;
        b.w = useCompactFootprint ? 1.5f + float((value >> 8) % 16) * .01f
                                  : 1.8f + float((value >> 8) % 65) * .01f;
        b.d = useCompactFootprint ? 1.5f + float((value >> 13) % 16) * .01f
                                  : 1.7f + float((value >> 13) % 65) * .01f;
        b.x = BuildingInset + ((value >> 19) % 2) * (ChunkSize - BuildingInset * 2 - b.w);
        b.y = BuildingInset + ((value >> 27) % 2) * (ChunkSize - BuildingInset * 2 - b.d);
        b.height = (region == 4 ? 100.f : region == 3 ? 45.f : 55.f) + float((value >> 35) % 55);
        b.palette = region == 1 ? 1 : region == 3 ? 2 : int((value >> 43) % 3);
        b.sign = m_text.Get("world.sign." + std::to_string(region));
        bool clear = true;
        for (int x = int(std::floor(b.x - .2f)); x <= int(std::floor(b.x + b.w + .2f)); ++x)
        {
            for (int y = int(std::floor(b.y - .2f)); y <= int(std::floor(b.y + b.d + .2f)); ++y)
            {
                if (WorldRoad(key.first, key.second, x, y))
                {
                    clear = false;
                }
            }
        }
        for (const Building& other : result.buildings)
        {
            if (b.x < other.x + other.w + MinimumBuildingGap &&
                b.x + b.w + MinimumBuildingGap > other.x &&
                b.y < other.y + other.d + MinimumBuildingGap &&
                b.y + b.d + MinimumBuildingGap > other.y)
            {
                clear = false;
            }
        }
        if (clear)
        {
            result.buildings.push_back(b);
        }
    }
    auto open = [&](Point point)
    {
        for (const Building& b : result.buildings)
        {
            if (point.x > b.x - .4f && point.x < b.x + b.w + .4f && point.y > b.y - .4f &&
                point.y < b.y + b.d + .4f)
            {
                return false;
            }
        }
        if (authored)
        {
            const Point native{static_cast<float>(key.first * ChunkSize) + point.x,
                               static_cast<float>(key.second * ChunkSize) + point.y};
            for (const Building& b : m_authoredBuildings)
            {
                if (native.x > b.x - .4f && native.x < b.x + b.w + .4f && native.y > b.y - .4f &&
                    native.y < b.y + b.d + .4f)
                {
                    return false;
                }
            }
            if (std::hypot(native.x - m_spawn.x, native.y - m_spawn.y) < 2)
            {
                return false;
            }
            for (const auto& node : m_storyNodes)
            {
                if (std::hypot(native.x - node.second.x, native.y - node.second.y) < 1.3f)
                {
                    return false;
                }
            }
            for (const Citizen& npc : m_authoredCitizens)
            {
                if (std::hypot(native.x - npc.home.x, native.y - npc.home.y) < 1)
                {
                    return false;
                }
            }
        }
        return WorldRoad(key.first,
                         key.second,
                         static_cast<int>(point.x),
                         static_cast<int>(point.y));
    };
    const int mobLimit = town.security >= 80   ? 0
                         : town.security >= 60 ? ((seed >> 8) % 4 == 0 ? 1 : 0)
                         : town.security >= 40 ? static_cast<int>((seed >> 8) % 2)
                                               : static_cast<int>((seed >> 8) % 3);
    for (int attempt = 0; attempt < 48 && static_cast<int>(result.enemies.size()) < mobLimit;
         ++attempt)
    {
        const auto value =
            WorldHash(key.first, key.second, m_populationSeed + attempt * 89ULL + 593);
        const Point point{.7f + float(value % 660) * .01f, .7f + float((value >> 16) % 660) * .01f};
        bool clear = open(point);
        for (const Enemy& other : result.enemies)
        {
            if (std::hypot(point.x - other.home.x, point.y - other.home.y) < 2)
            {
                clear = false;
            }
        }
        if (clear)
        {
            Enemy enemy;
            enemy.id = prefix + "/mob/" + std::to_string(result.enemies.size());
            enemy.home = enemy.position = point;
            enemy.chunkX = key.first;
            enemy.chunkY = key.second;
            enemy.streamed = true;
            enemy.health = enemy.maxHealth = region == 3 ? 36 : 30;
            enemy.experience = region == 3 ? 3 : 2;
            result.enemies.push_back(enemy);
        }
    }
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
        citizen.strike.x -= offsetX;
        citizen.strike.y -= offsetY;
    }
    for (Enemy& enemy : m_enemies)
    {
        if (enemy.streamed)
        {
            enemy.position.x -= offsetX;
            enemy.position.y -= offsetY;
            enemy.home.x -= offsetX;
            enemy.home.y -= offsetY;
            enemy.strike.x -= offsetX;
            enemy.strike.y -= offsetY;
        }
    }
    RefreshWorld();
}

void CityScene::RefreshWorld()
{
    m_nextStreamActors.clear();
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
    for (int x = -StreamRadius; x <= StreamRadius; ++x)
    {
        for (int y = -StreamRadius; y <= StreamRadius; ++y)
        {
            m_chunks.insert({x, y});
            const std::string terrainId =
                "terrain/" + std::to_string(m_originX + x) + "/" + std::to_string(m_originY + y);
            Actor* terrain = m_sceneGraph.Find(terrainId);
            if (!terrain)
            {
                terrain = &m_sceneGraph.Create<TerrainActor>(terrainId, "streamed", *this);
            }
            terrain->SetLocalPosition(
                {static_cast<float>(x * ChunkSize), static_cast<float>(y * ChunkSize)});
            m_nextStreamActors.insert(terrainId);
            const ChunkKey key(m_originX + x, m_originY + y);
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
                AddBuilding(std::move(building));
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
        AddBuilding(std::move(building));
    }
    for (const std::string& id : m_streamActors)
    {
        if (!m_nextStreamActors.count(id))
        {
            m_sceneGraph.Destroy(id);
        }
    }
    m_streamActors = std::move(m_nextStreamActors);
    m_townActorsDirty = true;
    RefreshEnemies();
    SyncSceneGraph();
    m_sceneGraph.FlushDestroyed();
}
