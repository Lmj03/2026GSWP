#include "stdafx.h"
#include "CityScene.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <iostream>

void CityScene::ResetProgression()
{
    m_level = 1;
    m_experience = 0;
    m_credits = 0;
    m_bag = Materials{};
    m_storage = Materials{};
    m_weapon = Weapon::Fists;
    m_swingWeapon = Weapon::Fists;
    m_knifeOwned = false;
    m_forgedKnife = false;
    m_vestCrafted = false;
    m_knifeUpgrade = 0;
    m_baseOpen = false;
    m_storagePage = false;
    m_restTime = 0;
    m_baseNotice.clear();
    m_pickupNotice.clear();
    m_pickupNoticeTime = 0;
    m_health = MaxHealth();
    m_recovery = 2;
    m_supplyOpened = false;
    m_levelComplete = false;
    m_attackDirection = {1, 0};
    m_attackVisualDirection = m_attackDirection;
    m_attackCooldown = 0;
    m_attackAnimation = 0;
    m_invulnerable = 0;
    m_respawnDelay = 0;
    m_combatNotice.clear();
    m_combatNoticeTime = 0;
    for (Enemy& enemy : m_enemies)
    {
        enemy.position = enemy.home;
        enemy.health = enemy.maxHealth;
        enemy.cooldown = 0;
        enemy.windup = 0;
        enemy.flash = 0;
        enemy.respawn = 0;
        enemy.loot = false;
        enemy.pickupDelay = 0;
        enemy.drop = Materials{};
        enemy.defeatedOnce = false;
        enemy.lootedOnce = false;
    }
}

std::uint64_t CityScene::RequiredExperience() const
{
    const auto level = static_cast<std::uint64_t>(m_level);
    return level * level * 4;
}

std::int64_t CityScene::MaxHealth() const
{
    return 100 + (static_cast<std::int64_t>(m_level) - 1) * 5 + (m_vestCrafted ? 30 : 0);
}

std::int64_t CityScene::BaseAttackDamage() const
{
    return 10 + (static_cast<std::int64_t>(m_level) - 1) * 2;
}

std::int64_t CityScene::AttackDamage() const
{
    return BaseAttackDamage() +
           (m_weapon == Weapon::Knife ? 12 + m_knifeUpgrade * 3 + (m_forgedKnife ? 8 : 0) : 0);
}

std::string CityScene::WeaponName() const
{
    if (m_weapon == Weapon::Fists)
    {
        return m_text.Get("weapon.fists");
    }
    return m_text.Format(m_forgedKnife ? "weapon.forged_knife" : "weapon.knife",
                         {{"upgrade", std::to_string(m_knifeUpgrade)}});
}

void CityScene::AddExperience(int amount)
{
    auto remaining = static_cast<std::uint64_t>(std::max(0, amount));
    bool leveled = false;
    while (remaining > 0 && m_level < std::numeric_limits<int>::max())
    {
        const auto needed = RequiredExperience() - m_experience;
        if (remaining < needed)
        {
            m_experience += remaining;
            break;
        }
        remaining -= needed;
        m_experience = 0;
        ++m_level;
        leveled = true;
    }
    if (leveled)
    {
        m_health = MaxHealth();
        m_combatNotice = m_text.Format("level.level_up", {{"level", std::to_string(m_level)}});
        if (m_level >= 5 && !m_knifeOwned)
        {
            m_knifeOwned = true;
            m_weapon = Weapon::Knife;
            m_combatNotice = m_text.Get("weapon.unlocked");
        }
    }
    else
    {
        m_combatNotice = m_text.Format("level.xp_gain", {{"xp", std::to_string(amount)}});
    }
    m_combatNoticeTime = 4;
}
