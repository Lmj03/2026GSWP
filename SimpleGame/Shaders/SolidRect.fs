#version 330 core
in vec4 v_Color;
in vec2 v_UV;
in float v_Emission;
uniform sampler2D u_Coverage;
uniform bool u_LinearOutput;
uniform float u_EmissionGain;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec3 color = max(v_Color.rgb, vec3(0.0));
    if (u_LinearOutput)
    {
        color =
            mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
        color *= 1.0 + max(v_Emission, 0.0) * u_EmissionGain;
    }
    FragColor = vec4(color, v_Color.a * texture(u_Coverage, v_UV).r);
}
