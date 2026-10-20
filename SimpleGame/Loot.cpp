#include "stdafx.h"
#include "CityScene.h"
#include <algorithm>
#include <cmath>
#include <limits>

void CityScene::CollectLoot(Enemy& enemy, float dt)
{
    Materials& collected = m_frameLoot;
    unsigned int& bundles = m_frameLootBundles;

    if (!enemy.loot)
    {
        return;
    }
    enemy.pickupDelay = std::max(0.f, enemy.pickupDelay - dt);
    Point local;
    if (enemy.pickupDelay > 0 || !EnemyLocal(enemy, enemy.position, local) ||
        std::hypot(local.x - m_player.x, local.y - m_player.y) > 1.6f ||
        !ClearReach(m_player, local))
    {
        return;
    }
    const auto limit = std::numeric_limits<std::uint64_t>::max();
    if (m_bag.scrap > limit - enemy.drop.scrap || m_bag.alloy > limit - enemy.drop.alloy ||
        m_bag.fabric > limit - enemy.drop.fabric || m_credits > limit - 8 || m_recovery == limit)
    {
        return;
    }
    m_bag.scrap += enemy.drop.scrap;
    m_bag.alloy += enemy.drop.alloy;
    m_bag.fabric += enemy.drop.fabric;
    collected.scrap += enemy.drop.scrap;
    collected.alloy += enemy.drop.alloy;
    collected.fabric += enemy.drop.fabric;
    m_credits += 8;
    ++m_recovery;
    ++bundles;
    enemy.loot = false;
    if (enemy.tutorial && !enemy.lootedOnce)
    {
        m_tutorialLoot = std::min(3, m_tutorialLoot + 1);
    }
    enemy.lootedOnce = true;
    enemy.drop = Materials{};
    if (bundles > 0)
    {
        m_pickupNotice = m_text.Format("loot.collected",
                                       {{"scrap", std::to_string(collected.scrap)},
                                        {"alloy", std::to_string(collected.alloy)},
                                        {"fabric", std::to_string(collected.fabric)},
                                        {"credits", std::to_string(bundles * 8)},
                                        {"recovery", std::to_string(bundles)}});
        m_pickupNoticeTime = 4;
    }
}
