#pragma once
#include "Resources.h"
#include <initializer_list>
#include <iostream>
#include <map>
#include <utility>

// UTF-8 key=value files. IDs are stable; text is editable independently of code.
class Localization
{
public:
    bool Load(const std::wstring& path)
    {
        std::string source;
        if (!ReadResource(path, source))
        {
            return false;
        }
        m_strings.clear();
        std::istringstream input(source);
        std::string line;
        int number = 0;
        while (std::getline(input, line))
        {
            ++number;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            const size_t separator = line.find('=');
            if (separator == std::string::npos || separator == 0 ||
                !m_strings.emplace(line.substr(0, separator), line.substr(separator + 1)).second)
            {
                std::cerr << "한글 문구 파일의 형식 또는 중복 키 오류: " << number << "행\n";
                return false;
            }
        }
        return !m_strings.empty();
    }

    bool Contains(const std::string& key) const
    {
        return m_strings.count(key) != 0;
    }

    std::string Get(const std::string& key) const
    {
        const auto found = m_strings.find(key);
        return found != m_strings.end() ? found->second : "[누락된 문구: " + key + "]";
    }

    std::string Format(const std::string& key,
                       std::initializer_list<std::pair<std::string, std::string>> arguments) const
    {
        std::string result = Get(key);
        for (const auto& argument : arguments)
        {
            const std::string token = "{" + argument.first + "}";
            size_t position = 0;
            while ((position = result.find(token, position)) != std::string::npos)
            {
                result.replace(position, token.size(), argument.second);
                position += argument.second.size();
            }
        }
        return result;
    }

private:
    std::map<std::string, std::string> m_strings;
};
