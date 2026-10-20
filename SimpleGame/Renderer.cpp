#include "stdafx.h"
#include "Renderer.h"
#include "FrameProfiler.h"
#include "MeshCache.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>

float Renderer::Width = 1280.f;
float Renderer::Height = 800.f;

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
            ++FrameProfiler::Instance().Current().fontHits;
            found->second.used = frame;
            return &found->second;
        }
        Image result;
        ++FrameProfiler::Instance().Current().fontMisses;
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
        FrameProfiler::Instance().Current().fontUploadBytes += coverage.size();
        result.used = frame;
        return &images.emplace(text, result).first->second;
    }
};

void Renderer::LoadBatchSettings()
{
    std::string source;
    if (!ReadResource(L"Data/Rendering/batching.cfg", source))
    {
        FrameProfiler::Instance().Event("batching_config=missing defaults=1");
        return;
    }
    std::istringstream lines(source);
    std::string line;
    int slots = 8, capacity = 16384, culling = 1;
    while (std::getline(lines, line))
    {
        std::istringstream input(line.substr(0, line.find('#')));
        std::string key, trailing;
        if (!(input >> key))
        {
            continue;
        }
        int value = 0;
        if (!(input >> value) || (input >> trailing) ||
            (key != "texture_slots" && key != "max_instances" && key != "culling"))
        {
            FrameProfiler::Instance().Event("batching_config=invalid defaults=1");
            return;
        }
        if (key == "texture_slots")
        {
            slots = value;
        }
        else if (key == "max_instances")
        {
            capacity = value;
        }
        else
        {
            culling = value;
        }
    }
    if (slots < 1 || slots > 8 || capacity < 256 || capacity > 65536 || culling < 0 || culling > 1)
    {
        FrameProfiler::Instance().Event("batching_config=out_of_range defaults=1");
        return;
    }
    m_textureSlots = slots;
    m_batchCapacity = capacity;
    m_culling = culling != 0;
}

Renderer::Renderer(int width, int height)
{
    static_assert(sizeof(Point) == 8 && sizeof(Color) == 20 && sizeof(Instance) == 60,
                  "Instance vertex attributes require packed float fields");
    LoadBatchSettings();
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
    const GLint units[] = {0, 1, 2, 3, 4, 5, 6, 7};
    glUniform1iv(glGetUniformLocation(m_program, "u_Coverage[0]"), 8, units);
    m_linearOutput = glGetUniformLocation(m_program, "u_LinearOutput");
    m_emissionGain = glGetUniformLocation(m_program, "u_EmissionGain");
    m_logicalSize = glGetUniformLocation(m_program, "u_LogicalSize");
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_meshVbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshVbo);
    const auto mesh = LoadUnitQuad();
    glBufferData(GL_ARRAY_BUFFER, sizeof(mesh), mesh.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Point), nullptr);
    glVertexAttribDivisor(0, 0);
    BindInstanceAttributes(0);
    glBindVertexArray(0);
    // Keep unused sampler slots complete and away from any active render target.
    glGenTextures(1, &m_fallbackTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fallbackTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const unsigned char white = 255;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    auto& profiler = FrameProfiler::Instance();
    profiler.Event("mesh_gpu_upload_bytes=" + std::to_string(sizeof(mesh)) +
                   " usage=static once=1");
    profiler.Event("batch_texture_slots=" + std::to_string(m_textureSlots) +
                   " max_instances_per_batch=" + std::to_string(m_batchCapacity) +
                   " culling=" + std::to_string(m_culling) +
                   " instance_bytes=" + std::to_string(sizeof(Instance)));
    for (GLenum key : {GL_VENDOR, GL_RENDERER, GL_VERSION})
    {
        const auto text = glGetString(key);
        profiler.Event("gl_info_" + std::to_string(key) + "=" +
                       (text ? reinterpret_cast<const char*>(text) : "unknown"));
    }
    m_font.reset(new FontCache());
    if (!m_font->dc || !m_font->font)
    {
        std::cerr << "한글 글꼴 초기화 실패.\n";
    }
    m_instances.reserve(32768);
    m_batches.reserve(64);
    if (!m_post.Initialize(m_viewWidth, m_viewHeight))
    {
        std::cerr << "후처리를 초기화하지 못해 기본 렌더링으로 진행합니다.\n";
    }
}

Renderer::~Renderer()
{
    m_font.reset();
    if (m_fallbackTexture)
    {
        glDeleteTextures(1, &m_fallbackTexture);
    }
    if (m_meshVbo)
    {
        glDeleteBuffers(1, &m_meshVbo);
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
    return m_program && m_vao && m_vbo && m_meshVbo && m_fallbackTexture && m_font && m_font->dc &&
           m_font->font;
}

void Renderer::Resize(int width, int height)
{
    width = std::max(1, width);
    height = std::max(1, height);
    // Expand the logical view to fill the window without stretching or letterboxing.
    const float scale = std::min(width / 1280.f, height / 800.f);
    Width = width / scale;
    Height = height / scale;
    m_viewWidth = width;
    m_viewHeight = height;
    m_viewX = 0;
    m_viewY = 0;
    if (m_post.Ready())
    {
        m_post.Resize(m_viewWidth, m_viewHeight);
    }
    glViewport(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
}

void Renderer::Begin(Color background)
{
    m_instances.clear();
    m_batches.clear();
    m_legacyStarted = false;
    if (m_font)
    {
        m_font->NextFrame();
    }
    m_overlay = false;
    m_hdrScene = m_post.Ready() && m_post.GetSettings().enabled;
    const auto& settings = m_post.GetSettings();
    std::ostringstream profileSettings;
    profileSettings << "viewport=" << m_viewWidth << 'x' << m_viewHeight << " hdr=" << m_hdrScene
                    << " bloom=" << settings.bloom
                    << " bloom_iterations=" << settings.bloomIterations
                    << " bloom_strength=" << settings.bloomStrength
                    << " bloom_radius=" << settings.bloomRadius
                    << " bloom_threshold=" << settings.bloomThreshold
                    << " bloom_knee=" << settings.bloomKnee << " edge_blur=" << settings.edgeBlur
                    << " edge_strength=" << settings.edgeStrength
                    << " edge_radius=" << settings.edgeRadius
                    << " edge_start=" << settings.edgeStart << " edge_end=" << settings.edgeEnd
                    << " vignette=" << settings.vignette
                    << " vignette_strength=" << settings.vignetteStrength
                    << " vignette_start=" << settings.vignetteStart
                    << " vignette_end=" << settings.vignetteEnd
                    << " emission_gain=" << settings.emissionGain
                    << " exposure=" << settings.exposure;
    if (m_profileSettings != profileSettings.str())
    {
        m_profileSettings = profileSettings.str();
        FrameProfiler::Instance().Event(m_profileSettings);
    }
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
    glClearColor(0, 0, 0, 1);
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

void Renderer::BindInstanceAttributes(size_t first)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    const size_t base = first * sizeof(Instance);
    const size_t offsets[] = {offsetof(Instance, a),
                              offsetof(Instance, c),
                              offsetof(Instance, color),
                              offsetof(Instance, color) + offsetof(Color, emission)};
    const GLint sizes[] = {4, 4, 4, 3};
    for (GLuint i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(i + 1);
        glVertexAttribPointer(i + 1,
                              sizes[i],
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Instance),
                              reinterpret_cast<void*>(base + offsets[i]));
        glVertexAttribDivisor(i + 1, 1);
    }
}

void Renderer::Flush()
{
    if (!IsInitialized() || m_instances.empty())
    {
        return;
    }
    const auto start = FrameProfiler::Clock::now();
    auto& profiler = FrameProfiler::Instance();
    profiler.SetStage(m_overlay ? FrameProfiler::Stage::Overlay : FrameProfiler::Stage::World);
    glUseProgram(m_program);
    glUniform1i(m_linearOutput, m_hdrScene ? 1 : 0);
    glUniform2f(m_logicalSize, Width, Height);
    glUniform1f(m_emissionGain, m_post.GetSettings().emissionGain);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    const size_t bytes = m_instances.size() * sizeof(Instance);
    if (bytes > m_gpuCapacity)
    {
        m_gpuCapacity = std::max(bytes, std::max(size_t(65536), m_gpuCapacity * 2));
        profiler.Event("instance_buffer_capacity_bytes=" + std::to_string(m_gpuCapacity));
    }
    // Orphan the previous storage to avoid explicitly waiting for in-flight frames.
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_gpuCapacity), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), m_instances.data());
    profiler.Current().uploadBytes += bytes;
    std::array<GLuint, 8> boundTextures = {};
    for (const Batch& batch : m_batches)
    {
        for (int slot = 0; slot < 8; ++slot)
        {
            const GLuint texture =
                slot < batch.textureCount ? batch.textures[slot] : m_fallbackTexture;
            if (boundTextures[slot] != texture)
            {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_2D, texture);
                boundTextures[slot] = texture;
                ++profiler.Current().textureBinds;
            }
        }
        // OpenGL 3.3 has no core base-instance draw; offset attribute pointers per batch.
        BindInstanceAttributes(batch.first);
        profiler.DrawInstances(6, batch.count);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
    m_instances.clear();
    m_batches.clear();
    profiler.Current().submitMs += FrameProfiler::Milliseconds(start);
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
        auto& profiler = FrameProfiler::Instance();
        profiler.SetStage(FrameProfiler::Stage::Post);
        const auto start = FrameProfiler::Clock::now();
        m_post.Present(m_viewX, m_viewY, m_viewWidth, m_viewHeight);
        profiler.Current().postMs += FrameProfiler::Milliseconds(start);
    }
    m_hdrScene = false;
    m_overlay = true;
    m_legacyStarted = false;
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

void Renderer::Queue(Point a,
                     Point b,
                     Point c,
                     Point d,
                     Color color,
                     GLuint texture,
                     bool ellipse,
                     unsigned int legacyVertices)
{
    auto& stats = FrameProfiler::Instance().Current();
    // Estimate the old adjacent-texture batching on the same requests, before culling.
    if (!m_legacyStarted || m_legacyTexture != texture)
    {
        ++stats.legacyDraws;
        m_legacyStarted = true;
        m_legacyTexture = texture;
    }
    stats.legacyVertices += legacyVertices;
    const float minX = std::min({a.x, b.x, c.x, d.x});
    const float maxX = std::max({a.x, b.x, c.x, d.x});
    const float minY = std::min({a.y, b.y, c.y, d.y});
    const float maxY = std::max({a.y, b.y, c.y, d.y});
    if (m_culling && (color.a <= 0 || maxX < 0 || maxY < 0 || minX > Width || minY > Height))
    {
        ++stats.culled;
        return;
    }
    if (m_batches.empty() || m_batches.back().count >= m_batchCapacity)
    {
        if (!m_batches.empty())
        {
            ++stats.capacitySplits;
        }
        Batch batch;
        batch.first = m_instances.size();
        m_batches.push_back(batch);
    }
    int slot = -1;
    if (texture)
    {
        Batch* batch = &m_batches.back();
        for (int i = 0; i < batch->textureCount; ++i)
        {
            if (batch->textures[i] == texture)
            {
                slot = i;
                break;
            }
        }
        if (slot < 0)
        {
            if (batch->textureCount == m_textureSlots)
            {
                ++stats.textureSplits;
                Batch next;
                next.first = m_instances.size();
                m_batches.push_back(next);
                batch = &m_batches.back();
            }
            slot = batch->textureCount++;
            batch->textures[slot] = texture;
        }
    }
    m_instances.push_back({a, b, c, d, color, static_cast<float>(slot), ellipse ? 1.f : 0.f});
    ++m_batches.back().count;
}

void Renderer::Triangle(Point a, Point b, Point c, Color color)
{
    // The second triangle degenerates; no per-frame topology is created.
    Queue(a, b, c, a, color, 0, false, 3);
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color color)
{
    Queue(a, b, c, d, color, 0, false, 6);
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
    Queue({center.x - rx, center.y - ry},
          {center.x + rx, center.y - ry},
          {center.x + rx, center.y + ry},
          {center.x - rx, center.y + ry},
          color,
          0,
          true,
          60);
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
    Queue({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, color, image->texture, false, 6);
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
