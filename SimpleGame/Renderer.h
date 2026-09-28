#pragma once
#include "Dependencies/glew.h"
#include <string>
#include <vector>
#include <memory>
#include <array>
#include "PostProcessing.h"

#include "RenderTypes.h"

// Ordered OpenGL triangles and UTF-8 text in logical 1280x800 screen space.
class Renderer
{
public:
    static constexpr float Width = 1280.f;
    static constexpr float Height = 800.f;
    static constexpr float TextScale = 1.3f;
    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool IsInitialized() const;
    void Resize(int width, int height);
    void Begin(Color background);
    void BeginOverlay();
    void End();

    const PostProcessing::Settings& PostSettings() const
    {
        return m_post.GetSettings();
    }

    bool PostAvailable() const
    {
        return m_post.Ready();
    }

    void TogglePostProcessing(PostProcessing::Effect effect)
    {
        m_post.Toggle(effect);
    }

    void AdjustExposure(float amount)
    {
        m_post.AdjustExposure(amount);
    }

    bool ReloadPostProcessing()
    {
        return m_post.ReloadSettings();
    }

    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Ellipse(Point center, float rx, float ry, Color color);
    void Glow(Point center, float rx, float ry, Color color);
    void Text(float x, float y, const std::string& text, float scale, Color color);
    float MeasureText(const std::string& text, float scale);
    void TextFitted(float x,
                    float y,
                    const std::string& text,
                    float scale,
                    Color color,
                    float maxWidth);
    float TextWrapped(float x,
                      float y,
                      const std::string& text,
                      float scale,
                      Color color,
                      float maxWidth);
    float WrappedHeight(const std::string& text, float scale, float maxWidth);

private:
    struct Instance
    {
        Point a, b, c, d;
        Color color;
        float textureSlot, ellipse;
    };

    struct Batch
    {
        std::array<GLuint, 8> textures = {};
        int textureCount = 0;
        size_t first = 0;
        GLsizei count = 0;
    };
    struct FontCache;
    GLuint m_program = 0, m_vao = 0, m_vbo = 0, m_meshVbo = 0;
    GLuint m_fallbackTexture = 0;
    PostProcessing m_post;
    GLint m_linearOutput = -1, m_emissionGain = -1;
    int m_viewX = 0, m_viewY = 0, m_viewWidth = static_cast<int>(Width),
        m_viewHeight = static_cast<int>(Height);
    bool m_hdrScene = false, m_overlay = false;
    std::unique_ptr<FontCache> m_font;
    std::vector<Instance> m_instances;
    std::vector<Batch> m_batches;
    size_t m_gpuCapacity = 0;
    bool m_legacyStarted = false;
    GLuint m_legacyTexture = 0;
    std::string m_profileSettings;
    int m_textureSlots = 8;
    int m_batchCapacity = 16384;
    bool m_culling = true;
    void LoadBatchSettings();
    void Queue(Point a,
               Point b,
               Point c,
               Point d,
               Color color,
               GLuint texture,
               bool ellipse,
               unsigned int legacyVertices);
    void BindInstanceAttributes(size_t first);
    void Flush();
    void TextLine(float x, float y, const std::wstring& text, float scale, Color color);
    std::vector<std::wstring> Wrap(const std::string& text, float scale, float maxWidth);
};
