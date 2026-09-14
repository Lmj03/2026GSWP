#include "stdafx.h"
#include "Renderer.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>

namespace
{
std::wstring Decode(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8,
                                          MB_ERR_INVALID_CHARS,
                                          text.data(),
                                          static_cast<int>(text.size()),
                                          nullptr,
                                          0);
    if (!count)
    {
        return L"\ufffd";
    }
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        &result[0],
                        count);
    return result;
}

GLuint Shader(GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    if (!shader)
    {
        return 0;
    }
    const char* code = source.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "셰이더 컴파일 실패: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}

// Windows only rasterizes text into memory. All onscreen composition uses OpenGL.
struct Renderer::FontCache
{
    struct Image
    {
        GLuint texture = 0;
        int width = 0, height = 0;
        std::uint64_t used = 0;
    };

    HDC dc = nullptr;
    HFONT font = nullptr;
    HGDIOBJ previousFont = nullptr;
    std::map<std::wstring, Image> images;
    std::uint64_t frame = 0;
    int lineHeight = 32;
    GLint maxTexture = 4096;

    FontCache()
    {
        dc = CreateCompatibleDC(nullptr);
        font = CreateFontW(-32,
                           0,
                           0,
                           0,
                           FW_NORMAL,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE,
                           L"Malgun Gothic");
        if (!dc || !font)
        {
            return;
        }
        previousFont = SelectObject(dc, font);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, RGB(0, 0, 0));
        SetBkMode(dc, OPAQUE);
        TEXTMETRICW metrics = {};
        if (GetTextMetricsW(dc, &metrics))
        {
            lineHeight = metrics.tmHeight;
        }
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    }

    ~FontCache()
    {
        for (const auto& item : images)
        {
            glDeleteTextures(1, &item.second.texture);
        }
        if (dc && previousFont)
        {
            SelectObject(dc, previousFont);
        }
        if (font)
        {
            DeleteObject(font);
        }
        if (dc)
        {
            DeleteDC(dc);
        }
    }

    int Width(const std::wstring& text) const
    {
        SIZE size = {};
        if (dc && !text.empty())
        {
            GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
        }
        return size.cx;
    }

    void NextFrame()
    {
        ++frame;
        // Eviction occurs before recording draw commands, never midway through a batch.
        while (images.size() > 192)
        {
            auto oldest = std::min_element(images.begin(),
                                           images.end(),
                                           [](const auto& a, const auto& b)
                                           {
                                               return a.second.used < b.second.used;
                                           });
            glDeleteTextures(1, &oldest->second.texture);
            images.erase(oldest);
        }
    }

    const Image* Raster(const std::wstring& text)
    {
        auto found = images.find(text);
        if (found != images.end())
        {
            found->second.used = frame;
            return &found->second;
        }
        Image result;
        result.width = std::max(1, std::min(maxTexture, Width(text) + 4));
        result.height = lineHeight + 4;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = result.width;
        info.bmiHeader.biHeight = -result.height; // top-down pixels match our UV coordinates
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || !pixels)
        {
            if (bitmap)
            {
                DeleteObject(bitmap);
            }
            return nullptr;
        }
        const HGDIOBJ previousBitmap = SelectObject(dc, bitmap);
        std::memset(pixels, 0, static_cast<size_t>(result.width) * result.height * 4);
        RECT bounds = {2, 2, result.width, result.height};
        DrawTextW(dc,
                  text.c_str(),
                  static_cast<int>(text.size()),
                  &bounds,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        GdiFlush();
        std::vector<unsigned char> coverage(static_cast<size_t>(result.width) * result.height);
        const auto bytes = static_cast<const unsigned char*>(pixels);
        for (size_t i = 0; i < coverage.size(); ++i)
        {
            coverage[i] = bytes[i * 4];
        }
        SelectObject(dc, previousBitmap);
        DeleteObject(bitmap);
        glGenTextures(1, &result.texture);
        glBindTexture(GL_TEXTURE_2D, result.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_R8,
                     result.width,
                     result.height,
                     0,
                     GL_RED,
                     GL_UNSIGNED_BYTE,
                     coverage.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        result.used = frame;
        return &images.emplace(text, result).first->second;
    }
};

Renderer::Renderer(int width, int height)
{
    Resize(width, height);
    std::string vs, fs;
    if (!ReadResource(L"Shaders/SolidRect.vs", vs) || !ReadResource(L"Shaders/SolidRect.fs", fs))
    {
        std::cerr << "실행 파일 옆의 Shaders 폴더를 찾을 수 없습니다.\n";
        return;
    }
    const GLuint vertex = Shader(GL_VERTEX_SHADER, vs), fragment = Shader(GL_FRAGMENT_SHADER, fs);
    if (!vertex || !fragment)
    {
        if (vertex)
        {
            glDeleteShader(vertex);
        }
        if (fragment)
        {
            glDeleteShader(fragment);
        }
        return;
    }
    m_program = glCreateProgram();
    if (m_program)
    {
        glAttachShader(m_program, vertex);
        glAttachShader(m_program, fragment);
        glLinkProgram(m_program);
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (!m_program)
    {
        return;
    }
    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[4096] = {};
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        std::cerr << "셰이더 연결 실패: " << log << '\n';
        glDeleteProgram(m_program);
        m_program = 0;
        return;
    }
    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "u_Coverage"), 0);
    m_linearOutput = glGetUniformLocation(m_program, "u_LinearOutput");
    m_emissionGain = glGetUniformLocation(m_program, "u_EmissionGain");
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, emission)));
    glBindVertexArray(0);
    glGenTextures(1, &m_white);
    glBindTexture(GL_TEXTURE_2D, m_white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const unsigned char white = 255;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    m_font.reset(new FontCache());
    if (!m_font->dc || !m_font->font)
    {
        std::cerr << "한글 글꼴 초기화 실패.\n";
    }
    m_vertices.reserve(180000);
    if (!m_post.Initialize(m_viewWidth, m_viewHeight))
    {
        std::cerr << "후처리를 초기화하지 못해 기본 렌더링으로 진행합니다.\n";
    }
}

Renderer::~Renderer()
{
    m_font.reset();
    if (m_white)
    {
        glDeleteTextures(1, &m_white);
    }
    if (m_vbo)
    {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao)
    {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_program)
    {
        glDeleteProgram(m_program);
    }
}

bool Renderer::IsInitialized() const
{
    return m_program && m_vao && m_vbo && m_white && m_font && m_font->dc && m_font->font;
}

void Renderer::Resize(int width, int height)
{
    width = std::max(1, width);
    height = std::max(1, height);
    const float scale = std::min(width / Width, height / Height);
    m_viewWidth = std::max(1, static_cast<int>(Width * scale));
    m_viewHeight = std::max(1, static_cast<int>(Height * scale));
    m_viewX = (width - m_viewWidth) / 2;
    m_viewY = (height - m_viewHeight) / 2;
    if (m_post.Ready())
    {
        m_post.Resize(m_viewWidth, m_viewHeight);
    }
    glViewport(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
}

void Renderer::Begin(Color background)
{
    m_vertices.clear();
    m_batches.clear();
    if (m_font)
    {
        m_font->NextFrame();
    }
    m_overlay = false;
    m_hdrScene = m_post.Ready() && m_post.GetSettings().enabled;
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
    glClearColor(0, 0, 0, 1); // clear letterbox bars independently of the scene target
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    if (m_hdrScene)
    {
        auto linear = [](float value)
        {
            return value <= .04045f ? value / 12.92f : std::pow((value + .055f) / 1.055f, 2.4f);
        };
        m_post.BeginScene(linear(background.r), linear(background.g), linear(background.b));
    }
    else
    {
        Rect(0, 0, Width, Height, background);
    }
}

void Renderer::Flush()
{
    if (!IsInitialized() || m_vertices.empty())
    {
        return;
    }
    glUseProgram(m_program);
    glUniform1i(m_linearOutput, m_hdrScene ? 1 : 0);
    glUniform1f(m_emissionGain, m_post.GetSettings().emissionGain);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(),
                 GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    for (const Batch& batch : m_batches)
    {
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
    }
    glBindVertexArray(0);
    m_vertices.clear();
    m_batches.clear();
}

void Renderer::BeginOverlay()
{
    if (m_overlay)
    {
        return;
    }
    Flush();
    if (m_hdrScene)
    {
        m_post.Present(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
    }
    m_hdrScene = false;
    m_overlay = true;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::End()
{
    BeginOverlay(); // Also resolve the world when the HUD is hidden.
    Flush();
}

void Renderer::UseBatch(GLuint texture, GLsizei count)
{
    if (m_batches.empty() || m_batches.back().texture != texture)
    {
        m_batches.push_back({texture, static_cast<GLint>(m_vertices.size()), 0});
    }
    m_batches.back().count += count;
}

void Renderer::Triangle(Point a, Point b, Point c, Color color)
{
    UseBatch(m_white, 3);
    for (const Point& p : {a, b, c})
    {
        m_vertices.push_back(
            {p.x, p.y, color.r, color.g, color.b, color.a, .5f, .5f, color.emission});
    }
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color color)
{
    Triangle(a, b, c, color);
    Triangle(a, c, d, color);
}

void Renderer::Rect(float x, float y, float width, float height, Color color)
{
    Quad({x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}, color);
}

void Renderer::Line(Point a, Point b, float width, Color color)
{
    const float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy);
    if (length < .001f)
    {
        return;
    }
    const float nx = -dy / length * width * .5f, ny = dx / length * width * .5f;
    Quad({a.x + nx, a.y + ny},
         {b.x + nx, b.y + ny},
         {b.x - nx, b.y - ny},
         {a.x - nx, a.y - ny},
         color);
}

void Renderer::Ellipse(Point center, float rx, float ry, Color color)
{
    for (int i = 0; i < 20; ++i)
    {
        const float a = i * 6.2831853f / 20, b = (i + 1) * 6.2831853f / 20;
        Triangle(center,
                 {center.x + std::cos(a) * rx, center.y + std::sin(a) * ry},
                 {center.x + std::cos(b) * rx, center.y + std::sin(b) * ry},
                 color);
    }
}

void Renderer::Glow(Point center, float rx, float ry, Color color)
{
    for (int i = 6; i > 0; --i)
    {
        Ellipse(center, rx * i / 6.f, ry * i / 6.f, color.Alpha(color.a * .055f));
    }
}

void Renderer::TextLine(float x, float y, const std::wstring& text, float scale, Color color)
{
    if (!m_font || text.empty() || scale <= 0)
    {
        return;
    }
    const FontCache::Image* image = m_font->Raster(text);
    if (!image)
    {
        return;
    }
    const float factor = 16 * scale * TextScale / m_font->lineHeight;
    const float w = image->width * factor, h = image->height * factor;
    x -= 2 * factor;
    y -= 2 * factor;
    UseBatch(image->texture, 6);
    const Point positions[] =
        {{x, y}, {x + w, y}, {x + w, y + h}, {x, y}, {x + w, y + h}, {x, y + h}};
    const Point uv[] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 6; ++i)
    {
        m_vertices.push_back({positions[i].x,
                              positions[i].y,
                              color.r,
                              color.g,
                              color.b,
                              color.a,
                              uv[i].x,
                              uv[i].y,
                              color.emission});
    }
}

std::vector<std::wstring> Renderer::Wrap(const std::string& text, float scale, float maxWidth)
{
    std::vector<std::wstring> lines;
    if (!m_font || scale <= 0 || maxWidth <= 0)
    {
        return lines;
    }
    const std::wstring decoded = Decode(text);
    const float factor = 16 * scale * TextScale / m_font->lineHeight;
    const int pixelWidth = std::max(1, static_cast<int>(maxWidth / factor));
    size_t position = 0;
    while (position < decoded.size())
    {
        const size_t end = decoded.find(L'\n', position);
        const size_t count = (end == std::wstring::npos ? decoded.size() : end) - position;
        size_t consumed = 0;
        if (!count)
        {
            lines.push_back(L"");
        }
        while (consumed < count)
        {
            const wchar_t* start = decoded.data() + position + consumed;
            int fits = 0;
            SIZE size = {};
            GetTextExtentExPointW(m_font->dc,
                                  start,
                                  static_cast<int>(count - consumed),
                                  pixelWidth,
                                  &fits,
                                  nullptr,
                                  &size);
            fits = std::max(1, fits);
            // Do not split UTF-16 surrogate pairs across lines.
            if (start[fits - 1] >= 0xd800 && start[fits - 1] <= 0xdbff && consumed + fits < count)
            {
                fits = fits > 1 ? fits - 1 : fits + 1;
            }
            lines.emplace_back(start, static_cast<size_t>(fits));
            consumed += fits;
        }
        if (end == std::wstring::npos)
        {
            break;
        }
        position = end + 1;
    }
    return lines;
}

void Renderer::Text(float x, float y, const std::string& text, float scale, Color color)
{
    const auto lines = Wrap(text, scale, 100000.f);
    for (const auto& line : lines)
    {
        TextLine(x, y, line, scale, color);
        y += 21 * scale * TextScale;
    }
}

float Renderer::MeasureText(const std::string& text, float scale)
{
    if (!m_font || scale <= 0)
    {
        return 0;
    }
    float width = 0;
    for (const auto& line : Wrap(text, scale, 100000.f))
    {
        width = std::max(width, m_font->Width(line) * 16 * scale * TextScale / m_font->lineHeight);
    }
    return width;
}

void Renderer::TextFitted(float x,
                          float y,
                          const std::string& text,
                          float scale,
                          Color color,
                          float maxWidth)
{
    const float width = MeasureText(text, scale);
    if (width > maxWidth && width > 0)
    {
        scale *= maxWidth / width;
    }
    Text(x, y, text, scale, color);
}

float Renderer::WrappedHeight(const std::string& text, float scale, float maxWidth)
{
    return static_cast<float>(Wrap(text, scale, maxWidth).size()) * 21 * scale * TextScale;
}

float Renderer::TextWrapped(float x,
                            float y,
                            const std::string& text,
                            float scale,
                            Color color,
                            float maxWidth)
{
    const float top = y;
    for (const auto& line : Wrap(text, scale, maxWidth))
    {
        TextLine(x, y, line, scale, color);
        y += 21 * scale * TextScale;
    }
    return y - top;
}
