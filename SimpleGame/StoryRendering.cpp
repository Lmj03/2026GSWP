#include "stdafx.h"
#include "CityScene.h"
#include <algorithm>
#include <cmath>

void CityScene::DrawPlayer(Renderer& r, Point position) const
{
    const Point p = Project(position.x, position.y);
    if (p.x < -60 || p.x > Renderer::Width + 60 || p.y < -90 || p.y > Renderer::Height + 90)
    {
        return;
    }
    const float s = m_zoom;
    const float stride = std::sin(m_walk) * 3.2f * s;
    const float bob = std::abs(std::sin(m_walk)) * .7f * s;
    const float sway = std::sin(m_walk * .5f) * 1.4f * s;
    const Color coat(.055f, .065f, .085f), edge(.19f, .22f, .25f), skin(.54f, .43f, .37f);
    const Color cyan(.15f, .68f, .74f, 1, .6f);
    auto at = [&](float x, float y)
    {
        return Point(p.x + x * s, p.y + y * s - bob);
    };
    r.Ellipse(at(0, 2), 12 * s, 4 * s, Color(0, 0, .01f, .55f));
    r.Line(at(-3, -22), at(-4 + stride / s, -3), 4.1f * s, Color(.10f, .12f, .15f));
    r.Line(at(3, -22), at(4 - stride / s, -3), 4.1f * s, Color(.075f, .085f, .11f));
    r.Rect(p.x + (-7 + stride / s) * s, p.y - 5 * s, 7 * s, 5 * s, coat);
    r.Rect(p.x + (1 - stride / s) * s, p.y - 5 * s, 7 * s, 5 * s, coat);
    r.Line(at(-4, -15), at(-4, -8), 1.3f * s, edge);
    r.Rect(p.x + 3 * s, p.y - 19 * s - bob, 3 * s, 5 * s, edge);
    r.Quad(at(-6, -43), at(6, -43), at(8 + sway / s, -15), at(-8 + sway / s, -15), coat);
    r.Quad(at(-6, -42), at(-1, -39), at(-2, -15), at(-8 + sway / s, -15), edge.Shade(.65f));
    r.Line(at(-1, -39), at(-1, -22), .9f * s, edge);
    r.Line(at(4, -40), at(-4, -29), 1.7f * s, Color(.17f, .16f, .15f));
    r.Line(at(-5, -25), at(5, -25), 2 * s, Color(.22f, .20f, .18f));
    r.Rect(p.x - s, p.y - 26 * s - bob, 3 * s, 3 * s, edge);
    r.Line(at(-6, -40), at(-9, -28 + stride / s), 4 * s, coat);
    r.Line(at(6, -40), at(9, -28 - stride / s), 4 * s, coat);
    r.Ellipse(at(-9, -27 + stride / s), 2 * s, 2.5f * s, edge);
    r.Ellipse(at(9, -27 - stride / s), 2 * s, 2.5f * s, edge);
    r.Line(at(-6, -39), at(-6, -35), 1.4f * s, cyan);
    r.Quad(at(-5, -45), at(-1, -40), at(-3, -36), at(-7, -41), edge);
    r.Quad(at(5, -45), at(1, -40), at(3, -36), at(7, -41), coat.Shade(1.7f));
    r.Ellipse(at(m_facing * .6f, -50), 4.4f * s, 6 * s, skin);
    r.Quad(at(-5, -54), at(1, -57), at(5, -53), at(-3, -50), Color(.018f, .022f, .03f));
    r.Triangle(at(-4, -54), at(-5, -48), at(-1, -51), Color(.025f, .03f, .04f));
    r.Line(at(-2 + m_facing, -50), at(2 + m_facing, -50), .8f * s, Color(.04f, .035f, .04f));
    r.Line(at(m_facing * 2, -48), at(m_facing * 2.4f, -46), .6f * s, edge);
    r.Line(at(-1, -45), at(2, -45), .7f * s, edge.Shade(.7f));
    r.Ellipse(at(0, 1), 5 * s, 2 * s, cyan.Alpha(.2f));
}

void CityScene::DrawCitizen(Renderer& r, const Citizen& npc, float phase) const
{
    if (!npc.active)
    {
        return;
    }
    const Point p = Project(npc.position.x, npc.position.y);
    if (p.x < -60 || p.x > Renderer::Width + 60 || p.y < -90 || p.y > Renderer::Height + 80)
    {
        return;
    }
    const float s = m_zoom;
    const Color color = npc.hostile ? Color(1, .24f, .28f) : Color(.3f, .82f, .8f);
    if (npc.memory.health <= 0)
    {
        r.Ellipse(p, 10 * s, 4 * s, Color(.1f, .12f, .17f));
        if (!npc.memory.looted && npc.memory.crimes > 0)
        {
            r.Rect(p.x - 4, p.y - 6, 8, 6, Color(1, .75f, .2f));
        }
        return;
    }
    const bool moving =
        npc.activity == NpcActivity::Delivering || npc.activity == NpcActivity::Fleeing ||
        npc.activity == NpcActivity::Attacking || npc.activity == NpcActivity::Extorting ||
        npc.activity == NpcActivity::Patrolling || npc.activity == NpcActivity::Travelling;
    DrawActor(r, npc.position, npc.role, moving ? m_time * 3 + phase : 0, npc.memory.helped);
    if (npc.occupation == Occupation::Worker)
    {
        r.Rect(p.x - 6 * s, p.y - 41 * s, 12 * s, 4 * s, Color(.95f, .63f, .2f));
    }
    if (npc.occupation == Occupation::Medic)
    {
        r.Rect(p.x - 2 * s, p.y - 26 * s, 4 * s, 10 * s, color);
        r.Rect(p.x - 5 * s, p.y - 23 * s, 10 * s, 4 * s, color);
    }
    if (npc.occupation == Occupation::Merchant || npc.activity == NpcActivity::Delivering)
    {
        r.Rect(p.x + 8 * s, p.y - 16 * s, 6 * s, 8 * s, Color(.42f, .28f, .15f));
    }
    if (npc.windup > 0)
    {
        r.Ellipse(Project(npc.strike.x, npc.strike.y), 27 * s, 13 * s, color.Alpha(.25f));
    }
    if (m_interface && npc.hostile &&
        std::hypot(npc.position.x - m_player.x, npc.position.y - m_player.y) < 4)
    {
        r.Rect(p.x - 20, p.y - 51 * s, 40, 4, color.Alpha(.2f));
        r.Rect(p.x - 20, p.y - 51 * s, 40.f * npc.memory.health / 36, 4, color);
    }
}

void CityScene::DrawStoryNode(Renderer& r, const std::string& id, Point position) const
{
    const Point p = Project(position.x, position.y);
    const float s = m_zoom;
    if (p.x < -100 || p.x > Renderer::Width + 100 || p.y < -100 || p.y > Renderer::Height + 100)
    {
        return;
    }
    if (id == "crime")
    {
        r.Ellipse(p, 20 * s, 7 * s, Color(.16f, .06f, .09f, .6f));
    }
    if (id == "terminal")
    {
        r.Rect(p.x - 5 * s, p.y - 26 * s, 10 * s, 26 * s, Color(.10f, .15f, .2f));
        r.Rect(p.x - 4 * s, p.y - 23 * s, 8 * s, 10 * s, Color(.1f, .7f, .75f, 1, 1));
    }
    Point target;
    if (StoryObjectiveLocal(target) &&
        std::hypot(position.x - target.x, position.y - target.y) < .01f)
    {
        r.Glow(p, 19 * s, 7 * s, Color(1, .72f, .2f, .5f));
        if (m_interface && std::hypot(position.x - m_player.x, position.y - m_player.y) < 2.5f)
        {
            r.Text(p.x - 25,
                   p.y - 95 * s,
                   m_text.Get("story.marker." + id),
                   .82f,
                   Color(1, .8f, .4f));
        }
    }
}

void CityScene::DrawStoryHud(Renderer& r) const
{
    const bool training = m_story.stage == StoryStage::Prepare && !m_levelComplete;
    const std::string key = !training               ? StoryObjectiveKey()
                            : !m_supplyOpened       ? "level.objective_supply"
                            : m_tutorialDefeats < 3 ? "level.objective_fight"
                            : m_tutorialLoot < 3    ? "level.objective_loot"
                                                    : "level.objective_return";
    const std::string objective = m_text.Get(key);
    const float height = 53 + r.WrappedHeight(objective, .92f, 330) + (training ? 29 : 0);
    r.Rect(24, 24, 362, height, Color(.012f, .026f, .042f, .8f));
    r.Text(40, 37, m_text.Get("hud.objective"), .94f, Color(1, .72f, .3f));
    const float textHeight = r.TextWrapped(40, 67, objective, .92f, Color(.8f, .9f, .95f), 330);
    if (training)
    {
        r.TextFitted(40,
                     73 + textHeight,
                     m_text.Format("level.progress",
                                   {{"kills", std::to_string(m_tutorialDefeats)},
                                    {"loot", std::to_string(m_tutorialLoot)}}),
                     .83f,
                     Color(.25f, .9f, .9f),
                     330);
    }
}

void CityScene::DrawDialogue(Renderer& r) const
{
    if (!m_dialogue.open)
    {
        return;
    }
    r.Rect(0, 0, Renderer::Width, Renderer::Height, Color(0, 0, .02f, .68f));
    r.Rect(300, 140, 680, 475, Color(.02f, .045f, .065f, .98f));
    r.TextFitted(324, 165, m_dialogue.title, 1.3f, Color(.25f, .9f, .9f), 540);
    r.Rect(884, 165, 72, 32, Color(.08f, .2f, .24f));
    r.Text(894, 167, m_text.Get("hud.close"), .85f, Color(.8f, .9f, .95f));
    r.TextWrapped(324, 212, m_dialogue.body, 1.03f, Color(.84f, .92f, .95f), 630);
    for (size_t i = 0; i < m_dialogue.options.size(); ++i)
    {
        r.Rect(324, 440 + static_cast<float>(i) * 42, 630, 36, Color(.04f, .1f, .14f));
        r.TextFitted(334,
                     445 + static_cast<float>(i) * 42,
                     m_text.Get(m_dialogue.options[i].key),
                     1.05f,
                     Color(1, .77f, .4f),
                     630);
    }
}

void CityScene::DrawJournal(Renderer& r) const
{
    if (!m_journalOpen)
    {
        return;
    }
    r.Rect(0, 0, Renderer::Width, Renderer::Height, Color(0, 0, .02f, .7f));
    r.Rect(270, 105, 740, 580, Color(.02f, .04f, .065f, .98f));
    r.Text(294, 124, m_text.Get("journal.title"), 1.3f, Color(.25f, .9f, .9f));
    r.Rect(914, 124, 72, 32, Color(.08f, .2f, .24f));
    r.Text(924, 126, m_text.Get("hud.close"), .85f, Color(.8f, .9f, .95f));
    r.Rect(810, 646, 70, 34, Color(.08f, .2f, .24f));
    r.Rect(900, 646, 70, 34, Color(.08f, .2f, .24f));
    r.Text(820, 648, m_text.Get("hud.previous"), .85f, Color(.8f, .9f, .95f));
    r.Text(910, 648, m_text.Get("hud.next"), .85f, Color(.8f, .9f, .95f));
    r.Text(294, 166, m_text.Get(StoryObjectiveKey()), .92f, Color(1, .77f, .4f));
    r.Text(294,
           201,
           m_text.Get(m_story.legitimacy >= 15    ? "journal.just"
                      : m_story.legitimacy <= -15 ? "journal.unjust"
                                                  : "journal.mixed"),
           .95f,
           Color(.8f, .9f, .95f));
    const size_t begin = m_journalPage * 5;
    float y = 246;
    for (size_t i = begin; i < std::min(begin + 5, m_story.journal.size()); ++i)
    {
        y += r.TextWrapped(294,
                           y,
                           m_text.Get(m_story.journal[i]),
                           .89f,
                           Color(.74f, .83f, .88f),
                           680) +
             12;
    }
    r.Text(294,
           650,
           m_text.Format(
               "hud.journal_page",
               {{"page", std::to_string(m_journalPage + 1)},
                {"pages", std::to_string(std::max(size_t(1), (m_story.journal.size() + 4) / 5))}}),
           .9f,
           Color(.4f, .65f, .75f));
}
