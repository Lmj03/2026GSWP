#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace Models
{
constexpr std::size_t MaxPalette = 48;
using Vector = std::array<float, 4>;
using Matrix = std::array<float, 16>;
Matrix Identity();
Matrix Multiply(const Matrix& a, const Matrix& b);
Matrix Transform(const Vector& translation, const Vector& rotation, const Vector& scale);
Vector RotateBlend(Vector a, Vector b, float factor);

struct Pose
{
    Vector translation = {{0, 0, 0, 0}};
    Vector rotation = {{0, 0, 0, 1}};
    Vector scale = {{1, 1, 1, 0}};
};

struct Node
{
    std::string name;
    int parent = -1;
    Pose bind;
    Matrix matrix = Identity();
    bool matrixAuthored = false;
};

// Material colors are linear, matching glTF. Geometry is immutable after import.
struct Vertex
{
    float position[3] = {};
    float normal[3] = {0, 1, 0};
    float color[4] = {1, 1, 1, 1};
    float emission[3] = {};
    std::uint32_t joints[4] = {};
    float weights[4] = {1, 0, 0, 0};
};

struct Joint
{
    size_t node = 0;
    Matrix inverseBind = Identity();
};

enum class Path
{
    Translation,
    Rotation,
    Scale
};

enum class Interpolation
{
    Step,
    Linear,
    Cubic
};

struct Channel
{
    size_t node = 0;
    Path path = Path::Translation;
    Interpolation interpolation = Interpolation::Linear;
    std::vector<float> times;
    std::vector<Vector> values;
};

struct Clip
{
    std::string name;
    float duration = 0;
    bool loop = false;
    bool rootMotion = false;
    float blendSeconds = .12f;
    std::array<std::vector<int>, 2> contactFrames;
    std::vector<Channel> channels;
};

struct Asset
{
    std::string id;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Node> nodes;
    // Parent-before-child traversal independent of the source node array order.
    std::vector<size_t> hierarchy;
    std::vector<Joint> palette;
    std::vector<Clip> clips;
    const Clip* FindClip(const std::string& name) const;
};

// Import is CPU-only and does not need a graphics context. Errors never return a partial asset.
bool LoadGlb(const std::wstring& file, Asset& output, std::string& error);
bool LoadObj(const std::wstring& file, Asset& output, std::string& error);

class Animator
{
public:
    // The referenced immutable asset must outlive this animator.
    void SetAsset(const Asset& asset);
    bool Play(const std::string& clip, float blendSeconds = -1, bool restart = false);
    void Update(float dt, float speed = 1);
    bool Socket(const std::string& node, Matrix& world) const;
    const std::vector<Matrix>& Palette() const;
    const Clip* Current() const;
    bool Finished() const;

private:
    const Asset* m_asset = nullptr;
    const Clip* m_clip = nullptr;
    float m_time = 0, m_blendTime = 0, m_blendDuration = 0;
    std::vector<Pose> m_pose, m_from;
    std::vector<Matrix> m_world, m_palette;
    void Evaluate();
};
}
