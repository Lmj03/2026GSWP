#include "stdafx.h"
#include "CityScene.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
const Color BaseCyan(.18f, .94f, .91f);
const Color BaseGold(1.f, .72f, .24f);
const Color BaseWhite(.83f, .94f, .98f);
const Color BaseMuted(.45f, .64f, .71f);
}

void CityScene::CollectNearbyLoot(float dt)
{
    Materials collected;
    unsigned int bundles = 0;
    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.loot)
        {
            continue;
        }
        enemy.pickupDelay = std::max(0.f, enemy.pickupDelay - dt);
        Point local;
        if (enemy.pickupDelay > 0 || !LevelLocal(enemy.position, local) ||
            std::hypot(local.x - m_player.x, local.y - m_player.y) > 1.6f ||
            !ClearReach(m_player, local))
        {
            continue;
        }
        const auto limit = std::numeric_limits<std::uint64_t>::max();
        if (m_bag.scrap > limit - enemy.drop.scrap || m_bag.alloy > limit - enemy.drop.alloy ||
            m_bag.fabric > limit - enemy.drop.fabric || m_credits > limit - 8 ||
            m_recovery == limit)
        {
            continue;
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
        enemy.lootedOnce = true;
        enemy.drop = Materials{};
    }
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

void CityScene::OpenBase()
{
    if (m_health <= 0 || NearbyLevelInteraction() != -3)
    {
        return;
    }
    m_baseOpen = true;
    m_storagePage = false;
    m_map = false;
    m_restTime = 0;
    m_attackAnimation = 0;
    m_walk = 0;
    m_dialoguePages.clear();
    m_noticeTime = 0;
    m_baseNotice = m_text.Get("base.welcome");

    const bool ready =
        m_supplyOpened && std::all_of(m_enemies.begin(),
                                      m_enemies.end(),
                                      [](const Enemy& enemy)
                                      {
                                          return enemy.defeatedOnce && enemy.lootedOnce;
                                      });
    if (ready && !m_levelComplete)
    {
        m_levelComplete = true;
        m_credits += 20;
        m_reputation += 5;
        m_baseNotice = m_text.Get("level.complete");
    }
}

void CityScene::CloseBase()
{
    m_baseOpen = false;
    m_restTime = 0;
    m_attackCooldown = .25f;
}

void CityScene::UpdateBase(float dt)
{
    if (m_restTime <= 0)
    {
        return;
    }
    m_restTime = std::max(0.f, m_restTime - dt);
    const double progress = 1.0 - m_restTime / 3.0;
    m_health = m_restHealth + static_cast<std::int64_t>((MaxHealth() - m_restHealth) * progress);
    if (m_restTime <= 0)
    {
        m_health = MaxHealth();
        m_baseNotice = m_text.Get("base.rested");
    }
}

CityScene::Recipe CityScene::RecipeFor(int action) const
{
    if (action == 3)
    {
        const auto rank = static_cast<std::uint64_t>(m_knifeUpgrade + 1);
        return {Materials{4 * rank, 2 * rank, 0}, 8 * rank};
    }
    if (action == 4)
    {
        return {Materials{12, 4, 0}, 20};
    }
    return {Materials{6, 0, 4}, 15};
}

bool CityScene::SpendMaterials(Materials cost, std::uint64_t credits)
{
    auto enough = [](std::uint64_t bag, std::uint64_t storage, std::uint64_t needed)
    {
        return bag >= needed || storage >= needed - bag;
    };
    if (m_credits < credits || !enough(m_bag.scrap, m_storage.scrap, cost.scrap) ||
        !enough(m_bag.alloy, m_storage.alloy, cost.alloy) ||
        !enough(m_bag.fabric, m_storage.fabric, cost.fabric))
    {
        m_baseNotice = m_text.Get("base.insufficient");
        return false;
    }
    // Validate the entire recipe before consuming anything. Prefer carried materials.
    auto consume = [](std::uint64_t& bag, std::uint64_t& storage, std::uint64_t needed)
    {
        const auto carried = std::min(bag, needed);
        bag -= carried;
        storage -= needed - carried;
    };
    consume(m_bag.scrap, m_storage.scrap, cost.scrap);
    consume(m_bag.alloy, m_storage.alloy, cost.alloy);
    consume(m_bag.fabric, m_storage.fabric, cost.fabric);
    m_credits -= credits;
    return true;
}

bool CityScene::HandleBaseInput(unsigned char key)
{
    if (!m_baseOpen)
    {
        return false;
    }
    if (key == 'e' || key == 27)
    {
        CloseBase();
        return true;
    }
    if (key == 9)
    {
        m_storagePage = !m_storagePage;
        return true;
    }
    if (m_restTime > 0)
    {
        return true;
    }
    if (m_storagePage)
    {
        if (key == '1' || key == '2')
        {
            Materials& source = key == '1' ? m_bag : m_storage;
            Materials& destination = key == '1' ? m_storage : m_bag;
            const auto limit = std::numeric_limits<std::uint64_t>::max();
            if (destination.scrap > limit - source.scrap ||
                destination.alloy > limit - source.alloy ||
                destination.fabric > limit - source.fabric)
            {
                m_baseNotice = m_text.Get("base.storage_full");
                return true;
            }
            destination.scrap += source.scrap;
            destination.alloy += source.alloy;
            destination.fabric += source.fabric;
            source = Materials{};
            m_baseNotice = m_text.Get(key == '1' ? "base.deposited" : "base.withdrawn");
        }
        return true;
    }
    if (key == '1')
    {
        if (m_health == MaxHealth())
        {
            m_baseNotice = m_text.Get("level.health_full");
        }
        else
        {
            m_restTime = 3;
            m_restHealth = m_health;
            m_baseNotice = m_text.Get("base.resting");
        }
        return true;
    }
    if (key == '2')
    {
        if (m_level < 5 || !m_knifeOwned)
        {
            m_baseNotice = m_text.Get("weapon.locked");
        }
        else
        {
            m_weapon = m_weapon == Weapon::Fists ? Weapon::Knife : Weapon::Fists;
            m_baseNotice = m_text.Format("weapon.equipped", {{"weapon", WeaponName()}});
        }
        return true;
    }
    if (key < '3' || key > '5')
    {
        return true;
    }
    const int action = key - '0';
    if (action != 5 && (m_level < 5 || !m_knifeOwned))
    {
        m_baseNotice = m_text.Get("weapon.locked");
        return true;
    }
    if ((action == 3 && m_knifeUpgrade >= 10) || (action == 4 && m_forgedKnife) ||
        (action == 5 && m_vestCrafted))
    {
        m_baseNotice = m_text.Get("base.already_done");
        return true;
    }
    const Recipe recipe = RecipeFor(action);
    if (!SpendMaterials(recipe.materials, recipe.credits))
    {
        return true;
    }
    if (action == 3)
    {
        ++m_knifeUpgrade;
        m_baseNotice = m_text.Get("base.upgraded");
    }
    else if (action == 4)
    {
        m_forgedKnife = true;
        m_weapon = Weapon::Knife;
        m_baseNotice = m_text.Get("base.knife_crafted");
    }
    else
    {
        m_vestCrafted = true;
        m_health += 30;
        m_baseNotice = m_text.Get("base.vest_crafted");
    }
    return true;
}

void CityScene::DrawBaseGround(Renderer& r) const
{
    Point local;
    if (!LevelLocal(m_spawn, local))
    {
        return;
    }
    const Point a = Project(local.x - 1.1f, local.y - 1.1f);
    const Point b = Project(local.x + 1.1f, local.y - 1.1f);
    const Point c = Project(local.x + 1.1f, local.y + 1.1f);
    const Point d = Project(local.x - 1.1f, local.y + 1.1f);
    r.Quad(a, b, c, d, Color(.03f, .15f, .18f, .8f));
    r.Line(a, b, 2, BaseCyan.Emissive(.7f));
    r.Line(b, c, 2, BaseCyan.Emissive(.7f));
    r.Line(c, d, 2, BaseCyan.Emissive(.7f));
    r.Line(d, a, 2, BaseCyan.Emissive(.7f));
}

void CityScene::DrawBaseProps(Renderer& r) const
{
    Point local;
    if (!LevelLocal(m_spawn, local))
    {
        return;
    }
    const Point center = Project(local.x, local.y);
    if (center.x < -150 || center.x > Renderer::Width + 150 || center.y < -100 ||
        center.y > Renderer::Height + 150)
    {
        return;
    }
    // A cot, storage locker and workbench identify the refuge on the existing street.
    const Point cot = Project(local.x - .75f, local.y + .5f);
    const Point bench = Project(local.x + .1f, local.y - .8f);
    const Point locker = Project(local.x + .8f, local.y - .1f);
    const float s = m_zoom;
    r.Rect(cot.x - 18 * s, cot.y - 8 * s, 36 * s, 12 * s, Color(.12f, .3f, .32f));
    r.Rect(cot.x - 15 * s, cot.y - 12 * s, 12 * s, 7 * s, BaseWhite);
    r.Rect(bench.x - 20 * s, bench.y - 16 * s, 40 * s, 12 * s, Color(.25f, .19f, .12f));
    r.Line({bench.x - 18 * s, bench.y - 18 * s}, {bench.x + 18 * s, bench.y - 18 * s}, 3, BaseGold);
    r.Rect(bench.x - 8 * s, bench.y - 25 * s, 16 * s, 9 * s, BaseCyan.Emissive(.6f));
    r.Rect(locker.x - 12 * s, locker.y - 32 * s, 24 * s, 32 * s, Color(.1f, .23f, .3f));
    r.Line({locker.x, locker.y - 29 * s}, {locker.x, locker.y - 3 * s}, 1, BaseCyan);
    r.Rect(locker.x + 3 * s, locker.y - 18 * s, 3 * s, 7 * s, BaseGold);
    if (m_interface)
    {
        const std::string title = m_text.Get("base.marker");
        r.Text(center.x - r.MeasureText(title, 1.f) * .5f, center.y - 73 * s, title, 1.f, BaseCyan);
    }
}

void CityScene::DrawBaseInterface(Renderer& r) const
{
    r.Rect(0, 0, Renderer::Width, Renderer::Height, Color(0, .01f, .025f, .78f));
    r.Rect(180, 92, 920, 616, Color(.02f, .045f, .07f, .99f));
    r.Rect(180, 92, 920, 3, BaseCyan);
    r.Text(212, 113, m_text.Get("base.title"), 1.45f, BaseCyan);
    r.Text(212,
           153,
           m_text.Get(m_storagePage ? "base.storage_tab" : "base.workshop_tab"),
           1.f,
           BaseWhite);
    r.Line({212, 189}, {1068, 189}, 1, BaseCyan.Alpha(.4f));

    if (m_storagePage)
    {
        r.Text(212, 206, m_text.Get("base.material"), 1.1f, BaseGold);
        r.Text(488, 206, m_text.Get("base.carried"), 1.1f, BaseGold);
        r.Text(790, 206, m_text.Get("base.stored"), 1.1f, BaseGold);
        const char* names[] = {"item.scrap", "item.alloy", "item.fabric"};
        const std::uint64_t bag[] = {m_bag.scrap, m_bag.alloy, m_bag.fabric};
        const std::uint64_t stored[] = {m_storage.scrap, m_storage.alloy, m_storage.fabric};
        for (int i = 0; i < 3; ++i)
        {
            const float y = 251.f + i * 43;
            r.Text(212, y, m_text.Get(names[i]), 1.05f, BaseWhite);
            r.TextFitted(488, y, std::to_string(bag[i]), 1.05f, BaseWhite, 250);
            r.TextFitted(790, y, std::to_string(stored[i]), 1.05f, BaseCyan, 250);
        }
        r.Text(212, 411, m_text.Get("base.deposit_action"), 1.1f, BaseWhite);
        r.Text(212, 453, m_text.Get("base.withdraw_action"), 1.1f, BaseWhite);
        r.TextWrapped(212, 503, m_text.Get("base.storage_help"), 1.f, BaseMuted, 840);
    }
    else
    {
        r.TextFitted(212,
                     207,
                     m_text.Format("base.level",
                                   {{"level", std::to_string(m_level)}, {"weapon", WeaponName()}}),
                     1.05f,
                     BaseWhite,
                     410);
        r.TextFitted(
            212,
            243,
            m_text.Format("level.health",
                          {{"hp", std::to_string(m_health)}, {"max", std::to_string(MaxHealth())}}),
            1.03f,
            BaseWhite,
            410);
        r.TextFitted(212,
                     279,
                     m_text.Format("base.damage",
                                   {{"base", std::to_string(BaseAttackDamage())},
                                    {"total", std::to_string(AttackDamage())}}),
                     1.03f,
                     BaseGold,
                     410);
        r.TextFitted(212,
                     315,
                     m_text.Format("base.credits", {{"credits", std::to_string(m_credits)}}),
                     1.03f,
                     BaseWhite,
                     410);
        r.Text(212, 364, m_text.Get("base.rest_action"), 1.1f, BaseCyan);
        r.Text(212, 410, m_text.Get("base.equip_action"), 1.1f, BaseCyan);
        r.TextWrapped(212,
                      455,
                      m_text.Get(m_level < 5 ? "weapon.locked" : "base.equipment_help"),
                      1.f,
                      BaseMuted,
                      398);
        if (m_restTime > 0)
        {
            r.Rect(212, 535, 398, 6, BaseCyan.Alpha(.2f));
            r.Rect(212, 535, 398 * (1 - m_restTime / 3), 6, BaseCyan);
        }
        for (int action = 3; action <= 5; ++action)
        {
            const float y = 207.f + (action - 3) * 110;
            const bool done = action == 3   ? m_knifeUpgrade >= 10
                              : action == 4 ? m_forgedKnife
                                            : m_vestCrafted;
            const std::string name =
                action == 3 ? m_text.Format("base.upgrade_action",
                                            {{"rank", std::to_string(m_knifeUpgrade)}})
                            : m_text.Get(action == 4 ? "base.forge_action" : "base.vest_action");
            const Recipe recipe = RecipeFor(action);
            r.TextFitted(660, y, name, 1.1f, done ? BaseMuted : BaseGold, 408);
            const std::string cost =
                action == 5 ? m_text.Format("base.fabric_cost",
                                            {{"scrap", std::to_string(recipe.materials.scrap)},
                                             {"fabric", std::to_string(recipe.materials.fabric)},
                                             {"credits", std::to_string(recipe.credits)}})
                            : m_text.Format("base.alloy_cost",
                                            {{"scrap", std::to_string(recipe.materials.scrap)},
                                             {"alloy", std::to_string(recipe.materials.alloy)},
                                             {"credits", std::to_string(recipe.credits)}});
            r.TextFitted(660, y + 35, done ? m_text.Get("base.done") : cost, .98f, BaseWhite, 408);
            r.TextFitted(660,
                         y + 65,
                         m_text.Get(action == 3   ? "base.upgrade_effect"
                                    : action == 4 ? "base.forge_effect"
                                                  : "base.vest_effect"),
                         .98f,
                         BaseMuted,
                         408);
        }
    }
    r.Line({212, 567}, {1068, 567}, 1, BaseCyan.Alpha(.35f));
    r.TextWrapped(212, 583, m_baseNotice, 1.03f, BaseGold, 856);
    r.Text(212, 669, m_text.Get("base.close_help"), .98f, BaseMuted);
}
