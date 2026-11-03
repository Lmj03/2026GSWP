#include "stdafx.h"
#include "FramePacer.h"
#include <Windows.h>
#include <thread>

FramePacer::FramePacer()
{
    // Supported on Windows 10 1803+. Older Windows keeps a non-spinning fallback.
    m_timer = CreateWaitableTimerExW(nullptr,
                                     nullptr,
                                     CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                     TIMER_MODIFY_STATE | SYNCHRONIZE);
    m_highResolution = m_timer != nullptr;
    if (!m_timer)
    {
        m_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
}

FramePacer::~FramePacer()
{
    if (m_timer)
    {
        CloseHandle(m_timer);
    }
}

void FramePacer::WaitUntil(Clock::time_point deadline)
{
    const auto remaining = deadline - Clock::now();
    if (remaining <= Clock::duration::zero())
    {
        return;
    }
    if (m_timer)
    {
        LARGE_INTEGER due = {};
        using TimerTicks = std::chrono::duration<long long, std::ratio<1, 10000000>>;
        const auto ticks = std::chrono::duration_cast<TimerTicks>(remaining).count();
        due.QuadPart = -(ticks > 0 ? ticks : 1);
        if (SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE) &&
            WaitForSingleObject(m_timer, INFINITE) == WAIT_OBJECT_0)
        {
            return;
        }
        CloseHandle(m_timer);
        m_timer = nullptr;
        m_highResolution = false;
    }
    std::this_thread::sleep_until(deadline);
}

const char* FramePacer::Mode() const
{
    return m_highResolution ? "win32_high_resolution_timer"
           : m_timer        ? "win32_waitable_timer"
                            : "standard_sleep";
}
