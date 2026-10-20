#pragma once
#include <cstdint>
#include <memory>
#include <string>

// Owns JSON copies only; the worker never touches Actors, OpenGL or FrameProfiler.
class TelemetryClient
{
public:
    struct Statistics
    {
        std::uint64_t sent = 0;
        std::uint64_t failed = 0;
        std::uint64_t replaced = 0;
    };

    TelemetryClient();
    ~TelemetryClient();
    TelemetryClient(const TelemetryClient&) = delete;
    TelemetryClient& operator=(const TelemetryClient&) = delete;
    bool Start(unsigned short port);
    void Submit(std::string json);
    void Stop();
    Statistics Status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
