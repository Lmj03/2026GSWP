#include "stdafx.h"
#include "CityScene.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace
{
const Color Cyan(.18f, .94f, .91f), Pink(1.f, .2f, .54f), Amber(1.f, .66f, .24f);
const Color White(.83f, .94f, .98f), Muted(.39f, .55f, .65f);

Color Accent(int palette)
{
    return palette == 1 ? Pink : palette == 2 ? Amber : Cyan;
}

float Distance(Point a, Point b)
{
    const float x = a.x - b.x, y = a.y - b.y;
    return std::sqrt(x * x + y * y);
}

int Mod(int n, int d)
{
    return (n % d + d) % d;
}

bool Road(int x, int y)
{
    return Mod(x, 8) == 3 || Mod(x, 8) == 4 || Mod(y, 8) == 3 || Mod(y, 8) == 4;
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
                m_buildings.push_back(b);
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
                npc.home = npc.position;
                LabelSpaces(npc.name);
                m_citizens.push_back(npc);
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
    m_authoredBuildings = m_buildings;
    m_authoredCitizens = m_citizens;
    if (!LoadLevelOne())
    {
        return false;
    }
    Reset();
    return true;
}

void CityScene::Reset()
{
    m_originX = m_originY = 0;
    m_generated.clear();
    m_remembered.clear();
    m_citizens.clear();
    m_player = m_camera = m_spawn;
    m_helped = false;
    m_map = false;
    m_reputation = 0;
    m_time = m_walk = 0;
    ResetLevelOne();
    ShowNotice(m_text.Get("notice.start"));
    RefreshWorld();
    Recenter();
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
    for (const Building& b : m_buildings)
    {
        if (x > b.x - radius && x < b.x + b.w + radius && y > b.y - radius &&
            y < b.y + b.d + radius)
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
    if (m_baseOpen)
    {
        UpdateBase(dt);
        return;
    }
    if (m_dialoguePages.size() <= 1)
    {
        m_noticeTime = std::max(0.f, m_noticeTime - dt);
    }
    if (m_map || m_dialoguePages.size() > 1)
    {
        return;
    }
    if (m_health <= 0)
    {
        UpdateLevelOne(dt);
        return;
    }
    float sx = (keys['d'] ? 1.f : 0.f) - (keys['a'] ? 1.f : 0.f);
    float sy = (keys['s'] ? 1.f : 0.f) - (keys['w'] ? 1.f : 0.f);
    const float length = std::sqrt(sx * sx + sy * sy);
    if (length > 0)
    {
        sx /= length;
        sy /= length;
        // Inverse isometric projection: keys move in screen directions.
        // Equal projected speed for all directions, independent of frame rate.
        const float speed = keys[16] ? 145.f : 88.f;
        const float dx = (sx / 64.f + sy / 32.f) * speed * dt;
        const float dy = (-sx / 64.f + sy / 32.f) * speed * dt;
        const float directionX = sx / 64.f + sy / 32.f;
        const float directionY = -sx / 64.f + sy / 32.f;
        const float directionLength = std::hypot(directionX, directionY);
        m_attackDirection = {directionX / directionLength, directionY / directionLength};
        const Point before = m_player;
        Move(m_player, dx, dy);
        if (Distance(before, m_player) > .001f)
        {
            m_walk += dt * (keys[16] ? 14 : 9);
        }
        else
        {
            m_walk = 0;
        }
        if (std::abs(sx) > .01f)
        {
            m_facing = sx > 0 ? 1.f : -1.f;
        }
    }
    else
    {
        m_walk = 0;
    }
    Recenter();
    const float follow = 1 - std::exp(-7 * dt);
    m_camera.x += (m_player.x - m_camera.x) * follow;
    m_camera.y += (m_player.y - m_camera.y) * follow;
    if (keys[' '])
    {
        Attack();
    }
    UpdateLevelOne(dt);
    for (size_t i = 0; i < m_citizens.size(); ++i)
    {
        Citizen& npc = m_citizens[i];
        Point target = npc.home;
        const float distance = Distance(npc.position, m_player);
        if (distance > 12)
        {
            continue; // distant actors stay dormant until approached
        }
        const float fromHome = Distance(npc.position, npc.home);
        if (npc.role == 2 && npc.remembersHelp && distance < 2.5f && fromHome < 4)
        {
            // Only witnesses near the help event receive this memory.
            target = {npc.position.x + (npc.position.x - m_player.x),
                      npc.position.y + (npc.position.y - m_player.y)};
        }
        else if (npc.role != 2 && npc.remembersHelp && distance < 3 && distance > 1.1f &&
                 fromHome < 4)
        {
            target = m_player;
        }
        else if (npc.role != 1)
        {
            target.x += std::sin(m_time * .35f + static_cast<float>(i)) * .35f;
            target.y += std::cos(m_time * .27f + static_cast<float>(i)) * .35f;
        }
        const float d = Distance(npc.position, target);
        if (d > .1f)
        {
            Move(npc.position,
                 (target.x - npc.position.x) / d * dt * .48f,
                 (target.y - npc.position.y) / d * dt * .48f);
        }
    }
}

int CityScene::NearbyCitizen() const
{
    int result = -1;
    float nearest = 1.65f;
    for (size_t i = 0; i < m_citizens.size(); ++i)
    {
        const float d = Distance(m_player, m_citizens[i].position);
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
    if (m_health <= 0 || m_map)
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
    if (npc.role == 0 && !m_helped)
    {
        m_helped = true;
        m_reputation += 5;
        for (Citizen& witness : m_citizens)
        {
            if (Distance(witness.position, npc.position) < 4.5f)
            {
                witness.remembersHelp = true;
                m_remembered.insert(witness.id);
            }
        }
        npc.remembersHelp = true;
        m_remembered.insert(npc.id);
        ShowNotice(m_text.Get("notice.helped"));
        return;
    }
    const std::string role = npc.role == 2 ? "lookout" : npc.role == 1 ? "merchant" : "resident";
    std::string key = npc.remembersHelp ? npc.dialogueRemembered : npc.dialogueNeutral;
    if (key.empty())
    {
        key = "dialogue." + role + (npc.remembersHelp ? ".remembered" : ".neutral");
    }
    ShowNotice(m_text.Format(key, {{"name", npc.name}}));
}

void CityScene::DrawGround(Renderer& r) const
{
    for (const auto& chunk : m_chunks)
    {
        for (int ix = 0; ix < ChunkSize; ++ix)
        {
            for (int iy = 0; iy < ChunkSize; ++iy)
            {
                const int x = chunk.first * ChunkSize + ix, y = chunk.second * ChunkSize + iy;
                const Point a = Project(float(x), float(y)), b = Project(x + 1.f, float(y));
                const Point c = Project(x + 1.f, y + 1.f), d = Project(float(x), y + 1.f);
                if (c.y < -80 || a.y > Renderer::Height + 80 || b.x < -100 ||
                    d.x > Renderer::Width + 100)
                {
                    continue;
                }
                const bool road = Road(x, y);
                const float variation =
                    Mod(x * 17 + y * 31 + static_cast<int>(m_originX % 7) * 8 * 17 +
                            static_cast<int>(m_originY % 7) * 8 * 31,
                        7) *
                    .003f;
                const Color tile =
                    road ? Color(.037f + variation, .061f + variation, .094f + variation)
                         : Color(.084f + variation, .12f + variation, .16f + variation);
                r.Quad(a, b, c, d, tile);
                r.Line(a, b, .65f, Color(.19f, .32f, .38f, road ? .12f : .35f));
                r.Line(a, d, .65f, Color(.19f, .32f, .38f, road ? .12f : .35f));
                if (road && Mod(x + y, 3) == 0)
                {
                    const Point p = Project(x + .5f, y + .5f);
                    r.Ellipse(p, 19 * m_zoom, 5 * m_zoom, Color(.16f, .28f, .38f, .18f));
                    r.Line({p.x - 11 * m_zoom, p.y},
                           {p.x + 12 * m_zoom, p.y - 1},
                           1,
                           Color(.3f, .55f, .66f, .15f));
                }
                if (Mod(x, 8) == 4 && !Road(-1, y))
                {
                    r.Line(Project(float(x), y + .18f),
                           Project(float(x), y + .68f),
                           1.6f * m_zoom,
                           Amber.Alpha(.45f));
                }
                if (Mod(y, 8) == 4 && !Road(x, -1))
                {
                    r.Line(Project(x + .18f, float(y)),
                           Project(x + .68f, float(y)),
                           1.6f * m_zoom,
                           Amber.Alpha(.45f));
                }
                if ((Mod(x, 8) == 2 || Mod(x, 8) == 5) && !road)
                {
                    r.Line(b, c, 2 * m_zoom, Cyan.Alpha(.12f));
                }
            }
        }
    }
    // Neon spill and elongated wet-road reflections remain below actors.
    for (const Building& b : m_buildings)
    {
        const Color glow = Accent(b.palette);
        const Point p = Project(b.x + b.w * .5f, b.y + b.d + .45f);
        if (p.x < -200 || p.x > Renderer::Width + 200 || p.y < -100 || p.y > Renderer::Height + 200)
        {
            continue;
        }
        r.Glow(p, 85 * m_zoom, 28 * m_zoom, glow.Alpha(.75f));
        for (int i = 0; i < 9; ++i)
        {
            const float offset = static_cast<float>(i);
            const float width = (35 - offset * 2.5f) * m_zoom;
            r.Line({p.x - width + offset * 3, p.y + offset * 5},
                   {p.x + width + offset * 3, p.y + offset * 5},
                   (3 - offset * .2f) * m_zoom,
                   glow.Alpha(.17f - offset * .014f));
        }
    }
}

void CityScene::DrawBuilding(Renderer& r, const Building& b) const
{
    const Point a = Project(b.x, b.y), right = Project(b.x + b.w, b.y);
    const Point front = Project(b.x + b.w, b.y + b.d), left = Project(b.x, b.y + b.d);
    const float h = b.height * m_zoom;
    if (right.x < -180 || left.x > Renderer::Width + 180 || front.y < -100 ||
        a.y - h > Renderer::Height + 100)
    {
        return;
    }
    const Point player = Project(m_player.x, m_player.y, 18);
    const bool covers = player.x > left.x - 10 && player.x < right.x + 10 &&
                        player.y > a.y - h - 10 && player.y < front.y + 8 &&
                        m_player.x + m_player.y < b.x + b.y + b.w + b.d;
    const float opacity = covers ? .23f : 1.f;
    const Color neon = Accent(b.palette).Alpha(opacity).Emissive(3.f);
    const Point at(a.x, a.y - h), bt(right.x, right.y - h);
    const Point ct(front.x, front.y - h), dt(left.x, left.y - h);
    const Color roof(.115f, .17f, .23f, opacity);
    const Color wallLeft(.066f, .087f, .145f, opacity), wallRight(.045f, .066f, .108f, opacity);

    r.Quad(right, front, ct, bt, wallRight);
    r.Quad(front, left, dt, ct, wallLeft);
    r.Quad(at, bt, ct, dt, roof);
    r.Line(at, bt, 2 * m_zoom, neon.Shade(.55f));
    r.Line(at, dt, 2 * m_zoom, Color(.32f, .42f, .48f, opacity));
    r.Line(bt, ct, 2 * m_zoom, neon);
    r.Line(dt, ct, 2 * m_zoom, neon);
    r.Line(ct, front, 1.5f * m_zoom, neon.Shade(.5f));

    // Windows lie on each isometric facade.
    for (int row = 0; row < static_cast<int>((b.height - 35) / 25); ++row)
    {
        const float z = 32.f + row * 25;
        for (int col = 0; col < static_cast<int>(b.w * 2) - 1; ++col)
        {
            const float x = b.x + .35f + col * .5f;
            const bool lit = Mod(col * 7 + row * 3 + b.palette, 5) != 0;
            const Color light = lit ? neon.Alpha(opacity * .58f) : Color(.12f, .17f, .23f, opacity);
            r.Quad(Project(x, b.y + b.d, z),
                   Project(x + .24f, b.y + b.d, z),
                   Project(x + .24f, b.y + b.d, z + 11),
                   Project(x, b.y + b.d, z + 11),
                   light);
        }
        for (int col = 0; col < static_cast<int>(b.d * 2) - 1; ++col)
        {
            const float y = b.y + .35f + col * .5f;
            const Color light = Mod(col + row, 3) == 0 ? Amber.Alpha(opacity * .48f)
                                                       : Color(.09f, .21f, .28f, opacity);
            r.Quad(Project(b.x + b.w, y, z),
                   Project(b.x + b.w, y + .24f, z),
                   Project(b.x + b.w, y + .24f, z + 11),
                   Project(b.x + b.w, y, z + 11),
                   light);
        }
    }
    // Storefront shutter, awning and rooftop utility unit.
    r.Quad(Project(b.x + .35f, b.y + b.d, 0),
           Project(b.x + 1.05f, b.y + b.d, 0),
           Project(b.x + 1.05f, b.y + b.d, 26),
           Project(b.x + .35f, b.y + b.d, 26),
           Color(.023f, .033f, .053f, opacity));
    for (int i = 0; i < 5; ++i)
    {
        r.Line(Project(b.x + .36f, b.y + b.d, 4.f + i * 4),
               Project(b.x + 1.04f, b.y + b.d, 4.f + i * 4),
               1,
               Muted.Alpha(opacity * .35f));
    }
    r.Quad(Project(b.x, b.y + b.d, 31),
           Project(b.x + b.w, b.y + b.d, 31),
           Project(b.x + b.w, b.y + b.d + .3f, 25),
           Project(b.x, b.y + b.d + .3f, 25),
           neon.Shade(.34f));
    r.Line(Project(b.x, b.y + b.d + .3f, 25),
           Project(b.x + b.w, b.y + b.d + .3f, 25),
           2 * m_zoom,
           neon);
    const Point unit = Project(b.x + b.w * .45f, b.y + b.d * .4f, b.height + 3);
    r.Quad({unit.x - 16 * m_zoom, unit.y},
           {unit.x, unit.y - 8 * m_zoom},
           {unit.x + 23 * m_zoom, unit.y + 3 * m_zoom},
           {unit.x + 7 * m_zoom, unit.y + 11 * m_zoom},
           Color(.18f, .24f, .29f, opacity));
    for (int i = 0; i < 4; ++i)
    {
        r.Line({unit.x - 9 * m_zoom + i * 5 * m_zoom, unit.y},
               {unit.x + 4 * m_zoom + i * 5 * m_zoom, unit.y + 6 * m_zoom},
               1,
               Color(.04f, .07f, .1f, opacity));
    }
    const Point antenna = Project(b.x + b.w * .75f, b.y + .45f, b.height);
    r.Line(antenna, {antenna.x, antenna.y - 26 * m_zoom}, 2, Muted.Alpha(opacity));
    r.Line({antenna.x - 9 * m_zoom, antenna.y - 18 * m_zoom},
           {antenna.x + 9 * m_zoom, antenna.y - 18 * m_zoom},
           1,
           Muted.Alpha(opacity));
    r.Glow({antenna.x, antenna.y - 27 * m_zoom}, 9, 9, Pink.Alpha(opacity));
    r.Rect(antenna.x - 1, antenna.y - 28 * m_zoom, 2, 2, Pink.Alpha(opacity).Emissive(3.f));

    float scale = (b.id == "arcadia" ? 1.4f : .95f) * m_zoom;
    const float maxSign = std::max(95.f, (b.w + b.d) * 24.f) * m_zoom;
    const float measured = r.MeasureText(b.sign, scale);
    if (measured > maxSign)
    {
        scale *= maxSign / measured;
    }
    const float signWidth = r.MeasureText(b.sign, scale) + 14 * m_zoom;
    const float signHeight = 16 * scale + 12 * m_zoom;
    const Point sign = Project(b.x + b.w * .55f, b.y + b.d, b.height * .65f);
    const float pulse = .86f + .14f * std::sin(m_time * 2 + b.palette);
    r.Glow(sign, signWidth * .8f, 27 * m_zoom, neon.Alpha(opacity * pulse));
    r.Rect(sign.x - signWidth * .5f,
           sign.y - 6 * m_zoom,
           signWidth,
           signHeight,
           Color(.015f, .025f, .045f, opacity));
    r.Line({sign.x - signWidth * .5f, sign.y - 6 * m_zoom},
           {sign.x + signWidth * .5f, sign.y - 6 * m_zoom},
           2,
           neon);
    r.Line({sign.x - signWidth * .5f, sign.y - 6 * m_zoom + signHeight},
           {sign.x + signWidth * .5f, sign.y - 6 * m_zoom + signHeight},
           1,
           neon.Alpha(opacity * .5f));
    r.Text(sign.x - signWidth * .5f + 7 * m_zoom, sign.y, b.sign, scale, neon.Shade(pulse));
    if (b.id == "arcadia")
    {
        r.Text(sign.x - signWidth * .5f,
               sign.y + signHeight + 2 * m_zoom,
               m_text.Get("sign.arcadia_motto"),
               .65f * m_zoom,
               White.Alpha(opacity * .7f));
    }
}

void CityScene::DrawActor(Renderer& r, Point position, int role, float phase, bool friendly) const
{
    const Point p = Project(position.x, position.y);
    if (p.x < -50 || p.x > Renderer::Width + 50 || p.y < -50 || p.y > Renderer::Height + 50)
    {
        return;
    }
    const bool player = role < 0;
    const float s = m_zoom, step = std::sin(phase) * (player ? 3.f : 1.2f) * s;
    const float bob = std::abs(std::sin(phase)) * 1.1f * s;
    const Color trim = (player      ? Cyan
                        : role == 2 ? Pink
                        : role == 1 ? Amber
                                    : Color(.53f, .68f, .87f))
                           .Emissive(player ? 1.5f : .25f);
    r.Ellipse({p.x, p.y + 2 * s}, 11 * s, 4 * s, Color(0, 0, .02f, .55f));
    if (player)
    {
        r.Glow(p, 32 * s, 12 * s, Cyan.Alpha(.6f));
        r.Line({p.x - 15 * s, p.y}, {p.x, p.y + 7 * s}, 1.5f, Cyan.Alpha(.8f));
        r.Line({p.x, p.y + 7 * s}, {p.x + 15 * s, p.y}, 1.5f, Cyan.Alpha(.8f));
    }
    r.Line({p.x - 4 * s, p.y - 11 * s}, {p.x - 5 * s + step, p.y}, 4 * s, Color(.12f, .18f, .24f));
    r.Line({p.x + 4 * s, p.y - 11 * s}, {p.x + 5 * s - step, p.y}, 4 * s, Color(.12f, .18f, .24f));
    r.Quad({p.x - 7 * s, p.y - 27 * s - bob},
           {p.x + 7 * s, p.y - 27 * s - bob},
           {p.x + 10 * s, p.y - 9 * s},
           {p.x - 9 * s, p.y - 9 * s},
           Color(.065f, .10f, .16f));
    r.Line({p.x - 7 * s, p.y - 25 * s},
           {p.x - 10 * s, p.y - 15 * s + step},
           3 * s,
           trim.Shade(.48f));
    r.Line({p.x + 7 * s, p.y - 25 * s},
           {p.x + 10 * s, p.y - 15 * s - step},
           3 * s,
           trim.Shade(.48f));
    r.Line({p.x - 7 * s, p.y - 26 * s - bob}, {p.x + 6 * s, p.y - 26 * s - bob}, 2 * s, trim);
    r.Rect(p.x - 3 * s, p.y - 24 * s - bob, 2 * s, 12 * s, trim.Shade(.6f));
    r.Ellipse({p.x, p.y - 32 * s - bob}, 7 * s, 8 * s, Color(.12f, .17f, .23f));
    r.Rect(p.x - 5 * s, p.y - 35 * s - bob, 10 * s, 4 * s, Color(.025f, .035f, .06f));
    r.Line({p.x - 4 * s + (player ? m_facing : 0), p.y - 33 * s - bob},
           {p.x + 4 * s + (player ? m_facing : 0), p.y - 33 * s - bob},
           2 * s,
           trim);
    if (player)
    {
        r.Glow({p.x, p.y - 33 * s}, 14 * s, 7 * s, Cyan.Alpha(.55f));
    }
    if (friendly)
    {
        r.Ellipse({p.x, p.y - 48 * s}, 2.5f * s, 2.5f * s, role == 2 ? Pink : Cyan);
    }
}

void CityScene::DrawWeather(Renderer& r) const
{
    if (m_rain)
    {
        for (int i = 0; i < 135; ++i)
        {
            const float speed = 210.f + Mod(i * 37, 120);
            const float x = std::fmod(i * 113.7f + m_time * 45.f, Renderer::Width + 120) - 60;
            const float y = std::fmod(i * 73.3f + m_time * speed, Renderer::Height + 120) - 60;
            r.Line({x, y},
                   {x + 4, y + 12.f + Mod(i, 8)},
                   .7f,
                   Color(.52f, .74f, .85f, .1f + Mod(i, 3) * .035f));
        }
    }
}

void CityScene::DrawMap(Renderer& r, bool expanded) const
{
    const float size = expanded ? 440.f : 152.f;
    const float left = expanded ? (Renderer::Width - size) * .5f : Renderer::Width - size - 56;
    const float top = expanded ? (Renderer::Height - size) * .5f - 10 : 113.f;
    r.Rect(left - 12,
           top - 12,
           size + 24,
           size + (expanded ? 72 : 48),
           Color(.012f, .024f, .04f, .95f));
    r.Line({left - 12, top - 12}, {left + size + 12, top - 12}, 2, Cyan.Alpha(.6f));
    const float radius = expanded ? 36.f : 20.f;
    const float minX = m_player.x - radius, minY = m_player.y - radius;
    const float unit = size / (radius * 2);
    auto mapRect = [&](float x, float y, float width, float height, Color color)
    {
        const float x0 = std::max(left, left + (x - minX) * unit);
        const float y0 = std::max(top, top + (y - minY) * unit);
        const float x1 = std::min(left + size, left + (x + width - minX) * unit);
        const float y1 = std::min(top + size, top + (y + height - minY) * unit);
        if (x1 > x0 && y1 > y0)
        {
            r.Rect(x0, y0, x1 - x0, y1 - y0, color);
        }
    };
    for (const auto& chunk : m_chunks)
    {
        mapRect(static_cast<float>(chunk.first * ChunkSize),
                static_cast<float>(chunk.second * ChunkSize),
                7.94f,
                7.94f,
                Color(.065f, .105f, .14f));
    }
    for (const Building& building : m_buildings)
    {
        mapRect(building.x,
                building.y,
                building.w,
                building.d,
                Accent(building.palette).Alpha(.5f));
    }
    for (const Citizen& npc : m_citizens)
    {
        const Point marker(left + (npc.position.x - minX) * unit,
                           top + (npc.position.y - minY) * unit);
        if (marker.x > left + 3 && marker.x < left + size - 3 && marker.y > top + 3 &&
            marker.y < top + size - 3)
        {
            r.Ellipse(marker, expanded ? 3.f : 1.5f, expanded ? 3.f : 1.5f, Amber);
        }
    }
    DrawLevelMap(r, left, top, size, radius);
    const Point marker(left + size * .5f, top + size * .5f);
    r.Glow(marker, 12, 12, Cyan);
    r.Ellipse(marker, 3.5f, 3.5f, White);
    r.Text(left,
           top + size + 10,
           m_text.Get(expanded ? "ui.map_expanded" : "ui.map"),
           1.03f,
           Muted);
    if (expanded)
    {
        r.Text(left, top + size + 35, m_text.Get("ui.map_streaming"), .92f, Muted);
    }
}

void CityScene::DrawInterface(Renderer& r) const
{
    const bool authored =
        m_originX >= -256 && m_originX <= 256 && m_originY >= -256 && m_originY <= 256 &&
        m_authoredChunks.count({static_cast<int>(m_originX), static_cast<int>(m_originY)});
    r.Rect(0, 0, Renderer::Width, 86, Color(.013f, .025f, .043f, .94f));
    r.Rect(0, 84, Renderer::Width, 1, Color(.2f, .6f, .65f, .35f));
    r.Rect(32, 25, 4, 34, Cyan);
    r.Text(49, 19, m_text.Get("ui.title"), 2.f, White);
    r.Text(50, 56, m_text.Get("ui.subtitle"), .89f, Muted);
    r.Text(Renderer::Width * .5f - 127,
           21,
           m_text.Get(authored ? "ui.district" : "ui.outskirts"),
           1.46f,
           Cyan);
    r.Text(Renderer::Width * .5f - 127, 52, m_text.Get("ui.authority"), .97f, Muted);
    r.Text(Renderer::Width - 264, 23, m_text.Get("ui.online"), 1.08f, Cyan);
    r.Text(Renderer::Width - 264, 50, m_text.Get("ui.curfew"), .97f, Amber);
    DrawMap(r, false);

    DrawLevelHud(r);

    const auto& post = r.PostSettings();
    auto state = [&](bool enabled)
    {
        return m_text.Get(enabled ? "ui.post_on" : "ui.post_off");
    };
    std::ostringstream exposure;
    exposure << std::fixed << std::setprecision(2) << post.exposure;
    r.Rect(32, 280, 352, 146, Color(.012f, .025f, .044f, .9f));
    r.Rect(32, 280, 3, 146, Cyan);
    r.Text(49,
           293,
           m_text.Get(r.PostAvailable() ? "ui.post_title" : "ui.post_unavailable"),
           1.08f,
           Cyan);
    r.Text(49,
           322,
           m_text.Format(
               "ui.post_lights",
               {{"all", state(post.enabled && r.PostAvailable())}, {"bloom", state(post.bloom)}}),
           .94f,
           White);
    r.Text(49,
           347,
           m_text.Format("ui.post_edges",
                         {{"vignette", state(post.vignette)}, {"blur", state(post.edgeBlur)}}),
           .94f,
           White);
    r.Text(49, 372, m_text.Format("ui.post_exposure", {{"value", exposure.str()}}), .94f, Muted);
    r.Text(49, 397, m_text.Get("ui.post_reload"), .92f, Muted);

    const int nearbyCitizenIndex = NearbyCitizen();
    if (nearbyCitizenIndex >= 0 && NearbyLevelInteraction() == -1)
    {
        const Citizen& npc = m_citizens[nearbyCitizenIndex];
        const Point p = Project(npc.position.x, npc.position.y, 61);
        const std::string label = m_text.Format("ui.interact", {{"name", npc.name}});
        const float width = r.MeasureText(label, 1.f) + 14;
        const float labelScale = std::min(1.08f, (width - 6) / std::max(1.f, width - 14));
        r.Rect(p.x - width * .5f, p.y - 5, width, 28, Color(.01f, .025f, .04f, .95f));
        r.Text(p.x - r.MeasureText(label, labelScale) * .5f,
               p.y,
               label,
               labelScale,
               npc.role == 2 ? Pink : Cyan);
    }
    if (m_noticeTime > 0)
    {
        const bool paged = m_dialoguePages.size() > 1;
        const float height = r.WrappedHeight(m_notice, 1.08f, 666) + 24 + (paged ? 25 : 0);
        // Keep the existing dialogue panel size; use its spare padding for larger text.
        float dialogueScale = 1.17f;
        const float textHeight = height - 12 - (paged ? 25 : 0);
        while (dialogueScale > 1.08f && r.WrappedHeight(m_notice, dialogueScale, 666) > textHeight)
        {
            dialogueScale = std::max(1.08f, dialogueScale - .01f);
        }
        const float top = Renderer::Height - 110 - height;
        const float left = (Renderer::Width - 700) * .5f;
        r.Rect(left, top, 700, height, Color(.012f, .028f, .046f, .97f));
        r.Rect(left, top, 3, height, Cyan);
        r.TextWrapped(left + 17, top + 6, m_notice, dialogueScale, White, 666);
        if (paged)
        {
            const std::string hint =
                m_text.Get(m_dialoguePage + 1 < m_dialoguePages.size() ? "ui.next_dialogue"
                                                                       : "ui.close_dialogue");
            r.Text(left + 17,
                   Renderer::Height - 135,
                   hint + "  " + std::to_string(m_dialoguePage + 1) + " / " +
                       std::to_string(m_dialoguePages.size()),
                   .92f,
                   Cyan);
        }
    }
    const float footerTop = Renderer::Height - 92;
    r.Rect(0, footerTop, Renderer::Width, 92, Color(.012f, .024f, .042f, .96f));
    r.Line({0, footerTop}, {Renderer::Width, footerTop}, 1, Color(.18f, .48f, .55f, .4f));
    r.TextFitted(32,
                 footerTop + 17,
                 m_text.Get(m_helped ? "ui.player_known" : "ui.player"),
                 1.03f,
                 White,
                 280);
    r.TextFitted(
        32,
        footerTop + 46,
        m_text.Format("ui.medkit",
                      {{"value", m_helped ? "0" : "1"}, {"rep", std::to_string(m_reputation)}}),
        .95f,
        Muted,
        280);
    r.TextFitted(340, footerTop + 17, m_text.Get("ui.controls_move"), 1.03f, Cyan, 570);
    r.TextFitted(340, footerTop + 46, m_text.Get("ui.controls_view"), .95f, Muted, 570);
    const std::string coordinate =
        m_text.Format("ui.chunk",
                      {{"x", std::to_string(m_originX)}, {"y", std::to_string(m_originY)}});
    const float width = r.MeasureText(coordinate, 1.03f);
    const float scale = width > 300 ? 1.03f * 300 / width : 1.03f;
    r.Text(Renderer::Width - 335, footerTop + 17, coordinate, scale, White);
    r.TextFitted(Renderer::Width - 335,
                 footerTop + 46,
                 m_text.Get("ui.controls_exit"),
                 .95f,
                 Muted,
                 300);
    if (m_map)
    {
        DrawMap(r, true);
    }
}

void CityScene::Draw(Renderer& r)
{
    r.Begin(Color(.018f, .032f, .055f));
    DrawGround(r);
    DrawLevelGround(r);

    struct DrawItem
    {
        float depth;
        int kind;
        size_t index;
    };

    std::vector<DrawItem> items;
    for (size_t i = 0; i < m_buildings.size(); ++i)
    {
        const Building& b = m_buildings[i];
        items.push_back({b.x + b.y + b.w + b.d, 0, i});
    }
    for (size_t i = 0; i < m_citizens.size(); ++i)
    {
        items.push_back({m_citizens[i].position.x + m_citizens[i].position.y, 1, i});
    }
    for (size_t i = 0; i < m_enemies.size(); ++i)
    {
        Point local;
        if (m_enemies[i].health > 0 && LevelLocal(m_enemies[i].position, local))
        {
            items.push_back({local.x + local.y, 3, i});
        }
    }
    Point base;
    if (LevelLocal(m_spawn, base))
    {
        items.push_back({base.x + base.y, 4, 0});
    }
    items.push_back({m_player.x + m_player.y, 2, 0});
    std::stable_sort(items.begin(),
                     items.end(),
                     [](const DrawItem& a, const DrawItem& b)
                     {
                         return a.depth < b.depth;
                     });
    for (const DrawItem& item : items)
    {
        if (item.kind == 0)
        {
            DrawBuilding(r, m_buildings[item.index]);
        }
        else if (item.kind == 1)
        {
            const Citizen& npc = m_citizens[item.index];
            DrawActor(r,
                      npc.position,
                      npc.role,
                      m_time * 2 + static_cast<float>(item.index),
                      npc.remembersHelp);
        }
        else if (item.kind == 3)
        {
            DrawEnemy(r, m_enemies[item.index]);
        }
        else if (item.kind == 4)
        {
            DrawBaseProps(r);
        }
        else
        {
            DrawActor(r, m_player, -1, m_walk, false);
            DrawAttack(r);
        }
    }
    // A surveillance drone and its scanning cone embody corporate presence.
    if (m_originX >= -8 && m_originX <= 8 && m_originY >= -8 && m_originY <= 8)
    {
        const float droneX =
            11.7f - static_cast<float>(m_originX * ChunkSize) + std::sin(m_time * .3f) * 1.1f;
        const float droneY =
            11.3f - static_cast<float>(m_originY * ChunkSize) + std::cos(m_time * .3f) * 1.1f;
        const Point drone = Project(droneX, droneY, 87 + std::sin(m_time * 2) * 3);
        const Point scan = Project(droneX + .8f, droneY + .8f);
        r.Triangle(drone,
                   {scan.x - 30 * m_zoom, scan.y},
                   {scan.x + 30 * m_zoom, scan.y},
                   Cyan.Alpha(.045f));
        r.Ellipse(scan, 31 * m_zoom, 10 * m_zoom, Cyan.Alpha(.055f));
        r.Line({drone.x - 18 * m_zoom, drone.y},
               {drone.x + 18 * m_zoom, drone.y},
               3 * m_zoom,
               Muted);
        r.Ellipse({drone.x - 18 * m_zoom, drone.y}, 9 * m_zoom, 3 * m_zoom, Color(.2f, .3f, .38f));
        r.Ellipse({drone.x + 18 * m_zoom, drone.y}, 9 * m_zoom, 3 * m_zoom, Color(.2f, .3f, .38f));
        r.Rect(drone.x - 7 * m_zoom,
               drone.y - 4 * m_zoom,
               14 * m_zoom,
               8 * m_zoom,
               Color(.1f, .17f, .23f));
        r.Glow(drone, 20 * m_zoom, 12 * m_zoom, Pink);
        r.Rect(drone.x - 2 * m_zoom, drone.y, 4 * m_zoom, 2 * m_zoom, Pink.Emissive(4.f));
    }
    DrawWeather(r);
    r.BeginOverlay();
    if (m_interface)
    {
        DrawInterface(r);
    }
    if (m_baseOpen)
    {
        DrawBaseInterface(r);
    }
    r.End();
}
