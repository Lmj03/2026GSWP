#include "stdafx.h"
#include "GameActors.h"
#include "CityScene.h"
#include <cmath>

namespace
{
bool ModelOnScreen(Point foot)
{
    return foot.x >= -100 && foot.x <= Renderer::Width + 100 && foot.y >= -100 &&
           foot.y <= Renderer::Height + 160;
}

void FaceMovement(ModelVisual& visual, Point before, Point after)
{
    const float dx = after.x - before.x, dy = after.y - before.y;
    const float distance = std::hypot(dx, dy);
    if (distance > .0001f && distance < 4)
    {
        visual.yaw = std::atan2(-dx, dy);
    }
}
}

PlayerActor::PlayerActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = tickEnabled = true;
    updateOrder = 10;
    drawOrder = 40;
}

Point& PlayerActor::Position()
{
    return m_position;
}

void PlayerActor::Update(float dt)
{
    if (m_scene.CanSimulate())
    {
        m_scene.UpdatePlayer(dt);
        m_visual.Bind(m_scene.m_models, "PLAYER_Vigilante");
        const bool attacking = m_scene.m_attackAnimation > 0;
        const bool running = m_scene.m_input && m_scene.m_input[16];
        const char* clip = m_scene.m_health <= 0 ? "defeat"
                           : attacking           ? "attack_melee"
                           : m_scene.m_walk > 0  ? (running ? "run" : "walk")
                                                 : "idle";
        const Point facing =
            attacking ? m_scene.m_attackVisualDirection : m_scene.m_attackDirection;
        m_visual.yaw = std::atan2(-facing.x, facing.y);
        float speed = 1;
        if (attacking && m_visual.Asset())
        {
            const auto* action = m_visual.Asset()->FindClip(clip);
            if (action)
            {
                speed = action->duration / (m_scene.m_swingWeapon == Weapon::Knife ? .32f : .22f);
            }
        }
        m_visual.Update(clip, dt, speed, attacking && !m_modelAttacking);
        m_modelAttacking = attacking;
    }
}

void PlayerActor::Draw(Renderer& r) const
{
    const Point position = WorldPosition();
    const Point foot = m_scene.Project(position.x, position.y);
    if (ModelOnScreen(foot) && m_visual.Draw(r, foot, 18 * m_scene.m_zoom))
    {
        return;
    }
    m_scene.DrawPlayer(r, position);
}

BuildingActor::BuildingActor(const std::string& id, CityScene& scene, const Building& data)
    : Actor(id),
      Building(data),
      m_scene(scene)
{
    renderEnabled = true;
    depthOffset = w + d;
    RefreshModel();
}

void BuildingActor::RefreshModel()
{
    m_visual.Bind(m_scene.m_models, m_scene.m_models.BuildingId(Building::id, palette));
}

Point BuildingActor::LocalPosition() const
{
    return {x, y};
}

void BuildingActor::SetLocalPosition(Point p)
{
    x = p.x;
    y = p.y;
}

void BuildingActor::Draw(Renderer& r) const
{
    Building data = *this;
    const Point p = WorldPosition();
    data.x = p.x;
    data.y = p.y;
    m_scene.DrawBuilding(r, data, &m_visual);
}

CitizenActor::CitizenActor(const std::string& id, CityScene& scene, const Citizen& data)
    : Actor(id),
      Citizen(data),
      m_scene(scene)
{
    renderEnabled = tickEnabled = true;
    updateOrder = 50;
    drawOrder = 10;
}

Point CitizenActor::LocalPosition() const
{
    return position;
}

void CitizenActor::SetLocalPosition(Point p)
{
    position = p;
}

void CitizenActor::Update(float dt)
{
    if (!m_scene.CanSimulate() || m_scene.m_health <= 0)
    {
        return;
    }
    Citizen data = *this;
    data.position = WorldPosition();
    const Point before = data.position;
    const Point offset = Parent() ? Parent()->WorldPosition() : Point();
    data.home = {home.x + offset.x, home.y + offset.y};
    m_scene.UpdateCitizen(data, dt, phase);
    static const char* assets[] = {"NPC_resident",
                                   "NPC_merchant",
                                   "NPC_raider",
                                   "NPC_worker",
                                   "NPC_medic",
                                   "NPC_mechanic",
                                   "NPC_guard"};
    const size_t occupationIndex = static_cast<size_t>(data.occupation);
    m_visual.Bind(m_scene.m_models, assets[occupationIndex < 7 ? occupationIndex : 0]);
    FaceMovement(m_visual, before, data.position);
    const bool attacking = data.windup > 0;
    const bool moving = std::hypot(data.position.x - before.x, data.position.y - before.y) > .0001f;
    m_visual.Update(data.memory.health <= 0 ? "defeat"
                    : attacking             ? "attack_melee"
                    : moving                ? "walk"
                                            : "idle",
                    dt,
                    1,
                    attacking && !m_modelAttacking);
    m_modelAttacking = attacking;
    data.position = {data.position.x - offset.x, data.position.y - offset.y};
    data.home = {data.home.x - offset.x, data.home.y - offset.y};
    static_cast<Citizen&>(*this) = data;
}

void CitizenActor::Draw(Renderer& r) const
{
    Citizen data = *this;
    data.position = WorldPosition();
    const Point foot = m_scene.Project(data.position.x, data.position.y);
    const bool drawn = data.active && data.memory.health > 0 && ModelOnScreen(foot) &&
                       m_visual.Draw(r, foot, 18 * m_scene.m_zoom);
    m_scene.DrawCitizen(r, data, phase, drawn);
}

EnemyActor::EnemyActor(const std::string& id, CityScene& scene, const Enemy& data)
    : Actor(id),
      Enemy(data),
      m_scene(scene)
{
    renderEnabled = tickEnabled = true;
    updateOrder = 20;
    drawOrder = 20;
}

Point EnemyActor::LocalPosition() const
{
    return position;
}

void EnemyActor::SetLocalPosition(Point p)
{
    position = p;
}

void EnemyActor::Update(float dt)
{
    if (m_scene.CanSimulate() && m_scene.m_health > 0)
    {
        const Point before = position;
        m_scene.UpdateEnemy(*this, dt);
        if (tutorial)
        {
            m_visual.Bind(m_scene.m_models, "ENEMY_street_gang");
        }
        FaceMovement(m_visual, before, position);
        const bool attacking = windup > 0;
        const bool moving = std::hypot(position.x - before.x, position.y - before.y) > .0001f;
        m_visual.Update(health <= 0 ? "defeat"
                        : attacking ? "attack_melee"
                        : moving    ? "walk"
                                    : "idle",
                        dt,
                        1,
                        attacking && !m_modelAttacking);
        m_modelAttacking = attacking;
    }
}

void EnemyActor::Draw(Renderer& r) const
{
    Point local;
    bool drawn = false;
    if (health > 0 && m_scene.EnemyLocal(*this, position, local))
    {
        const Point foot = m_scene.Project(local.x, local.y);
        drawn = ModelOnScreen(foot) && m_visual.Draw(r, foot, 18 * m_scene.m_zoom);
    }
    m_scene.DrawEnemy(r, *this, drawn);
}

TerrainActor::TerrainActor(const std::string& id,
                           CityScene& scene,
                           std::int64_t cx,
                           std::int64_t cy)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Terrain;
    m_surface.SetAsset(m_scene.TerrainModel(cx, cy));
}

void TerrainActor::Draw(Renderer& r) const
{
    m_scene.DrawTerrain(r, WorldPosition(), m_surface);
}

ReflectionActor::ReflectionActor(const std::string& id, CityScene& scene, BuildingActor& building)
    : Actor(id),
      m_scene(scene),
      m_building(building)
{
    renderEnabled = true;
    layer = RenderLayer::Ground;
}

void ReflectionActor::Draw(Renderer& r) const
{
    Building data = m_building;
    const Point p = WorldPosition();
    data.x = p.x;
    data.y = p.y;
    m_scene.DrawReflection(r, data);
}

LootActor::LootActor(const std::string& id, CityScene& scene, EnemyActor& enemy)
    : Actor(id),
      m_scene(scene),
      m_enemy(enemy)
{
    renderEnabled = tickEnabled = true;
    layer = RenderLayer::Ground;
    updateOrder = 40;
}

void LootActor::Update(float dt)
{
    if (m_scene.CanSimulate() && m_scene.m_health > 0)
    {
        m_scene.CollectLoot(m_enemy, dt);
    }
}

void LootActor::Draw(Renderer& r) const
{
    m_scene.DrawLoot(r, m_enemy);
}

WarningActor::WarningActor(const std::string& id, CityScene& scene, EnemyActor& enemy)
    : Actor(id),
      m_scene(scene),
      m_enemy(enemy)
{
    renderEnabled = true;
    layer = RenderLayer::Ground;
}

void WarningActor::Draw(Renderer& r) const
{
    m_scene.DrawWarning(r, m_enemy);
}

SupplyActor::SupplyActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Ground;
}

Point SupplyActor::LocalPosition() const
{
    return m_scene.m_supply;
}

void SupplyActor::SetLocalPosition(Point p)
{
    m_scene.m_supply = p;
}

void SupplyActor::Draw(Renderer& r) const
{
    m_scene.DrawSupply(r);
}

BaseActor::BaseActor(const std::string& id, CityScene& scene, bool ground)
    : Actor(id),
      m_scene(scene),
      m_ground(ground)
{
    renderEnabled = true;
    tickEnabled = !ground;
    layer = ground ? RenderLayer::Ground : RenderLayer::World;
    drawOrder = 30;
}

Point BaseActor::LocalPosition() const
{
    return m_ground ? Point() : m_scene.m_spawn;
}

void BaseActor::SetLocalPosition(Point p)
{
    if (!m_ground)
    {
        m_scene.m_spawn = p;
    }
}

void BaseActor::Update(float dt)
{
    if (m_scene.m_baseOpen)
    {
        m_scene.UpdateBase(dt);
    }
}

void BaseActor::Draw(Renderer& r) const
{
    if (m_ground)
    {
        m_scene.DrawBaseGround(r);
    }
    else
    {
        m_scene.DrawBaseProps(r);
    }
}

AttackActor::AttackActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    drawOrder = 41;
}

void AttackActor::Draw(Renderer& r) const
{
    m_scene.DrawAttack(r);
}

WeatherActor::WeatherActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Weather;
}

void WeatherActor::Draw(Renderer& r) const
{
    m_scene.DrawWeather(r);
}

DroneActor::DroneActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    tickEnabled = true;
    layer = RenderLayer::Air;
    SetLocalPosition({11.7f, 11.3f});
}

void DroneActor::Update(float dt)
{
    if (m_scene.CanSimulate())
    {
        m_scene.m_droneModel.Update("hover", dt);
    }
}

void DroneActor::Draw(Renderer& r) const
{
    m_scene.DrawDrone(r, WorldPosition());
}

InterfaceActor::InterfaceActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Interface;
}

void InterfaceActor::Draw(Renderer& r) const
{
    if (m_scene.m_interface)
    {
        m_scene.DrawInterface(r);
    }
}

BaseMenuActor::BaseMenuActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Modal;
}

void BaseMenuActor::Draw(Renderer& r) const
{
    if (m_scene.m_baseOpen)
    {
        m_scene.DrawBaseInterface(r);
    }
}
