#include "stdafx.h"
#include "CityScene.h"
#include <algorithm>
#include <limits>

void CityScene::OpenDialogue(const std::string& title,
                             const std::string& body,
                             const std::vector<DialogueOption>& options,
                             const std::string& npc)
{
    m_dialogue.open = true;
    m_dialogue.npcId = npc;
    m_dialogue.title = m_text.Get(title);
    m_dialogue.body = m_text.Get(body);
    m_dialogue.options = options;
    m_noticeTime = 0;
    m_dialoguePages.clear();
    m_attackAnimation = 0;
}

bool CityScene::HandleInteractionInput(unsigned char key)
{
    if (m_journalOpen)
    {
        if (key == 'j' || key == 27)
        {
            m_journalOpen = false;
        }
        if (key == '1' && m_journalPage > 0)
        {
            --m_journalPage;
        }
        if (key == '2' && (m_journalPage + 1) * 5 < m_story.journal.size())
        {
            ++m_journalPage;
        }
        return true;
    }
    if (!m_dialogue.open)
    {
        return false;
    }
    if (key == 27 || key == 'e')
    {
        m_dialogue = DialogueState{};
        return true;
    }
    if (key >= '1' && key <= '3')
    {
        const size_t index = key - '1';
        if (index < m_dialogue.options.size())
        {
            ChooseDialogue(m_dialogue.options[index].action);
        }
    }
    return true;
}

void CityScene::ToggleJournal()
{
    if (!m_baseOpen && !m_dialogue.open && !m_story.sceneActive)
    {
        m_journalOpen = !m_journalOpen;
        m_journalPage = m_story.journal.empty() ? 0 : (m_story.journal.size() - 1) / 5;
    }
}

void CityScene::ChooseDialogue(DialogueAction action)
{
    const std::string id = m_dialogue.npcId;
    if (action == DialogueAction::ProtectWitness && m_recovery == 0)
    {
        m_dialogue.body = m_text.Get("npc.need_medkit");
        return;
    }
    m_dialogue = DialogueState{};
    switch (action)
    {
    case DialogueAction::Close:
        return;
    case DialogueAction::ContinueStory:
        if (m_story.stage == StoryStage::Report)
        {
            RecordJustice("suppressed", 0, "journal.suppressed");
            m_story.stage = StoryStage::Family;
        }
        return;
    case DialogueAction::ComfortFamily:
    case DialogueAction::IgnoreFamily:
        if (m_story.stage != StoryStage::Family)
        {
            return;
        }
        m_story.familyMet = true;
        RecordJustice("family",
                      action == DialogueAction::ComfortFamily ? 8 : -4,
                      action == DialogueAction::ComfortFamily ? "journal.family_help"
                                                              : "journal.family_ignore");
        m_story.stage = StoryStage::Prepare;
        ShowNotice(m_text.Get("story.prepare"));
        return;
    case DialogueAction::ProtectWitness:
    case DialogueAction::ThreatenWitness:
        if (m_story.stage != StoryStage::Prepare || !m_levelComplete)
        {
            return;
        }
        if (action == DialogueAction::ProtectWitness)
        {
            --m_recovery;
        }
        RecordJustice("witness_choice",
                      action == DialogueAction::ProtectWitness ? 10 : -12,
                      action == DialogueAction::ProtectWitness ? "journal.protect"
                                                               : "journal.threaten");
        m_story.clueFound = true;
        m_story.stage = StoryStage::Confront;
        ShowNotice(m_text.Get("story.medical_link"));
        return;
    case DialogueAction::RestrainHeir:
    case DialogueAction::ExecuteHeir:
        if (m_story.stage != StoryStage::Sentence)
        {
            return;
        }
        m_story.heirExecuted = action == DialogueAction::ExecuteHeir;
        RecordJustice("sentence",
                      m_story.heirExecuted ? -10 : 8,
                      m_story.heirExecuted ? "journal.execute" : "journal.restrain");
        m_story.stage = StoryStage::Support;
        m_story.rebels = true;
        m_reputation += 20;
        AddExperience(8);
        ShowNotice(m_text.Get("story.fame"));
        return;
    case DialogueAction::FinishAct:
        if (m_story.stage == StoryStage::Support)
        {
            m_story.stage = StoryStage::Complete;
            RecordJustice("act_complete", 0, "journal.rebellion");
            ShowNotice(m_text.Get("story.act_complete"));
        }
        return;
    default:
        break;
    }
    auto found = std::find_if(m_citizens.begin(),
                              m_citizens.end(),
                              [&](const Citizen& npc)
                              {
                                  return npc.id == id;
                              });
    if (found == m_citizens.end() || !found->active || found->memory.health <= 0)
    {
        return;
    }
    Citizen& npc = *found;
    if (action == DialogueAction::Ask)
    {
        ShowNotice(m_text.Get("npc.info." + std::to_string(static_cast<int>(npc.occupation))));
    }
    else if (action == DialogueAction::Trade)
    {
        if (m_credits < 8 || m_recovery == UINT64_MAX)
        {
            ShowNotice(m_text.Get("npc.trade_failed"));
        }
        else
        {
            m_credits -= 8;
            ++m_recovery;
            ++npc.memory.services;
            npc.memory.notoriety = std::max(0, npc.memory.notoriety - 1);
            ShowNotice(m_text.Get("npc.traded"));
        }
    }
    else if (action == DialogueAction::Help)
    {
        if (npc.memory.helped)
        {
            ShowNotice(m_text.Get("npc.already_helped"));
        }
        else if (m_recovery == 0)
        {
            ShowNotice(m_text.Get("npc.need_medkit"));
        }
        else
        {
            --m_recovery;
            npc.memory.helped = true;
            npc.memory.health = 36;
            npc.memory.trust = std::min(100, npc.memory.trust + 20);
            npc.remembersHelp = true;
            m_helped = true;
            m_reputation += 2;
            m_remembered.insert(id);
            RecordJustice("npc/help/" + id, 2, "journal.help");
            ShowNotice(m_text.Get("npc.helped"));
        }
    }
    else if (action == DialogueAction::Deliver)
    {
        if (npc.memory.helped)
        {
            ShowNotice(m_text.Get("npc.already_helped"));
        }
        else if (m_bag.scrap < 2 || m_credits > UINT64_MAX - 5)
        {
            ShowNotice(m_text.Get("npc.need_scrap"));
        }
        else
        {
            m_bag.scrap -= 2;
            m_credits += 5;
            npc.memory.helped = true;
            npc.memory.trust = std::min(100, npc.memory.trust + 15);
            RecordJustice("npc/delivery/" + id, 2, "journal.delivery");
            ShowNotice(m_text.Get("npc.delivered"));
        }
    }
    else if (action == DialogueAction::Intimidate)
    {
        npc.memory.trust = std::max(-100, npc.memory.trust - 30);
        RecordJustice("npc/intimidate/" + id, -10, "journal.intimidate");
        ShowNotice(m_text.Get("npc.intimidated"));
    }
    else if (action == DialogueAction::Apologize)
    {
        const bool harmed = m_story.decisions.count("npc/harm/" + id) ||
                            m_story.decisions.count("npc/intimidate/" + id);
        if (!harmed || npc.memory.apology || m_credits < 5)
        {
            ShowNotice(m_text.Get("npc.apology_failed"));
        }
        else
        {
            m_credits -= 5;
            npc.memory.apology = true;
            npc.memory.trust = std::min(0, npc.memory.trust + 25);
            RecordJustice("npc/apology/" + id, 4, "journal.apology");
            ShowNotice(m_text.Get("npc.apologized"));
        }
    }
    npc.hostile = npc.memory.notoriety >= 60 || npc.memory.trust <= -40;
    RememberNpc(npc);
}

void CityScene::OpenNpcDialogue(Citizen& npc)
{
    if (npc.memory.health <= 0)
    {
        ShowNotice(m_text.Get("npc.defeated"));
        return;
    }
    if (npc.hostile)
    {
        if (npc.memory.notoriety < 60 && (m_story.decisions.count("npc/harm/" + npc.id) ||
                                          m_story.decisions.count("npc/intimidate/" + npc.id)))
        {
            OpenDialogue("npc.dialogue_title",
                         "npc.hostile_talk",
                         {{"choice.apologize", DialogueAction::Apologize},
                          {"choice.close", DialogueAction::Close}},
                         npc.id);
            m_dialogue.title = npc.name;
            return;
        }
        ShowNotice(m_text.Get("npc.hostile"));
        return;
    }
    std::vector<DialogueOption> options;
    options.push_back({"choice.ask", DialogueAction::Ask});
    if (npc.occupation == Occupation::Merchant || npc.occupation == Occupation::Medic)
    {
        options.push_back({"choice.trade", DialogueAction::Trade});
    }
    else if (npc.occupation == Occupation::Worker || npc.occupation == Occupation::Mechanic)
    {
        options.push_back({"choice.deliver", DialogueAction::Deliver});
    }
    else
    {
        options.push_back({"choice.help", DialogueAction::Help});
    }
    options.push_back(npc.memory.trust < 0
                          ? DialogueOption{"choice.apologize", DialogueAction::Apologize}
                          : DialogueOption{"choice.intimidate", DialogueAction::Intimidate});
    OpenDialogue("npc.dialogue_title", "npc.intro", options, npc.id);
    m_dialogue.title = npc.name;
    m_dialogue.body = m_text.Format(
        "npc.intro",
        {{"job", m_text.Get("npc.job." + std::to_string(static_cast<int>(npc.occupation)))},
         {"activity", m_text.Get("npc.activity." + std::to_string(static_cast<int>(npc.activity)))},
         {"notoriety", std::to_string(npc.memory.notoriety)},
         {"trust", std::to_string(npc.memory.trust)}});
    const std::string key = npc.remembersHelp ? npc.dialogueRemembered : npc.dialogueNeutral;
    if (!key.empty())
    {
        m_dialogue.body += " " + m_text.Get(key);
    }
    else
    {
        m_dialogue.body +=
            " " + m_text.Get(npc.memory.trust >= 15 ? "npc.friendly" : "npc.neutral");
    }
}
