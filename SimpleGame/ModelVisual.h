#pragma once
#include "Actor.h"
#include "ModelLibrary.h"
#include <functional>

// Reusable component for existing gameplay Actors. Animation state is never shared.
class ModelVisual
{
public:
    bool Bind(ModelLibrary& library, const std::string& id);
    void Update(const std::string& clip, float dt, float speed = 1, bool restart = false);
    bool Draw(Renderer& renderer,
              Point screenFoot,
              float pixelsPerUnit,
              const Models::DrawStyle& style = Models::DrawStyle()) const;
    Models::Animator& Animation();
    const Models::Asset* Asset() const;
    float yaw = 0;

private:
    std::string m_id;
    std::shared_ptr<const Models::Asset> m_asset;
    Models::Animator m_animation;
};

// Optional props/preview actors use the same SceneGraph coordinates and painter order.
// The projector converts a small scene-space WorldPosition, including parent translation.
class ModelActor final : public Actor
{
public:
    using Projector = std::function<Point(Point)>;
    ModelActor(const std::string& id,
               ModelLibrary& library,
               const std::string& assetId,
               Projector projector);
    void Update(float dt) override;
    void Draw(Renderer& renderer) const override;
    ModelVisual visual;
    std::string clip = "idle";
    float pixelsPerUnit = 18;
    float speed = 1;
    bool paused = false;

private:
    Projector m_projector;
};
