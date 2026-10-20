#include "stdafx.h"
#include "CityScene.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <iostream>

namespace
{

float Separation(Point a, Point b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

}

bool CityScene::LoadLevelOne()
{
    std::string source;
    if (!ReadResource(L"Data/level1.level", source))
    {
        return false;
    }
    Point supply;
    bool version = false;
    bool hasSupply = false;
    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line))
    {
        std::istringstream input(line.substr(0, line.find('#')));
        std::string type;
        std::string trailing;
        if (!(input >> type))
        {
            continue;
        }
        bool valid = false;
        if (type == "version")
        {
            int number = 0;
            valid = bool(input >> number) && number == 2 && !version;
            version = true;
        }
        else if (type == "supply")
        {
            valid = bool(input >> supply.x >> supply.y) && !hasSupply &&
                    CanStand(supply.x, supply.y, .35f);
            hasSupply = true;
        }
        if (!valid || (input >> trailing))
        {
            std::cerr << "Invalid level1.level record: " << line << '\n';
            return false;
        }
    }
    if (!version || !hasSupply)
    {
        return false;
    }
    m_supply = supply;
    SetEnemies({});
    return true;
}

int CityScene::NearbyLevelInteraction() const
{
    auto reachable = [&](Point world)
    {
        Point local;
        return LevelLocal(world, local) && Separation(local, m_player) < 1.5f &&
               ClearReach(m_player, local);
    };
    if (!m_supplyOpened && reachable(m_supply))
    {
        return -2;
    }
    return reachable(m_spawn) ? -3 : -1;
}

bool CityScene::InteractLevelOne()
{
    const int interaction = NearbyLevelInteraction();
    if (interaction == -2)
    {
        m_supplyOpened = true;
        m_bag.scrap += 2;
        m_credits += 5;
        ShowNotice(m_text.Get("level.supply_opened"));
        return true;
    }
    if (interaction == -3)
    {
        OpenBase();
        return true;
    }
    return false;
}

bool CityScene::LevelObjective(Point& target) const
{
    if (!m_supplyOpened)
    {
        return LevelLocal(m_supply, target);
    }
    if (m_tutorialDefeats >= 3 && m_tutorialLoot >= 3)
    {
        return LevelLocal(m_spawn, target);
    }
    float closest = std::numeric_limits<float>::max();
    bool found = false;
    for (const EnemyActor& enemy : m_enemies)
    {
        if (!enemy.tutorial || !enemy.EnabledInHierarchy() || (enemy.health <= 0 && !enemy.loot))
        {
            continue;
        }
        Point local;
        if (EnemyLocal(enemy, enemy.position, local))
        {
            const float distance = Separation(local, m_player);
            if (distance < closest)
            {
                closest = distance;
                target = local;
                found = true;
            }
        }
    }
    return found;
}
