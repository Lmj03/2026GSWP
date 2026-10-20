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

int Mod(int n, int d)
{
    return (n % d + d) % d;
}

}

void CityScene::DrawTerrain(Renderer& r, Point origin) const
{
    for (int ix = 0; ix < ChunkSize; ++ix)
    {
        for (int iy = 0; iy < ChunkSize; ++iy)
        {
            const int x = static_cast<int>(origin.x) + ix, y = static_cast<int>(origin.y) + iy;
            const Point a = Project(float(x), float(y)), b = Project(x + 1.f, float(y));
            const Point c = Project(x + 1.f, y + 1.f), d = Project(float(x), y + 1.f);
            if (c.y < -80 || a.y > Renderer::Height + 80 || b.x < -100 ||
                d.x > Renderer::Width + 100)
            {
                continue;
            }
            const auto cx = m_originX + static_cast<int>(origin.x) / ChunkSize;
            const auto cy = m_originY + static_cast<int>(origin.y) / ChunkSize;
            const bool road = WorldRoad(cx, cy, ix, iy);
            const int region = WorldRegion(cx, cy);
            const bool inTown = WorldTownIndex(cx, cy) >= 0;
            const float variation = Mod(x * 17 + y * 31 + static_cast<int>(m_originX % 7) * 8 * 17 +
                                            static_cast<int>(m_originY % 7) * 8 * 31,
                                        7) *
                                    .003f;
            const Color tile = road ? Color(.037f + variation, .061f + variation, .094f + variation)
                               : !inTown
                                   ? Color(.025f + variation, .038f + variation, .052f + variation)
                                   : Color(.06f + region * .006f + variation,
                                           .09f + region * .008f + variation,
                                           .13f + variation);
            r.Quad(a, b, c, d, tile);
            r.Line(a, b, .65f, Color(.19f, .32f, .38f, road ? .12f : .35f));
            r.Line(a, d, .65f, Color(.19f, .32f, .38f, road ? .12f : .35f));
            if (road && !inTown && Mod(x + y, 4) == 0)
            {
                const Point p = Project(x + .4f, y + .4f);
                const bool horizontal = WorldRoad(cx, cy, ix + 1, iy);
                r.Line(p,
                       horizontal ? Project(x + .8f, y + .4f) : Project(x + .4f, y + .8f),
                       2 * m_zoom,
                       Amber.Alpha(.6f));
            }
            if (road && Mod(x + y, 3) == 0)
            {
                const Point p = Project(x + .5f, y + .5f);
                r.Ellipse(p, 19 * m_zoom, 5 * m_zoom, Color(.16f, .28f, .38f, .18f));
                r.Line({p.x - 11 * m_zoom, p.y},
                       {p.x + 12 * m_zoom, p.y - 1},
                       1,
                       Color(.3f, .55f, .66f, .15f));
            }
            if (road && !WorldRoad(cx, cy, ix - 1, iy))
            {
                r.Line(Project(float(x), y + .18f),
                       Project(float(x), y + .68f),
                       1.6f * m_zoom,
                       Amber.Alpha(.45f));
            }
            if (road && !WorldRoad(cx, cy, ix, iy - 1))
            {
                r.Line(Project(x + .18f, float(y)),
                       Project(x + .68f, float(y)),
                       1.6f * m_zoom,
                       Amber.Alpha(.45f));
            }
            if (!road && WorldHash(cx, cy, ix * 13 + iy) % 23 == 0)
            {
                r.Line(b, c, 2 * m_zoom, Cyan.Alpha(.12f));
            }
        }
    }
}

void CityScene::DrawReflection(Renderer& r, const Building& b) const
{
    const Color glow = Accent(b.palette);
    const Point p = Project(b.x + b.w * .5f, b.y + b.d + .45f);
    if (p.x < -200 || p.x > Renderer::Width + 200 || p.y < -100 || p.y > Renderer::Height + 200)
    {
        return;
    }
    r.Glow(p, 85 * m_zoom, 28 * m_zoom, glow.Alpha(.75f));
    for (int i = 0; i < 9; ++i)
    {
        const float offset = static_cast<float>(i);
        const float width = (35 - offset * 2.5f) * m_zoom;
        r.Line({p.x - width + offset * 3, p.y + offset * 5},
               {p.x + width + offset * 3, p.y + offset * 5},
               (3 - offset * .2f) * m_zoom,
               glow.Alpha(.17f - offset * .014f));
    }
}

void CityScene::DrawBuilding(Renderer& r, const Building& b) const
{
    const Point a = Project(b.x, b.y), right = Project(b.x + b.w, b.y);
    const Point front = Project(b.x + b.w, b.y + b.d), left = Project(b.x, b.y + b.d);
    const float h = b.height * m_zoom;
    if (right.x < -180 || left.x > Renderer::Width + 180 || front.y < -100 ||
        a.y - h > Renderer::Height + 100)
    {
        return;
    }
    const Point player = Project(m_player.x, m_player.y, 18);
    const bool covers = player.x > left.x - 10 && player.x < right.x + 10 &&
                        player.y > a.y - h - 10 && player.y < front.y + 8 &&
                        m_player.x + m_player.y < b.x + b.y + b.w + b.d;
    const float opacity = covers ? .23f : 1.f;
    const Color neon = Accent(b.palette).Alpha(opacity).Emissive(3.f);
    const Point at(a.x, a.y - h), bt(right.x, right.y - h);
    const Point ct(front.x, front.y - h), dt(left.x, left.y - h);
    const Color roof(.115f, .17f, .23f, opacity);
    const Color wallLeft(.066f, .087f, .145f, opacity), wallRight(.045f, .066f, .108f, opacity);

    r.Quad(right, front, ct, bt, wallRight);
    r.Quad(front, left, dt, ct, wallLeft);
    r.Quad(at, bt, ct, dt, roof);
    r.Line(at, bt, 2 * m_zoom, neon.Shade(.55f));
    r.Line(at, dt, 2 * m_zoom, Color(.32f, .42f, .48f, opacity));
    r.Line(bt, ct, 2 * m_zoom, neon);
    r.Line(dt, ct, 2 * m_zoom, neon);
    r.Line(ct, front, 1.5f * m_zoom, neon.Shade(.5f));

    // Windows lie on each isometric facade.
    for (int row = 0; row < static_cast<int>((b.height - 35) / 25); ++row)
    {
        const float z = 32.f + row * 25;
        for (int col = 0; col < static_cast<int>(b.w * 2) - 1; ++col)
        {
            const float x = b.x + .35f + col * .5f;
            const bool lit = Mod(col * 7 + row * 3 + b.palette, 5) != 0;
            const Color light = lit ? neon.Alpha(opacity * .58f) : Color(.12f, .17f, .23f, opacity);
            r.Quad(Project(x, b.y + b.d, z),
                   Project(x + .24f, b.y + b.d, z),
                   Project(x + .24f, b.y + b.d, z + 11),
                   Project(x, b.y + b.d, z + 11),
                   light);
        }
        for (int col = 0; col < static_cast<int>(b.d * 2) - 1; ++col)
        {
            const float y = b.y + .35f + col * .5f;
            const Color light = Mod(col + row, 3) == 0 ? Amber.Alpha(opacity * .48f)
                                                       : Color(.09f, .21f, .28f, opacity);
            r.Quad(Project(b.x + b.w, y, z),
                   Project(b.x + b.w, y + .24f, z),
                   Project(b.x + b.w, y + .24f, z + 11),
                   Project(b.x + b.w, y, z + 11),
                   light);
        }
    }
    // Storefront shutter, awning and rooftop utility unit.
    r.Quad(Project(b.x + .35f, b.y + b.d, 0),
           Project(b.x + 1.05f, b.y + b.d, 0),
           Project(b.x + 1.05f, b.y + b.d, 26),
           Project(b.x + .35f, b.y + b.d, 26),
           Color(.023f, .033f, .053f, opacity));
    for (int i = 0; i < 5; ++i)
    {
        r.Line(Project(b.x + .36f, b.y + b.d, 4.f + i * 4),
               Project(b.x + 1.04f, b.y + b.d, 4.f + i * 4),
               1,
               Muted.Alpha(opacity * .35f));
    }
    r.Quad(Project(b.x, b.y + b.d, 31),
           Project(b.x + b.w, b.y + b.d, 31),
           Project(b.x + b.w, b.y + b.d + .3f, 25),
           Project(b.x, b.y + b.d + .3f, 25),
           neon.Shade(.34f));
    r.Line(Project(b.x, b.y + b.d + .3f, 25),
           Project(b.x + b.w, b.y + b.d + .3f, 25),
           2 * m_zoom,
           neon);
    const Point unit = Project(b.x + b.w * .45f, b.y + b.d * .4f, b.height + 3);
    r.Quad({unit.x - 16 * m_zoom, unit.y},
           {unit.x, unit.y - 8 * m_zoom},
           {unit.x + 23 * m_zoom, unit.y + 3 * m_zoom},
           {unit.x + 7 * m_zoom, unit.y + 11 * m_zoom},
           Color(.18f, .24f, .29f, opacity));
    for (int i = 0; i < 4; ++i)
    {
        r.Line({unit.x - 9 * m_zoom + i * 5 * m_zoom, unit.y},
               {unit.x + 4 * m_zoom + i * 5 * m_zoom, unit.y + 6 * m_zoom},
               1,
               Color(.04f, .07f, .1f, opacity));
    }
    const Point antenna = Project(b.x + b.w * .75f, b.y + .45f, b.height);
    r.Line(antenna, {antenna.x, antenna.y - 26 * m_zoom}, 2, Muted.Alpha(opacity));
    r.Line({antenna.x - 9 * m_zoom, antenna.y - 18 * m_zoom},
           {antenna.x + 9 * m_zoom, antenna.y - 18 * m_zoom},
           1,
           Muted.Alpha(opacity));
    r.Glow({antenna.x, antenna.y - 27 * m_zoom}, 9, 9, Pink.Alpha(opacity));
    r.Rect(antenna.x - 1, antenna.y - 28 * m_zoom, 2, 2, Pink.Alpha(opacity).Emissive(3.f));

    float scale = (b.id == "arcadia" ? 1.4f : .95f) * m_zoom;
    const float maxSign = std::max(95.f, (b.w + b.d) * 24.f) * m_zoom;
    const float nearestX = std::max(b.x, std::min(b.x + b.w, m_player.x));
    const float nearestY = std::max(b.y, std::min(b.y + b.d, m_player.y));
    const bool showName =
        m_interface && std::hypot(nearestX - m_player.x, nearestY - m_player.y) <= 2.5f;
    const float measured = showName ? r.MeasureText(b.sign, scale) : 0;
    if (measured > maxSign)
    {
        scale *= maxSign / measured;
    }
    const float signWidth = showName ? r.MeasureText(b.sign, scale) + 14 * m_zoom : maxSign * .75f;
    const float signHeight = 16 * scale + 12 * m_zoom;
    const Point sign = Project(b.x + b.w * .55f, b.y + b.d, b.height * .65f);
    const float pulse = .86f + .14f * std::sin(m_time * 2 + b.palette);
    r.Glow(sign, signWidth * .8f, 27 * m_zoom, neon.Alpha(opacity * pulse));
    r.Rect(sign.x - signWidth * .5f,
           sign.y - 6 * m_zoom,
           signWidth,
           signHeight,
           Color(.015f, .025f, .045f, opacity));
    r.Line({sign.x - signWidth * .5f, sign.y - 6 * m_zoom},
           {sign.x + signWidth * .5f, sign.y - 6 * m_zoom},
           2,
           neon);
    r.Line({sign.x - signWidth * .5f, sign.y - 6 * m_zoom + signHeight},
           {sign.x + signWidth * .5f, sign.y - 6 * m_zoom + signHeight},
           1,
           neon.Alpha(opacity * .5f));
    if (showName)
    {
        r.Text(sign.x - signWidth * .5f + 7 * m_zoom, sign.y, b.sign, scale, neon.Shade(pulse));
    }
    if (showName && b.id == "arcadia")
    {
        r.Text(sign.x - signWidth * .5f,
               sign.y + signHeight + 2 * m_zoom,
               m_text.Get("sign.arcadia_motto"),
               .65f * m_zoom,
               White.Alpha(opacity * .7f));
    }
}

void CityScene::DrawActor(Renderer& r, Point position, int role, float phase, bool friendly) const
{
    const Point p = Project(position.x, position.y);
    if (p.x < -50 || p.x > Renderer::Width + 50 || p.y < -50 || p.y > Renderer::Height + 50)
    {
        return;
    }
    const bool player = role < 0;
    const float s = m_zoom, step = std::sin(phase) * (player ? 3.f : 1.2f) * s;
    const float bob = std::abs(std::sin(phase)) * 1.1f * s;
    const Color trim = (player      ? Cyan
                        : role == 2 ? Pink
                        : role == 1 ? Amber
                                    : Color(.53f, .68f, .87f))
                           .Emissive(player ? 1.5f : .25f);
    r.Ellipse({p.x, p.y + 2 * s}, 11 * s, 4 * s, Color(0, 0, .02f, .55f));
    if (player)
    {
        r.Glow(p, 32 * s, 12 * s, Cyan.Alpha(.6f));
        r.Line({p.x - 15 * s, p.y}, {p.x, p.y + 7 * s}, 1.5f, Cyan.Alpha(.8f));
        r.Line({p.x, p.y + 7 * s}, {p.x + 15 * s, p.y}, 1.5f, Cyan.Alpha(.8f));
    }
    r.Line({p.x - 4 * s, p.y - 11 * s}, {p.x - 5 * s + step, p.y}, 4 * s, Color(.12f, .18f, .24f));
    r.Line({p.x + 4 * s, p.y - 11 * s}, {p.x + 5 * s - step, p.y}, 4 * s, Color(.12f, .18f, .24f));
    r.Quad({p.x - 7 * s, p.y - 27 * s - bob},
           {p.x + 7 * s, p.y - 27 * s - bob},
           {p.x + 10 * s, p.y - 9 * s},
           {p.x - 9 * s, p.y - 9 * s},
           Color(.065f, .10f, .16f));
    r.Line({p.x - 7 * s, p.y - 25 * s},
           {p.x - 10 * s, p.y - 15 * s + step},
           3 * s,
           trim.Shade(.48f));
    r.Line({p.x + 7 * s, p.y - 25 * s},
           {p.x + 10 * s, p.y - 15 * s - step},
           3 * s,
           trim.Shade(.48f));
    r.Line({p.x - 7 * s, p.y - 26 * s - bob}, {p.x + 6 * s, p.y - 26 * s - bob}, 2 * s, trim);
    r.Rect(p.x - 3 * s, p.y - 24 * s - bob, 2 * s, 12 * s, trim.Shade(.6f));
    r.Ellipse({p.x, p.y - 32 * s - bob}, 7 * s, 8 * s, Color(.12f, .17f, .23f));
    r.Rect(p.x - 5 * s, p.y - 35 * s - bob, 10 * s, 4 * s, Color(.025f, .035f, .06f));
    r.Line({p.x - 4 * s + (player ? m_facing : 0), p.y - 33 * s - bob},
           {p.x + 4 * s + (player ? m_facing : 0), p.y - 33 * s - bob},
           2 * s,
           trim);
    if (player)
    {
        r.Glow({p.x, p.y - 33 * s}, 14 * s, 7 * s, Cyan.Alpha(.55f));
    }
    if (friendly)
    {
        r.Ellipse({p.x, p.y - 48 * s}, 2.5f * s, 2.5f * s, role == 2 ? Pink : Cyan);
    }
}

void CityScene::DrawWeather(Renderer& r) const
{
    if (m_rain)
    {
        for (int i = 0; i < 135; ++i)
        {
            const float speed = 210.f + Mod(i * 37, 120);
            const float x = std::fmod(i * 113.7f + m_time * 45.f, Renderer::Width + 120) - 60;
            const float y = std::fmod(i * 73.3f + m_time * speed, Renderer::Height + 120) - 60;
            r.Line({x, y},
                   {x + 4, y + 12.f + Mod(i, 8)},
                   .7f,
                   Color(.52f, .74f, .85f, .1f + Mod(i, 3) * .035f));
        }
    }
}

void CityScene::DrawDrone(Renderer& r, Point position) const
{
    const float droneX = position.x + std::sin(m_time * .3f) * 1.1f;
    const float droneY = position.y + std::cos(m_time * .3f) * 1.1f;
    const Point drone = Project(droneX, droneY, 87 + std::sin(m_time * 2) * 3);
    const Point scan = Project(droneX + .8f, droneY + .8f);
    r.Triangle(drone,
               {scan.x - 30 * m_zoom, scan.y},
               {scan.x + 30 * m_zoom, scan.y},
               Cyan.Alpha(.045f));
    r.Ellipse(scan, 31 * m_zoom, 10 * m_zoom, Cyan.Alpha(.055f));
    r.Line({drone.x - 18 * m_zoom, drone.y}, {drone.x + 18 * m_zoom, drone.y}, 3 * m_zoom, Muted);
    r.Ellipse({drone.x - 18 * m_zoom, drone.y}, 9 * m_zoom, 3 * m_zoom, Color(.2f, .3f, .38f));
    r.Ellipse({drone.x + 18 * m_zoom, drone.y}, 9 * m_zoom, 3 * m_zoom, Color(.2f, .3f, .38f));
    r.Rect(drone.x - 7 * m_zoom,
           drone.y - 4 * m_zoom,
           14 * m_zoom,
           8 * m_zoom,
           Color(.1f, .17f, .23f));
    r.Glow(drone, 20 * m_zoom, 12 * m_zoom, Pink);
    r.Rect(drone.x - 2 * m_zoom, drone.y, 4 * m_zoom, 2 * m_zoom, Pink.Emissive(4.f));
}
