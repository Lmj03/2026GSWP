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

Point Direction(Point from, Point to)
{
    const float distance = Separation(from, to);
    return distance > .001f ? Point((to.x - from.x) / distance, (to.y - from.y) / distance)
                            : Point(1, 0);
}
}

void CityScene::Attack()
{
    if (m_health <= 0 || m_baseOpen || m_attackCooldown > 0 || m_map || m_dialoguePages.size() > 1)
    {
        return;
    }
    const bool knife = m_weapon == Weapon::Knife;
    m_swingWeapon = m_weapon;
    m_attackCooldown = knife ? .5f : .38f;
    m_attackAnimation = knife ? .32f : .22f;
    m_attackVisualDirection = m_attackDirection;
    Enemy* target = nullptr;
    float closest = knife ? 2.1f : 1.35f;
    for (EnemyActor& enemy : m_enemies)
    {
        Point local;
        if (!enemy.EnabledInHierarchy() || enemy.health <= 0 || !LevelLocal(enemy.position, local))
        {
            continue;
        }
        const float distance = Separation(m_player, local);
        const Point direction = Direction(m_player, local);
        const float facing = direction.x * m_attackDirection.x + direction.y * m_attackDirection.y;
        if (distance < closest && (facing > (knife ? .5f : .15f) || distance < .35f) &&
            ClearReach(m_player, local))
        {
            target = &enemy;
            closest = distance;
            m_attackVisualDirection = direction;
        }
    }
    // Snapshot damage before awarding XP; all hits in one swing use the same stats.
    const std::int64_t damage = AttackDamage();
    int experience = 0;
    for (EnemyActor& enemy : m_enemies)
    {
        Point local;
        if (!enemy.EnabledInHierarchy() || enemy.health <= 0 || !LevelLocal(enemy.position, local))
        {
            continue;
        }
        const Point direction = Direction(m_player, local);
        const float facing =
            direction.x * m_attackVisualDirection.x + direction.y * m_attackVisualDirection.y;
        const bool hit = knife ? Separation(m_player, local) < 2.1f && facing >= .5f &&
                                     ClearReach(m_player, local)
                               : &enemy == target;
        if (!hit)
        {
            continue;
        }
        enemy.health = static_cast<int>(std::max<std::int64_t>(0, enemy.health - damage));
        enemy.flash = .18f;
        if (enemy.health == 0)
        {
            enemy.windup = 0;
            enemy.loot = true;
            enemy.pickupDelay = .35f;
            enemy.drop = Materials{2, 1, 1};
            enemy.respawn = 25;
            enemy.defeatedOnce = true;
            experience += enemy.experience;
        }
    }
    if (experience > 0)
    {
        AddExperience(experience);
    }
}

void CityScene::UseRecovery()
{
    if (m_health <= 0 || m_baseOpen || m_map || m_dialoguePages.size() > 1)
    {
        return;
    }
    if (m_health >= MaxHealth() || m_recovery <= 0)
    {
        m_combatNotice = m_text.Get(m_recovery <= 0 ? "level.no_recovery" : "level.health_full");
    }
    else
    {
        const auto healed = std::min<std::int64_t>(35, MaxHealth() - m_health);
        --m_recovery;
        m_health += healed;
        m_combatNotice = m_text.Format("level.recovered", {{"amount", std::to_string(healed)}});
    }
    m_combatNoticeTime = 3;
}

void CityScene::RespawnPlayer()
{
    // Keep rewards, quest flags and uncollected drops. Only living enemies reset.
    m_originX = 0;
    m_originY = 0;
    m_citizens.clear();
    m_player = m_spawn;
    m_camera = m_spawn;
    m_health = MaxHealth();
    m_invulnerable = 2;
    m_attackAnimation = 0;
    m_attackCooldown = .5f;
    CloseBase();
    for (Enemy& enemy : m_enemies)
    {
        if (enemy.health > 0)
        {
            enemy.position = enemy.home;
            enemy.health = enemy.maxHealth;
            enemy.windup = 0;
            enemy.cooldown = 1;
        }
    }
    RefreshWorld();
    Recenter();
    ShowNotice(m_text.Get("level.respawned"));
}

void CityScene::UpdateEnemy(Enemy& enemy, float dt)
{
    enemy.flash = std::max(0.f, enemy.flash - dt);
    enemy.cooldown = std::max(0.f, enemy.cooldown - dt);
    Point local;
    Point home;
    if (!LevelLocal(enemy.position, local) || !LevelLocal(enemy.home, home))
    {
        enemy.windup = 0;
        return;
    }
    const float distance = Separation(local, m_player);
    if (enemy.health <= 0)
    {
        // Do not overwrite drops or create enemies on top of the player.
        if (m_levelComplete && !enemy.loot)
        {
            enemy.respawn = std::max(0.f, enemy.respawn - dt);
            if (enemy.respawn <= 0 && Separation(home, m_player) > 5)
            {
                enemy.health = enemy.maxHealth;
                enemy.position = enemy.home;
                enemy.cooldown = 1;
            }
        }
        return;
    }
    const bool pursue =
        distance < 4.2f && Separation(m_player, home) < 4.5f && ClearReach(local, m_player);
    if (!pursue)
    {
        enemy.windup = 0;
        const Point direction = Direction(local, home);
        const float step = std::min(Separation(local, home), dt * 1.3f);
        Point moved = local;
        Move(moved, direction.x * step, direction.y * step, .22f);
        enemy.position.x += moved.x - local.x;
        enemy.position.y += moved.y - local.y;
        if (Separation(enemy.position, enemy.home) < .15f)
        {
            enemy.health = enemy.maxHealth;
        }
        return;
    }
    if (enemy.windup > 0)
    {
        enemy.windup = std::max(0.f, enemy.windup - dt);
        if (enemy.windup == 0)
        {
            Point strike;
            if (LevelLocal(enemy.strike, strike) && Separation(m_player, strike) < .7f &&
                distance < 1.4f && ClearReach(local, m_player) && m_invulnerable <= 0)
            {
                m_health = std::max<std::int64_t>(0, m_health - 12);
                m_invulnerable = .7f;
                if (m_health == 0)
                {
                    m_respawnDelay = 2;
                    m_combatNotice = m_text.Get("level.defeated");
                    m_combatNoticeTime = 2;
                    return;
                }
            }
            enemy.cooldown = 1;
        }
    }
    else if (distance < 1.1f && enemy.cooldown <= 0)
    {
        // Lock the strike position now, so moving out of the warning really dodges it.
        const Point direction = Direction(local, m_player);
        enemy.strike = {enemy.position.x + direction.x * .75f,
                        enemy.position.y + direction.y * .75f};
        enemy.windup = .65f;
    }
    else if (distance > .8f)
    {
        const Point direction = Direction(local, m_player);
        const float step = std::min(distance - .8f, dt * 1.05f);
        Point moved = local;
        Move(moved, direction.x * step, direction.y * step, .22f);
        enemy.position.x += moved.x - local.x;
        enemy.position.y += moved.y - local.y;
    }
}

void CityScene::UpdatePlayerTimers(float dt)
{
    m_combatNoticeTime = std::max(0.f, m_combatNoticeTime - dt);
    m_pickupNoticeTime = std::max(0.f, m_pickupNoticeTime - dt);
    m_attackCooldown = std::max(0.f, m_attackCooldown - dt);
    m_attackAnimation = std::max(0.f, m_attackAnimation - dt);
    m_invulnerable = std::max(0.f, m_invulnerable - dt);
    if (m_health <= 0)
    {
        m_respawnDelay -= dt;
        if (m_respawnDelay <= 0)
        {
            RespawnPlayer();
        }
        return;
    }
}
