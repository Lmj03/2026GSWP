#include "stdafx.h"
#include "TelemetryClient.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
class HttpHandle
{
public:
    explicit HttpHandle(HINTERNET value = nullptr)
        : m_value(value)
    {
    }

    ~HttpHandle()
    {
        if (m_value)
        {
            WinHttpCloseHandle(m_value);
        }
    }

    HttpHandle(const HttpHandle&) = delete;
    HttpHandle& operator=(const HttpHandle&) = delete;

    HINTERNET Get() const
    {
        return m_value;
    }

private:
    HINTERNET m_value;
};
}

struct TelemetryClient::Impl
{
    std::mutex mutex;
    std::condition_variable ready;
    std::thread worker;
    std::string pending;
    bool stopping = true;
    std::atomic<std::uint64_t> sent{0};
    std::atomic<std::uint64_t> failed{0};
    std::atomic<std::uint64_t> replaced{0};

    void Run(unsigned short port)
    {
        HttpHandle session(WinHttpOpen(L"NeonVeilMonitoring/1.0",
                                       WINHTTP_ACCESS_TYPE_NO_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS,
                                       0));
        const bool bounded = session.Get() && WinHttpSetTimeouts(session.Get(), 500, 500, 500, 500);
        HttpHandle connection(bounded ? WinHttpConnect(session.Get(), L"127.0.0.1", port, 0)
                                      : nullptr);
        bool connected = false;
        bool reportedFailure = false;
        for (;;)
        {
            std::string json;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock,
                           [this]
                           {
                               return stopping || !pending.empty();
                           });
                if (stopping)
                {
                    break;
                }
                json.swap(pending);
            }
            HttpHandle request(connection.Get() ? WinHttpOpenRequest(connection.Get(),
                                                                     L"POST",
                                                                     L"/api/snapshot",
                                                                     nullptr,
                                                                     WINHTTP_NO_REFERER,
                                                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                                     0)
                                                : nullptr);
            DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            DWORD status = 0;
            DWORD size = sizeof(status);
            const bool ok =
                request.Get() &&
                WinHttpSetOption(request.Get(),
                                 WINHTTP_OPTION_REDIRECT_POLICY,
                                 &redirect,
                                 sizeof(redirect)) &&
                WinHttpSendRequest(request.Get(),
                                   L"Content-Type: application/json; charset=utf-8\r\n",
                                   static_cast<DWORD>(-1L),
                                   const_cast<char*>(json.data()),
                                   static_cast<DWORD>(json.size()),
                                   static_cast<DWORD>(json.size()),
                                   0) &&
                WinHttpReceiveResponse(request.Get(), nullptr) &&
                WinHttpQueryHeaders(request.Get(),
                                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    &status,
                                    &size,
                                    WINHTTP_NO_HEADER_INDEX) &&
                status >= 200 && status < 300;
            if (ok)
            {
                ++sent;
                if (!connected)
                {
                    std::cout << "[Monitor] Connected: http://127.0.0.1:" << port << '\n';
                }
                connected = true;
                reportedFailure = false;
            }
            else
            {
                ++failed;
                if (!reportedFailure)
                {
                    std::cerr << "[Monitor] Receiver unavailable or rejected JSON (HTTP " << status
                              << "). Retrying with the next snapshot.\n";
                }
                connected = false;
                reportedFailure = true;
            }
        }
    }
};

TelemetryClient::TelemetryClient()
    : m_impl(new Impl)
{
}

TelemetryClient::~TelemetryClient()
{
    Stop();
}

bool TelemetryClient::Start(unsigned short port)
{
    Stop();
    m_impl->stopping = false;
    try
    {
        m_impl->worker = std::thread(&Impl::Run, m_impl.get(), port);
    }
    catch (const std::exception& error)
    {
        m_impl->stopping = true;
        std::cerr << "[Monitor] Worker unavailable: " << error.what() << '\n';
        return false;
    }
    return true;
}

void TelemetryClient::Submit(std::string json)
{
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->stopping)
        {
            return;
        }
        if (!m_impl->pending.empty())
        {
            ++m_impl->replaced;
        }
        m_impl->pending = std::move(json);
    }
    m_impl->ready.notify_one();
}

void TelemetryClient::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->stopping = true;
        m_impl->pending.clear();
    }
    m_impl->ready.notify_one();
    if (m_impl->worker.joinable())
    {
        m_impl->worker.join();
    }
}

TelemetryClient::Statistics TelemetryClient::Status() const
{
    return {m_impl->sent.load(), m_impl->failed.load(), m_impl->replaced.load()};
}
