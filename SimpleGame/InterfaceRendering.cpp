#include "stdafx.h"
#include "CityScene.h"
#include "WorldLayout.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace
{
const Color Cyan(.18f, .94f, .91f);
const Color Pink(1.f, .2f, .54f);
const Color Amber(1.f, .66f, .24f);
const Color White(.83f, .94f, .98f);
const Color Muted(.39f, .55f, .65f);

Color Accent(int palette)
{
    return palette == 1 ? Pink : palette == 2 ? Amber : Cyan;
}

}

void CityScene::DrawMap(Renderer& r, bool expanded) const
{
    const float size = expanded ? 440.f : 152.f;
    const float left = expanded ? (Renderer::Width - size) * .5f : Renderer::Width - size - 36;
    const float top = expanded ? (Renderer::Height - size) * .5f - 10 : 36.f;
    r.Rect(left - 12,
           top - 12,
           size + 24,
           size + (expanded ? 72 : 48),
           Color(.012f, .024f, .04f, .95f));
    r.Line({left - 12, top - 12}, {left + size + 12, top - 12}, 2, Cyan.Alpha(.6f));
    const float radius = expanded ? 180.f : 20.f;
    Point center = m_player;
    if (expanded && !WorldTowns().empty())
    {
        float x0 = 100000, y0 = 100000, x1 = -100000, y1 = -100000;
        for (const auto& town : WorldTowns())
        {
            x0 = std::min(x0, town.x * 8.f);
            y0 = std::min(y0, town.y * 8.f);
            x1 = std::max(x1, (town.x + town.width) * 8.f);
            y1 = std::max(y1, (town.y + town.height) * 8.f);
        }
        TownScenePoint({(x0 + x1) * .5f, (y0 + y1) * .5f}, center);
    }
    const float minX = center.x - radius, minY = center.y - radius;
    const float unit = size / (radius * 2);
    auto mapRect = [&](float x, float y, float width, float height, Color color)
    {
        const float x0 = std::max(left, left + (x - minX) * unit);
        const float y0 = std::max(top, top + (y - minY) * unit);
        const float x1 = std::min(left + size, left + (x + width - minX) * unit);
        const float y1 = std::min(top + size, top + (y + height - minY) * unit);
        if (x1 > x0 && y1 > y0)
        {
            r.Rect(x0, y0, x1 - x0, y1 - y0, color);
        }
    };
    if (expanded)
    {
        for (const auto& road : WorldTownRoads())
        {
            Point a, b;
            if (TownScenePoint(WorldTowns()[static_cast<size_t>(road.from)].Center(), a) &&
                TownScenePoint(WorldTowns()[static_cast<size_t>(road.to)].Center(), b))
            {
                mapRect(std::min(a.x, b.x) - 1,
                        std::min(a.y, b.y) - 1,
                        std::abs(a.x - b.x) + 3,
                        std::abs(a.y - b.y) + 3,
                        Color(.32f, .37f, .39f));
            }
        }
        for (const auto& town : WorldTowns())
        {
            Point local;
            if (!TownScenePoint({town.x * 8.f, town.y * 8.f}, local))
            {
                continue;
            }
            mapRect(local.x,
                    local.y,
                    town.width * 8.f,
                    town.height * 8.f,
                    Accent(town.region % 3).Shade(.22f));
        }
    }
    for (const auto& chunk : m_chunks)
    {
        mapRect(static_cast<float>(chunk.first * ChunkSize),
                static_cast<float>(chunk.second * ChunkSize),
                7.94f,
                7.94f,
                WorldTownIndex(m_originX + chunk.first, m_originY + chunk.second) >= 0
                    ? Color(.065f, .105f, .14f)
                    : Color(.035f, .05f, .07f));
    }
    for (const Building& building : m_buildings)
    {
        mapRect(building.x,
                building.y,
                building.w,
                building.d,
                Accent(building.palette).Alpha(.5f));
    }
    for (const Citizen& npc : m_citizens)
    {
        if (!npc.active)
        {
            continue;
        }
        const Point marker(left + (npc.position.x - minX) * unit,
                           top + (npc.position.y - minY) * unit);
        if (marker.x > left + 3 && marker.x < left + size - 3 && marker.y > top + 3 &&
            marker.y < top + size - 3)
        {
            r.Ellipse(marker, expanded ? 3.f : 1.5f, expanded ? 3.f : 1.5f, Amber);
        }
    }
    DrawLevelMap(r, left, top, size, radius, center);
    if (expanded)
    {
        for (const auto& town : WorldTowns())
        {
            Point local;
            if (!TownScenePoint(town.Center(), local))
            {
                continue;
            }
            const float x = left + (local.x - minX) * unit;
            const float y = top + (local.y - minY) * unit;
            if (x > left + 25 && x < left + size - 25 && y > top + 12 && y < top + size - 12)
            {
                r.TextFitted(x - 38, y - 8, m_text.Get("town.name." + town.id), .64f, White, 76);
            }
        }
    }
    const Point marker(left + (m_player.x - minX) * unit, top + (m_player.y - minY) * unit);
    if (marker.x > left && marker.x < left + size && marker.y > top && marker.y < top + size)
    {
        r.Glow(marker, 12, 12, Cyan);
        r.Ellipse(marker, 3.5f, 3.5f, White);
    }
    r.Text(left,
           top + size + 10,
           m_text.Get(expanded ? "ui.map_expanded" : "ui.map"),
           1.03f,
           Muted);
    if (expanded)
    {
        r.Rect(left + 365, top + size + 6, 70, 32, Color(.08f, .2f, .24f));
        r.Text(left + 375, top + size + 8, m_text.Get("hud.close"), .85f, White);
    }
}

void CityScene::DrawInterface(Renderer& r) const
{
    DrawMap(r, false);
    DrawLevelHud(r);
    DrawStoryHud(r);
    const float right = Renderer::Width - 214;
    const int town = WorldTownIndex(m_originX, m_originY);
    const std::string district = m_text.Get(
        town >= 0 ? "town.name." + WorldTowns()[static_cast<size_t>(town)].id : "town.roads");
    r.TextFitted(right, 238, district, .94f, Cyan, 190);
    r.TextFitted(right,
                 267,
                 m_text.Format("hud.social", {{"rep", std::to_string(m_reputation)}}),
                 .82f,
                 White,
                 190);
    const int nearbyCitizenIndex = NearbyCitizen();
    if (nearbyCitizenIndex >= 0 && NearbyLevelInteraction() == -1)
    {
        const Citizen& npc = m_citizens[nearbyCitizenIndex];
        const Point p = Project(npc.position.x, npc.position.y, 76);
        const std::string label = m_text.Format("ui.interact", {{"name", npc.name}});
        const float width = std::min(300.f, r.MeasureText(label, .95f) + 20);
        r.Rect(p.x - width * .5f, p.y - 5, width, 30, Color(.01f, .025f, .04f, .9f));
        r.TextFitted(p.x - width * .5f + 10,
                     p.y,
                     label,
                     .95f,
                     npc.hostile ? Pink : Cyan,
                     width - 20);
    }
    if (m_noticeTime > 0)
    {
        const bool paged = m_dialoguePages.size() > 1;
        const float width = std::min(620.f, Renderer::Width - 48);
        const float height = r.WrappedHeight(m_notice, .96f, width - 32) + 24 + (paged ? 25 : 0);
        const float left = (Renderer::Width - width) * .5f;
        const float top = Renderer::Height - 24 - height;
        r.Rect(left, top, width, height, Color(.012f, .028f, .046f, .94f));
        r.TextWrapped(left + 16, top + 10, m_notice, .96f, White, width - 32);
        if (paged)
        {
            r.Text(left + 16,
                   top + height - 25,
                   std::to_string(m_dialoguePage + 1) + " / " +
                       std::to_string(m_dialoguePages.size()),
                   .82f,
                   Cyan);
        }
    }
    if (m_map)
    {
        DrawMap(r, true);
    }
}
