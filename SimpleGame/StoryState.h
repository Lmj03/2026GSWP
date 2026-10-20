#pragma once
#include "RenderTypes.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

enum class StoryStage
{
    Witness,
    Report,
    Family,
    Prepare,
    Confront,
    Sentence,
    Support,
    Complete
};
enum class DialogueAction
{
    Close,
    ContinueStory,
    ComfortFamily,
    IgnoreFamily,
    ProtectWitness,
    ThreatenWitness,
    RestrainHeir,
    ExecuteHeir,
    FinishAct,
    Ask,
    Help,
    Trade,
    Deliver,
    Intimidate,
    Apologize
};

struct DialogueOption
{
    std::string key;
    DialogueAction action;
};

struct StoryState
{
    StoryStage stage = StoryStage::Witness;
    float sceneTime = 0;
    bool sceneActive = false, familyMet = false, clueFound = false, heirExecuted = false;
    bool rebels = false;
    int legitimacy = 0;
    int minorGood = 0;
    std::set<std::string> decisions;
    std::vector<std::string> journal;

    bool Record(const std::string& id, int amount, const std::string& textKey)
    {
        if (!decisions.insert(id).second)
        {
            return false;
        }
        legitimacy = std::max(-100, std::min(100, legitimacy + amount));
        journal.push_back(textKey);
        return true;
    }
};

struct DialogueState
{
    bool open = false;
    std::string npcId, title, body;
    std::vector<DialogueOption> options;
};
