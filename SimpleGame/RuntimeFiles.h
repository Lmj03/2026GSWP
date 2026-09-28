#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>

inline std::wstring RuntimeFolder(const wchar_t* name)
{
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        return {};
    }
    const std::wstring path(executable, length);
    std::wstring folder = path.substr(0, path.find_last_of(L"\\/") + 1) + name;
    auto directory = [](const std::wstring& value)
    {
        if (CreateDirectoryW(value.c_str(), nullptr))
        {
            return true;
        }
        const DWORD attributes = GetFileAttributesW(value.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    };
    if (directory(folder))
    {
        return folder + L"/";
    }
    wchar_t local[32768] = {};
    const DWORD localLength = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
    if (!localLength || localLength >= 32768)
    {
        return {};
    }
    const std::wstring root = std::wstring(local) + L"/NeonVeil";
    if (!directory(root))
    {
        return {};
    }
    folder = root + L"/" + name;
    return directory(folder) ? folder + L"/" : std::wstring();
}

inline std::string PathUtf8(const std::wstring& path)
{
    const int count = WideCharToMultiByte(CP_UTF8,
                                          0,
                                          path.data(),
                                          static_cast<int>(path.size()),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);
    std::string text(count, '\0');
    if (count)
    {
        WideCharToMultiByte(CP_UTF8,
                            0,
                            path.data(),
                            static_cast<int>(path.size()),
                            &text[0],
                            count,
                            nullptr,
                            nullptr);
    }
    return text;
}
