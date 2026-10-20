#include "stdafx.h"
#include "WorldLayout.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace
{
std::vector<TownDefinition> towns;
std::vector<TownRoad> roads;

std::int64_t Floor(std::int64_t value, int divisor)
{
    return value / divisor - (value % divisor < 0 ? 1 : 0);
}

int Mod(std::int64_t value, int divisor)
{
    return static_cast<int>((value % divisor + divisor) % divisor);
}

std::uint64_t Mix(std::uint64_t v)
{
    v += 0x9e3779b97f4a7c15ULL;
    v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
    v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
    return v ^ (v >> 31);
}
}

std::uint64_t WorldHash(std::int64_t x, std::int64_t y, std::uint64_t salt)
{
    return Mix(static_cast<std::uint64_t>(x) + salt) ^ Mix(static_cast<std::uint64_t>(y) + 2026);
}

Point TownDefinition::Center() const
{
    return {static_cast<float>(x * 8 + width * 4), static_cast<float>(y * 8 + height * 4)};
}

const std::vector<TownDefinition>& WorldTowns()
{
    return towns;
}

const std::vector<TownRoad>& WorldTownRoads()
{
    return roads;
}

int WorldTownIndex(std::int64_t x, std::int64_t y)
{
    for (size_t i = 0; i < towns.size(); ++i)
    {
        const auto& town = towns[i];
        if (x >= town.x && x < town.x + town.width && y >= town.y && y < town.y + town.height)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int WorldTownAt(Point native)
{
    if (!std::isfinite(native.x) || !std::isfinite(native.y) || std::abs(native.x) > 32768 ||
        std::abs(native.y) > 32768)
    {
        return -1;
    }
    return WorldTownIndex(static_cast<std::int64_t>(std::floor(native.x / 8)),
                          static_cast<std::int64_t>(std::floor(native.y / 8)));
}

bool LoadWorldLayout(std::string& error)
{
    std::string source;
    if (!ReadResource(L"Data/World/towns.world", source))
    {
        error = "Cannot read Data/World/towns.world";
        return false;
    }
    std::vector<TownDefinition> next;
    std::vector<std::pair<std::string, std::string>> links;
    std::map<std::string, int> ids;
    std::istringstream lines(source);
    std::string line;
    bool version = false;
    int lineNumber = 0;
    while (std::getline(lines, line))
    {
        ++lineNumber;
        std::istringstream input(line.substr(0, line.find('#')));
        std::string type, trailing;
        if (!(input >> type))
        {
            continue;
        }
        bool valid = false;
        if (type == "version")
        {
            int value = 0;
            valid = bool(input >> value) && value == 1 && !version;
            version = true;
        }
        else if (type == "town")
        {
            TownDefinition town;
            valid = bool(input >> town.id >> town.x >> town.y >> town.width >> town.height >>
                         town.region >> town.security >> town.population >> town.hostiles) &&
                    !town.id.empty() && town.id.size() <= 40 && town.id != "roads" &&
                    town.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789") ==
                        std::string::npos &&
                    town.x >= -1024 && town.x <= 1024 && town.y >= -1024 && town.y <= 1024 &&
                    town.width >= 7 && town.width <= 15 && town.height >= 7 && town.height <= 15 &&
                    town.width % 2 == 1 && town.height % 2 == 1 && town.region >= 0 &&
                    town.region < 5 && town.security >= 0 && town.security <= 100 &&
                    town.population == 70 && town.hostiles >= 0 &&
                    town.hostiles <= town.population && !ids.count(town.id);
            for (const auto& old : next)
            {
                valid = valid && !(town.x < old.x + old.width && town.x + town.width > old.x &&
                                   town.y < old.y + old.height && town.y + town.height > old.y);
            }
            if (valid)
            {
                ids[town.id] = static_cast<int>(next.size());
                next.push_back(town);
            }
        }
        else if (type == "road")
        {
            std::string from, to;
            valid = bool(input >> from >> to) && from != to;
            links.push_back({from, to});
        }
        if (!valid || (input >> trailing))
        {
            error = "Invalid towns.world record at line " + std::to_string(lineNumber);
            return false;
        }
    }
    if (!version || next.size() != 7 || next.front().id != "afterglow")
    {
        error = "towns.world requires version 1 and seven towns, afterglow first";
        return false;
    }
    std::vector<TownRoad> nextRoads;
    std::set<std::pair<int, int>> unique;
    for (const auto& link : links)
    {
        if (!ids.count(link.first) || !ids.count(link.second))
        {
            error = "Unknown town in road";
            return false;
        }
        const int from = ids.at(link.first), to = ids.at(link.second);
        const Point a = next[from].Center(), b = next[to].Center();
        if ((a.x != b.x && a.y != b.y) ||
            !unique.insert({std::min(from, to), std::max(from, to)}).second)
        {
            error = "Roads must be unique and axis-aligned";
            return false;
        }
        nextRoads.push_back({from, to});
    }
    std::set<int> connected{0};
    for (size_t pass = 0; pass < next.size(); ++pass)
    {
        for (const auto& road : nextRoads)
        {
            if (connected.count(road.from) || connected.count(road.to))
            {
                connected.insert(road.from);
                connected.insert(road.to);
            }
        }
    }
    const auto& stage = next.front();
    if (connected.size() != next.size() || stage.width < 7 || stage.height < 7 || stage.x > 0 ||
        stage.y > 0 || stage.x + stage.width < 3 || stage.y + stage.height < 3)
    {
        error = "Road graph must connect all towns and Afterglow must contain the authored core";
        return false;
    }
    towns = std::move(next);
    roads = std::move(nextRoads);
    return true;
}

int WorldRegion(std::int64_t x, std::int64_t y)
{
    const int town = WorldTownIndex(x, y);
    return town >= 0 ? towns[town].region : 0;
}

bool WorldRoad(std::int64_t x, std::int64_t y, int tx, int ty)
{
    x += Floor(tx, 8);
    y += Floor(ty, 8);
    tx = Mod(tx, 8);
    ty = Mod(ty, 8);
    const int town = WorldTownIndex(x, y);
    if (town >= 0)
    {
        return tx == 3 || tx == 4 || ty == 3 || ty == 4;
    }
    for (const auto& road : roads)
    {
        const Point a = towns[road.from].Center(), b = towns[road.to].Center();
        const int ax = static_cast<int>(std::floor(a.x / 8)),
                  ay = static_cast<int>(std::floor(a.y / 8));
        const int bx = static_cast<int>(std::floor(b.x / 8)),
                  by = static_cast<int>(std::floor(b.y / 8));
        if (ay == by && y == ay && x >= std::min(ax, bx) && x <= std::max(ax, bx) && ty >= 3 &&
            ty <= 5)
        {
            return true;
        }
        if (ax == bx && x == ax && y >= std::min(ay, by) && y <= std::max(ay, by) && tx >= 3 &&
            tx <= 5)
        {
            return true;
        }
    }
    return false;
}
