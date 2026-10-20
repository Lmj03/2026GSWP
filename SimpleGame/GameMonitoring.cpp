#include "stdafx.h"
#include "CityScene.h"
#include "FrameProfiler.h"
#include "Resources.h"
#include "WorldLayout.h"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

namespace
{
// JSON keeps UTF-8 text intact and escapes every JSON control character.
class Json
{
public:
    Json()
    {
        m_text.imbue(std::locale::classic());
        m_text << std::setprecision(9) << std::boolalpha;
    }

    void Object(const std::string& key = "")
    {
        Prefix(key);
        m_text << '{';
        m_first.push_back(true);
    }

    void Array(const std::string& key)
    {
        Prefix(key);
        m_text << '[';
        m_first.push_back(true);
    }

    void EndObject()
    {
        m_first.pop_back();
        m_text << '}';
    }

    void EndArray()
    {
        m_first.pop_back();
        m_text << ']';
    }

    template <class T> void Field(const std::string& key, T value)
    {
        Prefix(key);
        m_text << value;
    }

    void Field(const std::string& key, const std::string& value)
    {
        Prefix(key);
        Quote(value);
    }

    void Field(const std::string& key, const char* value)
    {
        Field(key, std::string(value));
    }

    void Field(const std::string& key, double value)
    {
        Prefix(key);
        m_text << (std::isfinite(value) ? value : 0.0);
    }

    void Field(const std::string& key, float value)
    {
        Field(key, static_cast<double>(value));
    }

    std::string Text() const
    {
        return m_text.str();
    }

private:
    void Prefix(const std::string& key)
    {
        if (!m_first.empty())
        {
            if (!m_first.back())
            {
                m_text << ',';
            }
            m_first.back() = false;
        }
        if (!key.empty())
        {
            Quote(key);
            m_text << ':';
        }
    }

    void Quote(const std::string& value)
    {
        const char* hex = "0123456789abcdef";
        m_text << '"';
        for (unsigned char ch : value)
        {
            if (ch == '"' || ch == '\\')
            {
                m_text << '\\' << static_cast<char>(ch);
            }
            else if (ch < 32)
            {
                m_text << "\\u00" << hex[ch >> 4] << hex[ch & 15];
            }
            else
            {
                m_text << static_cast<char>(ch);
            }
        }
        m_text << '"';
    }

    std::ostringstream m_text;
    std::vector<bool> m_first;
};

struct Village
{
    std::string name;
    int loaded = 0;
    int expected = 49;
    int town = -1;
    int population = 0;
    int living = 0;
    int hostiles = 0;
    int travelling = 0;
};
}

void CityScene::PublishMonitoring(bool focused)
{
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::now();
    if (!m_monitorInitialized)
    {
        m_monitorInitialized = true;
        bool enabled = true;
        unsigned short port = 8765;
        std::string source;
        if (ReadResource(L"Data/Monitoring/monitor.cfg", source))
        {
            std::istringstream lines(source);
            std::string line;
            while (std::getline(lines, line))
            {
                std::istringstream input(line.substr(0, line.find('#')));
                std::string key;
                int value = 0;
                if (!(input >> key >> value))
                {
                    continue;
                }
                if (key == "enabled")
                {
                    enabled = value != 0;
                }
                else if (key == "interval_ms" && value >= 250 && value <= 60000)
                {
                    m_monitorInterval = value;
                }
                else if (key == "port" && value >= 1024 && value <= 65535)
                {
                    port = static_cast<unsigned short>(value);
                }
                else
                {
                    std::cerr << "[Monitor] Ignoring invalid setting: " << key << '\n';
                }
            }
        }
        m_monitorStart = m_monitorNext = now;
        // Wall-clock epoch + process ID distinguishes restarts, including reset game time.
        m_monitorSession = std::to_string(GetCurrentProcessId()) + "-" +
                           std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count());
        m_monitorEnabled = enabled && m_telemetry.Start(port);
        FrameProfiler::Instance().Event("monitor enabled=" + std::to_string(m_monitorEnabled) +
                                        " interval_ms=" + std::to_string(m_monitorInterval) +
                                        " port=" + std::to_string(port));
    }
    if (!m_monitorEnabled || now < m_monitorNext)
    {
        return;
    }
    m_monitorNext = now + std::chrono::milliseconds(m_monitorInterval);
    ++m_monitorSequence;
    m_telemetry.Submit(MonitoringSnapshot(focused));
}

void CityScene::StopMonitoring()
{
    m_monitorEnabled = false;
    m_monitorInitialized = true;
    m_telemetry.Stop();
}

std::string CityScene::MonitoringSnapshot(bool focused) const
{
    Json json;
    std::map<std::string, Village> villages;
    for (size_t i = 0; i < WorldTowns().size(); ++i)
    {
        const auto& town = WorldTowns()[i];
        Village village;
        village.name = m_text.Get("town.name." + town.id);
        village.expected = town.width * town.height;
        village.town = static_cast<int>(i);
        villages.emplace(town.id, village);
    }
    auto villageFor = [&](std::int64_t x, std::int64_t y)
    {
        const int index = WorldTownIndex(x, y);
        if (index >= 0)
        {
            return WorldTowns()[static_cast<size_t>(index)].id;
        }
        Village road;
        road.name = m_text.Get("town.roads");
        road.expected = 1;
        villages.emplace("roads", road);
        return std::string("roads");
    };
    for (const auto& entry : m_townResidents)
    {
        const Citizen& npc = entry.second;
        const auto cx = static_cast<std::int64_t>(std::floor(npc.position.x / ChunkSize));
        const auto cy = static_cast<std::int64_t>(std::floor(npc.position.y / ChunkSize));
        Village& village = villages[villageFor(cx, cy)];
        village.population += npc.memory.health > 0 ? 1 : 0;
        village.living += npc.active && npc.memory.health > 0 ? 1 : 0;
        village.hostiles += npc.active && npc.memory.health > 0 && npc.hostile ? 1 : 0;
        village.travelling += npc.travelling ? 1 : 0;
    }
    for (const auto& chunk : m_chunks)
    {
        ++villages[villageFor(m_originX + chunk.first, m_originY + chunk.second)].loaded;
    }
    auto position = [&](Point local, std::int64_t originX, std::int64_t originY)
    {
        const auto dx = static_cast<std::int64_t>(std::floor(local.x / ChunkSize));
        const auto dy = static_cast<std::int64_t>(std::floor(local.y / ChunkSize));
        const std::string village = villageFor(originX + dx, originY + dy);
        json.Field("village_id", village);
        json.Object("position");
        // Large world coordinates remain strings: JS numbers lose int64 precision.
        json.Field("chunk_x", std::to_string(originX + dx));
        json.Field("chunk_y", std::to_string(originY + dy));
        json.Field("local_x", local.x - static_cast<float>(dx * ChunkSize));
        json.Field("local_y", local.y - static_cast<float>(dy * ChunkSize));
        json.EndObject();
    };
    json.Object();
    json.Field("schema_version", 1);
    json.Field("type", "world_snapshot");
    json.Field("session_id", m_monitorSession);
    json.Field("sequence", m_monitorSequence);
    json.Field("reset_epoch", m_monitorReset);
    json.Field("interval_ms", m_monitorInterval);
    json.Field(
        "uptime_seconds",
        std::chrono::duration<double>(std::chrono::steady_clock::now() - m_monitorStart).count());
    json.Field("scope", "loaded_chunks");
    json.Object("world");
    json.Field("origin_chunk_x", std::to_string(m_originX));
    json.Field("origin_chunk_y", std::to_string(m_originY));
    json.Field("loaded_chunks", m_chunks.size());
    json.Field("actors", m_sceneGraph.Size());
    json.Field("remembered_npcs", m_npcMemory.size());
    json.Field("configured_towns", WorldTowns().size());
    json.Field("registered_npcs", m_townResidents.size());
    json.EndObject();
    const char* pause = !focused                     ? "monitor.pause.focus"
                        : m_dialogue.open            ? "monitor.pause.dialogue"
                        : m_journalOpen              ? "monitor.pause.journal"
                        : m_baseOpen                 ? "monitor.pause.base"
                        : m_map                      ? "monitor.pause.map"
                        : m_story.sceneActive        ? "monitor.pause.story"
                        : m_dialoguePages.size() > 1 ? "monitor.pause.notice"
                        : m_health <= 0              ? "monitor.pause.respawn"
                                                     : "";
    json.Object("simulation");
    json.Field("focused", focused);
    json.Field("paused", *pause != '\0');
    json.Field("reason", *pause ? m_text.Get(pause) : "");
    json.EndObject();
    json.Object("player");
    json.Field("level", m_level);
    json.Field("health", m_health);
    json.Field("max_health", MaxHealth());
    json.Field("legitimacy", m_story.legitimacy);
    json.Field("reputation", m_reputation);
    json.Field("story_stage", static_cast<int>(m_story.stage));
    json.Field("objective", m_text.Get(StoryObjectiveKey()));
    position(m_player, m_originX, m_originY);
    json.EndObject();
    const auto& metrics = FrameProfiler::Instance().LastFrame();
    const auto& counters = metrics.counters;
    json.Object("profiling");
    json.Field("frame", std::to_string(metrics.frame));
    json.Field("fps", metrics.fps);
    json.Field("interval_ms", metrics.intervalMs);
    json.Field("update_cpu_ms", metrics.updateMs);
    json.Field("render_cpu_ms", metrics.renderMs);
    json.Field("submit_cpu_ms", counters.submitMs);
    json.Field("post_cpu_ms", counters.postMs);
    json.Field("swap_ms", metrics.swapMs);
    json.Field("draw_total", counters.draws[0] + counters.draws[1] + counters.draws[2]);
    json.Field("draw_world", counters.draws[0]);
    json.Field("draw_overlay", counters.draws[1]);
    json.Field("draw_post", counters.draws[2]);
    json.Field("instances", counters.instances);
    json.Field("upload_bytes", counters.uploadBytes);
    json.EndObject();
    const auto transport = m_telemetry.Status();
    json.Object("transport");
    json.Field("sent", transport.sent);
    json.Field("failed", transport.failed);
    json.Field("replaced", transport.replaced);
    json.EndObject();
    json.Array("npcs");
    for (const auto& entry : m_townResidents)
    {
        const Citizen& npc = entry.second;
        const Actor* actor = m_sceneGraph.Find("citizen/" + npc.id);
        json.Object();
        json.Field("id", "citizen/" + npc.id);
        json.Field("name", npc.name.empty() ? npc.id : npc.name);
        json.Field("job_id", static_cast<int>(npc.occupation));
        json.Field("job",
                   m_text.Get("npc.job." + std::to_string(static_cast<int>(npc.occupation))));
        json.Field("activity_id", static_cast<int>(npc.activity));
        json.Field("activity",
                   m_text.Get("npc.activity." + std::to_string(static_cast<int>(npc.activity))));
        json.Field("health", npc.memory.health);
        json.Field("max_health", 36);
        json.Field("notoriety", npc.memory.notoriety);
        json.Field("trust", npc.memory.trust);
        json.Field("crimes", npc.memory.crimes);
        json.Field("services", npc.memory.services);
        json.Field("helped", npc.memory.helped);
        json.Field("apology", npc.memory.apology);
        json.Field("looted", npc.memory.looted);
        json.Field("active", npc.active);
        json.Field("enabled", actor && actor->EnabledInHierarchy());
        json.Field("loaded", actor != nullptr);
        json.Field("travelling", npc.travelling);
        json.Field("home_town",
                   npc.homeTown >= 0 ? WorldTowns()[static_cast<size_t>(npc.homeTown)].id : "");
        json.Field("destination_town",
                   npc.destinationTown >= 0
                       ? WorldTowns()[static_cast<size_t>(npc.destinationTown)].id
                       : "");
        json.Field("route_step", npc.routeStep);
        json.Field("route_count", npc.routeCount);
        json.Field("hostile", npc.hostile);
        json.Field("story", npc.story);
        position(npc.position, 0, 0);
        json.EndObject();
    }
    json.EndArray();
    json.Array("mobs");
    for (const EnemyActor& enemy : m_enemies)
    {
        const auto cx = (enemy.streamed ? m_originX : 0) +
                        static_cast<std::int64_t>(std::floor(enemy.position.x / ChunkSize));
        const auto cy = (enemy.streamed ? m_originY : 0) +
                        static_cast<std::int64_t>(std::floor(enemy.position.y / ChunkSize));
        if (cx < m_originX - StreamRadius || cx > m_originX + StreamRadius ||
            cy < m_originY - StreamRadius || cy > m_originY + StreamRadius)
        {
            continue;
        }
        const bool enabled = enemy.EnabledInHierarchy();
        json.Object();
        json.Field("id", enemy.Id());
        json.Field("name", m_text.Get(enemy.tutorial ? "monitor.mob" : "story.name.heir"));
        json.Field("health", enemy.health);
        json.Field("max_health", enemy.maxHealth);
        json.Field("enabled", enabled);
        json.Field("combat_enabled", enabled && enemy.tickEnabled);
        json.Field("tutorial", enemy.tutorial);
        json.Field("streamed", enemy.streamed);
        json.Field("generation", enemy.generation);
        json.Field("windup", enemy.windup);
        json.Field("cooldown", enemy.cooldown);
        json.Field("respawn", enemy.respawn);
        json.Field("loot", enemy.loot);
        const char* state = !enabled             ? "monitor.mob.inactive"
                            : enemy.health <= 0  ? "monitor.mob.defeated"
                            : !enemy.tickEnabled ? "monitor.mob.scripted"
                            : enemy.windup > 0   ? "monitor.mob.windup"
                            : enemy.cooldown > 0 ? "monitor.mob.cooldown"
                                                 : "monitor.mob.ready";
        json.Field("state", m_text.Get(state));
        position(enemy.position, enemy.streamed ? m_originX : 0, enemy.streamed ? m_originY : 0);
        json.EndObject();
    }
    json.EndArray();
    json.Array("villages");
    for (const auto& item : villages)
    {
        json.Object();
        json.Field("id", item.first);
        json.Field("name", item.second.name);
        json.Field("loaded_chunks", item.second.loaded);
        json.Field("expected_chunks",
                   item.second.town >= 0 ? item.second.expected : std::max(1, item.second.loaded));
        json.Field("kind", item.second.town >= 0 ? "town" : "roads");
        json.Field("population", item.second.population);
        json.Field("living_population", item.second.living);
        json.Field("hostile_population", item.second.hostiles);
        json.Field("travelling_population", item.second.travelling);
        if (item.second.town >= 0)
        {
            const auto& town = WorldTowns()[static_cast<size_t>(item.second.town)];
            json.Field("security", town.security);
            json.Field("target_population", town.population);
            json.Field("initial_hostiles", town.hostiles);
        }
        json.EndObject();
    }
    json.EndArray();
    json.EndObject();
    return json.Text();
}
