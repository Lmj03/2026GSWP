#include "stdafx.h"
#include "MeshCache.h"
#include "Resources.h"
#include "RuntimeFiles.h"
#include "FrameProfiler.h"
#include <sstream>

std::array<Point, 6> LoadUnitQuad()
{
    const std::array<Point, 6> canonical = {{{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}}};
    auto read = [&](std::istream& input, std::array<Point, 6>& result)
    {
        std::string magic, trailing;
        int version = 0, count = 0;
        if (!(input >> magic >> version >> count) || magic != "NEON_QUAD" || version != 1 ||
            count != 6)
        {
            return false;
        }
        for (size_t i = 0; i < result.size(); ++i)
        {
            if (!(input >> result[i].x >> result[i].y) || result[i].x != canonical[i].x ||
                result[i].y != canonical[i].y)
            {
                return false;
            }
        }
        return !(input >> trailing);
    };
    auto& profiler = FrameProfiler::Instance();
    const auto start = FrameProfiler::Clock::now();
    const std::wstring folder = RuntimeFolder(L"Cache");
    const std::wstring path = folder + L"unit_quad_v1.mesh";
    std::array<Point, 6> result;
    if (!folder.empty())
    {
        std::ifstream input(path.c_str(), std::ios::binary);
        if (input && read(input, result))
        {
            profiler.Event("mesh_cache=hit version=1 vertices=6 path=" + PathUtf8(path) +
                           " load_ms=" + std::to_string(FrameProfiler::Milliseconds(start)));
            return result;
        }
    }
    std::string source;
    bool packaged = ReadResource(L"Data/Meshes/unit_quad.mesh", source);
    std::istringstream resource(source);
    packaged = packaged && read(resource, result);
    if (!packaged)
    {
        result = canonical;
    }
    profiler.Event(std::string("mesh_cache=miss_or_invalid source=") +
                   (packaged ? "packaged" : "generated") + " version=1 vertices=6");
    if (!folder.empty())
    {
        const std::wstring temporary =
            path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
        std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
        output << "NEON_QUAD 1 6\n";
        for (const Point& p : result)
        {
            output << p.x << ' ' << p.y << '\n';
        }
        output.flush();
        const bool written = bool(output);
        output.close();
        if (written && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            profiler.Event("mesh_cache=saved path=" + PathUtf8(path));
        }
        else
        {
            DeleteFileW(temporary.c_str());
            profiler.Event("mesh_cache=save_failed using_memory_copy=1");
        }
    }
    else
    {
        profiler.Event("mesh_cache=directory_unavailable using_memory_copy=1");
    }
    return result;
}
