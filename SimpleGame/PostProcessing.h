#pragma once
#include "Dependencies/glew.h"

// Internal HDR rendering, tone mapped to an ordinary SDR window.
class PostProcessing
{
public:
    struct Settings
    {
        bool enabled = true, bloom = true, vignette = true, edgeBlur = true;
        float exposure = 1.15f, emissionGain = 1.f;
        float bloomStrength = .28f, bloomThreshold = 1.f, bloomKnee = .5f;
        float bloomRadius = 1.8f;
        int bloomIterations = 3;
        float vignetteStrength = .38f, vignetteStart = .32f, vignetteEnd = 1.4f;
        float edgeStrength = .65f, edgeStart = .55f, edgeEnd = 1.3f, edgeRadius = 2.6f;
    };
    enum class Effect
    {
        All,
        Bloom,
        Vignette,
        EdgeBlur
    };
    ~PostProcessing();
    bool Initialize(int width, int height);
    bool Resize(int width, int height);
    bool Ready() const;
    bool ReloadSettings();
    void Toggle(Effect effect);
    void AdjustExposure(float amount);

    const Settings& GetSettings() const
    {
        return m_settings;
    }

    void BeginScene(float r, float g, float b);
    void Present(int x, int y, int width, int height);

private:
    struct Target
    {
        GLuint framebuffer = 0, texture = 0;
    };

    struct Targets
    {
        Target scene, bloom[2], edge[2];
        int width = 0, height = 0, halfWidth = 0, halfHeight = 0;
    };

    struct ExtractUniforms
    {
        GLint source = -1, texel = -1, threshold = -1, knee = -1, extract = -1;
    };

    struct BlurUniforms
    {
        GLint source = -1, step = -1;
    };

    struct CompositeUniforms
    {
        GLint scene = -1, bloom = -1, edge = -1, exposure = -1, bloomStrength = -1;
        GLint vignetteStrength = -1, vignetteStart = -1, vignetteEnd = -1;
        GLint edgeStrength = -1, edgeStart = -1, edgeEnd = -1;
    };

    Settings m_settings;
    Targets m_targets;
    GLuint m_vao = 0, m_extract = 0, m_blur = 0, m_composite = 0;
    ExtractUniforms m_extractUniforms;
    BlurUniforms m_blurUniforms;
    CompositeUniforms m_compositeUniforms;
    static bool CreateTarget(Target& target, int width, int height);
    static void DestroyTarget(Target& target);
    static void DestroyTargets(Targets& targets);
    void Downsample(GLuint source, const Target& target, bool extract);
    void Blur(GLuint source, const Target& target, bool horizontal, float radius);
};
