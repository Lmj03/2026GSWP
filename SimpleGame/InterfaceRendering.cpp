#include "stdafx.h"
#include "CityScene.h"
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
    const float left = expanded ? (Renderer::Width - size) * .5f : Renderer::Width - size - 56;
    const float top = expanded ? (Renderer::Height - size) * .5f - 10 : 113.f;
    r.Rect(left - 12,
           top - 12,
           size + 24,
           size + (expanded ? 72 : 48),
           Color(.012f, .024f, .04f, .95f));
    r.Line({left - 12, top - 12}, {left + size + 12, top - 12}, 2, Cyan.Alpha(.6f));
    const float radius = expanded ? 36.f : 20.f;
    const float minX = m_player.x - radius, minY = m_player.y - radius;
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
    for (const auto& chunk : m_chunks)
    {
        mapRect(static_cast<float>(chunk.first * ChunkSize),
                static_cast<float>(chunk.second * ChunkSize),
                7.94f,
                7.94f,
                Color(.065f, .105f, .14f));
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
        const Point marker(left + (npc.position.x - minX) * unit,
                           top + (npc.position.y - minY) * unit);
        if (marker.x > left + 3 && marker.x < left + size - 3 && marker.y > top + 3 &&
            marker.y < top + size - 3)
        {
            r.Ellipse(marker, expanded ? 3.f : 1.5f, expanded ? 3.f : 1.5f, Amber);
        }
    }
    DrawLevelMap(r, left, top, size, radius);
    const Point marker(left + size * .5f, top + size * .5f);
    r.Glow(marker, 12, 12, Cyan);
    r.Ellipse(marker, 3.5f, 3.5f, White);
    r.Text(left,
           top + size + 10,
           m_text.Get(expanded ? "ui.map_expanded" : "ui.map"),
           1.03f,
           Muted);
    if (expanded)
    {
        r.Text(left, top + size + 35, m_text.Get("ui.map_streaming"), .92f, Muted);
    }
}

void CityScene::DrawInterface(Renderer& r) const
{
    const bool authored =
        m_originX >= -256 && m_originX <= 256 && m_originY >= -256 && m_originY <= 256 &&
        m_authoredChunks.count({static_cast<int>(m_originX), static_cast<int>(m_originY)});
    r.Rect(0, 0, Renderer::Width, 86, Color(.013f, .025f, .043f, .94f));
    r.Rect(0, 84, Renderer::Width, 1, Color(.2f, .6f, .65f, .35f));
    r.Rect(32, 25, 4, 34, Cyan);
    r.Text(49, 19, m_text.Get("ui.title"), 2.f, White);
    r.Text(50, 56, m_text.Get("ui.subtitle"), .89f, Muted);
    r.Text(Renderer::Width * .5f - 127,
           21,
           m_text.Get(authored ? "ui.district" : "ui.outskirts"),
           1.46f,
           Cyan);
    r.Text(Renderer::Width * .5f - 127, 52, m_text.Get("ui.authority"), .97f, Muted);
    r.Text(Renderer::Width - 264, 23, m_text.Get("ui.online"), 1.08f, Cyan);
    r.Text(Renderer::Width - 264, 50, m_text.Get("ui.curfew"), .97f, Amber);
    DrawMap(r, false);

    DrawLevelHud(r);

    const auto& post = r.PostSettings();
    auto state = [&](bool enabled)
    {
        return m_text.Get(enabled ? "ui.post_on" : "ui.post_off");
    };
    std::ostringstream exposure;
    exposure << std::fixed << std::setprecision(2) << post.exposure;
    r.Rect(32, 280, 352, 146, Color(.012f, .025f, .044f, .9f));
    r.Rect(32, 280, 3, 146, Cyan);
    r.Text(49,
           293,
           m_text.Get(r.PostAvailable() ? "ui.post_title" : "ui.post_unavailable"),
           1.08f,
           Cyan);
    r.Text(49,
           322,
           m_text.Format(
               "ui.post_lights",
               {{"all", state(post.enabled && r.PostAvailable())}, {"bloom", state(post.bloom)}}),
           .94f,
           White);
    r.Text(49,
           347,
           m_text.Format("ui.post_edges",
                         {{"vignette", state(post.vignette)}, {"blur", state(post.edgeBlur)}}),
           .94f,
           White);
    r.Text(49, 372, m_text.Format("ui.post_exposure", {{"value", exposure.str()}}), .94f, Muted);
    r.Text(49, 397, m_text.Get("ui.post_reload"), .92f, Muted);

    const int nearbyCitizenIndex = NearbyCitizen();
    if (nearbyCitizenIndex >= 0 && NearbyLevelInteraction() == -1)
    {
        const Citizen& npc = m_citizens[nearbyCitizenIndex];
        const Point p = Project(npc.position.x, npc.position.y, 61);
        const std::string label = m_text.Format("ui.interact", {{"name", npc.name}});
        const float width = r.MeasureText(label, 1.f) + 14;
        const float labelScale = std::min(1.08f, (width - 6) / std::max(1.f, width - 14));
        r.Rect(p.x - width * .5f, p.y - 5, width, 28, Color(.01f, .025f, .04f, .95f));
        r.Text(p.x - r.MeasureText(label, labelScale) * .5f,
               p.y,
               label,
               labelScale,
               npc.role == 2 ? Pink : Cyan);
    }
    if (m_noticeTime > 0)
    {
        const bool paged = m_dialoguePages.size() > 1;
        const float height = r.WrappedHeight(m_notice, 1.08f, 666) + 24 + (paged ? 25 : 0);
        // Keep the existing dialogue panel size; use its spare padding for larger text.
        float dialogueScale = 1.17f;
        const float textHeight = height - 12 - (paged ? 25 : 0);
        while (dialogueScale > 1.08f && r.WrappedHeight(m_notice, dialogueScale, 666) > textHeight)
        {
            dialogueScale = std::max(1.08f, dialogueScale - .01f);
        }
        const float top = Renderer::Height - 110 - height;
        const float left = (Renderer::Width - 700) * .5f;
        r.Rect(left, top, 700, height, Color(.012f, .028f, .046f, .97f));
        r.Rect(left, top, 3, height, Cyan);
        r.TextWrapped(left + 17, top + 6, m_notice, dialogueScale, White, 666);
        if (paged)
        {
            const std::string hint =
                m_text.Get(m_dialoguePage + 1 < m_dialoguePages.size() ? "ui.next_dialogue"
                                                                       : "ui.close_dialogue");
            r.Text(left + 17,
                   Renderer::Height - 135,
                   hint + "  " + std::to_string(m_dialoguePage + 1) + " / " +
                       std::to_string(m_dialoguePages.size()),
                   .92f,
                   Cyan);
        }
    }
    const float footerTop = Renderer::Height - 92;
    r.Rect(0, footerTop, Renderer::Width, 92, Color(.012f, .024f, .042f, .96f));
    r.Line({0, footerTop}, {Renderer::Width, footerTop}, 1, Color(.18f, .48f, .55f, .4f));
    r.TextFitted(32,
                 footerTop + 17,
                 m_text.Get(m_helped ? "ui.player_known" : "ui.player"),
                 1.03f,
                 White,
                 280);
    r.TextFitted(
        32,
        footerTop + 46,
        m_text.Format("ui.medkit",
                      {{"value", m_helped ? "0" : "1"}, {"rep", std::to_string(m_reputation)}}),
        .95f,
        Muted,
        280);
    r.TextFitted(340, footerTop + 17, m_text.Get("ui.controls_move"), 1.03f, Cyan, 570);
    r.TextFitted(340, footerTop + 46, m_text.Get("ui.controls_view"), .95f, Muted, 570);
    const std::string coordinate =
        m_text.Format("ui.chunk",
                      {{"x", std::to_string(m_originX)}, {"y", std::to_string(m_originY)}});
    const float width = r.MeasureText(coordinate, 1.03f);
    const float scale = width > 300 ? 1.03f * 300 / width : 1.03f;
    r.Text(Renderer::Width - 335, footerTop + 17, coordinate, scale, White);
    r.TextFitted(Renderer::Width - 335,
                 footerTop + 46,
                 m_text.Get("ui.controls_exit"),
                 .95f,
                 Muted,
                 300);
    if (m_map)
    {
        DrawMap(r, true);
    }
}
