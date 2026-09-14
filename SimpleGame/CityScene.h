#pragma once
#include "Renderer.h"
#include "Localization.h"
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

class CityScene
{
public:
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
    struct Materials
    {
        std::uint64_t scrap = 0;
        std::uint64_t alloy = 0;
        std::uint64_t fabric = 0;
    };

    enum class Weapon
    {
        Fists,
        Knife
    };

    struct Recipe
    {
        Materials materials;
        std::uint64_t credits = 0;
    };

    struct Enemy
    {
        std::string id;
        Point home;
        Point position;
        Point strike;
        int health = 30;
        int maxHealth = 30;
        int experience = 2;
        float cooldown = 0;
        float windup = 0;
        float flash = 0;
        float respawn = 0;
        bool loot = false;
        float pickupDelay = 0;
        Materials drop;
        bool defeatedOnce = false;
        bool lootedOnce = false;
    };

    std::vector<Enemy> m_enemies;
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
    void CollectNearbyLoot(float dt);
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
    void ResetLevelOne();
    void UpdateLevelOne(float dt);
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
    void DrawLevelGround(Renderer& renderer) const;
    void DrawEnemy(Renderer& renderer, const Enemy& enemy) const;
    void DrawAttack(Renderer& renderer) const;
    void DrawLevelHud(Renderer& renderer) const;
    void DrawLevelMap(Renderer& renderer, float left, float top, float size, float radius) const;

    struct Building
    {
        std::string id, sign;
        float x, y, w, d, height;
        int palette;
    };

    struct Citizen
    {
        std::string id, name;
        Point position, home;
        int role; // 0: resident, 1: merchant, 2: lookout
        bool remembersHelp = false;
        std::string dialogueNeutral, dialogueRemembered;
    };

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
    std::vector<Building> m_buildings;
    std::vector<Citizen> m_citizens;
    Point m_spawn, m_player, m_camera;
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
    void DrawGround(Renderer& renderer) const;
    void DrawBuilding(Renderer& renderer, const Building& building) const;
    void DrawActor(Renderer& renderer, Point position, int role, float phase, bool friendly) const;
    void DrawWeather(Renderer& renderer) const;
    void DrawInterface(Renderer& renderer) const;
    void DrawMap(Renderer& renderer, bool expanded) const;
};
