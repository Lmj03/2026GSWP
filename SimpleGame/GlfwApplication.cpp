#include "stdafx.h"
#include "GlfwApplication.h"
#include "FrameProfiler.h"
#include "Resources.h"
#define GLFW_INCLUDE_NONE
#include "Dependencies/GLFW/glfw-3.4/include/GLFW/glfw3.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace
{
int LegacyKey(int key)
{
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
    {
        return 'a' + key - GLFW_KEY_A;
    }
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
    {
        return '0' + key - GLFW_KEY_0;
    }
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
    {
        return '0' + key - GLFW_KEY_KP_0;
    }
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
        return 27;
    case GLFW_KEY_TAB:
        return 9;
    case GLFW_KEY_SPACE:
        return ' ';
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_ADD:
        return '=';
    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        return '-';
    case GLFW_KEY_LEFT_BRACKET:
        return '[';
    case GLFW_KEY_RIGHT_BRACKET:
        return ']';
    default:
        return -1;
    }
}
}

GlfwApplication::~GlfwApplication()
{
    Shutdown();
}

void GlfwApplication::LoadSettings()
{
    std::string source;
    if (!ReadResource(L"Data/Rendering/window.cfg", source))
    {
        return;
    }
    Settings candidate;
    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line))
    {
        std::istringstream input(line.substr(0, line.find('#')));
        std::string key, extra;
        int value = 0;
        if (!(input >> key))
        {
            continue;
        }
        if (!(input >> value) || (input >> extra))
        {
            FrameProfiler::Instance().Event("window_config=invalid defaults=1");
            return;
        }
        if (key == "frame_interval_ms")
        {
            candidate.frameIntervalMs = value;
        }
        else if (key == "swap_interval")
        {
            candidate.swapInterval = value;
        }
        else if (key == "background_interval_ms")
        {
            candidate.backgroundIntervalMs = value;
        }
        else
        {
            FrameProfiler::Instance().Event("window_config=unknown_key defaults=1");
            return;
        }
    }
    if (candidate.frameIntervalMs < 0 || candidate.frameIntervalMs > 1000 ||
        candidate.swapInterval < 0 || candidate.swapInterval > 1 ||
        candidate.backgroundIntervalMs < 20 || candidate.backgroundIntervalMs > 1000)
    {
        FrameProfiler::Instance().Event("window_config=out_of_range defaults=1");
        return;
    }
    m_settings = candidate;
}

bool GlfwApplication::Initialize()
{
    LoadSettings();
    glfwSetErrorCallback(Error);
    if (!glfwInit())
    {
        return false;
    }
    m_glfwInitialized = true;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    m_window = glfwCreateWindow(static_cast<int>(Renderer::Width),
                                static_cast<int>(Renderer::Height),
                                "네온 베일 | 잔광 지구 - OpenGL 프로토타입",
                                nullptr,
                                nullptr);
    if (!m_window)
    {
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(m_settings.swapInterval);
    glfwSetWindowPos(m_window, 80, 60);
    glewExperimental = GL_TRUE;
    const GLenum error = glewInit();
    if (error != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3이 필요합니다. GLEW: " << glewGetErrorString(error) << '\n';
        return false;
    }
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glfwGetFramebufferSize(m_window, &m_framebufferWidth, &m_framebufferHeight);
    m_renderer.reset(
        new Renderer(std::max(1, m_framebufferWidth), std::max(1, m_framebufferHeight)));
    if (!m_renderer->IsInitialized() || !m_city.Load())
    {
        std::cerr << "초기화에 실패했습니다. 위의 자원 또는 셰이더 오류를 확인하세요.\n";
        return false;
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, Key);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSize);
    glfwSetWindowFocusCallback(m_window, Focus);
    glfwSetWindowIconifyCallback(m_window, Iconify);
    glfwSetScrollCallback(m_window, Scroll);
    glfwSetMouseButtonCallback(m_window, Mouse);
    glfwShowWindow(m_window);
    m_focused = glfwGetWindowAttrib(m_window, GLFW_FOCUSED) == GLFW_TRUE;
    m_iconified = glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE;
    m_reportStart = FramePacer::Clock::now();
    LogEnvironment();
    std::cout << "네온 베일 / GLFW / OpenGL 3.3 / 계속 확장되는 도시\n"
              << "WASD 이동 | Shift 달리기 | E 대화/거점 | Space 공격 | H 회복제\n"
              << "M 지도 | J 기록 | R 비 | Tab 안내 숨김 | 휠/+/- 확대/축소\n"
              << "F2 처음부터 | F5 후처리 | F6 블룸 | F7 비네트 | F8 가장자리 흐림\n"
              << "F9 후처리 설정 다시 읽기 | [ / ] 노출 조절 | Esc 닫기/종료\n";
    return true;
}

void GlfwApplication::Shutdown()
{
    m_city.StopMonitoring();
    if (m_window)
    {
        glfwMakeContextCurrent(m_window);
    }
    // Both model and 2D GPU resources are released before their context is destroyed.
    m_renderer.reset();
    if (m_glfwInitialized)
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
        m_glfwInitialized = false;
        FrameProfiler::Instance().Finish();
    }
}

int GlfwApplication::Run()
{
    if (!Initialize())
    {
        return 1;
    }
    auto previous = FramePacer::Clock::now();
    bool wasActive = false;
    while (!glfwWindowShouldClose(m_window))
    {
        const auto pollStart = FrameProfiler::Clock::now();
        glfwPollEvents();
        m_pollMs += FrameProfiler::Milliseconds(pollStart);
        if (glfwWindowShouldClose(m_window))
        {
            break;
        }
        const bool active =
            m_focused && !m_iconified && m_framebufferWidth > 0 && m_framebufferHeight > 0;
        if (!active)
        {
            ClearInput();
            previous = FramePacer::Clock::now();
            wasActive = false;
            m_city.PublishMonitoring(false);
            ReportWindowStats();
            const auto waitStart = FrameProfiler::Clock::now();
            glfwWaitEventsTimeout(m_settings.backgroundIntervalMs / 1000.0);
            m_waitMs += FrameProfiler::Milliseconds(waitStart);
            continue;
        }
        const auto frameStart = FramePacer::Clock::now();
        if (!wasActive)
        {
            // Discard time spent unfocused/minimized; do not perform catch-up simulation.
            previous = frameStart;
        }
        wasActive = true;
        const float elapsed = std::chrono::duration<float>(frameStart - previous).count();
        const float dt = std::min(.05f, elapsed);
        if (elapsed > .05f)
        {
            ++m_dtClamps;
        }
        previous = frameStart;
        if (m_resizePending)
        {
            m_renderer->Resize(m_framebufferWidth, m_framebufferHeight);
            m_resizePending = false;
        }
        // A resize and click can arrive in the same event poll. Resolve against the new view.
        for (size_t i = 0; i < m_pointerCount; ++i)
        {
            m_city.HandlePointer(m_pointerPresses[i].x * Renderer::Width,
                                 m_pointerPresses[i].y * Renderer::Height);
        }
        m_pointerCount = 0;
        // Keep taps shorter than a frame without turning one press into multiple attacks.
        const bool spaceHeld = m_keys[' '];
        m_keys[' '] = spaceHeld || m_spacePressed;
        const auto updateStart = FrameProfiler::Clock::now();
        m_city.Update(dt, m_keys);
        FrameProfiler::Instance().AddUpdate(FrameProfiler::Milliseconds(updateStart));
        m_keys[' '] = spaceHeld;
        m_spacePressed = false;
        m_city.PublishMonitoring(true);
        auto& profiler = FrameProfiler::Instance();
        profiler.BeginFrame();
        m_city.Draw(*m_renderer);
        profiler.BeforeSwap();
        glfwSwapBuffers(m_window);
        profiler.EndFrame();
        ++m_frames;
        // VSync is the sole pacer when requested: avoid adding a second refresh-sized wait.
        if (!m_settings.swapInterval && m_settings.frameIntervalMs > 0)
        {
            const auto deadline =
                frameStart + std::chrono::milliseconds(m_settings.frameIntervalMs);
            if (FramePacer::Clock::now() > deadline)
            {
                ++m_overruns;
            }
            const auto waitStart = FrameProfiler::Clock::now();
            m_pacer.WaitUntil(deadline);
            m_waitMs += FrameProfiler::Milliseconds(waitStart);
        }
        ReportWindowStats();
    }
    Shutdown();
    return 0;
}

void GlfwApplication::ClearInput()
{
    std::fill(m_keys, m_keys + 256, false);
    m_leftShift = m_rightShift = m_spacePressed = false;
    m_scrollRemainder = 0;
    m_pointerCount = 0;
}

void GlfwApplication::Action(unsigned char key)
{
    FrameProfiler::Instance().Event("action_key_code=" + std::to_string(key));
    if (m_city.HandleInteractionInput(key) || m_city.HandleBaseInput(key))
    {
        return;
    }
    switch (key)
    {
    case 27:
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        break;
    case 'e':
        m_city.Interact();
        break;
    case 'h':
        m_city.UseRecovery();
        break;
    case 'r':
        m_city.ToggleRain();
        break;
    case 'j':
        m_city.ToggleJournal();
        break;
    case 'm':
        m_city.ToggleMap();
        break;
    case 9:
        m_city.ToggleInterface();
        break;
    case '=':
        m_city.Zoom(1);
        break;
    case '-':
        m_city.Zoom(-1);
        break;
    case '[':
        m_renderer->AdjustExposure(-.1f);
        break;
    case ']':
        m_renderer->AdjustExposure(.1f);
        break;
    }
}

void GlfwApplication::Special(int key)
{
    FrameProfiler::Instance().Event("special_key_code=" + std::to_string(key));
    switch (key)
    {
    case GLFW_KEY_F2:
        m_city.Reset();
        break;
    case GLFW_KEY_F5:
        m_renderer->TogglePostProcessing(PostProcessing::Effect::All);
        break;
    case GLFW_KEY_F6:
        m_renderer->TogglePostProcessing(PostProcessing::Effect::Bloom);
        break;
    case GLFW_KEY_F7:
        m_renderer->TogglePostProcessing(PostProcessing::Effect::Vignette);
        break;
    case GLFW_KEY_F8:
        m_renderer->TogglePostProcessing(PostProcessing::Effect::EdgeBlur);
        break;
    case GLFW_KEY_F9:
        m_city.NotifyGraphicsReload(m_renderer->ReloadPostProcessing());
        break;
    }
}

GlfwApplication& GlfwApplication::From(GLFWwindow* window)
{
    return *static_cast<GlfwApplication*>(glfwGetWindowUserPointer(window));
}

void GlfwApplication::Error(int code, const char* description)
{
    const std::string text = description ? description : "unknown";
    std::cerr << "[GLFW] " << code << ": " << text << '\n';
    FrameProfiler::Instance().Event("glfw_error code=" + std::to_string(code) + " reason=" + text);
}

void GlfwApplication::Key(GLFWwindow* window, int key, int, int action, int)
{
    auto& app = From(window);
    if (action == GLFW_REPEAT || (action == GLFW_PRESS && !app.m_focused))
    {
        return;
    }
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
    {
        bool& held = key == GLFW_KEY_LEFT_SHIFT ? app.m_leftShift : app.m_rightShift;
        held = action == GLFW_PRESS;
        app.m_keys[16] = app.m_leftShift || app.m_rightShift;
        return;
    }
    const int legacy = LegacyKey(key);
    if (legacy >= 0)
    {
        app.m_keys[legacy] = action == GLFW_PRESS;
        if (action == GLFW_PRESS)
        {
            app.m_spacePressed = app.m_spacePressed || legacy == ' ';
            app.Action(static_cast<unsigned char>(legacy));
        }
    }
    else if (action == GLFW_PRESS && key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
    {
        app.Special(key);
    }
}

void GlfwApplication::FramebufferSize(GLFWwindow* window, int width, int height)
{
    auto& app = From(window);
    if (app.m_framebufferWidth == width && app.m_framebufferHeight == height)
    {
        return;
    }
    app.m_framebufferWidth = width;
    app.m_framebufferHeight = height;
    app.m_resizePending = true;
    FrameProfiler::Instance().Event("framebuffer=" + std::to_string(width) + "x" +
                                    std::to_string(height));
}

void GlfwApplication::Focus(GLFWwindow* window, int focused)
{
    auto& app = From(window);
    app.m_focused = focused == GLFW_TRUE;
    if (!app.m_focused)
    {
        app.ClearInput();
    }
    FrameProfiler::Instance().Event(app.m_focused ? "focus=1" : "focus=0");
}

void GlfwApplication::Iconify(GLFWwindow* window, int iconified)
{
    auto& app = From(window);
    app.m_iconified = iconified == GLFW_TRUE;
    if (app.m_iconified)
    {
        app.ClearInput();
    }
    FrameProfiler::Instance().Event(app.m_iconified ? "iconified=1" : "iconified=0");
}

void GlfwApplication::Scroll(GLFWwindow* window, double, double y)
{
    auto& app = From(window);
    if (!app.m_focused || !std::isfinite(y))
    {
        return;
    }
    // High-resolution wheels/trackpads keep fractional motion instead of multiplying zoom jumps.
    app.m_scrollRemainder += std::max(-16.0, std::min(16.0, y));
    while (std::abs(app.m_scrollRemainder) >= 1)
    {
        const int direction = app.m_scrollRemainder > 0 ? 1 : -1;
        app.m_city.Zoom(direction);
        app.m_scrollRemainder -= direction;
    }
}

void GlfwApplication::Mouse(GLFWwindow* window, int button, int action, int)
{
    auto& app = From(window);
    if (!app.m_focused || button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    {
        return;
    }
    int width = 0, height = 0;
    double x = 0, y = 0;
    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);
    if (width > 0 && height > 0)
    {
        // Cursor coordinates use window units, independent of DPI/framebuffer dimensions.
        if (app.m_pointerCount < app.m_pointerPresses.size())
        {
            app.m_pointerPresses[app.m_pointerCount++] = {static_cast<float>(x / width),
                                                          static_cast<float>(y / height)};
        }
    }
}

void GlfwApplication::LogEnvironment() const
{
    auto& profiler = FrameProfiler::Instance();
    const auto glText = [](GLenum name)
    {
        const auto value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "unknown";
    };
    std::ostringstream text;
    text << "framework=GLFW version=" << glfwGetVersionString()
         << " swap_interval_requested=" << m_settings.swapInterval
         << " frame_interval_ms=" << (m_settings.swapInterval ? 0 : m_settings.frameIntervalMs)
         << " background_interval_ms=" << m_settings.backgroundIntervalMs
         << " pacer=" << m_pacer.Mode() << " framebuffer=" << m_framebufferWidth << 'x'
         << m_framebufferHeight << " gl_vendor=" << glText(GL_VENDOR)
         << " gl_renderer=" << glText(GL_RENDERER) << " gl_version=" << glText(GL_VERSION);
    profiler.Event(text.str());
    std::cout << "[Window] " << text.str() << '\n';
}

void GlfwApplication::ReportWindowStats()
{
    const auto now = FramePacer::Clock::now();
    const double seconds = std::chrono::duration<double>(now - m_reportStart).count();
    if (seconds < 1)
    {
        return;
    }
    std::ostringstream text;
    text << "window_stats seconds=" << seconds << " frames=" << m_frames << " poll_ms=" << m_pollMs
         << " wait_ms=" << m_waitMs << " deadline_misses=" << m_overruns
         << " dt_clamps=" << m_dtClamps << " focused=" << m_focused << " iconified=" << m_iconified
         << " pacer=" << m_pacer.Mode();
    FrameProfiler::Instance().Event(text.str());
    m_reportStart = now;
    m_frames = m_overruns = m_dtClamps = 0;
    m_pollMs = m_waitMs = 0;
}
