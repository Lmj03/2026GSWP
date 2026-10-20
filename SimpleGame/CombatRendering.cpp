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
    if (m_interface && std::hypot(local.x - m_player.x, local.y - m_player.y) < 2.5f)
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

    Point local;
    if (EnemyLocal(enemy, enemy.position, local))
    {
        const Point p = Project(local.x, local.y);
        r.Glow(p, 20 * m_zoom, 10 * m_zoom, LevelGold);
        r.Rect(p.x - 5, p.y - 9, 10, 8, LevelGold.Emissive(.6f));
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
    if (EnemyLocal(enemy, enemy.strike, local))
    {
        const Point p = Project(local.x, local.y);
        r.Ellipse(p, 32 * m_zoom, 16 * m_zoom, LevelRed.Alpha(.22f + (.65f - enemy.windup) * .5f));
    }
}

void CityScene::DrawEnemy(Renderer& r, const Enemy& enemy) const
{
    if (!enemy.tutorial && enemy.health <= 0 && m_story.stage == StoryStage::Sentence)
    {
        Point down;
        if (EnemyLocal(enemy, enemy.position, down))
        {
            const Point p = Project(down.x, down.y);
            r.Ellipse(p, 12 * m_zoom, 5 * m_zoom, Color(.2f, .18f, .23f));
        }
        return;
    }
    Point local;
    if (enemy.health <= 0 || !EnemyLocal(enemy, enemy.position, local))
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
    if (!m_interface || std::hypot(local.x - m_player.x, local.y - m_player.y) > 4.5f)
    {
        return;
    }
    r.Rect(p.x - 23, p.y - 62 * m_zoom, 46, 5, Color(.06f, .02f, .03f));
    r.Rect(p.x - 23, p.y - 62 * m_zoom, 46.f * enemy.health / enemy.maxHealth, 5, color);
    if (m_interface)
    {
        const std::string label = m_text.Get(!enemy.tutorial    ? "story.name.heir"
                                             : enemy.windup > 0 ? "level.enemy_windup"
                                                                : "level.enemy");
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
    const float left = 24;
    const float top = Renderer::Height - 186;
    r.Rect(left, top, 330, 162, Color(.012f, .025f, .044f, .82f));
    r.TextFitted(left + 16,
                 top + 12,
                 m_text.Format("level.stats",
                               {{"level", std::to_string(m_level)}, {"weapon", WeaponName()}}),
                 1.03f,
                 LevelWhite,
                 298);
    r.TextFitted(
        left + 16,
        top + 43,
        m_text.Format("level.health",
                      {{"hp", std::to_string(m_health)}, {"max", std::to_string(MaxHealth())}}),
        .92f,
        LevelWhite,
        298);
    r.Rect(left + 16, top + 70, 298, 6, LevelRed.Alpha(.22f));
    r.Rect(left + 16,
           top + 70,
           298.f * static_cast<float>(m_health) / static_cast<float>(MaxHealth()),
           6,
           LevelRed);
    r.TextFitted(left + 16,
                 top + 87,
                 m_text.Format("level.experience",
                               {{"xp", std::to_string(m_experience)},
                                {"next", std::to_string(RequiredExperience())}}),
                 .83f,
                 LevelCyan,
                 298);
    r.Rect(left + 16, top + 111, 298, 4, LevelCyan.Alpha(.2f));
    r.Rect(left + 16,
           top + 111,
           298.f * static_cast<float>(m_experience) / static_cast<float>(RequiredExperience()),
           4,
           LevelCyan);
    r.TextFitted(left + 16,
                 top + 129,
                 m_text.Format("hud.credits", {{"credits", std::to_string(m_credits)}}),
                 .86f,
                 LevelGold,
                 152);
    r.TextFitted(left + 180,
                 top + 129,
                 m_text.Format("level.recovery", {{"count", std::to_string(m_recovery)}}),
                 .86f,
                 LevelCyan,
                 130);
    const float noticeWidth = std::min(540.f, Renderer::Width - 410);
    const float noticeLeft = (Renderer::Width - noticeWidth) * .5f;
    float noticeTop = Renderer::Height - 220;
    if (m_combatNoticeTime > 0)
    {
        noticeTop -= r.WrappedHeight(m_combatNotice, .93f, noticeWidth);
        r.TextWrapped(noticeLeft, noticeTop, m_combatNotice, .93f, LevelGold, noticeWidth);
        noticeTop -= 8;
    }
    if (m_pickupNoticeTime > 0)
    {
        noticeTop -= r.WrappedHeight(m_pickupNotice, .86f, noticeWidth);
        r.TextWrapped(noticeLeft, noticeTop, m_pickupNotice, .86f, LevelCyan, noticeWidth);
    }
}

void CityScene::DrawLevelMap(Renderer& r,
                             float left,
                             float top,
                             float size,
                             float radius,
                             Point center) const
{
    auto marker = [&](Point local, Color color)
    {
        const Point p(left + size * .5f + (local.x - center.x) * size / (2 * radius),
                      top + size * .5f + (local.y - center.y) * size / (2 * radius));
        if (p.x > left + 4 && p.x < left + size - 4 && p.y > top + 4 && p.y < top + size - 4)
        {
            r.Ellipse(p, 3, 3, color);
        }
    };
    Point local;
    if (LevelLocal(m_spawn, local))
    {
        marker(local, LevelCyan);
    }
    if (!m_supplyOpened && LevelLocal(m_supply, local))
    {
        marker(local, LevelGold);
    }
    for (const EnemyActor& enemy : m_enemies)
    {
        if (!enemy.EnabledInHierarchy())
        {
            continue;
        }
        if (enemy.health > 0 || enemy.loot)
        {
            if (EnemyLocal(enemy, enemy.position, local))
            {
                marker(local, enemy.loot ? LevelGold : LevelRed);
            }
        }
    }
}
