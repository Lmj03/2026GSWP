#include "stdafx.h"
#include "CityScene.h"
#include "Resources.h"
#include "WorldLayout.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace
{

float Distance(Point a, Point b)
{
    const float x = a.x - b.x, y = a.y - b.y;
    return std::sqrt(x * x + y * y);
}

void LabelSpaces(std::string& text)
{
    std::replace(text.begin(), text.end(), '_', ' ');
}
}

bool CityScene::Load()
{
    if (!m_text.Load(L"Data/Locale/ko.txt"))
    {
        std::cerr << "Data/Locale/ko.txt 한글 문구 파일을 불러올 수 없습니다.\n";
        return false;
    }
    std::string source;
    if (!LoadWorldLayout(source))
    {
        std::cerr << source << '\n';
        return false;
    }
    if (!ReadResource(L"Data/afterglow.district", source))
    {
        std::cerr << "Data/afterglow.district 지역 파일을 불러올 수 없습니다.\n";
        return false;
    }
    m_chunks.clear();
    m_buildings.clear();
    m_citizens.clear();
    std::set<std::string> ids;
    std::istringstream lines(source);
    std::string line;
    bool version = false, spawn = false;
    int number = 0;
    while (std::getline(lines, line))
    {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream input(line);
        std::string type, trailing;
        if (!(input >> type))
        {
            continue;
        }
        bool valid = true;
        if (type == "version")
        {
            int value = 0;
            valid = bool(input >> value) && value == 1 && !version;
            version = true;
        }
        else if (type == "chunk")
        {
            int x = 0, y = 0;
            valid = bool(input >> x >> y) && x >= -256 && x <= 256 && y >= -256 && y <= 256;
            if (valid)
            {
                valid = m_chunks.insert({x, y}).second;
            }
        }
        else if (type == "spawn")
        {
            valid = bool(input >> m_spawn.x >> m_spawn.y) && !spawn && std::isfinite(m_spawn.x) &&
                    std::isfinite(m_spawn.y);
            spawn = true;
        }
        else if (type == "building")
        {
            Building b{};
            valid =
                bool(input >> b.id >> b.x >> b.y >> b.w >> b.d >> b.height >> b.palette >> b.sign);
            valid = valid && std::isfinite(b.x) && std::isfinite(b.y) && b.x >= -2048 &&
                    b.y >= -2048 && b.x + b.w <= 2056 && b.y + b.d <= 2056 && b.w > 0 && b.w <= 8 &&
                    b.d > 0 && b.d <= 8 && b.height >= 20 && b.height <= 400 && b.palette >= 0 &&
                    b.palette <= 2 && b.sign.size() <= 80 && ids.insert(b.id).second;
            if (valid)
            {
                LabelSpaces(b.sign);
                AddBuilding(b);
            }
        }
        else if (type == "npc")
        {
            Citizen npc{};
            valid =
                bool(input >> npc.id >> npc.role >> npc.position.x >> npc.position.y >> npc.name);
            valid = valid && npc.role >= 0 && npc.role <= 2 && std::isfinite(npc.position.x) &&
                    std::isfinite(npc.position.y) && npc.name.size() <= 80 &&
                    ids.insert(npc.id).second;
            if (valid)
            {
                npc.occupation = npc.role == 2   ? Occupation::Raider
                                 : npc.role == 1 ? Occupation::Merchant
                                                 : Occupation::Resident;
                if (npc.id == "mechanic")
                {
                    npc.occupation = Occupation::Mechanic;
                }
                npc.home = npc.position;
                LabelSpaces(npc.name);
                AddCitizen(npc);
            }
        }
        else if (type == "dialogue")
        {
            std::string id, neutral, remembered;
            valid = bool(input >> id >> neutral >> remembered);
            auto citizen = std::find_if(m_citizens.begin(),
                                        m_citizens.end(),
                                        [&](const Citizen& value)
                                        {
                                            return value.id == id;
                                        });
            valid = valid && citizen != m_citizens.end() && m_text.Contains(neutral) &&
                    m_text.Contains(remembered);
            if (valid)
            {
                citizen->dialogueNeutral = neutral;
                citizen->dialogueRemembered = remembered;
            }
        }
        else
        {
            valid = false;
        }
        if (!valid || (input >> trailing))
        {
            std::cerr << "지역 데이터 형식 오류 " << number << "행: " << line << '\n';
            return false;
        }
    }
    if (!version || !spawn || m_chunks.empty() || !CanStand(m_spawn.x, m_spawn.y))
    {
        std::cerr << "지역에는 version 1, 청크, 장애물과 겹치지 않는 시작점이 필요합니다.\n";
        return false;
    }
    for (const Building& b : m_buildings)
    {
        // Check every overlapped tile, including footprints spanning several chunks.
        for (int x = static_cast<int>(std::floor(b.x)); x < b.x + b.w; ++x)
        {
            for (int y = static_cast<int>(std::floor(b.y)); y < b.y + b.d; ++y)
            {
                if (!HasGround(x + .5f, y + .5f))
                {
                    std::cerr << "등록된 청크 밖에 있는 건물: " << b.id << '\n';
                    return false;
                }
            }
        }
    }
    for (const Citizen& npc : m_citizens)
    {
        if (!CanStand(npc.position.x, npc.position.y))
        {
            std::cerr << "NPC가 장애물 또는 청크 경계와 겹칩니다: " << npc.id << '\n';
            return false;
        }
    }
    m_authoredChunks = m_chunks;
    m_authoredBuildings = m_buildings.CopyData<Building>();
    m_authoredCitizens = m_citizens.CopyData<Citizen>();
    m_streamActors = m_nextStreamActors;
    if (!LoadLevelOne())
    {
        return false;
    }
    if (!LoadStory())
    {
        std::cerr << "Data/Story/act1.story 오류\n";
        return false;
    }
    Reset();
    return true;
}

void CityScene::Reset()
{
    ++m_monitorReset;
    UnloadStreamEnemies(false);
    UnloadTownActors();
    m_enemyMemory.clear();
    m_populationSeed =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    m_originX = m_originY = 0;
    m_generated.clear();
    m_remembered.clear();
    m_citizens.clear();
    m_player = m_camera = m_spawn;
    m_helped = false;
    m_map = false;
    m_reputation = 0;
    m_time = m_walk = 0;
    ResetProgression();
    ResetStory();
    InitializeTownPopulation();
    ShowNotice(m_text.Get("notice.start"));
    RefreshWorld();
    Recenter();
    RefreshTownActors();
    SpawnEnemies(true);
}

Point CityScene::Project(float x, float y, float height) const
{
    return {Renderer::Width * .5f + (x - y - m_camera.x + m_camera.y) * 32 * m_zoom,
            Renderer::Height * .5f + 30 + (x + y - m_camera.x - m_camera.y) * 16 * m_zoom -
                height * m_zoom};
}

bool CityScene::HasGround(float x, float y) const
{
    // Small local coordinates; the distant world origin is stored as int64.
    if (!std::isfinite(x) || !std::isfinite(y) || x < -4096 || x > 4096 || y < -4096 || y > 4096)
    {
        return false;
    }
    const int cx = static_cast<int>(std::floor(x / ChunkSize));
    const int cy = static_cast<int>(std::floor(y / ChunkSize));
    return m_chunks.count({cx, cy}) != 0;
}

bool CityScene::CanStand(float x, float y, float radius) const
{
    if (!HasGround(x - radius, y - radius) || !HasGround(x + radius, y - radius) ||
        !HasGround(x - radius, y + radius) || !HasGround(x + radius, y + radius))
    {
        return false;
    }
    for (const BuildingActor& b : m_buildings)
    {
        if (!b.EnabledInHierarchy())
        {
            continue;
        }
        const Point p = b.WorldPosition();
        if (x > p.x - radius && x < p.x + b.w + radius && y > p.y - radius &&
            y < p.y + b.d + radius)
        {
            return false;
        }
    }
    return true;
}

void CityScene::Move(Point& position, float x, float y, float radius)
{
    // Small substeps keep collision stable through timer stalls.
    const int steps =
        std::max(1, static_cast<int>(std::ceil(std::max(std::abs(x), std::abs(y)) / .08f)));
    for (int i = 0; i < steps; ++i)
    {
        if (CanStand(position.x + x / steps, position.y, radius))
        {
            position.x += x / steps;
        }
        if (CanStand(position.x, position.y + y / steps, radius))
        {
            position.y += y / steps;
        }
    }
}

void CityScene::Zoom(int direction)
{
    m_zoom = std::max(.7f, std::min(1.65f, m_zoom + direction * .1f));
}

void CityScene::Update(float dt, const bool* keys)
{
    m_time += dt;
    m_input = keys;
    m_frameLoot = Materials{};
    m_frameLootBundles = 0;
    if (!m_baseOpen && m_dialoguePages.size() <= 1)
    {
        m_noticeTime = std::max(0.f, m_noticeTime - dt);
    }
    SyncSceneGraph();
    m_sceneGraph.Update(dt);
    if (CanSimulate() && m_health > 0)
    {
        UpdateTownPopulation(dt);
        UpdatePopulation(dt);
    }
    m_input = nullptr;
}

int CityScene::NearbyCitizen() const
{
    int result = -1;
    float nearest = 1.65f;
    for (size_t i = 0; i < m_citizens.size(); ++i)
    {
        if (!m_citizens[i].EnabledInHierarchy() || !m_citizens[i].active)
        {
            continue;
        }
        const float d = Distance(m_player, m_citizens[i].WorldPosition());
        if (d < nearest)
        {
            nearest = d;
            result = static_cast<int>(i);
        }
    }
    return result;
}

void CityScene::ShowNotice(const std::string& text)
{
    m_dialoguePages.clear();
    m_dialoguePage = 0;
    size_t start = 0, position = 0, characters = 0;
    // Bound the paragraph without splitting UTF-8 characters.
    while (position < text.size())
    {
        const unsigned char first = static_cast<unsigned char>(text[position]);
        const size_t bytes = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        position = std::min(text.size(), position + bytes);
        if (++characters == 72)
        {
            m_dialoguePages.push_back(text.substr(start, position - start));
            start = position;
            characters = 0;
        }
    }
    if (start < text.size())
    {
        m_dialoguePages.push_back(text.substr(start));
    }
    if (m_dialoguePages.empty())
    {
        m_dialoguePages.push_back("");
    }
    m_notice = m_dialoguePages.front();
    m_noticeTime = 8;
}

void CityScene::NotifyGraphicsReload(bool success)
{
    ShowNotice(m_text.Get(success ? "notice.post_reloaded" : "notice.post_reload_failed"));
}

void CityScene::Interact()
{
    if (m_baseOpen)
    {
        CloseBase();
        return;
    }
    if (m_health <= 0 || m_map || m_story.sceneActive || m_journalOpen)
    {
        return;
    }
    if (m_dialoguePages.size() > 1)
    {
        if (m_dialoguePage + 1 < m_dialoguePages.size())
        {
            m_notice = m_dialoguePages[++m_dialoguePage];
        }
        else
        {
            m_dialoguePages.clear();
            m_noticeTime = 0;
        }
        return;
    }
    if (InteractStory())
    {
        return;
    }
    if (InteractLevelOne())
    {
        return;
    }
    const int index = NearbyCitizen();
    if (index < 0)
    {
        ShowNotice(m_text.Get("notice.closer"));
        return;
    }
    Citizen& npc = m_citizens[index];
    if (npc.story)
    {
        ShowNotice(m_text.Get("story.after." + npc.id.substr(6)));
        return;
    }
    OpenNpcDialogue(npc);
}

void CityScene::Draw(Renderer& r)
{
    r.Begin(Color(.018f, .032f, .055f));
    m_sceneGraph.Draw(r);
    r.End();
}

bool CityScene::LevelLocal(Point world, Point& local) const
{
    // The encounter keeps small authored coordinates, even after very long trips.
    // Check the origin before multiplication/conversion; never accumulate recenter offsets.
    if (m_originX < -8 || m_originX > 8 || m_originY < -8 || m_originY > 8)
    {
        return false;
    }
    local = {world.x - static_cast<float>(m_originX * ChunkSize),
             world.y - static_cast<float>(m_originY * ChunkSize)};
    return HasGround(local.x, local.y);
}

bool CityScene::ClearReach(Point from, Point to) const
{
    const int steps = std::max(1, static_cast<int>(std::ceil(Distance(from, to) / .12f)));
    for (int step = 0; step <= steps; ++step)
    {
        const float t = static_cast<float>(step) / steps;
        if (!CanStand(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, .08f))
        {
            return false;
        }
    }
    return true;
}
