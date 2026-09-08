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
    void Reset();
    void Zoom(int direction);
    void ToggleRain() { m_rain = !m_rain; }
    void ToggleMap() { m_map = !m_map; }
    void ToggleInterface() { m_interface = !m_interface; }

private:
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
    using ChunkKey = std::pair<std::int64_t,std::int64_t>;
    struct ChunkContent { std::vector<Building> buildings; std::vector<Citizen> citizens; };
    std::int64_t m_originX = 0, m_originY = 0;
    std::set<std::pair<int,int>> m_authoredChunks;
    std::vector<Building> m_authoredBuildings;
    std::vector<Citizen> m_authoredCitizens;
    std::map<ChunkKey,ChunkContent> m_generated;
    std::set<std::string> m_remembered;
    Localization m_text;
    std::set<std::pair<int,int>> m_chunks;
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
