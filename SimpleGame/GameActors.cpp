#include "stdafx.h"
#include "GameActors.h"
#include "CityScene.h"

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
    }
}

void PlayerActor::Draw(Renderer& r) const
{
    m_scene.DrawActor(r, WorldPosition(), -1, m_scene.m_walk, false);
}

BuildingActor::BuildingActor(const std::string& id, CityScene& scene, const Building& data)
    : Actor(id),
      Building(data),
      m_scene(scene)
{
    renderEnabled = true;
    depthOffset = w + d;
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
    m_scene.DrawBuilding(r, data);
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
    const Point offset = Parent() ? Parent()->WorldPosition() : Point();
    data.home = {home.x + offset.x, home.y + offset.y};
    m_scene.UpdateCitizen(data, dt, phase);
    SetWorldPosition(data.position);
}

void CitizenActor::Draw(Renderer& r) const
{
    m_scene.DrawActor(r, WorldPosition(), role, m_scene.m_time * 2 + phase, remembersHelp);
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
        m_scene.UpdateEnemy(*this, dt);
    }
}

void EnemyActor::Draw(Renderer& r) const
{
    m_scene.DrawEnemy(r, *this);
}

TerrainActor::TerrainActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Terrain;
}

void TerrainActor::Draw(Renderer& r) const
{
    m_scene.DrawTerrain(r, WorldPosition());
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
    layer = RenderLayer::Air;
    SetLocalPosition({11.7f, 11.3f});
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
