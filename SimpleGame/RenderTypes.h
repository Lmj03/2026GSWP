#pragma once

struct Point
{
    float x, y;

    Point(float px = 0, float py = 0)
        : x(px),
          y(py)
    {
    }
};

struct Color
{
    float r, g, b, a, emission;

    Color(float red = 1, float green = 1, float blue = 1, float alpha = 1, float light = 0)
        : r(red),
          g(green),
          b(blue),
          a(alpha),
          emission(light)
    {
    }

    Color Alpha(float alpha) const
    {
        return Color(r, g, b, alpha, emission);
    }

    Color Shade(float factor) const
    {
        return Color(r * factor, g * factor, b * factor, a, emission);
    }

    Color Emissive(float strength) const
    {
        return Color(r, g, b, a, strength);
    }
};
