#pragma once
#include "RenderTypes.h"
#include <cstdint>
#include <string>

class Renderer;
class SceneGraph;

enum class RenderLayer
{
    Terrain,
    Ground,
    World,
    Air,
    Weather,
    Interface,
    Modal
};

// One logical scene object. Mesh vertices and font glyphs remain renderer primitives.
class Actor
{
public:
    explicit Actor(std::string id);
    virtual ~Actor() = default;
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    const std::string& Id() const;
    Actor* Parent() const;
    SceneGraph& Graph() const;
    virtual Point LocalPosition() const;
    virtual void SetLocalPosition(Point position);
    Point WorldPosition() const;
    void SetWorldPosition(Point position);
    bool EnabledInHierarchy() const;
    bool VisibleInHierarchy() const;
    bool PendingDestroy() const;
    void Destroy();
    virtual void Update(float dt);
    virtual void Draw(Renderer& renderer) const;
    virtual float Depth() const;

    bool enabled = true;
    bool visible = true;
    bool tickEnabled = false;
    bool renderEnabled = false;
    int updateOrder = 0;
    int drawOrder = 0;
    RenderLayer layer = RenderLayer::World;
    float depthOffset = 0;

protected:
    Point m_position;

private:
    friend class SceneGraph;
    std::string m_id;
    Actor* m_parent = nullptr;
    SceneGraph* m_graph = nullptr;
    bool m_pendingDestroy = false;
    std::uint64_t m_sequence = 0;
};
