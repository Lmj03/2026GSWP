#include "stdafx.h"
#include "ModelRenderer.h"
#include "Dependencies/glew.h"
#include "FrameProfiler.h"
#include "Resources.h"
#include <cmath>
#include <cstddef>
#include <map>
#include <iostream>

namespace
{
constexpr GLsizei TargetSize = 512;

GLuint Program(const wchar_t* vertex, const wchar_t* fragment)
{
    std::string sources[2];
    if (!ReadResource(vertex, sources[0]) || !ReadResource(fragment, sources[1]))
    {
        return 0;
    }
    const GLenum types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    const GLuint program = glCreateProgram();
    bool valid = true;
    for (size_t i = 0; i < 2; ++i)
    {
        const GLuint shader = glCreateShader(types[i]);
        const char* text = sources[i].c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint success = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[4096] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "[Model] shader: " << log << '\n';
            valid = false;
        }
        glAttachShader(program, shader);
        glDeleteShader(shader);
    }
    glLinkProgram(program);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success || !valid)
    {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "[Model] program: " << log << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
}

struct ModelRenderer::State
{
    struct Mesh
    {
        std::shared_ptr<const Models::Asset> owner;
        GLuint vao = 0, vertices = 0, indices = 0;
        GLsizei count = 0;
    };

    std::map<const Models::Asset*, Mesh> meshes;
    GLuint program = 0, compose = 0, framebuffer = 0, color = 0, depth = 0, emptyVao = 0;
    GLint palette = -1, scale = -1, yaw = -1, hdr = -1, emission = -1;
    GLint foot = -1, screen = -1;
    bool attempted = false, ready = false;

    ~State()
    {
        for (const auto& pair : meshes)
        {
            const auto& mesh = pair.second;
            glDeleteBuffers(1, &mesh.vertices);
            glDeleteBuffers(1, &mesh.indices);
            glDeleteVertexArrays(1, &mesh.vao);
        }
        glDeleteVertexArrays(1, &emptyVao);
        glDeleteRenderbuffers(1, &depth);
        glDeleteTextures(1, &color);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteProgram(program);
        glDeleteProgram(compose);
    }

    bool Initialize()
    {
        if (attempted)
        {
            return ready;
        }
        attempted = true;
        program = Program(L"Shaders/Model.vs", L"Shaders/Model.fs");
        compose = Program(L"Shaders/ModelComposite.vs", L"Shaders/ModelComposite.fs");
        if (!program || !compose)
        {
            FrameProfiler::Instance().Event("model_gpu initialization_failed=shader");
            return false;
        }
        palette = glGetUniformLocation(program, "u_Palette[0]");
        scale = glGetUniformLocation(program, "u_Scale");
        yaw = glGetUniformLocation(program, "u_Yaw");
        hdr = glGetUniformLocation(program, "u_HDR");
        emission = glGetUniformLocation(program, "u_EmissionGain");
        foot = glGetUniformLocation(compose, "u_Foot");
        screen = glGetUniformLocation(compose, "u_Screen");
        glGenVertexArrays(1, &emptyVao);
        glGenFramebuffers(1, &framebuffer);
        glGenTextures(1, &color);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA16F,
                     TargetSize,
                     TargetSize,
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, TargetSize, TargetSize);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        ready = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        FrameProfiler::Instance().Event(
            std::string("model_gpu target=512x512 palette_limit=48 ready=") + (ready ? "1" : "0"));
        return ready;
    }

    const Mesh& Upload(const std::shared_ptr<const Models::Asset>& asset)
    {
        static_assert(sizeof(Models::Vertex) == 84,
                      "Model vertex attributes require packed 32-bit fields");
        const auto found = meshes.find(asset.get());
        if (found != meshes.end())
        {
            return found->second;
        }
        Mesh mesh;
        mesh.owner = asset;
        mesh.count = static_cast<GLsizei>(asset->indices.size());
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vertices);
        glGenBuffers(1, &mesh.indices);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
        glBufferData(GL_ARRAY_BUFFER,
                     asset->vertices.size() * sizeof(Models::Vertex),
                     asset->vertices.data(),
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     asset->indices.size() * sizeof(std::uint32_t),
                     asset->indices.data(),
                     GL_STATIC_DRAW);
        const size_t offsets[] = {offsetof(Models::Vertex, position),
                                  offsetof(Models::Vertex, normal),
                                  offsetof(Models::Vertex, color),
                                  offsetof(Models::Vertex, emission),
                                  offsetof(Models::Vertex, joints),
                                  offsetof(Models::Vertex, weights)};
        const GLint sizes[] = {3, 3, 4, 3, 4, 4};
        for (GLuint i = 0; i < 6; ++i)
        {
            glEnableVertexAttribArray(i);
            if (i == 4)
            {
                glVertexAttribIPointer(i,
                                       4,
                                       GL_UNSIGNED_INT,
                                       sizeof(Models::Vertex),
                                       reinterpret_cast<void*>(offsets[i]));
            }
            else
            {
                glVertexAttribPointer(i,
                                      sizes[i],
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(Models::Vertex),
                                      reinterpret_cast<void*>(offsets[i]));
            }
        }
        const size_t bytes = asset->vertices.size() * sizeof(Models::Vertex) +
                             asset->indices.size() * sizeof(std::uint32_t);
        FrameProfiler::Instance().Current().uploadBytes += bytes;
        FrameProfiler::Instance().Event("model_gpu_upload id=" + asset->id +
                                        " bytes=" + std::to_string(bytes) + " mesh_draws=1");
        return meshes.emplace(asset.get(), std::move(mesh)).first->second;
    }
};

ModelRenderer::ModelRenderer()
    : m_state(new State())
{
}

ModelRenderer::~ModelRenderer() = default;

bool ModelRenderer::Draw(const std::shared_ptr<const Models::Asset>& asset,
                         const Models::Animator& animation,
                         Point foot,
                         float pixelsPerUnit,
                         float yaw,
                         bool hdr,
                         float emissionGain,
                         float width,
                         float height)
{
    if (!asset || animation.Palette().empty() ||
        animation.Palette().size() != asset->palette.size() ||
        animation.Palette().size() > Models::MaxPalette || !std::isfinite(pixelsPerUnit) ||
        pixelsPerUnit <= 0 || !std::isfinite(yaw))
    {
        return false;
    }
    GLint target = 0, viewport[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (!m_state->Initialize())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        return false;
    }
    const auto start = FrameProfiler::Clock::now();
    const auto& mesh = m_state->Upload(asset);
    auto& profiler = FrameProfiler::Instance();
    // Reuse one color/depth target; resolve immediately to preserve SceneGraph painter order.
    glBindFramebuffer(GL_FRAMEBUFFER, m_state->framebuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glViewport(0, 0, TargetSize, TargetSize);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glClearDepth(1);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_state->program);
    glUniformMatrix4fv(m_state->palette,
                       static_cast<GLsizei>(animation.Palette().size()),
                       GL_FALSE,
                       animation.Palette()[0].data());
    profiler.Current().uploadBytes += animation.Palette().size() * sizeof(Models::Matrix);
    glUniform1f(m_state->scale, pixelsPerUnit);
    glUniform1f(m_state->yaw, yaw);
    glUniform1i(m_state->hdr, hdr ? 1 : 0);
    glUniform1f(m_state->emission, emissionGain);
    glBindVertexArray(mesh.vao);
    profiler.DrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_state->compose);
    glUniform2f(m_state->foot, foot.x, foot.y);
    glUniform2f(m_state->screen, width, height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_state->color);
    ++profiler.Current().textureBinds;
    glBindVertexArray(m_state->emptyVao);
    profiler.DrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    profiler.Current().submitMs += FrameProfiler::Milliseconds(start);
    return true;
}
