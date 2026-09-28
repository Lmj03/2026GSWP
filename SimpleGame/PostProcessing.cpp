#include "stdafx.h"
#include "PostProcessing.h"
#include "FrameProfiler.h"
#include "Resources.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>

namespace
{
GLuint Compile(GLenum type, const std::string& source)
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
        std::cerr << "후처리 셰이더 컴파일 오류: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Program(const wchar_t* fragmentPath)
{
    std::string vertexSource, fragmentSource;
    if (!ReadResource(L"Shaders/PostProcess.vs", vertexSource) ||
        !ReadResource(fragmentPath, fragmentSource))
    {
        std::cerr << "후처리 셰이더 파일을 찾을 수 없습니다. Shaders 폴더를 확인하세요.\n";
        return 0;
    }
    const GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource);
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
        return 0;
    }
    const GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (!program)
    {
        return 0;
    }
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "후처리 셰이더 연결 오류: " << log << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
}

PostProcessing::~PostProcessing()
{
    DestroyTargets(m_targets);
    if (m_vao)
    {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_extract)
    {
        glDeleteProgram(m_extract);
    }
    if (m_blur)
    {
        glDeleteProgram(m_blur);
    }
    if (m_composite)
    {
        glDeleteProgram(m_composite);
    }
}

bool PostProcessing::Initialize(int width, int height)
{
    ReloadSettings(); // A missing/invalid settings file leaves usable defaults.
    m_extract = Program(L"Shaders/BloomExtract.fs");
    m_blur = Program(L"Shaders/GaussianBlur.fs");
    m_composite = Program(L"Shaders/PostComposite.fs");
    if (!m_extract || !m_blur || !m_composite)
    {
        return false;
    }
    glGenVertexArrays(1, &m_vao);
    m_extractUniforms.source = glGetUniformLocation(m_extract, "u_Source");
    m_extractUniforms.texel = glGetUniformLocation(m_extract, "u_Texel");
    m_extractUniforms.threshold = glGetUniformLocation(m_extract, "u_Threshold");
    m_extractUniforms.knee = glGetUniformLocation(m_extract, "u_Knee");
    m_extractUniforms.extract = glGetUniformLocation(m_extract, "u_Extract");
    m_blurUniforms.source = glGetUniformLocation(m_blur, "u_Source");
    m_blurUniforms.step = glGetUniformLocation(m_blur, "u_Step");
    m_compositeUniforms.scene = glGetUniformLocation(m_composite, "u_Scene");
    m_compositeUniforms.bloom = glGetUniformLocation(m_composite, "u_Bloom");
    m_compositeUniforms.edge = glGetUniformLocation(m_composite, "u_Edge");
    m_compositeUniforms.exposure = glGetUniformLocation(m_composite, "u_Exposure");
    m_compositeUniforms.bloomStrength = glGetUniformLocation(m_composite, "u_BloomStrength");
    m_compositeUniforms.vignetteStrength = glGetUniformLocation(m_composite, "u_VignetteStrength");
    m_compositeUniforms.vignetteStart = glGetUniformLocation(m_composite, "u_VignetteStart");
    m_compositeUniforms.vignetteEnd = glGetUniformLocation(m_composite, "u_VignetteEnd");
    m_compositeUniforms.edgeStrength = glGetUniformLocation(m_composite, "u_EdgeStrength");
    m_compositeUniforms.edgeStart = glGetUniformLocation(m_composite, "u_EdgeStart");
    m_compositeUniforms.edgeEnd = glGetUniformLocation(m_composite, "u_EdgeEnd");
    return m_vao && Resize(width, height);
}

bool PostProcessing::Ready() const
{
    return m_vao && m_extract && m_blur && m_composite && m_targets.scene.framebuffer;
}

bool PostProcessing::CreateTarget(Target& target, int width, int height)
{
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    const bool complete = target.texture && target.framebuffer &&
                          glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete)
    {
        DestroyTarget(target);
    }
    return complete;
}

void PostProcessing::DestroyTarget(Target& target)
{
    if (target.framebuffer)
    {
        glDeleteFramebuffers(1, &target.framebuffer);
    }
    if (target.texture)
    {
        glDeleteTextures(1, &target.texture);
    }
    target = Target{};
}

void PostProcessing::DestroyTargets(Targets& targets)
{
    DestroyTarget(targets.scene);
    for (int i = 0; i < 2; ++i)
    {
        DestroyTarget(targets.bloom[i]);
        DestroyTarget(targets.edge[i]);
    }
    targets = Targets{};
}

bool PostProcessing::Resize(int width, int height)
{
    if (!m_vao || !m_extract || !m_blur || !m_composite)
    {
        return false;
    }
    width = std::max(1, width);
    height = std::max(1, height);
    GLint textureLimit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureLimit);
    // Cap intermediate allocation at 4096 pixels per axis on oversized windows.
    const int cap = std::max(1, std::min(4096, textureLimit));
    const float scale = std::min(1.f, static_cast<float>(cap) / std::max(width, height));
    width = std::max(1, static_cast<int>(width * scale));
    height = std::max(1, static_cast<int>(height * scale));
    if (m_targets.width == width && m_targets.height == height && Ready())
    {
        return true;
    }
    Targets candidate;
    candidate.width = width;
    candidate.height = height;
    candidate.halfWidth = std::max(1, (width + 1) / 2);
    candidate.halfHeight = std::max(1, (height + 1) / 2);
    glActiveTexture(GL_TEXTURE0);
    bool complete = CreateTarget(candidate.scene, width, height);
    for (int i = 0; i < 2 && complete; ++i)
    {
        complete = CreateTarget(candidate.bloom[i], candidate.halfWidth, candidate.halfHeight) &&
                   CreateTarget(candidate.edge[i], candidate.halfWidth, candidate.halfHeight);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!complete)
    {
        DestroyTargets(candidate);
        std::cerr << "HDR 렌더링 버퍼 생성 실패. 기존 버퍼 또는 기본 렌더링을 유지합니다.\n";
        return false;
    }
    DestroyTargets(m_targets);
    m_targets = candidate;
    return true;
}

bool PostProcessing::ReloadSettings()
{
    std::string source;
    if (!ReadResource(L"Data/Rendering/postprocess.cfg", source))
    {
        std::cerr << "Data/Rendering/postprocess.cfg 파일을 찾을 수 없어 현재 설정을 유지합니다.\n";
        return false;
    }

    struct Option
    {
        const char* key;
        float Settings::* member;
        float minimum, maximum;
    };

    const Option options[] = {{"exposure", &Settings::exposure, .25f, 4.f},
                              {"emission_gain", &Settings::emissionGain, 0, 8},
                              {"bloom_strength", &Settings::bloomStrength, 0, 2},
                              {"bloom_threshold", &Settings::bloomThreshold, 0, 8},
                              {"bloom_knee", &Settings::bloomKnee, 0, 1},
                              {"bloom_radius", &Settings::bloomRadius, .1f, 6},
                              {"vignette_strength", &Settings::vignetteStrength, 0, .95f},
                              {"vignette_start", &Settings::vignetteStart, 0, 1.4f},
                              {"vignette_end", &Settings::vignetteEnd, .1f, 2},
                              {"edge_strength", &Settings::edgeStrength, 0, 1},
                              {"edge_start", &Settings::edgeStart, 0, 1.4f},
                              {"edge_end", &Settings::edgeEnd, .1f, 2},
                              {"edge_radius", &Settings::edgeRadius, .1f, 6}};
    Settings candidate;
    std::set<std::string> seen;
    std::istringstream lines(source);
    std::string line;
    int number = 0;
    while (std::getline(lines, line))
    {
        ++number;
        line = line.substr(0, line.find('#'));
        if (line.find_first_not_of(" \t\r") == std::string::npos)
        {
            continue;
        }
        const size_t equals = line.find('=');
        std::string key, extra;
        float value = 0;
        bool valid = equals != std::string::npos;
        if (valid)
        {
            std::istringstream name(line.substr(0, equals)), data(line.substr(equals + 1));
            valid = bool(name >> key) && !(name >> extra) && bool(data >> value) &&
                    !(data >> extra) && std::isfinite(value) && seen.insert(key).second;
        }
        bool recognized = false;
        if (valid)
        {
            for (const Option& option : options)
            {
                if (key == option.key)
                {
                    recognized = true;
                    valid = value >= option.minimum && value <= option.maximum;
                    if (valid)
                    {
                        candidate.*(option.member) = value;
                    }
                    break;
                }
            }
            if (key == "enabled" || key == "bloom" || key == "vignette" || key == "edge_blur")
            {
                recognized = true;
                valid = value == 0 || value == 1;
                if (key == "enabled")
                {
                    candidate.enabled = value == 1;
                }
                if (key == "bloom")
                {
                    candidate.bloom = value == 1;
                }
                if (key == "vignette")
                {
                    candidate.vignette = value == 1;
                }
                if (key == "edge_blur")
                {
                    candidate.edgeBlur = value == 1;
                }
            }
            if (key == "bloom_iterations")
            {
                recognized = true;
                valid = value >= 1 && value <= 6 && std::floor(value) == value;
                if (valid)
                {
                    candidate.bloomIterations = static_cast<int>(value);
                }
            }
        }
        if (!valid || !recognized)
        {
            std::cerr << "후처리 설정 오류 " << number << "행: " << line << " (현재 설정 유지)\n";
            return false;
        }
    }
    if (seen.empty() || candidate.vignetteEnd <= candidate.vignetteStart + .01f ||
        candidate.edgeEnd <= candidate.edgeStart + .01f)
    {
        std::cerr
            << "후처리 설정이 비어 있거나 반경이 잘못되었습니다. 끝 반경은 시작 반경보다 0.01 넘게 커야 합니다. 현재 설정을 유지합니다.\n";
        return false;
    }
    m_settings = candidate;
    return true;
}

void PostProcessing::Toggle(Effect effect)
{
    switch (effect)
    {
    case Effect::All:
        m_settings.enabled = !m_settings.enabled;
        break;
    case Effect::Bloom:
        m_settings.bloom = !m_settings.bloom;
        break;
    case Effect::Vignette:
        m_settings.vignette = !m_settings.vignette;
        break;
    case Effect::EdgeBlur:
        m_settings.edgeBlur = !m_settings.edgeBlur;
        break;
    }
}

void PostProcessing::AdjustExposure(float amount)
{
    if (std::isfinite(amount))
    {
        m_settings.exposure = std::max(.25f, std::min(4.f, m_settings.exposure + amount));
    }
}

void PostProcessing::BeginScene(float r, float g, float b)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_targets.scene.framebuffer);
    glViewport(0, 0, m_targets.width, m_targets.height);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glActiveTexture(GL_TEXTURE0);
    glClearColor(r, g, b, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void PostProcessing::Downsample(GLuint source, const Target& target, bool extract)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, m_targets.halfWidth, m_targets.halfHeight);
    glUseProgram(m_extract);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(m_extractUniforms.source, 0);
    glUniform2f(m_extractUniforms.texel, 1.f / m_targets.width, 1.f / m_targets.height);
    glUniform1f(m_extractUniforms.threshold, m_settings.bloomThreshold);
    glUniform1f(m_extractUniforms.knee, m_settings.bloomKnee);
    glUniform1i(m_extractUniforms.extract, extract ? 1 : 0);
    FrameProfiler::Instance().DrawArrays(GL_TRIANGLES, 0, 3);
}

void PostProcessing::Blur(GLuint source, const Target& target, bool horizontal, float radius)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, m_targets.halfWidth, m_targets.halfHeight);
    glUseProgram(m_blur);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(m_blurUniforms.source, 0);
    glUniform2f(m_blurUniforms.step,
                horizontal ? radius / m_targets.halfWidth : 0.f,
                horizontal ? 0.f : radius / m_targets.halfHeight);
    FrameProfiler::Instance().DrawArrays(GL_TRIANGLES, 0, 3);
}

void PostProcessing::Present(int x, int y, int width, int height)
{
    if (!Ready())
    {
        return;
    }
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindVertexArray(m_vao);
    if (m_settings.bloom && m_settings.bloomStrength > 0)
    {
        Downsample(m_targets.scene.texture, m_targets.bloom[0], true);
        for (int i = 0; i < m_settings.bloomIterations; ++i)
        {
            Blur(m_targets.bloom[0].texture, m_targets.bloom[1], true, m_settings.bloomRadius);
            Blur(m_targets.bloom[1].texture, m_targets.bloom[0], false, m_settings.bloomRadius);
        }
    }
    if (m_settings.edgeBlur && m_settings.edgeStrength > 0)
    {
        Downsample(m_targets.scene.texture, m_targets.edge[0], false);
        for (int i = 0; i < 2; ++i)
        {
            Blur(m_targets.edge[0].texture, m_targets.edge[1], true, m_settings.edgeRadius);
            Blur(m_targets.edge[1].texture, m_targets.edge[0], false, m_settings.edgeRadius);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(x, y, width, height);
    glUseProgram(m_composite);
    const GLuint textures[] = {m_targets.scene.texture,
                               m_targets.bloom[0].texture,
                               m_targets.edge[0].texture};
    for (int i = 0; i < 3; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    glUniform1i(m_compositeUniforms.scene, 0);
    glUniform1i(m_compositeUniforms.bloom, 1);
    glUniform1i(m_compositeUniforms.edge, 2);
    glUniform1f(m_compositeUniforms.exposure, m_settings.exposure);
    glUniform1f(m_compositeUniforms.bloomStrength,
                m_settings.bloom ? m_settings.bloomStrength : 0.f);
    glUniform1f(m_compositeUniforms.vignetteStrength,
                m_settings.vignette ? m_settings.vignetteStrength : 0.f);
    glUniform1f(m_compositeUniforms.vignetteStart, m_settings.vignetteStart);
    glUniform1f(m_compositeUniforms.vignetteEnd, m_settings.vignetteEnd);
    glUniform1f(m_compositeUniforms.edgeStrength,
                m_settings.edgeBlur ? m_settings.edgeStrength : 0.f);
    glUniform1f(m_compositeUniforms.edgeStart, m_settings.edgeStart);
    glUniform1f(m_compositeUniforms.edgeEnd, m_settings.edgeEnd);
    FrameProfiler::Instance().DrawArrays(GL_TRIANGLES, 0, 3);
    // Restore state expected by the normal geometry/text renderer.
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
