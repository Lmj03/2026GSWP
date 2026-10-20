#include "stdafx.h"
#include "CityScene.h"
#include <cmath>

void CityScene::HandlePointer(float x, float y)
{
    auto inside = [&](float left, float top, float width, float height)
    {
        return x >= left && x <= left + width && y >= top && y <= top + height;
    };
    if (m_dialogue.open)
    {
        if (inside(884, 165, 72, 32))
        {
            HandleInteractionInput(27);
            return;
        }
        for (size_t i = 0; i < m_dialogue.options.size(); ++i)
        {
            if (inside(324, 440 + static_cast<float>(i) * 42, 630, 36))
            {
                const DialogueAction action = m_dialogue.options[i].action;
                ChooseDialogue(action);
                return;
            }
        }
        return;
    }
    if (m_journalOpen)
    {
        if (inside(914, 124, 72, 32))
        {
            HandleInteractionInput(27);
        }
        else if (inside(810, 646, 70, 34))
        {
            HandleInteractionInput('1');
        }
        else if (inside(900, 646, 70, 34))
        {
            HandleInteractionInput('2');
        }
        return;
    }
    if (m_baseOpen)
    {
        if (inside(1000, 113, 72, 32))
        {
            HandleBaseInput(27);
        }
        else if (inside(212, 150, 856, 34))
        {
            HandleBaseInput(9);
        }
        else if (m_storagePage)
        {
            if (inside(212, 408, 840, 36))
            {
                HandleBaseInput('1');
            }
            else if (inside(212, 450, 840, 36))
            {
                HandleBaseInput('2');
            }
        }
        else
        {
            if (inside(212, 360, 398, 36))
            {
                HandleBaseInput('1');
            }
            else if (inside(212, 405, 398, 36))
            {
                HandleBaseInput('2');
            }
            for (int action = 3; action <= 5; ++action)
            {
                if (inside(660, 203 + (action - 3) * 110.f, 408, 36))
                {
                    HandleBaseInput(static_cast<unsigned char>('0' + action));
                    return;
                }
            }
        }
        return;
    }
    if (!m_interface)
    {
        return;
    }
    if (m_map)
    {
        const float left = (Renderer::Width - 440) * .5f;
        const float top = (Renderer::Height - 440) * .5f - 10;
        if (inside(left + 365, top + 446, 70, 32))
        {
            m_map = false;
        }
        return;
    }
    if (inside(Renderer::Width - 200, 24, 176, 200))
    {
        m_map = true;
        return;
    }
    if (inside(204, Renderer::Height - 60, 130, 28))
    {
        UseRecovery();
        return;
    }
    if (inside(24, 24, 362, 55))
    {
        ToggleJournal();
        return;
    }
    if (m_noticeTime > 0 && m_dialoguePages.size() > 1 &&
        inside(Renderer::Width * .5f - 310, Renderer::Height - 190, 620, 166))
    {
        Interact();
        return;
    }
    const int nearby = NearbyCitizen();
    if (nearby >= 0)
    {
        const Point p = Project(m_citizens[nearby].position.x, m_citizens[nearby].position.y, 76);
        if (inside(p.x - 150, p.y - 5, 300, 30))
        {
            Interact();
            return;
        }
    }
    const int interaction = NearbyLevelInteraction();
    if (interaction != -1)
    {
        Point local;
        if (LevelLocal(interaction == -3 ? m_spawn : m_supply, local))
        {
            const Point p = Project(local.x, local.y);
            if (inside(p.x - 85, p.y - 95, 170, 130))
            {
                InteractLevelOne();
            }
        }
    }
    for (const auto& node : m_storyNodes)
    {
        Point local;
        if (LevelLocal(node.second, local) &&
            std::hypot(local.x - m_player.x, local.y - m_player.y) < 1.65f)
        {
            const Point p = Project(local.x, local.y);
            if (inside(p.x - 100, p.y - 115, 200, 145))
            {
                InteractStory();
                return;
            }
        }
    }
}
