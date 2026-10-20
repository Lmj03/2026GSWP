#include "stdafx.h"
#include "CityScene.h"
#include "FrameProfiler.h"
#include "WorldLayout.h"
#include <algorithm>
#include <cmath>

bool CityScene::EnemyLocal(const Enemy& enemy, Point point, Point& local) const
{
    if (!enemy.streamed)
    {
        return LevelLocal(point, local);
    }
    local = point;
    return HasGround(local.x, local.y);
}

void CityScene::RememberEnemy(const Enemy& enemy)
{
    if (!enemy.streamed)
    {
        return;
    }
    if (enemy.health == enemy.maxHealth && !enemy.defeatedOnce && !enemy.loot &&
        enemy.generation == 0)
    {
        m_enemyMemory.erase(enemy.id);
        return;
    }
    Enemy saved = enemy;
    const float x = static_cast<float>((enemy.chunkX - m_originX) * ChunkSize);
    const float y = static_cast<float>((enemy.chunkY - m_originY) * ChunkSize);
    saved.position = {enemy.position.x - x, enemy.position.y - y};
    saved.home = {enemy.home.x - x, enemy.home.y - y};
    saved.strike = {enemy.strike.x - x, enemy.strike.y - y};
    m_enemyMemory[enemy.id] = saved;
}

void CityScene::UnloadStreamEnemies(bool remember)
{
    ActorView<EnemyActor> remaining;
    for (EnemyActor& enemy : m_enemies)
    {
        if (!enemy.streamed)
        {
            remaining.Add(enemy);
            continue;
        }
        if (remember)
        {
            RememberEnemy(enemy);
        }
        enemy.Destroy();
    }
    m_enemies = std::move(remaining);
    m_sceneGraph.FlushDestroyed();
}

void CityScene::RefreshEnemies()
{
    ActorView<EnemyActor> remaining;
    int removed = 0;
    for (EnemyActor& enemy : m_enemies)
    {
        if (enemy.streamed && !m_generated.count({enemy.chunkX, enemy.chunkY}))
        {
            RememberEnemy(enemy);
            enemy.Destroy();
            ++removed;
        }
        else
        {
            remaining.Add(enemy);
        }
    }
    m_enemies = std::move(remaining);
    m_populationTimer = 0;
    if (removed)
    {
        FrameProfiler::Instance().Event("population unload_mobs=" + std::to_string(removed) +
                                        " remembered=" + std::to_string(m_enemyMemory.size()));
    }
}

void CityScene::SpawnEnemies(bool initial)
{
    constexpr size_t MaximumLoadedMobs = 48;
    int spawned = 0;
    for (const auto& chunk : m_generated)
    {
        for (const Enemy& prototype : chunk.second.enemies)
        {
            if (m_enemies.size() >= MaximumLoadedMobs || (!initial && spawned >= 6))
            {
                break;
            }
            if (m_sceneGraph.Find("enemy/" + prototype.id))
            {
                continue;
            }
            Enemy enemy = prototype;
            const auto saved = m_enemyMemory.find(enemy.id);
            if (saved != m_enemyMemory.end())
            {
                enemy = saved->second;
            }
            const float x = static_cast<float>((enemy.chunkX - m_originX) * ChunkSize);
            const float y = static_cast<float>((enemy.chunkY - m_originY) * ChunkSize);
            enemy.home = {enemy.home.x + x, enemy.home.y + y};
            enemy.position = {enemy.position.x + x, enemy.position.y + y};
            enemy.strike = {enemy.strike.x + x, enemy.strike.y + y};
            const float distance =
                std::hypot(enemy.position.x - m_player.x, enemy.position.y - m_player.y);
            if (distance > 28 || !CanStand(enemy.position.x, enemy.position.y, .25f))
            {
                continue;
            }
            if (enemy.health > 0)
            {
                if (distance < 4.5f)
                {
                    continue;
                }
                Point base;
                if (LevelLocal(m_spawn, base) &&
                    std::hypot(enemy.position.x - base.x, enemy.position.y - base.y) < 4.5f)
                {
                    continue;
                }
                bool clear = true;
                for (const auto& node : m_storyNodes)
                {
                    Point local;
                    if (LevelLocal(node.second, local) &&
                        std::hypot(enemy.position.x - local.x, enemy.position.y - local.y) < 1.8f)
                    {
                        clear = false;
                    }
                }
                for (const EnemyActor& other : m_enemies)
                {
                    Point local;
                    if (EnemyLocal(other, other.position, local) &&
                        std::hypot(enemy.position.x - local.x, enemy.position.y - local.y) < 1.5f)
                    {
                        clear = false;
                    }
                }
                for (const Citizen& npc : m_citizens)
                {
                    if (npc.active && std::hypot(enemy.position.x - npc.position.x,
                                                 enemy.position.y - npc.position.y) < .7f)
                    {
                        clear = false;
                    }
                }
                const Point screen = Project(enemy.position.x, enemy.position.y);
                if (!initial && screen.x > -60 && screen.x < Renderer::Width + 60 &&
                    screen.y > -60 && screen.y < Renderer::Height + 90)
                {
                    clear = false;
                }
                if (!clear)
                {
                    continue;
                }
            }
            AddEnemy(enemy);
            ++spawned;
        }
    }
    if (spawned)
    {
        FrameProfiler::Instance().Event("population spawn_mobs=" + std::to_string(spawned) +
                                        " loaded_mobs=" + std::to_string(m_enemies.size()) +
                                        " loaded_npcs=" + std::to_string(m_citizens.size()));
    }
}

void CityScene::UpdatePopulation(float dt)
{
    m_populationTimer -= dt;
    if (m_populationTimer <= 0)
    {
        m_populationTimer = 2;
        // Run after SceneGraph::Update: pending IDs have been released before reuse.
        SpawnEnemies();
    }
}

bool CityScene::RespawnEnemy(Enemy& enemy)
{
    if (!enemy.streamed)
    {
        return false;
    }
    const float x = static_cast<float>((enemy.chunkX - m_originX) * ChunkSize);
    const float y = static_cast<float>((enemy.chunkY - m_originY) * ChunkSize);
    for (int attempt = 0; attempt < 24; ++attempt)
    {
        const auto hash =
            WorldHash(enemy.chunkX,
                      enemy.chunkY,
                      m_populationSeed + enemy.generation * 997ULL + attempt * 43ULL + 811);
        Point point{x + .75f + static_cast<float>(hash % 650) * .01f,
                    y + .75f + static_cast<float>((hash >> 16) % 650) * .01f};
        if (!CanStand(point.x, point.y, .25f) ||
            std::hypot(point.x - m_player.x, point.y - m_player.y) < 5)
        {
            continue;
        }
        Point base;
        if (LevelLocal(m_spawn, base) && std::hypot(point.x - base.x, point.y - base.y) < 4.5f)
        {
            continue;
        }
        bool clear = true;
        for (const auto& node : m_storyNodes)
        {
            Point local;
            if (LevelLocal(node.second, local) &&
                std::hypot(point.x - local.x, point.y - local.y) < 1.8f)
            {
                clear = false;
            }
        }
        for (const EnemyActor& other : m_enemies)
        {
            Point local;
            if (&other != &enemy && EnemyLocal(other, other.position, local) &&
                std::hypot(point.x - local.x, point.y - local.y) < 1.5f)
            {
                clear = false;
            }
        }
        for (const Citizen& npc : m_citizens)
        {
            if (npc.active && std::hypot(point.x - npc.position.x, point.y - npc.position.y) < .7f)
            {
                clear = false;
            }
        }
        const Point screen = Project(point.x, point.y);
        if (!clear || (screen.x > -60 && screen.x < Renderer::Width + 60 && screen.y > -60 &&
                       screen.y < Renderer::Height + 90))
        {
            continue;
        }
        enemy.home = enemy.position = point;
        enemy.health = enemy.maxHealth;
        enemy.cooldown = 1;
        enemy.windup = 0;
        ++enemy.generation;
        FrameProfiler::Instance().Event("population respawn=" + enemy.id +
                                        " generation=" + std::to_string(enemy.generation));
        return true;
    }
    return false;
}
