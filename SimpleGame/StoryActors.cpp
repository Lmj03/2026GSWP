#include "stdafx.h"
#include "StoryActors.h"
#include "CityScene.h"

StoryActor::StoryActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    tickEnabled = true;
    updateOrder = 5;
}

void StoryActor::Update(float dt)
{
    m_scene.UpdateStory(dt);
}

StoryNodeActor::StoryNodeActor(const std::string& id, CityScene& scene, const std::string& node)
    : Actor(id),
      m_scene(scene),
      m_node(node)
{
    renderEnabled = true;
}

void StoryNodeActor::Draw(Renderer& renderer) const
{
    m_scene.DrawStoryNode(renderer, m_node, WorldPosition());
}

DialogueActor::DialogueActor(const std::string& id, CityScene& scene)
    : Actor(id),
      m_scene(scene)
{
    renderEnabled = true;
    layer = RenderLayer::Modal;
    depthOffset = 20;
}

void DialogueActor::Draw(Renderer& renderer) const
{
    m_scene.DrawDialogue(renderer);
    m_scene.DrawJournal(renderer);
}
