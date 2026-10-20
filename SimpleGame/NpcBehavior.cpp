#include "stdafx.h"
#include "CityScene.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cmath>

void CityScene::RememberNpc(const Citizen& npc)
{
    m_npcMemory[npc.id] = npc.memory;
}

void CityScene::UpdateNpc(Citizen& npc, float dt, float phase)
{
    if (!npc.active || npc.story)
    {
        return;
    }
    npc.flash = std::max(0.f, npc.flash - dt);
    npc.cooldown = std::max(0.f, npc.cooldown - dt);
    if (npc.memory.health <= 0)
    {
        npc.activity = NpcActivity::Defeated;
        npc.travelling = false;
        npc.destinationTown = -1;
        npc.routeCount = 0;
        npc.routeStep = 0;
        npc.pickupDelay = std::max(0.f, npc.pickupDelay - dt);
        if (!npc.memory.looted && npc.memory.crimes > 0 && npc.pickupDelay == 0 &&
            std::hypot(npc.position.x - m_player.x, npc.position.y - m_player.y) < 1.6f &&
            ClearReach(m_player, npc.position) && m_bag.scrap <= UINT64_MAX - 2)
        {
            m_bag.scrap += 2;
            npc.memory.looted = true;
            if (npc.memory.notoriety >= 60)
            {
                m_tutorialLoot = std::min(3, m_tutorialLoot + 1);
            }
            ShowNotice(m_text.Get("npc.loot"));
            RememberNpc(npc);
        }
        return;
    }
    npc.hostile = npc.memory.notoriety >= 60 || npc.memory.trust <= -40;
    const float playerDistance =
        std::hypot(npc.position.x - m_player.x, npc.position.y - m_player.y);
    if (npc.travelling && (!npc.hostile || playerDistance > 3))
    {
        AdvanceTownTravel(npc, dt, true);
        return;
    }
    if (npc.travelling)
    {
        npc.home = npc.position;
    }
    if (playerDistance > 12)
    {
        return;
    }
    npc.actionTimer -= dt;
    Point target = npc.home;
    float speed = .65f;
    auto moveTo = [&](Point p, float rate)
    {
        const float distance = std::hypot(p.x - npc.position.x, p.y - npc.position.y);
        if (distance > .12f)
        {
            const float step = std::min(distance, dt * rate);
            Move(npc.position,
                 (p.x - npc.position.x) / distance * step,
                 (p.y - npc.position.y) / distance * step,
                 .16f);
        }
    };
    if (npc.hostile)
    {
        npc.activity = NpcActivity::Attacking;
        const float distance = std::hypot(npc.position.x - m_player.x, npc.position.y - m_player.y);
        if (distance > 4.5f || std::hypot(m_player.x - npc.home.x, m_player.y - npc.home.y) > 5)
        {
            npc.windup = 0;
            moveTo(npc.home, 1);
            return;
        }
        if (npc.windup > 0)
        {
            npc.windup = std::max(0.f, npc.windup - dt);
            if (npc.windup == 0)
            {
                if (m_health > 0 && m_invulnerable <= 0 &&
                    std::hypot(m_player.x - npc.strike.x, m_player.y - npc.strike.y) < .7f &&
                    distance < 1.4f && ClearReach(npc.position, m_player))
                {
                    m_health = std::max<std::int64_t>(0, m_health - 8);
                    m_invulnerable = .7f;
                    if (m_health == 0)
                    {
                        m_respawnDelay = 2;
                    }
                }
                npc.cooldown = 1.1f;
            }
        }
        else if (distance < 1.05f && npc.cooldown == 0 && ClearReach(npc.position, m_player))
        {
            npc.strike = m_player;
            npc.windup = .65f;
        }
        else if (distance > .8f)
        {
            moveTo(m_player, 1.0f);
        }
        return;
    }
    Citizen* other = nullptr;
    float closest = 3.5f;
    for (Citizen& candidate : m_citizens)
    {
        if (candidate.id == npc.id || !candidate.active || candidate.story ||
            candidate.memory.health <= 0)
        {
            continue;
        }
        const float d = std::hypot(candidate.position.x - npc.position.x,
                                   candidate.position.y - npc.position.y);
        bool relevant = false;
        if (npc.occupation == Occupation::Raider)
        {
            relevant = candidate.occupation != Occupation::Raider &&
                       candidate.occupation != Occupation::Guard;
        }
        else if (npc.occupation == Occupation::Medic)
        {
            relevant = candidate.memory.health < 36;
        }
        else if (npc.occupation == Occupation::Guard)
        {
            relevant = candidate.memory.notoriety >= 60;
        }
        else
        {
            relevant = candidate.hostile;
        }
        if (relevant && d < closest)
        {
            closest = d;
            other = &candidate;
        }
    }
    if (npc.occupation == Occupation::Raider && other)
    {
        if (npc.activity != NpcActivity::Restitution || npc.actionTimer < 6)
        {
            npc.activity = NpcActivity::Extorting;
        }
        target = other->position;
        if (closest < 1.1f && npc.actionTimer <= 0 && ClearReach(npc.position, other->position))
        {
            if (npc.memory.crimes > 0 && npc.memory.services < npc.memory.crimes / 3)
            {
                npc.activity = NpcActivity::Restitution;
                other->memory.health = std::min(36, other->memory.health + 4);
                ++npc.memory.services;
                npc.memory.notoriety = std::max(0, npc.memory.notoriety - 6);
            }
            else
            {
                other->memory.health = std::max(1, other->memory.health - 4);
                ++npc.memory.crimes;
                npc.memory.notoriety = std::min(100, npc.memory.notoriety + 12);
            }
            npc.actionTimer = 8;
            RememberNpc(*other);
            RememberNpc(npc);
            FrameProfiler::Instance().Event(
                "npc_action=" + npc.id + " kind=" + std::to_string(static_cast<int>(npc.activity)) +
                " notoriety=" + std::to_string(npc.memory.notoriety));
        }
    }
    else if ((npc.occupation == Occupation::Medic || npc.occupation == Occupation::Guard) && other)
    {
        npc.activity = NpcActivity::Working;
        target = other->position;
        if (closest < 1.15f && npc.actionTimer <= 0 && ClearReach(npc.position, other->position))
        {
            if (npc.occupation == Occupation::Medic)
            {
                other->memory.health = std::min(36, other->memory.health + 8);
            }
            else
            {
                other->cooldown = 2;
                other->windup = 0;
                other->memory.notoriety = std::max(0, other->memory.notoriety - 6);
            }
            ++npc.memory.services;
            npc.memory.notoriety = std::max(0, npc.memory.notoriety - 3);
            npc.actionTimer = 6;
            RememberNpc(*other);
            RememberNpc(npc);
        }
    }
    else if (other || (m_story.legitimacy < -30 && npc.occupation == Occupation::Resident))
    {
        npc.activity = NpcActivity::Fleeing;
        const Point threat = other ? other->position : m_player;
        target = {npc.position.x + (npc.position.x - threat.x),
                  npc.position.y + (npc.position.y - threat.y)};
        if (std::hypot(target.x - npc.home.x, target.y - npc.home.y) > 4)
        {
            target = npc.home;
        }
        speed = 1.2f;
    }
    else
    {
        const float shift = std::fmod(m_time + phase * 7, 40.f);
        if (npc.occupation == Occupation::Merchant || npc.occupation == Occupation::Mechanic)
        {
            npc.activity = NpcActivity::Trading;
        }
        else if (npc.occupation == Occupation::Worker)
        {
            npc.activity = shift < 20 ? NpcActivity::Working : NpcActivity::Delivering;
            target.y += shift < 20 ? 1.2f : -1.2f;
        }
        else
        {
            npc.activity =
                npc.occupation == Occupation::Guard ? NpcActivity::Patrolling : NpcActivity::Idle;
            target.x += std::sin(m_time * .16f + phase) * 1.2f;
            target.y += std::cos(m_time * .13f + phase) * 1.2f;
        }
        if (npc.actionTimer <= 0)
        {
            npc.actionTimer = 8;
            if (npc.occupation == Occupation::Worker && npc.memory.notoriety > 0)
            {
                ++npc.memory.services;
                npc.memory.notoriety = std::max(0, npc.memory.notoriety - 2);
                RememberNpc(npc);
            }
        }
    }
    moveTo(target, speed);
}

bool CityScene::AttackCitizens(bool knife, std::int64_t damage, float maximumDistance)
{
    bool anyHit = false;
    Citizen* closestNpc = nullptr;
    float closest = maximumDistance;
    for (Citizen& npc : m_citizens)
    {
        if (!npc.active || npc.story || npc.memory.health <= 0)
        {
            continue;
        }
        const float dx = npc.position.x - m_player.x, dy = npc.position.y - m_player.y;
        const float distance = std::hypot(dx, dy);
        const float facing =
            distance > .001f
                ? (dx * m_attackVisualDirection.x + dy * m_attackVisualDirection.y) / distance
                : 1;
        if (distance < closest && (distance < .35f || facing >= (knife ? .5f : .15f)) &&
            ClearReach(m_player, npc.position))
        {
            closest = distance;
            closestNpc = &npc;
        }
    }
    for (Citizen& npc : m_citizens)
    {
        if (!npc.active || npc.story || npc.memory.health <= 0)
        {
            continue;
        }
        const float dx = npc.position.x - m_player.x, dy = npc.position.y - m_player.y;
        const float distance = std::hypot(dx, dy);
        const float facing =
            distance > .001f
                ? (dx * m_attackVisualDirection.x + dy * m_attackVisualDirection.y) / distance
                : 1;
        const bool hit =
            knife ? distance < 2.1f && facing >= .5f && ClearReach(m_player, npc.position)
                  : &npc == closestNpc;
        if (!hit)
        {
            continue;
        }
        anyHit = true;
        const bool criminal = npc.memory.notoriety >= 60 && npc.memory.crimes > 0;
        if (!criminal)
        {
            RecordJustice("npc/harm/" + npc.id, -15, "journal.harm");
        }
        npc.memory.health = static_cast<int>(std::max<std::int64_t>(0, npc.memory.health - damage));
        npc.memory.trust = -60;
        npc.flash = .2f;
        npc.hostile = true;
        if (npc.memory.health == 0)
        {
            if (criminal)
            {
                m_tutorialDefeats = std::min(3, m_tutorialDefeats + 1);
            }
            npc.windup = 0;
            npc.pickupDelay = .35f;
            if (criminal)
            {
                RecordJustice("npc/stop/" + npc.id, 3, "journal.stop");
                AddExperience(2);
            }
        }
        RememberNpc(npc);
    }
    return anyHit;
}
