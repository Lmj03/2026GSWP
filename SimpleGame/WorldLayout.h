#pragma once
#include <cstdint>
#include "RenderTypes.h"
#include <string>
#include <vector>

struct TownDefinition
{
    std::string id;
    int x = 0;
    int y = 0;
    int width = 7;
    int height = 7;
    int region = 0;
    int security = 50;
    int population = 70;
    int hostiles = 0;
    Point Center() const;
};

struct TownRoad
{
    int from = 0;
    int to = 0;
};

bool LoadWorldLayout(std::string& error);
const std::vector<TownDefinition>& WorldTowns();
const std::vector<TownRoad>& WorldTownRoads();
int WorldTownIndex(std::int64_t chunkX, std::int64_t chunkY);
int WorldTownAt(Point native);

std::uint64_t WorldHash(std::int64_t x, std::int64_t y, std::uint64_t salt = 0);
int WorldRegion(std::int64_t chunkX, std::int64_t chunkY);
bool WorldRoad(std::int64_t chunkX, std::int64_t chunkY, int tileX, int tileY);
