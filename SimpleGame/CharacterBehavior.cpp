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

float Distance(Point a, Point b)
{
    const float x = a.x - b.x, y = a.y - b.y;
    return std::sqrt(x * x + y * y);
}

}

void CityScene::UpdatePlayer(float dt)
{
    if (m_health <= 0)
    {
        UpdatePlayerTimers(dt);
        return;
    }
    const bool* keys = m_input;
    float sx = (keys['d'] ? 1.f : 0.f) - (keys['a'] ? 1.f : 0.f);
    float sy = (keys['s'] ? 1.f : 0.f) - (keys['w'] ? 1.f : 0.f);
    const float length = std::sqrt(sx * sx + sy * sy);
    if (length > 0)
    {
        sx /= length;
        sy /= length;
        // Inverse isometric projection: keys move in screen directions.
        // Equal projected speed for all directions, independent of frame rate.
        const float speed = keys[16] ? 145.f : 88.f;
        const float dx = (sx / 64.f + sy / 32.f) * speed * dt;
        const float dy = (-sx / 64.f + sy / 32.f) * speed * dt;
        const float directionX = sx / 64.f + sy / 32.f;
        const float directionY = -sx / 64.f + sy / 32.f;
        const float directionLength = std::hypot(directionX, directionY);
        m_attackDirection = {directionX / directionLength, directionY / directionLength};
        const Point before = m_player;
        Move(m_player, dx, dy);
        if (Distance(before, m_player) > .001f)
        {
            m_walk += dt * (keys[16] ? 14 : 9);
        }
        else
        {
            m_walk = 0;
        }
        if (std::abs(sx) > .01f)
        {
            m_facing = sx > 0 ? 1.f : -1.f;
        }
    }
    else
    {
        m_walk = 0;
    }
    Recenter();
    const float follow = 1 - std::exp(-7 * dt);
    m_camera.x += (m_player.x - m_camera.x) * follow;
    m_camera.y += (m_player.y - m_camera.y) * follow;
    if (keys[' '])
    {
        Attack();
    }
    UpdatePlayerTimers(dt);
}

void CityScene::UpdateCitizen(Citizen& npc, float dt, float phase)
{
    UpdateNpc(npc, dt, phase);
}
