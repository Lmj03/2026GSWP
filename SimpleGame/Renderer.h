#pragma once
#include "Dependencies/glew.h"
#include <string>
#include <vector>
#include <memory>

struct Point
{
    float x, y;
    Point(float px = 0, float py = 0) : x(px), y(py) {}
};

struct Color
{
    float r, g, b, a;
    Color(float red = 1, float green = 1, float blue = 1, float alpha = 1)
        : r(red), g(green), b(blue), a(alpha) {}
    Color Alpha(float alpha) const { return Color(r, g, b, alpha); }
    Color Shade(float factor) const { return Color(r * factor, g * factor, b * factor, a); }
};

// Ordered OpenGL triangles and UTF-8 text in logical 1280x800 screen space.
class Renderer
{
public:
    static constexpr float Width = 1280.f;
    static constexpr float Height = 800.f;
    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool IsInitialized() const;
    void Resize(int width, int height);
    void Begin(Color background);
    void End();
    void Triangle(Point a, Point b, Point c, Color color);
    void Quad(Point a, Point b, Point c, Point d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Line(Point a, Point b, float width, Color color);
    void Ellipse(Point center, float rx, float ry, Color color);
    void Glow(Point center, float rx, float ry, Color color);
    void Text(float x, float y, const std::string& text, float scale, Color color);
    float MeasureText(const std::string& text, float scale);
    float TextWrapped(float x, float y, const std::string& text, float scale,
        Color color, float maxWidth);
    float WrappedHeight(const std::string& text, float scale, float maxWidth);
private:
    struct Vertex { float x, y, r, g, b, a, u, v; };
    struct Batch { GLuint texture; GLint first; GLsizei count; };
    struct FontCache;
    GLuint m_program = 0, m_vao = 0, m_vbo = 0;
    GLuint m_white = 0;
    std::unique_ptr<FontCache> m_font;
    std::vector<Vertex> m_vertices;
    std::vector<Batch> m_batches;
    void UseBatch(GLuint texture, GLsizei count);
    void TextLine(float x, float y, const std::wstring& text, float scale, Color color);
    std::vector<std::wstring> Wrap(const std::string& text, float scale, float maxWidth);
};

