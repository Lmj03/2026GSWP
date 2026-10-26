#include "stdafx.h"
#include "ModelAsset.h"
#include <algorithm>
#include <cmath>

namespace Models
{
Matrix Identity()
{
    return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
}

Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result = {};
    for (size_t column = 0; column < 4; ++column)
    {
        for (size_t row = 0; row < 4; ++row)
        {
            for (size_t k = 0; k < 4; ++k)
            {
                result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
            }
        }
    }
    return result;
}

namespace
{
Vector Normalized(Vector q)
{
    float length = 0;
    for (float v : q)
    {
        length += v * v;
    }
    if (length < 1e-12f)
    {
        return {{0, 0, 0, 1}};
    }
    for (float& v : q)
    {
        v /= std::sqrt(length);
    }
    return q;
}

Vector Lerp(const Vector& a, const Vector& b, float factor)
{
    Vector result;
    for (size_t i = 0; i < 4; ++i)
    {
        result[i] = a[i] + (b[i] - a[i]) * factor;
    }
    return result;
}

Vector Sample(const Channel& channel, float time)
{
    const bool cubic = channel.interpolation == Interpolation::Cubic;
    auto value = [&](size_t index)
    {
        return channel.values[index * (cubic ? 3 : 1) + (cubic ? 1 : 0)];
    };
    if (time <= channel.times.front())
    {
        return value(0);
    }
    if (time >= channel.times.back())
    {
        return value(channel.times.size() - 1);
    }
    const size_t right =
        std::upper_bound(channel.times.begin(), channel.times.end(), time) - channel.times.begin();
    const size_t left = right - 1;
    const float interval = channel.times[right] - channel.times[left];
    const float t = (time - channel.times[left]) / interval;
    if (channel.interpolation == Interpolation::Step)
    {
        return value(left);
    }
    if (!cubic)
    {
        return channel.path == Path::Rotation ? RotateBlend(value(left), value(right), t)
                                              : Lerp(value(left), value(right), t);
    }
    Vector result;
    const float t2 = t * t, t3 = t2 * t;
    for (size_t i = 0; i < 4; ++i)
    {
        result[i] = (2 * t3 - 3 * t2 + 1) * value(left)[i] +
                    (t3 - 2 * t2 + t) * interval * channel.values[left * 3 + 2][i] +
                    (-2 * t3 + 3 * t2) * value(right)[i] +
                    (t3 - t2) * interval * channel.values[right * 3][i];
    }
    return channel.path == Path::Rotation ? Normalized(result) : result;
}
}

Vector RotateBlend(Vector a, Vector b, float factor)
{
    a = Normalized(a);
    b = Normalized(b);
    float dot = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        dot += a[i] * b[i];
    }
    if (dot < 0)
    {
        dot = -dot;
        for (float& v : b)
        {
            v = -v;
        }
    }
    if (dot > .9995f)
    {
        return Normalized(Lerp(a, b, factor));
    }
    const float angle = std::acos(std::max(-1.f, std::min(1.f, dot)));
    const float denominator = std::sin(angle);
    Vector result;
    for (size_t i = 0; i < 4; ++i)
    {
        result[i] =
            (a[i] * std::sin((1 - factor) * angle) + b[i] * std::sin(factor * angle)) / denominator;
    }
    return Normalized(result);
}

Matrix Transform(const Vector& t, const Vector& rotation, const Vector& s)
{
    const auto q = Normalized(rotation);
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    return {{(1 - 2 * (y * y + z * z)) * s[0],
             2 * (x * y + z * w) * s[0],
             2 * (x * z - y * w) * s[0],
             0,
             2 * (x * y - z * w) * s[1],
             (1 - 2 * (x * x + z * z)) * s[1],
             2 * (y * z + x * w) * s[1],
             0,
             2 * (x * z + y * w) * s[2],
             2 * (y * z - x * w) * s[2],
             (1 - 2 * (x * x + y * y)) * s[2],
             0,
             t[0],
             t[1],
             t[2],
             1}};
}

const Clip* Asset::FindClip(const std::string& name) const
{
    for (const auto& clip : clips)
    {
        if (clip.name == name)
        {
            return &clip;
        }
    }
    return nullptr;
}

void Animator::SetAsset(const Asset& asset)
{
    m_asset = &asset;
    m_clip = nullptr;
    m_time = m_blendTime = m_blendDuration = 0;
    m_pose.resize(asset.nodes.size());
    m_from.resize(asset.nodes.size());
    m_world.resize(asset.nodes.size());
    m_palette.resize(asset.palette.size());
    Evaluate();
}

bool Animator::Play(const std::string& name, float blendSeconds, bool restart)
{
    const Clip* clip = m_asset ? m_asset->FindClip(name) : nullptr;
    if (!clip)
    {
        return false;
    }
    if (m_clip == clip && !restart)
    {
        return true;
    }
    m_from = m_pose;
    m_blendDuration = std::max(0.f, blendSeconds < 0 ? clip->blendSeconds : blendSeconds);
    m_blendTime = m_time = 0;
    m_clip = clip;
    Evaluate();
    return true;
}

void Animator::Update(float dt, float speed)
{
    if (!m_asset || !std::isfinite(dt) || !std::isfinite(speed) || dt < 0 || speed < 0)
    {
        return;
    }
    m_blendTime = std::min(m_blendDuration, m_blendTime + dt);
    if (m_clip && m_clip->duration > 0)
    {
        const double next = static_cast<double>(m_time) + static_cast<double>(dt) * speed;
        m_time = m_clip->loop
                     ? static_cast<float>(std::fmod(next, m_clip->duration))
                     : static_cast<float>(std::min(next, static_cast<double>(m_clip->duration)));
    }
    Evaluate();
}

void Animator::Evaluate()
{
    for (size_t i = 0; i < m_pose.size(); ++i)
    {
        m_pose[i] = m_asset->nodes[i].bind;
    }
    if (m_clip)
    {
        for (const auto& channel : m_clip->channels)
        {
            Vector& target = channel.path == Path::Translation ? m_pose[channel.node].translation
                             : channel.path == Path::Rotation  ? m_pose[channel.node].rotation
                                                               : m_pose[channel.node].scale;
            target = Sample(channel, m_time);
        }
    }
    if (m_blendDuration > 0 && m_blendTime < m_blendDuration)
    {
        const float factor = m_blendTime / m_blendDuration;
        for (size_t i = 0; i < m_pose.size(); ++i)
        {
            m_pose[i].translation = Lerp(m_from[i].translation, m_pose[i].translation, factor);
            m_pose[i].rotation = RotateBlend(m_from[i].rotation, m_pose[i].rotation, factor);
            m_pose[i].scale = Lerp(m_from[i].scale, m_pose[i].scale, factor);
        }
    }
    for (size_t i : m_asset->hierarchy)
    {
        const auto& node = m_asset->nodes[i];
        const Matrix local =
            node.matrixAuthored
                ? node.matrix
                : Transform(m_pose[i].translation, m_pose[i].rotation, m_pose[i].scale);
        m_world[i] =
            node.parent < 0 ? local : Multiply(m_world[static_cast<size_t>(node.parent)], local);
    }
    for (size_t i = 0; i < m_palette.size(); ++i)
    {
        const auto& joint = m_asset->palette[i];
        m_palette[i] = Multiply(m_world[joint.node], joint.inverseBind);
    }
}

bool Animator::Socket(const std::string& name, Matrix& world) const
{
    if (m_asset)
    {
        for (size_t i = 0; i < m_asset->nodes.size(); ++i)
        {
            if (m_asset->nodes[i].name == name)
            {
                world = m_world[i];
                return true;
            }
        }
    }
    return false;
}

const std::vector<Matrix>& Animator::Palette() const
{
    return m_palette;
}

const Clip* Animator::Current() const
{
    return m_clip;
}

bool Animator::Finished() const
{
    return m_clip && !m_clip->loop && m_time >= m_clip->duration;
}
}
