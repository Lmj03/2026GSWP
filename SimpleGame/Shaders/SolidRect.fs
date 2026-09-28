#version 330 core
in vec4 v_Color;
in vec2 v_UV;
in float v_Emission;
flat in int v_TextureSlot;
flat in int v_Ellipse;
uniform sampler2D u_Coverage[8];
uniform bool u_LinearOutput;
uniform float u_EmissionGain;
layout(location = 0) out vec4 FragColor;

// GLSL 3.30 requires constant sampler-array indices. Font images have no mipmaps.
float Coverage()
{
    switch (v_TextureSlot)
    {
    case 0:
        return textureLod(u_Coverage[0], v_UV, 0.0).r;
    case 1:
        return textureLod(u_Coverage[1], v_UV, 0.0).r;
    case 2:
        return textureLod(u_Coverage[2], v_UV, 0.0).r;
    case 3:
        return textureLod(u_Coverage[3], v_UV, 0.0).r;
    case 4:
        return textureLod(u_Coverage[4], v_UV, 0.0).r;
    case 5:
        return textureLod(u_Coverage[5], v_UV, 0.0).r;
    case 6:
        return textureLod(u_Coverage[6], v_UV, 0.0).r;
    case 7:
        return textureLod(u_Coverage[7], v_UV, 0.0).r;
    default:
        return 1.0;
    }
}

void main()
{
    if (v_Ellipse != 0)
    {
        vec2 p = v_UV * 2.0 - 1.0;
        if (dot(p, p) > 1.0)
        {
            discard;
        }
    }
    vec3 color = max(v_Color.rgb, vec3(0.0));
    if (u_LinearOutput)
    {
        color =
            mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
        color *= 1.0 + max(v_Emission, 0.0) * u_EmissionGain;
    }
    FragColor = vec4(color, v_Color.a * Coverage());
}
