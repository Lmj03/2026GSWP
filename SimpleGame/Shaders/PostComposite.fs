#version 330 core
in vec2 v_UV;
uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform sampler2D u_Edge;
uniform float u_Exposure;
uniform float u_BloomStrength;
uniform float u_VignetteStrength;
uniform float u_VignetteStart;
uniform float u_VignetteEnd;
uniform float u_EdgeStrength;
uniform float u_EdgeStart;
uniform float u_EdgeEnd;
layout(location = 0) out vec4 FragColor;

vec3 linearToSrgb(vec3 color)
{
    return mix(color * 12.92,
               1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), color));
}

void main()
{
    // Elliptical screen mask: center stays sharp; edges transition continuously.
    float radius = length((v_UV - 0.5) * 2.0);
    vec3 hdr = texture(u_Scene, v_UV).rgb;
    if (u_EdgeStrength > 0.0)
    {
        float weight = smoothstep(u_EdgeStart, u_EdgeEnd, radius) * u_EdgeStrength;
        hdr = mix(hdr, texture(u_Edge, v_UV).rgb, weight);
    }
    if (u_BloomStrength > 0.0)
    {
        hdr += texture(u_Bloom, v_UV).rgb * u_BloomStrength;
    }
    float vignette = smoothstep(u_VignetteStart, u_VignetteEnd, radius);
    hdr *= 1.0 - u_VignetteStrength * vignette * vignette;
    // Exponential tone mapping preserves HDR headroom; output is encoded once.
    vec3 mapped = 1.0 - exp(-max(hdr, vec3(0.0)) * u_Exposure);
    FragColor = vec4(linearToSrgb(clamp(mapped, 0.0, 1.0)), 1.0);
}
