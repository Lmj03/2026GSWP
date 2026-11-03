#pragma once
#include "CityScene.h"
#include "FramePacer.h"
#include <array>
#include <memory>

struct GLFWwindow;

class GlfwApplication
{
public:
    GlfwApplication() = default;
    ~GlfwApplication();
    int Run();

private:
    struct Settings
    {
        int frameIntervalMs = 16;
        int swapInterval = 0;
        int backgroundIntervalMs = 100;
    };

    Settings m_settings;
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<Renderer> m_renderer;
    CityScene m_city;
    FramePacer m_pacer;
    bool m_glfwInitialized = false;
    bool m_keys[256] = {};
    bool m_spacePressed = false;
    bool m_leftShift = false, m_rightShift = false;
    bool m_focused = false, m_iconified = false;
    bool m_resizePending = false;
    int m_framebufferWidth = 0, m_framebufferHeight = 0;
    double m_scrollRemainder = 0;
    std::array<Point, 8> m_pointerPresses = {};
    size_t m_pointerCount = 0;
    FramePacer::Clock::time_point m_reportStart;
    unsigned int m_frames = 0, m_overruns = 0, m_dtClamps = 0;
    double m_pollMs = 0, m_waitMs = 0;

    bool Initialize();
    void Shutdown();
    void LoadSettings();
    void ClearInput();
    void Action(unsigned char key);
    void Special(int key);
    void ReportWindowStats();
    void LogEnvironment() const;
    static GlfwApplication& From(GLFWwindow* window);
    static void Error(int code, const char* description);
    static void Key(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void FramebufferSize(GLFWwindow* window, int width, int height);
    static void Focus(GLFWwindow* window, int focused);
    static void Iconify(GLFWwindow* window, int iconified);
    static void Scroll(GLFWwindow* window, double x, double y);
    static void Mouse(GLFWwindow* window, int button, int action, int mods);
};
