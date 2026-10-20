#include "stdafx.h"
#include "CityScene.h"
#include "WorldLayout.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cmath>

namespace
{
std::uint64_t Identity(const std::string& id)
{
    std::uint64_t value = 1469598103934665603ULL;
    for (unsigned char ch : id)
    {
        value = (value ^ ch) * 1099511628211ULL;
    }
    return value;
}

Point Junction(Point p)
{
    return {std::floor(p.x / 8) * 8 + 4, std::floor(p.y / 8) * 8 + 4};
}

Point ResidentHome(const TownDefinition& town, size_t slot)
{
    const int cell = static_cast<int>(slot % (town.width * town.height));
    const bool alternate = (slot / static_cast<size_t>(town.width * town.height)) % 2 != 0;
    return {(town.x + cell % town.width) * 8.f + (alternate ? 4.6f : 3.4f),
            (town.y + cell / town.width) * 8.f + (alternate ? 3.4f : 4.6f)};
}
}

bool CityScene::TownScenePoint(Point native, Point& local) const
{
    // The seven settlements are finite; never convert a distant int64 origin to float.
    if (m_originX < -2048 || m_originX > 2048 || m_originY < -2048 || m_originY > 2048)
    {
        return false;
    }
    local = {native.x - static_cast<float>(m_originX * ChunkSize),
             native.y - static_cast<float>(m_originY * ChunkSize)};
    return true;
}

void CityScene::SyncTownResidents()
{
    if (m_originX < -2048 || m_originX > 2048 || m_originY < -2048 || m_originY > 2048)
    {
        return;
    }
    const float x = static_cast<float>(m_originX * ChunkSize);
    const float y = static_cast<float>(m_originY * ChunkSize);
    for (const CitizenActor& actor : m_citizens)
    {
        const auto found = m_townResidents.find(actor.id);
        if (found == m_townResidents.end())
        {
            continue;
        }
        Citizen npc = actor;
        npc.position = {npc.position.x + x, npc.position.y + y};
        npc.home = {npc.home.x + x, npc.home.y + y};
        npc.strike = {npc.strike.x + x, npc.strike.y + y};
        found->second = npc;
    }
}

void CityScene::UnloadTownActors()
{
    SyncTownResidents();
    for (CitizenActor& npc : m_citizens)
    {
        npc.Destroy();
    }
    m_citizens.clear();
    m_sceneGraph.FlushDestroyed();
    m_townActorsDirty = true;
}

void CityScene::InitializeTownPopulation()
{
    m_townResidents.clear();
    const auto& towns = WorldTowns();
    for (Citizen npc : m_authoredCitizens)
    {
        npc.homeTown = 0;
        npc.travelWait = -1;
        m_townResidents.emplace(npc.id, npc);
    }
    for (size_t townIndex = 0; townIndex < towns.size(); ++townIndex)
    {
        const auto& town = towns[townIndex];
        const int count =
            town.population - (townIndex == 0 ? static_cast<int>(m_authoredCitizens.size()) : 0);
        std::vector<Point> homes;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int y = 0; y < town.height; ++y)
            {
                for (int x = 0; x < town.width; ++x)
                {
                    const int cx = town.x + x, cy = town.y + y;
                    if (townIndex == 0 && m_authoredChunks.count({cx, cy}))
                    {
                        continue;
                    }
                    homes.push_back({cx * 8.f + (pass == 0 ? 3.4f : 4.6f),
                                     cy * 8.f + (pass == 0 ? 4.6f : 3.4f)});
                }
            }
        }
        std::sort(homes.begin(),
                  homes.end(),
                  [&](Point a, Point b)
                  {
                      const auto ah = WorldHash(static_cast<std::int64_t>(a.x * 10),
                                                static_cast<std::int64_t>(a.y * 10),
                                                m_populationSeed);
                      const auto bh = WorldHash(static_cast<std::int64_t>(b.x * 10),
                                                static_cast<std::int64_t>(b.y * 10),
                                                m_populationSeed);
                      return ah != bh ? ah < bh : a.x != b.x ? a.x < b.x : a.y < b.y;
                  });
        for (int i = 0; i < count; ++i)
        {
            Citizen npc{};
            const auto hash = WorldHash(static_cast<std::int64_t>(townIndex), i, m_populationSeed);
            npc.id = "town/" + town.id + "/resident/" + std::to_string(i);
            npc.homeTown = static_cast<int>(townIndex);
            npc.home = npc.position = homes[static_cast<size_t>(i) % homes.size()];
            npc.occupation = static_cast<Occupation>((hash >> 8) % 7);
            // Reserve a distinct initial criminal population; neutral jobs are not raiders.
            if (npc.occupation == Occupation::Raider)
            {
                npc.occupation = Occupation::Resident;
            }
            if (i < town.hostiles)
            {
                npc.occupation = Occupation::Raider;
                npc.memory.notoriety = 65 + static_cast<int>(hash % 26);
                npc.memory.crimes = 1 + static_cast<int>((hash >> 12) % 4);
                npc.hostile = true;
            }
            else if (i < town.hostiles + town.security / 10)
            {
                npc.occupation = Occupation::Guard;
            }
            npc.role = npc.occupation == Occupation::Raider     ? 2
                       : npc.occupation == Occupation::Merchant ? 1
                                                                : 0;
            npc.name = m_text.Get("npc.job." + std::to_string(static_cast<int>(npc.occupation))) +
                       " " + std::to_string(100 + hash % 900);
            npc.travelWait = 18 + static_cast<float>((hash >> 20) % 75);
            npc.actionTimer = 3 + static_cast<float>(hash % 5);
            m_townResidents.emplace(npc.id, npc);
        }
        FrameProfiler::Instance().Event("town init=" + town.id +
                                        " chunks=" + std::to_string(town.width * town.height) +
                                        " population=" + std::to_string(town.population) +
                                        " security=" + std::to_string(town.security) +
                                        " hostile_target=" + std::to_string(town.hostiles));
    }
    m_townActorsDirty = true;
    m_townRefreshTimer = 0;
}

void CityScene::RefreshTownActors()
{
    std::set<std::string> retained;
    const auto old = m_citizens;
    m_citizens.clear();
    for (const auto& entry : m_townResidents)
    {
        Citizen npc = entry.second;
        Point local;
        if (!TownScenePoint(npc.position, local) || !HasGround(local.x, local.y))
        {
            continue;
        }
        const int cx = static_cast<int>(std::floor(local.x / ChunkSize));
        const int cy = static_cast<int>(std::floor(local.y / ChunkSize));
        // Unload before the terrain edge so travelling NPCs can continue offscreen.
        if (std::abs(cx) > StreamRadius - 1 || std::abs(cy) > StreamRadius - 1)
        {
            continue;
        }
        npc.position = local;
        TownScenePoint(npc.home, npc.home);
        TownScenePoint(npc.strike, npc.strike);
        retained.insert(npc.id);
        AddCitizen(std::move(npc));
    }
    for (CitizenActor& actor : old)
    {
        if (!retained.count(actor.id))
        {
            actor.Destroy();
        }
    }
    m_sceneGraph.FlushDestroyed();
    m_townActorsDirty = false;
    m_townRefreshTimer = .5f;
}

void CityScene::BeginTownTravel(Citizen& npc, int destination)
{
    const int current = WorldTownAt(npc.position);
    if (current < 0 || destination < 0 || current == destination)
    {
        return;
    }
    const auto& towns = WorldTowns();
    const Point a = towns[current].Center(), b = towns[destination].Center();
    const Point start = Junction(npc.position);
    const Point home = ResidentHome(
        towns[destination],
        static_cast<size_t>(WorldHash(npc.homeTown,
                                      destination,
                                      m_populationSeed + Identity(npc.id) + npc.memory.services)));
    const Point end = Junction(home);
    npc.route = {start, Point(a.x, start.y), a, b, Point(b.x, end.y), end, home, home};
    npc.routeCount = 7;
    npc.routeStep = 0;
    npc.destinationTown = destination;
    npc.travelling = true;
    npc.activity = NpcActivity::Travelling;
    npc.windup = 0;
    FrameProfiler::Instance().Event("town trip_start=" + npc.id + " from=" + towns[current].id +
                                    " to=" + towns[destination].id);
}

void CityScene::AdvanceTownTravel(Citizen& npc, float dt, bool local)
{
    float remaining = dt * 1.6f;
    npc.activity = NpcActivity::Travelling;
    npc.windup = 0;
    while (npc.travelling && npc.routeStep < npc.routeCount && remaining > 0)
    {
        Point target = npc.route[static_cast<size_t>(npc.routeStep)];
        if (local && !TownScenePoint(target, target))
        {
            return;
        }
        const float distance = std::hypot(target.x - npc.position.x, target.y - npc.position.y);
        if (distance < .16f)
        {
            ++npc.routeStep;
            continue;
        }
        const float step = std::min(distance, remaining);
        const float dx = (target.x - npc.position.x) / distance * step;
        const float dy = (target.y - npc.position.y) / distance * step;
        if (local)
        {
            const Point before = npc.position;
            Move(npc.position, dx, dy, .16f);
            if (std::hypot(npc.position.x - before.x, npc.position.y - before.y) < step * .5f)
            {
                return;
            }
        }
        else
        {
            npc.position.x += dx;
            npc.position.y += dy;
        }
        remaining -= step;
    }
    if (npc.routeStep >= npc.routeCount)
    {
        const int destination = npc.destinationTown;
        npc.travelling = false;
        npc.destinationTown = -1;
        npc.home = npc.position;
        npc.activity = NpcActivity::Idle;
        npc.travelWait =
            35 +
            static_cast<float>(
                WorldHash(npc.homeTown, destination, m_populationSeed + npc.memory.services) % 50);
        FrameProfiler::Instance().Event("town trip_arrive=" + npc.id + " town=" +
                                        WorldTowns()[static_cast<size_t>(destination)].id);
    }
}

void CityScene::UpdateTownPopulation(float dt)
{
    SyncTownResidents();
    const auto& towns = WorldTowns();
    std::vector<int> away(towns.size(), 0), present(towns.size(), 0), inbound(towns.size(), 0);
    for (const auto& entry : m_townResidents)
    {
        const Citizen& npc = entry.second;
        const int current = WorldTownAt(npc.position);
        if (current >= 0 && npc.memory.health > 0)
        {
            ++present[static_cast<size_t>(current)];
        }
        if (npc.memory.health > 0 && (npc.travelling || current != npc.homeTown))
        {
            ++away[static_cast<size_t>(npc.homeTown)];
        }
        if (npc.memory.health > 0 && npc.travelling && npc.destinationTown >= 0)
        {
            ++inbound[static_cast<size_t>(npc.destinationTown)];
        }
    }
    for (auto& entry : m_townResidents)
    {
        Citizen& npc = entry.second;
        auto* actor = dynamic_cast<CitizenActor*>(m_sceneGraph.Find("citizen/" + npc.id));
        if (npc.active && !npc.story && npc.memory.health > 0 && npc.travelWait >= 0)
        {
            if (npc.travelling)
            {
                if (!actor)
                {
                    AdvanceTownTravel(npc, dt, false);
                }
            }
            else
            {
                npc.travelWait = std::max(0.f, npc.travelWait - dt);
                const int current = WorldTownAt(npc.position);
                if (npc.travelWait <= 0 && current >= 0 && npc.windup <= 0)
                {
                    const bool returning = current != npc.homeTown;
                    std::vector<int> choices;
                    for (const auto& road : WorldTownRoads())
                    {
                        const int other = road.from == current ? road.to
                                          : road.to == current ? road.from
                                                               : -1;
                        if (other >= 0 && (!returning || other == npc.homeTown) &&
                            present[static_cast<size_t>(other)] +
                                    inbound[static_cast<size_t>(other)] <
                                towns[static_cast<size_t>(other)].population + 6)
                        {
                            choices.push_back(other);
                        }
                    }
                    if (!choices.empty() &&
                        (returning || away[static_cast<size_t>(npc.homeTown)] < 4))
                    {
                        const int destination =
                            choices[static_cast<size_t>(
                                        WorldHash(npc.homeTown,
                                                  static_cast<std::int64_t>(Identity(npc.id) &
                                                                            0x7fffffffffffffffULL),
                                                  m_populationSeed + npc.memory.services)) %
                                    choices.size()];
                        BeginTownTravel(npc, destination);
                        ++inbound[static_cast<size_t>(destination)];
                        if (!returning)
                        {
                            ++away[static_cast<size_t>(npc.homeTown)];
                        }
                    }
                    npc.travelWait = std::max(8.f, npc.travelWait);
                }
            }
        }
        if (actor)
        {
            Citizen data = npc;
            TownScenePoint(data.position, data.position);
            TownScenePoint(data.home, data.home);
            TownScenePoint(data.strike, data.strike);
            static_cast<Citizen&>(*actor) = std::move(data);
        }
    }
    m_townRefreshTimer -= dt;
    if (m_townActorsDirty || m_townRefreshTimer <= 0)
    {
        RefreshTownActors();
    }
}
