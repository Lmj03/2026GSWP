#pragma once
#include "Actor.h"
#include "GameplayTypes.h"

class CityScene;

// Gameplay actors own their component data; CityScene supplies world/gameplay services.
class PlayerActor final : public Actor
{
public:
    PlayerActor(const std::string& id, CityScene& scene);
    Point& Position();
    void Update(float dt) override;
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class BuildingActor final : public Actor, public Building
{
public:
    BuildingActor(const std::string& id, CityScene& scene, const Building& data);
    Point LocalPosition() const override;
    void SetLocalPosition(Point position) override;
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class CitizenActor final : public Actor, public Citizen
{
public:
    CitizenActor(const std::string& id, CityScene& scene, const Citizen& data);
    Point LocalPosition() const override;
    void SetLocalPosition(Point position) override;
    void Update(float dt) override;
    void Draw(Renderer& renderer) const override;
    float phase = 0;

private:
    CityScene& m_scene;
};

class EnemyActor final : public Actor, public Enemy
{
public:
    EnemyActor(const std::string& id, CityScene& scene, const Enemy& data);
    Point LocalPosition() const override;
    void SetLocalPosition(Point position) override;
    void Update(float dt) override;
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class TerrainActor final : public Actor
{
public:
    TerrainActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class ReflectionActor final : public Actor
{
public:
    ReflectionActor(const std::string& id, CityScene& scene, BuildingActor& building);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
    BuildingActor& m_building;
};

class LootActor final : public Actor
{
public:
    LootActor(const std::string& id, CityScene& scene, EnemyActor& enemy);
    void Update(float dt) override;
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
    EnemyActor& m_enemy;
};

class WarningActor final : public Actor
{
public:
    WarningActor(const std::string& id, CityScene& scene, EnemyActor& enemy);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
    EnemyActor& m_enemy;
};

class SupplyActor final : public Actor
{
public:
    SupplyActor(const std::string& id, CityScene& scene);
    Point LocalPosition() const override;
    void SetLocalPosition(Point position) override;
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class BaseActor final : public Actor
{
public:
    BaseActor(const std::string& id, CityScene& scene, bool ground);
    Point LocalPosition() const override;
    void SetLocalPosition(Point position) override;
    void Update(float dt) override;
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
    bool m_ground;
};

class AttackActor final : public Actor
{
public:
    AttackActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class WeatherActor final : public Actor
{
public:
    WeatherActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class DroneActor final : public Actor
{
public:
    DroneActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class InterfaceActor final : public Actor
{
public:
    InterfaceActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};

class BaseMenuActor final : public Actor
{
public:
    BaseMenuActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};
