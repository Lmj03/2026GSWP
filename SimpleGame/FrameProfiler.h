#pragma once
#include "Dependencies/glew.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>

class FrameProfiler
{
public:
    using Clock = std::chrono::steady_clock;
    enum class Stage
    {
        World,
        Overlay,
        Post
    };

    struct Counters
    {
        std::uint64_t draws[3] = {};
        std::uint64_t instancedDraws = 0, instances = 0, vertices = 0, uploadBytes = 0;
        std::uint64_t textureBinds = 0, textureSplits = 0, capacitySplits = 0, culled = 0;
        std::uint64_t fontHits = 0, fontMisses = 0, fontUploadBytes = 0;
        std::uint64_t legacyDraws = 0, legacyVertices = 0;
        double submitMs = 0, postMs = 0;
    };

    struct Metrics
    {
        Counters counters;
        std::uint64_t frame = 0;
        double fps = 0;
        double intervalMs = 0;
        double updateMs = 0;
        double renderMs = 0;
        double swapMs = 0;
    };

    const Metrics& LastFrame() const;
    static FrameProfiler& Instance();
    void BeginFrame();
    void BeforeSwap();
    void EndFrame();
    void Finish();
    void Event(const std::string& message);
    void SetStage(Stage stage);
    void AddUpdate(double milliseconds);
    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    void DrawInstances(GLsizei vertices, GLsizei instances);
    Counters& Current();
    static double Milliseconds(Clock::time_point start);

private:
    FrameProfiler();
    ~FrameProfiler();
    FrameProfiler(const FrameProfiler&) = delete;
    FrameProfiler& operator=(const FrameProfiler&) = delete;
    void WriteBuffered();
    Clock::time_point m_start, m_windowStart, m_frameStart, m_swapStart, m_previousEnd;
    bool m_started = false, m_diskFailed = false;
    Stage m_stage = Stage::World;
    Counters m_current;
    Metrics m_metrics;
    std::ofstream m_framesFile, m_eventsFile;
    std::string m_rows;
    std::uint64_t m_frameId = 0, m_windowFrames = 0, m_drawTotal = 0;
    std::uint64_t m_drawMin = UINT64_MAX, m_drawMax = 0;
    std::uint64_t m_pendingUpdates = 0, m_frameUpdates = 0;
    double m_pendingUpdateMs = 0, m_updateMs = 0, m_renderMs = 0;
    double m_lastLoggingMs = 0;
};
