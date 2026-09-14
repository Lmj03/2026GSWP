#version 330 core
in vec2 v_UV;
uniform sampler2D u_Source;
uniform vec2 u_Texel;
uniform float u_Threshold;
uniform float u_Knee;
uniform bool u_Extract;
layout(location = 0) out vec4 FragColor;

vec3 filterLight(vec3 color)
{
    if (!u_Extract)
    {
        return color;
    }
    // Peak channel preserves saturated pink and cyan neon equally well.
    float peak = max(color.r, max(color.g, color.b));
    float knee = max(u_Threshold * u_Knee, 0.0001);
    float soft = clamp(peak - u_Threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(peak - u_Threshold, soft) / max(peak, 0.0001);
    return color * contribution;
}

void main()
{
    // Filter each source pixel before reducing resolution, retaining thin lights.
    vec2 offset = u_Texel * 0.5;
    vec3 result = filterLight(texture(u_Source, v_UV + vec2(-offset.x, -offset.y)).rgb);
    result += filterLight(texture(u_Source, v_UV + vec2(offset.x, -offset.y)).rgb);
    result += filterLight(texture(u_Source, v_UV + vec2(-offset.x, offset.y)).rgb);
    result += filterLight(texture(u_Source, v_UV + offset).rgb);
    FragColor = vec4(result * 0.25, 1.0);
}
