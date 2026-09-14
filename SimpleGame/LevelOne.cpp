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
const Color LevelCyan(.18f, .94f, .91f);
const Color LevelRed(1.f, .24f, .32f);
const Color LevelGold(1.f, .72f, .24f);
const Color LevelWhite(.83f, .94f, .98f);

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
    m_enemies = std::move(enemies);
    return true;
}

void CityScene::ResetLevelOne()
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

bool CityScene::LevelLocal(Point world, Point& local) const
{
    // The encounter keeps small authored coordinates, even after very long trips.
    // Check the origin before multiplication/conversion; never accumulate recenter offsets.
    if (m_originX < -8 || m_originX > 8 || m_originY < -8 || m_originY > 8)
    {
        return false;
    }
    local = {world.x - static_cast<float>(m_originX * ChunkSize),
             world.y - static_cast<float>(m_originY * ChunkSize)};
    return HasGround(local.x, local.y);
}

bool CityScene::ClearReach(Point from, Point to) const
{
    const int steps = std::max(1, static_cast<int>(std::ceil(Separation(from, to) / .12f)));
    for (int step = 0; step <= steps; ++step)
    {
        const float t = static_cast<float>(step) / steps;
        if (!CanStand(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, .08f))
        {
            return false;
        }
    }
    return true;
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
    for (Enemy& enemy : m_enemies)
    {
        Point local;
        if (enemy.health <= 0 || !LevelLocal(enemy.position, local))
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
    for (Enemy& enemy : m_enemies)
    {
        Point local;
        if (enemy.health <= 0 || !LevelLocal(enemy.position, local))
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

void CityScene::UpdateLevelOne(float dt)
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
    for (Enemy& enemy : m_enemies)
    {
        enemy.flash = std::max(0.f, enemy.flash - dt);
        enemy.cooldown = std::max(0.f, enemy.cooldown - dt);
        Point local;
        Point home;
        if (!LevelLocal(enemy.position, local) || !LevelLocal(enemy.home, home))
        {
            enemy.windup = 0;
            continue;
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
            continue;
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
            continue;
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
                        break;
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
    if (m_health > 0)
    {
        CollectNearbyLoot(dt);
    }
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

void CityScene::DrawLevelGround(Renderer& r) const
{
    auto marker = [&](Point world, Color color, const std::string& key)
    {
        Point local;
        if (!LevelLocal(world, local))
        {
            return;
        }
        const Point p = Project(local.x, local.y);
        if (p.x < -100 || p.x > Renderer::Width + 100 || p.y < -100 || p.y > Renderer::Height + 100)
        {
            return;
        }
        r.Glow(p, 24 * m_zoom, 12 * m_zoom, color);
        r.Ellipse(p, 15 * m_zoom, 7 * m_zoom, color.Alpha(.4f));
        r.Rect(p.x - 5 * m_zoom, p.y - 9 * m_zoom, 10 * m_zoom, 8 * m_zoom, color.Emissive(.6f));
        if (m_interface)
        {
            const std::string label = m_text.Get(key);
            r.Text(p.x - r.MeasureText(label, .9f) * .5f, p.y + 12, label, .9f, color);
        }
    };
    DrawBaseGround(r);
    if (!m_supplyOpened)
    {
        marker(m_supply, LevelGold, "level.supply_marker");
    }
    for (const Enemy& enemy : m_enemies)
    {
        if (enemy.loot)
        {
            marker(enemy.position, LevelGold, "level.loot_marker");
            Point local;
            if (LevelLocal(enemy.position, local))
            {
                const Point p = Project(local.x, local.y);
                r.Rect(p.x - 15, p.y - 8, 6, 6, LevelCyan.Emissive(.5f));
                r.Rect(p.x + 9, p.y - 8, 6, 6, Color(.8f, .45f, 1.f).Emissive(.5f));
            }
        }
        if (enemy.health > 0 && enemy.windup > 0)
        {
            Point local;
            if (LevelLocal(enemy.strike, local))
            {
                const Point p = Project(local.x, local.y);
                r.Ellipse(p,
                          32 * m_zoom,
                          16 * m_zoom,
                          LevelRed.Alpha(.22f + (.65f - enemy.windup) * .5f));
            }
        }
    }
}

void CityScene::DrawEnemy(Renderer& r, const Enemy& enemy) const
{
    Point local;
    if (enemy.health <= 0 || !LevelLocal(enemy.position, local))
    {
        return;
    }
    const Point p = Project(local.x, local.y);
    if (p.x < -50 || p.x > Renderer::Width + 50 || p.y < -50 || p.y > Renderer::Height + 50)
    {
        return;
    }
    DrawActor(r, local, 2, enemy.windup > 0 ? 0 : m_time * 4, false);
    const Color color = enemy.flash > 0 ? LevelWhite : LevelRed;
    r.Ellipse({p.x, p.y + 2}, 13 * m_zoom, 6 * m_zoom, color.Alpha(.4f));
    r.Rect(p.x - 23, p.y - 62 * m_zoom, 46, 5, Color(.06f, .02f, .03f));
    r.Rect(p.x - 23, p.y - 62 * m_zoom, 46.f * enemy.health / enemy.maxHealth, 5, color);
    if (m_interface)
    {
        const std::string label =
            m_text.Get(enemy.windup > 0 ? "level.enemy_windup" : "level.enemy");
        r.Text(p.x - r.MeasureText(label, .85f) * .5f, p.y - 82 * m_zoom, label, .85f, color);
    }
}

void CityScene::DrawAttack(Renderer& r) const
{
    const Point p = Project(m_player.x, m_player.y);
    if (m_invulnerable > 0)
    {
        r.Glow(p, 22 * m_zoom, 12 * m_zoom, LevelRed.Alpha(.5f));
    }
    if (m_health <= 0)
    {
        return;
    }
    const bool swinging = m_attackAnimation > 0;
    if ((swinging ? m_swingWeapon : m_weapon) == Weapon::Knife)
    {
        const Point direction = swinging ? m_attackVisualDirection : m_attackDirection;
        const float center = std::atan2(direction.y, direction.x);
        const float progress = swinging ? 1 - m_attackAnimation / .32f : .5f;
        const float angle = center - 1.04719755f + progress * 2.0943951f;
        auto point = [&](float radians, float radius)
        {
            return Project(m_player.x + std::cos(radians) * radius,
                           m_player.y + std::sin(radians) * radius,
                           22);
        };
        if (swinging)
        {
            // The visible sweep uses the same 2.1-tile, 120-degree sector as the hit test.
            for (int segment = 0; segment < 12; ++segment)
            {
                const float a = center - 1.04719755f + 2.0943951f * progress * segment / 12;
                const float b = center - 1.04719755f + 2.0943951f * progress * (segment + 1) / 12;
                r.Quad(point(a, 1.65f),
                       point(a, 2.1f),
                       point(b, 2.1f),
                       point(b, 1.65f),
                       LevelCyan.Alpha(.16f + segment * .025f).Emissive(.4f));
                r.Line(point(a, 2.1f), point(b, 2.1f), 2 * m_zoom, LevelWhite.Alpha(.65f));
            }
        }
        const Point hand = point(angle, .55f);
        const Point hilt = point(angle, .75f);
        const Point tip = point(angle, swinging ? 2.1f : 1.15f);
        r.Line({p.x, p.y - 22 * m_zoom}, hand, 4 * m_zoom, LevelCyan);
        r.Line(hand, hilt, 4 * m_zoom, Color(.2f, .23f, .3f));
        r.Line(hilt, tip, 3 * m_zoom, LevelWhite);
        r.Glow(tip, 7 * m_zoom, 5 * m_zoom, LevelCyan.Alpha(.35f));
        return;
    }
    if (!swinging)
    {
        return;
    }
    const float reach = .35f + .8f * std::sin(m_attackAnimation / .22f * 3.14159265f);
    const Point hand = Project(m_player.x + m_attackVisualDirection.x * reach,
                               m_player.y + m_attackVisualDirection.y * reach,
                               22);
    r.Line({p.x, p.y - 22 * m_zoom}, hand, 5 * m_zoom, LevelCyan);
    r.Ellipse(hand, 5 * m_zoom, 5 * m_zoom, LevelWhite);
    r.Glow(hand, 12 * m_zoom, 8 * m_zoom, LevelCyan.Alpha(.5f));
}

void CityScene::DrawLevelHud(Renderer& r) const
{
    const int defeated = static_cast<int>(std::count_if(m_enemies.begin(),
                                                        m_enemies.end(),
                                                        [](const Enemy& enemy)
                                                        {
                                                            return enemy.defeatedOnce;
                                                        }));
    const int looted = static_cast<int>(std::count_if(m_enemies.begin(),
                                                      m_enemies.end(),
                                                      [](const Enemy& enemy)
                                                      {
                                                          return enemy.lootedOnce;
                                                      }));
    const std::string objective = !m_supplyOpened    ? "level.objective_supply"
                                  : defeated < 3     ? "level.objective_fight"
                                  : looted < 3       ? "level.objective_loot"
                                  : !m_levelComplete ? "level.objective_return"
                                                     : "level.objective_done";
    r.Rect(32, 112, 352, 152, Color(.012f, .025f, .044f, .94f));
    r.Rect(32, 112, 3, 152, LevelGold);
    r.Text(49, 125, m_text.Get("level.title"), 1.19f, LevelGold);
    r.TextWrapped(49, 160, m_text.Get(objective), 1.f, LevelWhite, 318);
    r.Text(49,
           234,
           m_text.Format("level.progress",
                         {{"kills", std::to_string(defeated)}, {"loot", std::to_string(looted)}}),
           .96f,
           LevelCyan);

    const float left = Renderer::Width - 324;
    r.Rect(left, 318, 292, 224, Color(.012f, .025f, .044f, .94f));
    r.TextFitted(left + 14,
                 330,
                 m_text.Format("level.stats",
                               {{"level", std::to_string(m_level)}, {"weapon", WeaponName()}}),
                 1.03f,
                 LevelWhite,
                 264);
    r.TextFitted(left + 14,
                 360,
                 m_text.Format("level.attack", {{"damage", std::to_string(AttackDamage())}}),
                 .96f,
                 LevelGold,
                 264);
    r.TextFitted(
        left + 14,
        390,
        m_text.Format("level.health",
                      {{"hp", std::to_string(m_health)}, {"max", std::to_string(MaxHealth())}}),
        .96f,
        LevelWhite,
        264);
    r.Rect(left + 14, 416, 264, 5, LevelRed.Alpha(.2f));
    r.Rect(left + 14,
           416,
           264.f * static_cast<float>(m_health) / static_cast<float>(MaxHealth()),
           5,
           LevelRed);
    r.TextFitted(left + 14,
                 433,
                 m_text.Format("level.experience",
                               {{"xp", std::to_string(m_experience)},
                                {"next", std::to_string(RequiredExperience())}}),
                 .96f,
                 LevelCyan,
                 264);
    r.Rect(left + 14, 459, 264, 5, LevelCyan.Alpha(.2f));
    r.Rect(left + 14,
           459,
           264.f * static_cast<float>(m_experience) / static_cast<float>(RequiredExperience()),
           5,
           LevelCyan);
    r.TextFitted(left + 14,
                 480,
                 m_text.Format("level.inventory",
                               {{"credits", std::to_string(m_credits)},
                                {"scrap", std::to_string(m_bag.scrap)}}),
                 .93f,
                 LevelGold,
                 264);
    r.TextFitted(left + 14,
                 510,
                 m_text.Format("level.recovery", {{"count", std::to_string(m_recovery)}}),
                 .93f,
                 LevelWhite,
                 264);

    if (m_combatNoticeTime > 0)
    {
        r.TextWrapped(420, 94, m_combatNotice, 1.02f, LevelGold, 510);
    }
    if (m_pickupNoticeTime > 0)
    {
        const float top =
            m_combatNoticeTime > 0 ? 106 + r.WrappedHeight(m_combatNotice, 1.02f, 510) : 94;
        r.TextWrapped(420, top, m_pickupNotice, .96f, LevelCyan, 510);
    }
    Point target;
    if (LevelLocal(LevelObjective(), target))
    {
        Point p = Project(target.x, target.y);
        p.x = std::max(408.f, std::min(922.f, p.x));
        p.y = std::max(242.f, std::min(560.f, p.y));
        r.Triangle({p.x, p.y + 7}, {p.x - 6, p.y - 5}, {p.x + 6, p.y - 5}, LevelGold);
        r.Text(p.x - 18, p.y - 26, m_text.Get("level.target"), .9f, LevelGold);
    }
    else
    {
        r.TextWrapped(420, 242, m_text.Get("level.return_district"), 1.f, LevelGold, 500);
    }
}

void CityScene::DrawLevelMap(Renderer& r, float left, float top, float size, float radius) const
{
    auto marker = [&](Point world, Color color)
    {
        Point local;
        if (!LevelLocal(world, local))
        {
            return;
        }
        const Point p(left + size * .5f + (local.x - m_player.x) * size / (2 * radius),
                      top + size * .5f + (local.y - m_player.y) * size / (2 * radius));
        if (p.x > left + 4 && p.x < left + size - 4 && p.y > top + 4 && p.y < top + size - 4)
        {
            r.Ellipse(p, 3, 3, color);
        }
    };
    marker(m_spawn, LevelCyan);
    if (!m_supplyOpened)
    {
        marker(m_supply, LevelGold);
    }
    for (const Enemy& enemy : m_enemies)
    {
        if (enemy.health > 0 || enemy.loot)
        {
            marker(enemy.position, enemy.loot ? LevelGold : LevelRed);
        }
    }
}
