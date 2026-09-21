#include "stdafx.h"
#include "SceneGraph.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>

void SceneGraph::Attach(std::unique_ptr<Actor> actor, const std::string& parent)
{
    if (actor->Id().empty() || m_actors.count(actor->Id()))
    {
        throw std::invalid_argument("Actor ID must be nonempty and unique");
    }
    Actor* owner = parent.empty() ? nullptr : Find(parent);
    if (!parent.empty() && !owner)
    {
        throw std::invalid_argument("Actor parent does not exist");
    }
    actor->m_parent = owner;
    actor->m_graph = this;
    actor->m_sequence = ++m_sequence;
    const std::string id = actor->Id();
    m_actors.emplace(id, std::move(actor));
}

Actor* SceneGraph::Find(const std::string& id) const
{
    const auto found = m_actors.find(id);
    return found == m_actors.end() || found->second->PendingDestroy() ? nullptr
                                                                      : found->second.get();
}

bool SceneGraph::Reparent(const std::string& id, const std::string& parent, bool keepWorldPosition)
{
    Actor* actor = Find(id);
    Actor* owner = parent.empty() ? nullptr : Find(parent);
    if (!actor || (!parent.empty() && !owner))
    {
        return false;
    }
    for (Actor* ancestor = owner; ancestor; ancestor = ancestor->Parent())
    {
        if (ancestor == actor)
        {
            return false;
        }
    }
    const Point previous = actor->WorldPosition();
    actor->m_parent = owner;
    if (keepWorldPosition)
    {
        actor->SetWorldPosition(previous);
    }
    return true;
}

void SceneGraph::Destroy(const std::string& id)
{
    Actor* target = Find(id);
    if (!target)
    {
        return;
    }
    for (auto& entry : m_actors)
    {
        for (Actor* parent = entry.second.get(); parent; parent = parent->Parent())
        {
            if (parent == target)
            {
                entry.second->m_pendingDestroy = true;
                break;
            }
        }
    }
    // Lifetime ends only at FlushDestroyed, never while an Actor callback is on the stack.
}

void SceneGraph::FlushDestroyed()
{
    if (m_updating || m_drawing)
    {
        return;
    }
    for (auto it = m_actors.begin(); it != m_actors.end();)
    {
        if (it->second->PendingDestroy())
        {
            it = m_actors.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SceneGraph::Update(float dt)
{
    if (!std::isfinite(dt) || dt < 0 || m_updating || m_drawing)
    {
        return;
    }
    FlushDestroyed();
    std::vector<Actor*> actors;
    for (const auto& entry : m_actors)
    {
        if (entry.second->tickEnabled)
        {
            actors.push_back(entry.second.get());
        }
    }
    std::sort(actors.begin(),
              actors.end(),
              [](const Actor* a, const Actor* b)
              {
                  return a->updateOrder == b->updateOrder ? a->m_sequence < b->m_sequence
                                                          : a->updateOrder < b->updateOrder;
              });
    m_updating = true;
    try
    {
        for (Actor* actor : actors)
        {
            if (actor->tickEnabled && actor->EnabledInHierarchy())
            {
                actor->Update(dt);
            }
        }
    }
    catch (...)
    {
        m_updating = false;
        FlushDestroyed();
        throw;
    }
    m_updating = false;
    FlushDestroyed();
}

void SceneGraph::Draw(Renderer& renderer) const
{
    if (m_drawing || m_updating)
    {
        return;
    }

    // Callbacks may request deletion; keep every snapshot pointer alive until Draw returns.
    struct DrawingScope
    {
        bool& flag;

        explicit DrawingScope(bool& value)
            : flag(value)
        {
            flag = true;
        }

        ~DrawingScope()
        {
            flag = false;
        }
    } scope(m_drawing);

    struct Entry
    {
        const Actor* actor;
        float depth;
    };

    std::vector<Entry> actors;
    for (const auto& item : m_actors)
    {
        if (item.second->renderEnabled && item.second->VisibleInHierarchy())
        {
            const float depth = item.second->Depth();
            actors.push_back({item.second.get(), std::isfinite(depth) ? depth : 0});
        }
    }
    std::sort(actors.begin(),
              actors.end(),
              [](const Entry& a, const Entry& b)
              {
                  if (a.actor->layer != b.actor->layer)
                  {
                      return a.actor->layer < b.actor->layer;
                  }
                  if (a.depth != b.depth)
                  {
                      return a.depth < b.depth;
                  }
                  return a.actor->drawOrder == b.actor->drawOrder
                             ? a.actor->m_sequence < b.actor->m_sequence
                             : a.actor->drawOrder < b.actor->drawOrder;
              });
    for (const Entry& entry : actors)
    {
        if (!entry.actor->renderEnabled || !entry.actor->VisibleInHierarchy())
        {
            continue;
        }
        if (entry.actor->layer >= RenderLayer::Interface)
        {
            renderer.BeginOverlay();
        }
        entry.actor->Draw(renderer);
    }
}

size_t SceneGraph::Size() const
{
    return m_actors.size();
}
