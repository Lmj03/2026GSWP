#pragma once
#include "Renderer.h"
#include "Localization.h"
#include "SceneGraph.h"
#include "GameActors.h"
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

class CityScene
{
public:
    CityScene();
    bool Load();
    void Update(float dt, const bool* keys);
    void Draw(Renderer& renderer);
    void Interact();
    void UseRecovery();
    bool HandleBaseInput(unsigned char key);
    void NotifyGraphicsReload(bool success);
    void Reset();
    void Zoom(int direction);

    void ToggleRain()
    {
        m_rain = !m_rain;
    }

    void ToggleMap()
    {
        m_map = !m_map;
    }

    void ToggleInterface()
    {
        m_interface = !m_interface;
    }

private:
    friend class PlayerActor;
    friend class EnemyActor;
    friend class CitizenActor;
    friend class BuildingActor;
    friend class TerrainActor;
    friend class ReflectionActor;
    friend class LootActor;
    friend class SupplyActor;
    friend class BaseActor;
    friend class AttackActor;
    friend class WeatherActor;
    friend class DroneActor;
    friend class InterfaceActor;
    friend class BaseMenuActor;
    friend class WarningActor;
    SceneGraph m_sceneGraph;
    PlayerActor& m_playerActor;
    const bool* m_input = nullptr;
    Materials m_frameLoot;
    unsigned int m_frameLootBundles = 0;
    std::set<std::string> m_streamActors;
    std::set<std::string> m_nextStreamActors;
    void AddBuilding(Building building);
    void AddCitizen(Citizen citizen);
    void SetEnemies(const std::vector<Enemy>& enemies);
    void SyncSceneGraph();
    void UpdatePlayer(float dt);
    void UpdateCitizen(Citizen& citizen, float dt, float phase);
    void UpdateEnemy(Enemy& enemy, float dt);
    void CollectLoot(Enemy& enemy, float dt);
    void DrawTerrain(Renderer& renderer, Point origin) const;
    void DrawReflection(Renderer& renderer, const Building& building) const;
    void DrawSupply(Renderer& renderer) const;
    void DrawMarker(Renderer& renderer, Point world, Color color, const std::string& key) const;
    void DrawLoot(Renderer& renderer, const Enemy& enemy) const;
    void DrawWarning(Renderer& renderer, const Enemy& enemy) const;
    void DrawDrone(Renderer& renderer, Point position) const;
    bool CanSimulate() const;

    ActorView<EnemyActor> m_enemies;
    Point m_supply;
    Point m_attackDirection = {1, 0};
    Point m_attackVisualDirection = {1, 0};
    int m_level = 1;
    std::uint64_t m_experience = 0;
    std::int64_t m_health = 100;
    std::uint64_t m_credits = 0;
    std::uint64_t m_recovery = 2;
    Materials m_bag;
    Materials m_storage;
    Weapon m_weapon = Weapon::Fists;
    Weapon m_swingWeapon = Weapon::Fists;
    bool m_knifeOwned = false;
    bool m_forgedKnife = false;
    bool m_vestCrafted = false;
    int m_knifeUpgrade = 0;
    bool m_baseOpen = false;
    bool m_storagePage = false;
    float m_restTime = 0;
    std::int64_t m_restHealth = 0;
    std::string m_baseNotice;
    std::string m_pickupNotice;
    float m_pickupNoticeTime = 0;

    void OpenBase();
    void CloseBase();
    void UpdateBase(float dt);
    bool SpendMaterials(Materials cost, std::uint64_t credits);
    Recipe RecipeFor(int action) const;
    void DrawBaseGround(Renderer& renderer) const;
    void DrawBaseProps(Renderer& renderer) const;
    void DrawBaseInterface(Renderer& renderer) const;
    std::string WeaponName() const;
    std::int64_t AttackDamage() const;
    bool m_supplyOpened = false;
    bool m_levelComplete = false;
    float m_attackCooldown = 0;
    float m_attackAnimation = 0;
    float m_invulnerable = 0;
    float m_respawnDelay = 0;
    std::string m_combatNotice;
    float m_combatNoticeTime = 0;

    bool LoadLevelOne();
    void ResetProgression();
    void UpdatePlayerTimers(float dt);
    void Attack();
    bool InteractLevelOne();
    int NearbyLevelInteraction() const;
    void RespawnPlayer();
    void AddExperience(int amount);
    std::uint64_t RequiredExperience() const;
    std::int64_t MaxHealth() const;
    std::int64_t BaseAttackDamage() const;
    bool LevelLocal(Point world, Point& local) const;
    bool ClearReach(Point from, Point to) const;
    Point LevelObjective() const;
    void DrawEnemy(Renderer& renderer, const Enemy& enemy) const;
    void DrawAttack(Renderer& renderer) const;
    void DrawLevelHud(Renderer& renderer) const;
    void DrawLevelMap(Renderer& renderer, float left, float top, float size, float radius) const;

    static constexpr int ChunkSize = 8;
    static constexpr int StreamRadius = 6;
    using ChunkKey = std::pair<std::int64_t, std::int64_t>;

    struct ChunkContent
    {
        std::vector<Building> buildings;
        std::vector<Citizen> citizens;
    };

    std::int64_t m_originX = 0, m_originY = 0;
    std::set<std::pair<int, int>> m_authoredChunks;
    std::vector<Building> m_authoredBuildings;
    std::vector<Citizen> m_authoredCitizens;
    std::map<ChunkKey, ChunkContent> m_generated;
    std::set<std::string> m_remembered;
    Localization m_text;
    std::set<std::pair<int, int>> m_chunks;
    ActorView<BuildingActor> m_buildings;
    ActorView<CitizenActor> m_citizens;
    Point m_spawn, m_camera;
    Point& m_player;
    float m_time = 0, m_zoom = 1.15f, m_walk = 0, m_noticeTime = 0;
    float m_facing = 1;
    int m_reputation = 0;
    bool m_helped = false, m_rain = true, m_map = false, m_interface = true;
    std::string m_notice;
    std::vector<std::string> m_dialoguePages;
    size_t m_dialoguePage = 0;

    void Recenter();
    void RefreshWorld();
    ChunkContent GenerateChunk(const ChunkKey& key) const;
    void ShowNotice(const std::string& text);
    Point Project(float x, float y, float height = 0) const;
    bool HasGround(float x, float y) const;
    bool CanStand(float x, float y, float radius = .18f) const;
    void Move(Point& position, float x, float y, float radius = .18f);
    int NearbyCitizen() const;
    void DrawBuilding(Renderer& renderer, const Building& building) const;
    void DrawActor(Renderer& renderer, Point position, int role, float phase, bool friendly) const;
    void DrawWeather(Renderer& renderer) const;
    void DrawInterface(Renderer& renderer) const;
    void DrawMap(Renderer& renderer, bool expanded) const;
};
