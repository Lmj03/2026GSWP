#include "stdafx.h"
#include "FrameProfiler.h"
#include "RuntimeFiles.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

FrameProfiler& FrameProfiler::Instance()
{
    static FrameProfiler profiler;
    return profiler;
}

FrameProfiler::FrameProfiler()
{
    m_start = Clock::now();
    const std::wstring folder = RuntimeFolder(L"Logs");
    SYSTEMTIME time = {};
    GetSystemTime(&time);
    std::wostringstream name;
    name << L"render-" << time.wYear << L"-" << time.wMonth << L"-" << time.wDay << L"-"
         << time.wHour << L"-" << time.wMinute << L"-" << time.wSecond << L"-" << time.wMilliseconds
         << L"-" << GetCurrentProcessId();
    if (!folder.empty())
    {
        const std::wstring prefix = folder + name.str();
        m_framesFile.open((prefix + L".csv").c_str(), std::ios::binary);
        m_eventsFile.open((prefix + L".log").c_str(), std::ios::binary);
        if (m_framesFile && m_eventsFile)
        {
            std::cout << "[Profile] Files: " << PathUtf8(prefix) << ".csv / .log\n";
        }
    }
    if (!m_framesFile.is_open() || !m_eventsFile.is_open())
    {
        m_diskFailed = true;
        std::cerr << "[Profile] File logging unavailable; console profiling remains active.\n";
    }
    else
    {
        m_framesFile
            << "frame,elapsed_s,frame_interval_ms,update_calls,update_ms,render_cpu_ms,"
               "submit_cpu_ms,post_cpu_ms,swap_ms,previous_log_ms,draw_total,draw_world,"
               "draw_overlay,draw_post,instanced_draws,instances,submitted_vertices,"
               "instance_upload_bytes,texture_binds,texture_splits,capacity_splits,culled,"
               "font_hits,font_misses,font_upload_bytes,legacy_draw_estimate,legacy_vertices\n";
    }
    Event("schema=2 renderer=ordered_quad_instances_v1 cpu_times_only=1 timer_ms=16");
#ifdef _DEBUG
    Event("configuration=Debug");
#else
    Event("configuration=Release");
#endif
}

FrameProfiler::~FrameProfiler()
{
    Finish();
}

double FrameProfiler::Milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void FrameProfiler::Event(const std::string& message)
{
    if (!m_diskFailed)
    {
        m_eventsFile << std::fixed << std::setprecision(3) << Milliseconds(m_start) / 1000.0 << " "
                     << message << '\n';
    }
}

void FrameProfiler::BeginFrame()
{
    m_frameStart = Clock::now();
    if (!m_started)
    {
        m_windowStart = m_previousEnd = m_frameStart;
        m_started = true;
    }
    m_current = Counters{};
    m_stage = Stage::World;
    m_updateMs = m_pendingUpdateMs;
    m_frameUpdates = m_pendingUpdates;
    m_pendingUpdateMs = 0;
    m_pendingUpdates = 0;
}

void FrameProfiler::SetStage(Stage stage)
{
    m_stage = stage;
}

FrameProfiler::Counters& FrameProfiler::Current()
{
    return m_current;
}

void FrameProfiler::AddUpdate(double milliseconds)
{
    m_pendingUpdateMs += milliseconds;
    ++m_pendingUpdates;
}

const FrameProfiler::Metrics& FrameProfiler::LastFrame() const
{
    return m_metrics;
}

void FrameProfiler::DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(mode, first, count);
    ++m_current.draws[static_cast<int>(m_stage)];
    m_current.vertices += count;
}

void FrameProfiler::DrawInstances(GLsizei vertices, GLsizei instances)
{
    glDrawArraysInstanced(GL_TRIANGLES, 0, vertices, instances);
    ++m_current.draws[static_cast<int>(m_stage)];
    ++m_current.instancedDraws;
    m_current.instances += instances;
    m_current.vertices += static_cast<std::uint64_t>(vertices) * instances;
}

void FrameProfiler::BeforeSwap()
{
    m_swapStart = Clock::now();
    m_renderMs = std::chrono::duration<double, std::milli>(m_swapStart - m_frameStart).count();
}

void FrameProfiler::WriteBuffered()
{
    if (!m_diskFailed)
    {
        m_framesFile << m_rows;
        m_framesFile.flush();
        m_eventsFile.flush();
        if (!m_framesFile || !m_eventsFile)
        {
            m_diskFailed = true;
            std::cerr << "[Profile] Disk write failed; further file logging disabled.\n";
        }
    }
    m_rows.clear();
}

void FrameProfiler::EndFrame()
{
    const auto now = Clock::now();
    const double interval = std::chrono::duration<double, std::milli>(now - m_previousEnd).count();
    const double swap = std::chrono::duration<double, std::milli>(now - m_swapStart).count();
    m_previousEnd = now;
    const auto& c = m_current;
    const std::uint64_t draws = c.draws[0] + c.draws[1] + c.draws[2];
    ++m_frameId;
    m_metrics.counters = c;
    m_metrics.frame = m_frameId;
    m_metrics.intervalMs = interval;
    m_metrics.updateMs = m_updateMs;
    m_metrics.renderMs = m_renderMs;
    m_metrics.swapMs = swap;
    ++m_windowFrames;
    m_drawTotal += draws;
    m_drawMin = std::min(m_drawMin, draws);
    m_drawMax = std::max(m_drawMax, draws);
    const auto loggingStart = Clock::now();
    if (!m_diskFailed)
    {
        std::ostringstream row;
        row << std::fixed << std::setprecision(3) << m_frameId << ','
            << std::chrono::duration<double>(now - m_start).count() << ',' << interval << ','
            << m_frameUpdates << ',' << m_updateMs << ',' << m_renderMs << ',' << c.submitMs << ','
            << c.postMs << ',' << swap << ',' << m_lastLoggingMs << ',' << draws << ','
            << c.draws[0] << ',' << c.draws[1] << ',' << c.draws[2] << ',' << c.instancedDraws
            << ',' << c.instances << ',' << c.vertices << ',' << c.uploadBytes << ','
            << c.textureBinds << ',' << c.textureSplits << ',' << c.capacitySplits << ','
            << c.culled << ',' << c.fontHits << ',' << c.fontMisses << ',' << c.fontUploadBytes
            << ',' << c.legacyDraws << ',' << c.legacyVertices << '\n';
        m_rows += row.str();
    }
    const double elapsed = std::chrono::duration<double>(now - m_windowStart).count();
    if (elapsed >= 1.0)
    {
        m_metrics.fps = static_cast<double>(m_windowFrames) / elapsed;
        std::ostringstream line;
        line << std::fixed << std::setprecision(1)
             << "[Profile] FPS: " << static_cast<double>(m_windowFrames) / elapsed
             << " | DrawCalls/frame: " << draws
             << " (avg: " << static_cast<double>(m_drawTotal) / m_windowFrames
             << ", min: " << m_drawMin << ", max: " << m_drawMax << ") | W/UI/Post: " << c.draws[0]
             << '/' << c.draws[1] << '/' << c.draws[2] << " | instances: " << c.instances
             << " | upload KiB: " << c.uploadBytes / 1024.0;
        std::cout << line.str() << '\n';
        Event(line.str());
        WriteBuffered();
        m_windowStart = now;
        m_windowFrames = m_drawTotal = m_drawMax = 0;
        m_drawMin = UINT64_MAX;
    }
    m_lastLoggingMs = Milliseconds(loggingStart);
}

void FrameProfiler::Finish()
{
    WriteBuffered();
}
