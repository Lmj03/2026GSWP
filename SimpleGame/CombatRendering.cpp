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

}

void CityScene::DrawMarker(Renderer& r, Point world, Color color, const std::string& key) const
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
}

void CityScene::DrawSupply(Renderer& r) const
{
    if (!m_supplyOpened)
    {
        DrawMarker(r, m_supply, LevelGold, "level.supply_marker");
    }
}

void CityScene::DrawLoot(Renderer& r, const Enemy& enemy) const
{
    if (!enemy.loot)
    {
        return;
    }

    DrawMarker(r, enemy.position, LevelGold, "level.loot_marker");
    Point local;
    if (LevelLocal(enemy.position, local))
    {
        const Point p = Project(local.x, local.y);
        r.Rect(p.x - 15, p.y - 8, 6, 6, LevelCyan.Emissive(.5f));
        r.Rect(p.x + 9, p.y - 8, 6, 6, Color(.8f, .45f, 1.f).Emissive(.5f));
    }
}

void CityScene::DrawWarning(Renderer& r, const Enemy& enemy) const
{
    if (enemy.health <= 0 || enemy.windup <= 0)
    {
        return;
    }

    Point local;
    if (LevelLocal(enemy.strike, local))
    {
        const Point p = Project(local.x, local.y);
        r.Ellipse(p, 32 * m_zoom, 16 * m_zoom, LevelRed.Alpha(.22f + (.65f - enemy.windup) * .5f));
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
