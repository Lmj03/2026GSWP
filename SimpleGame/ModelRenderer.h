#pragma once
#include "ModelAsset.h"
#include "RenderTypes.h"
#include <memory>

// GL resources live on the render thread, independently of CPU imports/Actor lifetime.
class ModelRenderer
{
public:
    ModelRenderer();
    ~ModelRenderer();
    bool Draw(const std::shared_ptr<const Models::Asset>& asset,
              const Models::Animator& animation,
              Point foot,
              float pixelsPerUnit,
              float yaw,
              const Models::DrawStyle& style,
              bool hdr,
              float emissionGain,
              float width,
              float height);

private:
    struct State;
    std::unique_ptr<State> m_state;
};
