#include "stdafx.h"
#include "CityScene.h"
#include "FrameProfiler.h"
#include "WorldLayout.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
constexpr float QuarterTurn = 1.57079632679f;

std::uint64_t TileBit(int x, int y)
{
    return std::uint64_t(1) << (y * 8 + x);
}

bool AppendSurface(Models::Asset& mesh,
                   const std::shared_ptr<const Models::Asset>& source,
                   float x,
                   float y,
                   float width,
                   float depth,
                   int turn,
                   float tint)
{
    if (!source)
    {
        return false;
    }
    const float spanX = source->maximum[0] - source->minimum[0];
    const float spanZ = source->maximum[2] - source->minimum[2];
    if (spanX <= 0 || spanZ <= 0)
    {
        return false;
    }
    const auto first = mesh.indices.size();
    for (size_t i = 0; i < source->indices.size(); i += 3)
    {
        const auto& face = source->vertices[source->indices[i]];
        // Top surfaces and paint only. Buried slab sides must not cross neighboring tiles.
        if (face.normal[1] < .9f || face.position[1] < -.001f)
        {
            continue;
        }
        for (size_t j = 0; j < 3; ++j)
        {
            auto vertex = source->vertices[source->indices[i + j]];
            const float u = (vertex.position[0] - source->minimum[0]) / spanX - .5f;
            const float v = .5f - (vertex.position[2] - source->minimum[2]) / spanZ;
            const float tx = turn == 1 ? -v : turn == 2 ? -u : u;
            const float ty = turn == 1 ? u : turn == 2 ? -v : v;
            vertex.position[0] = x - 4.f + tx * width;
            vertex.position[2] = -(y - 4.f + ty * depth);
            vertex.normal[0] = vertex.normal[2] = 0;
            vertex.normal[1] = 1;
            for (size_t channel = 0; channel < 3; ++channel)
            {
                vertex.color[channel] *= tint;
            }
            mesh.indices.push_back(static_cast<std::uint32_t>(mesh.vertices.size()));
            mesh.vertices.push_back(vertex);
        }
    }
    return mesh.indices.size() > first;
}

void OrderSurface(Models::Asset& mesh)
{
    std::vector<size_t> faces(mesh.indices.size() / 3);
    std::iota(faces.begin(), faces.end(), 0);
    std::stable_sort(faces.begin(),
                     faces.end(),
                     [&](size_t a, size_t b)
                     {
                         return mesh.vertices[mesh.indices[a * 3]].position[1] <
                                mesh.vertices[mesh.indices[b * 3]].position[1];
                     });
    std::vector<std::uint32_t> ordered;
    ordered.reserve(mesh.indices.size());
    for (size_t face : faces)
    {
        ordered.insert(ordered.end(),
                       mesh.indices.begin() + face * 3,
                       mesh.indices.begin() + face * 3 + 3);
    }
    mesh.indices = std::move(ordered);
}
}

void CityScene::LoadEnvironmentModels()
{
    m_supplyModel.Bind(m_models, "ENV_prop_crate");
    m_terminalModel.Bind(m_models, "ENV_prop_terminal");
    m_refugeGround.Bind(m_models, "ENV_road_sidewalk");
    m_refugeModels[0].Bind(m_models, "ENV_prop_bench");
    m_refugeModels[1].Bind(m_models, "ENV_prop_terminal");
    m_refugeModels[2].Bind(m_models, "ENV_prop_crate");
    m_droneModel.Bind(m_models, "ENEMY_surveillance_drone");
}

std::shared_ptr<const Models::Asset> CityScene::TerrainModel(std::int64_t cx, std::int64_t cy)
{
    static_assert(ChunkSize == 8, "Environment surface templates match the 8x8 road grid");
    if (!m_models.Enabled())
    {
        return {};
    }
    const bool inTown = WorldTownIndex(cx, cy) >= 0;
    const auto variant = static_cast<int>(WorldHash(cx, cy, 1209) % 4);
    std::uint64_t mask = 0;
    if (!inTown)
    {
        for (int y = 0; y < ChunkSize; ++y)
        {
            for (int x = 0; x < ChunkSize; ++x)
            {
                if (WorldRoad(cx, cy, x, y))
                {
                    mask |= TileBit(x, y);
                }
            }
        }
    }
    const std::string id = inTown ? "terrain_model/town/" + std::to_string(variant)
                                  : "terrain_model/open/" + std::to_string(mask);
    const auto cached = m_terrainModels.find(id);
    if (cached != m_terrainModels.end())
    {
        return cached->second;
    }
    auto mesh = std::make_shared<Models::Asset>();
    mesh->id = id;
    mesh->nodes.emplace_back();
    mesh->hierarchy.push_back(0);
    mesh->palette.emplace_back();
    bool complete = true;
    auto surface = [&](const char* asset, float x, float y, float w, float d, int turn, float tint)
    {
        complete = AppendSurface(*mesh, m_models.Get(asset), x, y, w, d, turn, tint) && complete;
    };
    if (inTown)
    {
        constexpr int edges[] = {0, 3, 5, 8};
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                const bool street = row == 1 || col == 1;
                const bool junction = row == 1 && col == 1;
                const char* asset = !street ? "ENV_road_sidewalk"
                                    : junction
                                        ? (variant == 2 ? "ENV_road_corner" : "ENV_road_junction")
                                    : variant == 1 && row == 1 ? "ENV_road_crossing"
                                    : variant == 3             ? "ENV_road_alley"
                                                               : "ENV_road_straight";
                const int turn = !street ? (row == 0 ? 2 : 0) : (row == 1 && !junction ? 1 : 0);
                surface(asset,
                        (edges[col] + edges[col + 1]) * .5f,
                        (edges[row] + edges[row + 1]) * .5f,
                        static_cast<float>(edges[col + 1] - edges[col]),
                        static_cast<float>(edges[row + 1] - edges[row]),
                        turn,
                        street ? .6f : .42f);
            }
        }
        if (variant == 0 || variant == 2)
        {
            surface("ENV_prop_manhole", 4.55f, 4.55f, .58f, .58f, 0, .75f);
        }
    }
    else
    {
        surface("ENV_road_alley", 4, 4, 8, 8, 0, .20f);
        // Greedily combine contiguous highway tiles; use exactly the existing road mask.
        for (int y = 0; y < ChunkSize; ++y)
        {
            for (int x = 0; x < ChunkSize; ++x)
            {
                if (!(mask & TileBit(x, y)))
                {
                    continue;
                }
                int endX = x + 1, endY = y + 1;
                while (endX < ChunkSize && (mask & TileBit(endX, y)))
                {
                    ++endX;
                }
                while (endY < ChunkSize)
                {
                    bool full = true;
                    for (int tx = x; tx < endX; ++tx)
                    {
                        full = full && (mask & TileBit(tx, endY));
                    }
                    if (!full)
                    {
                        break;
                    }
                    ++endY;
                }
                for (int ty = y; ty < endY; ++ty)
                {
                    for (int tx = x; tx < endX; ++tx)
                    {
                        mask &= ~TileBit(tx, ty);
                    }
                }
                surface("ENV_road_straight",
                        (x + endX) * .5f,
                        (y + endY) * .5f,
                        static_cast<float>(endX - x),
                        static_cast<float>(endY - y),
                        endX - x > endY - y ? 1 : 0,
                        .6f);
            }
        }
    }
    if (!complete)
    {
        return m_terrainModels.emplace(id, nullptr).first->second;
    }
    OrderSurface(*mesh);
    FrameProfiler::Instance().Event("environment_surface id=" + id +
                                    " triangles=" + std::to_string(mesh->indices.size() / 3));
    return m_terrainModels.emplace(id, std::move(mesh)).first->second;
}

EnvironmentPropActor::EnvironmentPropActor(const std::string& id,
                                           CityScene& scene,
                                           const char* asset,
                                           float scale,
                                           float yaw,
                                           float height)
    : Actor(id),
      m_scene(scene),
      m_scale(scale),
      m_height(height)
{
    renderEnabled = m_model.Bind(m_scene.m_models, asset);
    m_model.yaw = yaw;
}

void EnvironmentPropActor::Draw(Renderer& renderer) const
{
    const auto* building = dynamic_cast<const BuildingActor*>(Parent());
    const float opacity =
        building ? m_scene.BuildingOpacity(*building, building->WorldPosition()) : 1.f;
    m_scene.DrawEnvironmentModel(renderer, m_model, WorldPosition(), m_scale, m_height, opacity);
}

void CityScene::AddBuildingDecoration(const BuildingActor& building)
{
    if (!m_models.Enabled())
    {
        return;
    }
    const std::string& id = building.Building::id;
    const char* asset = id == "arcadia"  ? "ENV_prop_camera"
                        : id == "repair" ? "ENV_prop_pipe"
                        : id == "market" ? "ENV_prop_neon_sign"
                        : id == "east"   ? "ENV_prop_vent"
                                         : nullptr;
    if (!asset)
    {
        return;
    }
    const float offset = building.w * (id == "market" ? .58f : .82f);
    const float elevation = id == "repair" ? 0.f : building.height * .36f;
    auto& prop = m_sceneGraph.Create<EnvironmentPropActor>(building.Id() + "/decoration",
                                                           building.Id(),
                                                           *this,
                                                           asset,
                                                           .48f,
                                                           0.f,
                                                           elevation);
    prop.SetLocalPosition({offset, building.d});
    prop.depthOffset = building.w - offset + .01f;
}

void CityScene::RefreshEnvironmentProps()
{
    if (!m_models.Enabled())
    {
        return;
    }
    const char* assets[] = {"ENV_prop_streetlamp",
                            "ENV_prop_bollard",
                            "ENV_prop_barrier",
                            "ENV_prop_dumpster",
                            "ENV_prop_bench"};
    const float scales[] = {.50f, .50f, .40f, .48f, .50f};
    const Point pads[] = {{2.4f, 2.4f}, {5.6f, 2.4f}, {2.4f, 5.6f}, {5.6f, 5.6f}};
    auto nearObjective = [&](Point p, Point native, float margin)
    {
        Point local;
        return LevelLocal(native, local) && std::hypot(local.x - p.x, local.y - p.y) < margin;
    };
    for (const auto& chunk : m_chunks)
    {
        const auto cx = m_originX + chunk.first, cy = m_originY + chunk.second;
        if (WorldTownIndex(cx, cy) < 0)
        {
            continue;
        }
        const auto hash = WorldHash(cx, cy, 1388);
        const auto type = static_cast<size_t>(hash % 5);
        for (size_t i = 0; i < 4; ++i)
        {
            const auto& pad = pads[((hash >> 8) + i) % 4];
            const Point p{chunk.first * ChunkSize + pad.x, chunk.second * ChunkSize + pad.y};
            if (WorldRoad(cx, cy, static_cast<int>(pad.x), static_cast<int>(pad.y)) ||
                !CanStand(p.x, p.y, .55f) || nearObjective(p, m_spawn, 1.7f) ||
                nearObjective(p, m_supply, 1.4f))
            {
                continue;
            }
            if (std::any_of(m_storyNodes.begin(),
                            m_storyNodes.end(),
                            [&](const auto& node)
                            {
                                return nearObjective(p, node.second, 1.4f);
                            }))
            {
                continue;
            }
            const std::string id =
                "environment/prop/" + std::to_string(cx) + "/" + std::to_string(cy);
            auto* actor = m_sceneGraph.Find(id);
            if (!actor)
            {
                actor = &m_sceneGraph.Create<EnvironmentPropActor>(id,
                                                                   "streamed",
                                                                   *this,
                                                                   assets[type],
                                                                   scales[type],
                                                                   ((hash >> 16) & 1) ? QuarterTurn
                                                                                      : 0.f);
            }
            actor->SetLocalPosition(p);
            m_nextStreamActors.insert(id);
            break;
        }
    }
}

bool CityScene::DrawEnvironmentModel(Renderer& renderer,
                                     const ModelVisual& model,
                                     Point position,
                                     float scale,
                                     float height,
                                     float opacity) const
{
    if (!model.Asset())
    {
        return false;
    }
    const Point foot = Project(position.x, position.y, height);
    if (foot.x < -140 || foot.x > Renderer::Width + 140 || foot.y < -140 ||
        foot.y > Renderer::Height + 140)
    {
        return true;
    }
    Models::DrawStyle style;
    style.opacity = opacity;
    return model.Draw(renderer, foot, scale * 32 * m_zoom, style);
}
