#pragma once
#include "Actor.h"
class CityScene;

class StoryActor final : public Actor
{
public:
    StoryActor(const std::string& id, CityScene& scene);
    void Update(float dt) override;

private:
    CityScene& m_scene;
};

class StoryNodeActor final : public Actor
{
public:
    StoryNodeActor(const std::string& id, CityScene& scene, const std::string& node);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
    std::string m_node;
};

class DialogueActor final : public Actor
{
public:
    DialogueActor(const std::string& id, CityScene& scene);
    void Draw(Renderer& renderer) const override;

private:
    CityScene& m_scene;
};
