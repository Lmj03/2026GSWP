#pragma once
#include "Actor.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <iterator>

class SceneGraph
{
public:
    SceneGraph() = default;
    SceneGraph(const SceneGraph&) = delete;
    SceneGraph& operator=(const SceneGraph&) = delete;

    template <class T, class... Args>
    T& Create(const std::string& id, const std::string& parent, Args&&... args)
    {
        auto object = std::make_unique<T>(id, std::forward<Args>(args)...);
        T& result = *object;
        Attach(std::move(object), parent);
        return result;
    }

    Actor* Find(const std::string& id) const;
    bool Reparent(const std::string& id, const std::string& parent, bool keepWorldPosition = true);
    void Destroy(const std::string& id);
    void FlushDestroyed();
    void Update(float dt);
    void Draw(Renderer& renderer) const;
    size_t Size() const;

private:
    std::map<std::string, std::unique_ptr<Actor>> m_actors;
    std::uint64_t m_sequence = 0;
    bool m_updating = false;
    mutable bool m_drawing = false;
    void Attach(std::unique_ptr<Actor> actor, const std::string& parent);
};

// Non-owning typed index. Stable Actor addresses are owned only by SceneGraph.
template <class T> class ActorView
{
public:
    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        typename std::vector<T*>::const_iterator value;

        T& operator*() const
        {
            return **value;
        }

        T* operator->() const
        {
            return *value;
        }

        Iterator& operator++()
        {
            ++value;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator old = *this;
            ++value;
            return old;
        }

        bool operator==(const Iterator& other) const
        {
            return value == other.value;
        }

        bool operator!=(const Iterator& other) const
        {
            return value != other.value;
        }
    };

    Iterator begin() const
    {
        return {m_items.begin()};
    }

    Iterator end() const
    {
        return {m_items.end()};
    }

    T& operator[](size_t index) const
    {
        return *m_items[index];
    }

    void Add(T& actor)
    {
        m_items.push_back(&actor);
    }

    void clear()
    {
        m_items.clear();
    }

    size_t size() const
    {
        return m_items.size();
    }

    bool empty() const
    {
        return m_items.empty();
    }

    template <class Data> std::vector<Data> CopyData() const
    {
        std::vector<Data> result;
        for (T* actor : m_items)
        {
            result.push_back(static_cast<const Data&>(*actor));
        }
        return result;
    }

private:
    std::vector<T*> m_items;
};
