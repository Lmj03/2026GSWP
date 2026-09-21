#pragma once
#include "RenderTypes.h"
#include <string>
#include <cstdint>

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
