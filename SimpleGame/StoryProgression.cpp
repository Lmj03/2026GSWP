#include "stdafx.h"
#include "CityScene.h"
#include "StoryActors.h"
#include "Resources.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

bool CityScene::LoadStory()
{
    std::string source;
    if (!ReadResource(L"Data/Story/act1.story", source))
    {
        return false;
    }
    std::istringstream lines(source);
    std::string line;
    bool version = false;
    while (std::getline(lines, line))
    {
        std::istringstream input(line.substr(0, line.find('#')));
        std::string type, id, trailing;
        if (!(input >> type))
        {
            continue;
        }
        if (type == "version")
        {
            int v = 0;
            if (!(input >> v) || v != 1 || version)
            {
                return false;
            }
            version = true;
        }
        else if (type == "node")
        {
            Point p;
            if (!(input >> id >> p.x >> p.y) || !std::isfinite(p.x) || !std::isfinite(p.y) ||
                !CanStand(p.x, p.y, .3f) || !m_storyNodes.emplace(id, p).second)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
        if (input >> trailing)
        {
            return false;
        }
    }
    const char* ids[] = {"crime", "terminal", "family", "witness", "target", "rebels"};
    if (!version || m_storyNodes.size() != 6)
    {
        return false;
    }
    for (const char* id : ids)
    {
        const auto found = m_storyNodes.find(id);
        if (found == m_storyNodes.end())
        {
            return false;
        }
        auto& actor = m_sceneGraph.Create<StoryNodeActor>("story/node/" + std::string(id),
                                                          "authored",
                                                          *this,
                                                          id);
        actor.SetLocalPosition(found->second);
    }
    m_sceneGraph.Create<StoryActor>("story/controller", "", *this);
    m_sceneGraph.Create<DialogueActor>("dialogue", "screen", *this);
    for (const char* id : {"family", "witness", "rebels"})
    {
        Citizen npc{};
        npc.id = "story/" + std::string(id);
        npc.story = true;
        npc.active = false;
        npc.home = npc.position = m_storyNodes.at(id);
        npc.role = 0;
        npc.name = m_text.Get("story.name." + std::string(id));
        npc.occupation = std::string(id) == "witness" ? Occupation::Worker : Occupation::Resident;
        m_authoredCitizens.push_back(npc);
    }
    for (int i = 0; i < 3; ++i)
    {
        Citizen npc{};
        npc.id = i == 0 ? "story/victim" : "story/guard" + std::to_string(i);
        npc.name = m_text.Get(i == 0 ? "story.name.victim" : "story.name.guard");
        const Point crime = m_storyNodes.at("crime");
        npc.home = npc.position =
            i == 0 ? crime : Point(crime.x + (i == 1 ? -.7f : .6f), crime.y + .6f);
        npc.occupation = i == 0 ? Occupation::Resident : Occupation::Guard;
        npc.role = i == 0 ? 0 : 2;
        npc.story = true;
        m_authoredCitizens.push_back(npc);
    }
    Enemy heir;
    heir.id = "story_heir";
    heir.home = heir.position = m_storyNodes.at("target");
    heir.health = heir.maxHealth = 90;
    heir.experience = 0;
    heir.tutorial = false;
    std::vector<Enemy> enemies = m_enemies.CopyData<Enemy>();
    enemies.push_back(heir);
    SetEnemies(enemies);
    return true;
}

void CityScene::ResetStory()
{
    m_story = StoryState{};
    m_dialogue = DialogueState{};
    m_journalOpen = false;
    m_npcMemory.clear();
    m_journalPage = 0;
    for (EnemyActor& enemy : m_enemies)
    {
        if (!enemy.tutorial)
        {
            enemy.enabled = false;
        }
    }
    RecordJustice("origin", 0, "journal.origin");
}

bool CityScene::RecordJustice(const std::string& id, int amount, const std::string& key)
{
    if (amount > 0 && id.find("npc/") == 0)
    {
        if (m_story.decisions.count(id))
        {
            return false;
        }
        amount = std::min(amount, 12 - m_story.minorGood);
        m_story.minorGood += amount;
    }
    const bool recorded = m_story.Record(id, amount, key);
    if (recorded)
    {
        FrameProfiler::Instance().Event("justice_event=" + id + " delta=" + std::to_string(amount) +
                                        " value=" + std::to_string(m_story.legitimacy));
    }
    return recorded;
}

void CityScene::UpdateStory(float dt)
{
    if (!m_input || m_baseOpen || m_dialogue.open || m_journalOpen || m_map ||
        m_dialoguePages.size() > 1)
    {
        return;
    }
    for (Citizen& npc : m_citizens)
    {
        if (npc.id == "story/family")
        {
            npc.active = m_story.stage >= StoryStage::Family;
        }
        if (npc.id == "story/witness")
        {
            npc.active = m_story.stage >= StoryStage::Prepare;
        }
        if (npc.id == "story/rebels")
        {
            npc.active = m_story.stage >= StoryStage::Support;
        }
        if (npc.id.find("story/guard") == 0)
        {
            npc.active = m_story.stage <= StoryStage::Report;
            npc.memory.notoriety =
                m_story.stage == StoryStage::Witness && m_story.sceneTime < 2 ? 0 : 75;
            if (m_story.sceneActive)
            {
                const float blend = std::min(1.f, m_story.sceneTime);
                const float side = npc.id == "story/guard1" ? -.4f : .4f;
                npc.position = {npc.home.x + (m_player.x + side - npc.home.x) * blend,
                                npc.home.y + (m_player.y - npc.home.y) * blend};
            }
            else
            {
                npc.position = npc.home;
            }
        }
        if (npc.id == "story/victim")
        {
            npc.memory.health =
                m_story.stage == StoryStage::Witness && m_story.sceneTime < 2 ? 36 : 0;
        }
    }
    for (EnemyActor& enemy : m_enemies)
    {
        if (!enemy.tutorial)
        {
            enemy.enabled = m_story.stage <= StoryStage::Report ||
                            m_story.stage == StoryStage::Confront ||
                            m_story.stage == StoryStage::Sentence;
            enemy.tickEnabled = m_story.stage == StoryStage::Confront;
            if (m_story.stage <= StoryStage::Report)
            {
                const Point crime = m_storyNodes.at("crime");
                enemy.position = {crime.x + .2f, crime.y - .7f};
            }
            else if (!enemy.defeatedOnce && m_story.stage == StoryStage::Confront &&
                     !m_story.decisions.count("encounter_started"))
            {
                enemy.position = enemy.home;
                m_story.decisions.insert("encounter_started");
            }
            if (m_story.stage == StoryStage::Confront && enemy.health <= 0)
            {
                m_story.stage = StoryStage::Sentence;
                ShowNotice(m_text.Get("story.defeated"));
            }
            if (m_story.stage == StoryStage::Confront || m_story.stage == StoryStage::Sentence)
            {
                m_sceneGraph.Find("story/node/target")->SetLocalPosition(enemy.position);
            }
        }
    }
    if (m_story.stage == StoryStage::Witness)
    {
        Point local;
        if (!m_story.sceneActive && LevelLocal(m_storyNodes.at("crime"), local) &&
            std::hypot(local.x - m_player.x, local.y - m_player.y) < 2.6f)
        {
            m_story.sceneActive = true;
            m_story.sceneTime = 0;
            m_attackAnimation = 0;
            ShowNotice(m_text.Get("story.blocked"));
        }
        if (m_story.sceneActive)
        {
            m_story.sceneTime += dt;
            if (m_story.sceneTime >= 5)
            {
                m_story.sceneActive = false;
                m_story.stage = StoryStage::Report;
                RecordJustice("witness", 0, "journal.witness");
                ShowNotice(m_text.Get("story.report_prompt"));
            }
        }
    }
}

std::string CityScene::StoryObjectiveKey() const
{
    return "story.objective." + std::to_string(static_cast<int>(m_story.stage));
}

Point CityScene::StoryObjective() const
{
    if (m_story.stage == StoryStage::Confront || m_story.stage == StoryStage::Sentence)
    {
        for (const Enemy& enemy : m_enemies)
        {
            if (!enemy.tutorial)
            {
                return enemy.position;
            }
        }
    }
    const char* nodes[] =
        {"crime", "terminal", "family", "witness", "target", "target", "rebels", "rebels"};
    return m_storyNodes.at(nodes[static_cast<int>(m_story.stage)]);
}

bool CityScene::StoryObjectiveLocal(Point& local) const
{
    if (m_story.stage == StoryStage::Prepare && !m_levelComplete)
    {
        return LevelObjective(local);
    }
    return LevelLocal(StoryObjective(), local);
}

bool CityScene::InteractStory()
{
    const auto stage = m_story.stage;
    if (stage == StoryStage::Complete || stage == StoryStage::Witness)
    {
        return false;
    }
    Point local;
    if (!StoryObjectiveLocal(local) ||
        std::hypot(local.x - m_player.x, local.y - m_player.y) > 1.65f ||
        !ClearReach(m_player, local))
    {
        return false;
    }
    if (stage == StoryStage::Prepare && !m_levelComplete)
    {
        return false;
    }
    const auto next =
        std::vector<DialogueOption>{{"choice.continue", DialogueAction::ContinueStory}};
    if (stage == StoryStage::Report)
    {
        OpenDialogue("story.title", "story.suppressed", next);
    }
    else if (stage == StoryStage::Family)
    {
        OpenDialogue("story.name.family",
                     "story.family",
                     {{"choice.comfort", DialogueAction::ComfortFamily},
                      {"choice.ignore", DialogueAction::IgnoreFamily}});
    }
    else if (stage == StoryStage::Prepare)
    {
        OpenDialogue("story.name.witness",
                     "story.witness",
                     {{"choice.protect", DialogueAction::ProtectWitness},
                      {"choice.threaten", DialogueAction::ThreatenWitness}});
    }
    else if (stage == StoryStage::Confront)
    {
        ShowNotice(m_text.Get("story.confront"));
    }
    else if (stage == StoryStage::Sentence)
    {
        OpenDialogue("story.title",
                     "story.sentence",
                     {{"choice.restrain", DialogueAction::RestrainHeir},
                      {"choice.execute", DialogueAction::ExecuteHeir}});
    }
    else if (stage == StoryStage::Support)
    {
        OpenDialogue("story.name.rebels",
                     m_story.legitimacy >= 0 ? "story.rebels" : "story.rebels_dark",
                     {{"choice.finish_act", DialogueAction::FinishAct}});
    }
    return true;
}
