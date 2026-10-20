#pragma once
#include "RenderTypes.h"
#include <string>
#include <cstdint>
#include <array>

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
    bool tutorial = true;
    bool streamed = false;
    std::int64_t chunkX = 0;
    std::int64_t chunkY = 0;
    unsigned int generation = 0;
};

struct Building
{
    std::string id, sign;
    float x, y, w, d, height;
    int palette;
};

enum class Occupation
{
    Resident,
    Merchant,
    Raider,
    Worker,
    Medic,
    Mechanic,
    Guard
};
enum class NpcActivity
{
    Idle,
    Trading,
    Working,
    Delivering,
    Patrolling,
    Extorting,
    Fleeing,
    Attacking,
    Defeated,
    Restitution,
    Travelling
};

struct NpcMemory
{
    int notoriety = 0, trust = 0, health = 36;
    bool helped = false, apology = false, looted = false;
    int crimes = 0, services = 0;
};

struct Citizen
{
    std::string id, name;
    Point position, home;
    int role; // 0: resident, 1: merchant, 2: lookout
    bool remembersHelp = false;
    std::string dialogueNeutral, dialogueRemembered;
    Occupation occupation = Occupation::Resident;
    NpcActivity activity = NpcActivity::Idle;
    NpcMemory memory;
    float actionTimer = 4, cooldown = 0, windup = 0, flash = 0, pickupDelay = 0;
    Point strike;
    bool hostile = false, active = true, story = false;
    int homeTown = -1;
    int destinationTown = -1;
    float travelWait = -1;
    bool travelling = false;
    std::array<Point, 8> route = {};
    int routeStep = 0;
    int routeCount = 0;
};
