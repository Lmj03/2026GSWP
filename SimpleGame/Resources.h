#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <string>

// Executable-relative assets, with source-tree fallbacks for Visual Studio.
inline bool ReadResource(const std::wstring& relative, std::string& text)
{
    wchar_t executable[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768) return false;
    const std::wstring path(executable, length);
    const std::wstring folder = path.substr(0, path.find_last_of(L"\\/") + 1);
    const std::wstring candidates[] = {folder + relative,
        folder + L"../../SimpleGame/" + relative,
        folder + L"../SimpleGame/" + relative, relative, L"SimpleGame/" + relative};
    for (const auto& candidate : candidates)
    {
        std::ifstream file(candidate.c_str(), std::ios::binary);
        if (!file) continue;
        std::ostringstream stream;
        stream << file.rdbuf();
        text = stream.str();
        if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef &&
            static_cast<unsigned char>(text[1]) == 0xbb &&
            static_cast<unsigned char>(text[2]) == 0xbf) text.erase(0, 3);
        return true;
    }
    return false;
}
