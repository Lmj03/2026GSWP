#include "stdafx.h"
#include "Actor.h"
#include "SceneGraph.h"
#include <utility>
#include <stdexcept>

Actor::Actor(std::string id)
    : m_id(std::move(id))
{
}

const std::string& Actor::Id() const
{
    return m_id;
}

Actor* Actor::Parent() const
{
    return m_parent;
}

SceneGraph& Actor::Graph() const
{
    if (!m_graph)
    {
        throw std::logic_error("Actor is not attached to a scene graph");
    }
    return *m_graph;
}

Point Actor::LocalPosition() const
{
    return m_position;
}

void Actor::SetLocalPosition(Point position)
{
    m_position = position;
}

Point Actor::WorldPosition() const
{
    const Point local = LocalPosition();
    const Point parent = m_parent ? m_parent->WorldPosition() : Point();
    return {parent.x + local.x, parent.y + local.y};
}

void Actor::SetWorldPosition(Point position)
{
    const Point parent = m_parent ? m_parent->WorldPosition() : Point();
    SetLocalPosition({position.x - parent.x, position.y - parent.y});
}

bool Actor::EnabledInHierarchy() const
{
    return enabled && !m_pendingDestroy && (!m_parent || m_parent->EnabledInHierarchy());
}

bool Actor::VisibleInHierarchy() const
{
    return visible && EnabledInHierarchy() && (!m_parent || m_parent->VisibleInHierarchy());
}

bool Actor::PendingDestroy() const
{
    return m_pendingDestroy;
}

void Actor::Destroy()
{
    Graph().Destroy(Id());
}

void Actor::Update(float)
{
}

void Actor::Draw(Renderer&) const
{
}

float Actor::Depth() const
{
    const Point position = WorldPosition();
    return position.x + position.y + depthOffset;
}
