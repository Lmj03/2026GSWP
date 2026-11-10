#include "stdafx.h"
#include "ModelVisual.h"
#include "Renderer.h"

bool ModelVisual::Bind(ModelLibrary& library, const std::string& id)
{
    if (m_id == id)
    {
        return bool(m_asset);
    }
    SetAsset(library.Get(id));
    m_id = id;
    return bool(m_asset);
}

void ModelVisual::SetAsset(std::shared_ptr<const Models::Asset> asset)
{
    m_id.clear();
    m_asset = std::move(asset);
    m_animation = Models::Animator();
    if (m_asset)
    {
        m_animation.SetAsset(*m_asset);
        m_animation.Play("idle", 0);
    }
}

void ModelVisual::Update(const std::string& clip, float dt, float speed, bool restart)
{
    if (m_asset)
    {
        // Missing optional actions fall back to idle without repeatedly loading files.
        if (!m_animation.Play(clip, -1, restart))
        {
            m_animation.Play("idle");
        }
        m_animation.Update(dt, speed);
    }
}

bool ModelVisual::Draw(Renderer& renderer,
                       Point screenFoot,
                       float pixelsPerUnit,
                       const Models::DrawStyle& style) const
{
    return renderer.Model(m_asset, m_animation, screenFoot, pixelsPerUnit, yaw, style);
}

Models::Animator& ModelVisual::Animation()
{
    return m_animation;
}

const Models::Asset* ModelVisual::Asset() const
{
    return m_asset.get();
}

ModelActor::ModelActor(const std::string& id,
                       ModelLibrary& library,
                       const std::string& assetId,
                       Projector projector)
    : Actor(id),
      m_projector(std::move(projector))
{
    tickEnabled = renderEnabled = true;
    visual.Bind(library, assetId);
}

void ModelActor::Update(float dt)
{
    if (!paused)
    {
        visual.Update(clip, dt, speed);
    }
}

void ModelActor::Draw(Renderer& renderer) const
{
    if (m_projector)
    {
        visual.Draw(renderer, m_projector(WorldPosition()), pixelsPerUnit);
    }
}
