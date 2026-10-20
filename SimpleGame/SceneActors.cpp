#include "stdafx.h"
#include "CityScene.h"
#include <algorithm>

CityScene::CityScene()
    : m_playerActor(m_sceneGraph.Create<PlayerActor>("player", "", *this)),
      m_player(m_playerActor.Position())
{
    m_sceneGraph.Create<Actor>("world", "");
    m_sceneGraph.Create<Actor>("streamed", "world");
    m_sceneGraph.Create<Actor>("authored", "world");
    m_sceneGraph.Create<Actor>("screen", "");
    m_sceneGraph.Reparent("player", "world");
    m_sceneGraph.Create<AttackActor>("player/attack", "player", *this);
    m_sceneGraph.Create<BaseActor>("base", "authored", *this, false);
    m_sceneGraph.Create<BaseActor>("base/ground", "base", *this, true);
    m_sceneGraph.Create<SupplyActor>("supply", "authored", *this);
    m_sceneGraph.Create<DroneActor>("drone", "authored", *this);
    m_sceneGraph.Create<WeatherActor>("weather", "screen", *this);
    m_sceneGraph.Create<InterfaceActor>("hud", "screen", *this);
    m_sceneGraph.Create<BaseMenuActor>("base/menu", "screen", *this);
}

bool CityScene::CanSimulate() const
{
    return m_input && !m_baseOpen && !m_map && !m_dialogue.open && !m_journalOpen &&
           !m_story.sceneActive && m_dialoguePages.size() <= 1;
}

void CityScene::AddBuilding(Building building)
{
    const std::string id = "building/" + building.id;
    auto* actor = dynamic_cast<BuildingActor*>(m_sceneGraph.Find(id));
    if (!actor)
    {
        actor = &m_sceneGraph.Create<BuildingActor>(id, "streamed", *this, building);
        m_sceneGraph.Create<ReflectionActor>(id + "/reflection", id, *this, *actor);
    }
    else
    {
        static_cast<Building&>(*actor) = std::move(building);
        actor->depthOffset = actor->w + actor->d;
    }
    m_buildings.Add(*actor);
    m_nextStreamActors.insert(id);
}

void CityScene::AddCitizen(Citizen citizen)
{
    const auto memory = m_npcMemory.find(citizen.id);
    if (memory != m_npcMemory.end() && !m_townResidents.count(citizen.id))
    {
        citizen.memory = memory->second;
    }
    const std::string id = "citizen/" + citizen.id;
    auto* actor = dynamic_cast<CitizenActor*>(m_sceneGraph.Find(id));
    if (actor)
    {
        citizen.actionTimer = actor->actionTimer;
        citizen.cooldown = actor->cooldown;
        citizen.windup = actor->windup;
        citizen.strike = actor->strike;
        citizen.flash = actor->flash;
        citizen.pickupDelay = actor->pickupDelay;
        citizen.activity = actor->activity;
        citizen.hostile = actor->hostile;
        citizen.active = actor->active;
    }
    if (!actor)
    {
        actor = &m_sceneGraph.Create<CitizenActor>(id, "streamed", *this, citizen);
    }
    else
    {
        static_cast<Citizen&>(*actor) = std::move(citizen);
    }
    actor->phase = static_cast<float>(m_citizens.size());
    m_citizens.Add(*actor);
}

void CityScene::SetEnemies(const std::vector<Enemy>& enemies)
{
    for (EnemyActor& enemy : m_enemies)
    {
        enemy.Destroy();
    }
    m_enemies.clear();
    m_sceneGraph.FlushDestroyed();
    for (const Enemy& data : enemies)
    {
        AddEnemy(data);
    }
}

void CityScene::AddEnemy(const Enemy& data)
{
    const std::string id = "enemy/" + data.id;
    EnemyActor& actor =
        m_sceneGraph.Create<EnemyActor>(id, data.streamed ? "streamed" : "authored", *this, data);
    m_enemies.Add(actor);
    m_sceneGraph.Create<LootActor>(id + "/loot", id, *this, actor);
    m_sceneGraph.Create<WarningActor>(id + "/warning", id, *this, actor);
}

void CityScene::SyncSceneGraph()
{
    Actor& root = *m_sceneGraph.Find("authored");
    root.enabled = m_originX >= -8 && m_originX <= 8 && m_originY >= -8 && m_originY <= 8;
    if (root.enabled)
    {
        root.SetLocalPosition({-static_cast<float>(m_originX * ChunkSize),
                               -static_cast<float>(m_originY * ChunkSize)});
    }
}
