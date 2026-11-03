#pragma once
#include <chrono>

// One reusable OS timer. Deadlines include update/render/swap time, never an extra fixed delay.
class FramePacer
{
public:
    using Clock = std::chrono::steady_clock;
    FramePacer();
    ~FramePacer();
    FramePacer(const FramePacer&) = delete;
    FramePacer& operator=(const FramePacer&) = delete;
    void WaitUntil(Clock::time_point deadline);
    const char* Mode() const;

private:
    void* m_timer = nullptr;
    bool m_highResolution = false;
};
