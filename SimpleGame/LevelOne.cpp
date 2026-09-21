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
        std::cerr << "Data/level1.level 파일을 읽을 수 없습니다.\n";
        return false;
    }

    std::vector<Enemy> enemies;
    std::set<std::string> ids;
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
            valid = bool(input >> number) && number == 1 && !version;
            version = true;
        }
        else if (type == "supply")
        {
            valid = bool(input >> supply.x >> supply.y) && !hasSupply &&
                    CanStand(supply.x, supply.y, .35f);
            hasSupply = true;
        }
        else if (type == "enemy")
        {
            Enemy enemy;
            valid = bool(input >> enemy.id >> enemy.home.x >> enemy.home.y >> enemy.maxHealth >>
                         enemy.experience) &&
                    enemy.maxHealth > 0 && enemy.maxHealth <= 10000 && enemy.experience > 0 &&
                    enemy.experience <= 10000 && ids.insert(enemy.id).second &&
                    CanStand(enemy.home.x, enemy.home.y, .25f) &&
                    Separation(enemy.home, m_spawn) >= 4.f;
            enemy.position = enemy.home;
            enemy.health = enemy.maxHealth;
            enemies.push_back(enemy);
        }
        if (!valid || (input >> trailing))
        {
            std::cerr << "레벨 1 데이터 형식 또는 배치 오류: " << line << '\n';
            return false;
        }
    }
    if (!version || !hasSupply || enemies.size() != 3)
    {
        std::cerr << "레벨 1에는 version 1, 보급 상자, 적 3명이 필요합니다.\n";
        return false;
    }
    m_supply = supply;
    SetEnemies(enemies);
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

Point CityScene::LevelObjective() const
{
    if (!m_supplyOpened)
    {
        return m_supply;
    }
    float closest = std::numeric_limits<float>::max();
    Point target = m_spawn;
    for (const Enemy& enemy : m_enemies)
    {
        if (enemy.lootedOnce && !m_levelComplete)
        {
            continue;
        }
        if (m_levelComplete && enemy.health <= 0 && !enemy.loot)
        {
            continue;
        }
        Point local;
        if (LevelLocal(enemy.position, local))
        {
            const float distance = Separation(local, m_player);
            if (distance < closest)
            {
                closest = distance;
                target = enemy.position;
            }
        }
    }
    return target;
}
