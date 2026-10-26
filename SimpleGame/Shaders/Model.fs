#version 330 core
in vec3 v_Normal;
in vec4 v_Color;
in vec3 v_Emission;
uniform bool u_HDR;
uniform float u_EmissionGain;
out vec4 FragColor;
void main()
{
    vec3 normal = normalize(v_Normal);
    float light = 0.42 + 0.58 * abs(dot(normal, normalize(vec3(-0.5, 1, 0.6))));
    vec3 color = max(v_Color.rgb, vec3(0)) * light + max(v_Emission, vec3(0)) * u_EmissionGain;
    if (!u_HDR)
    {
        color = mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
                    step(vec3(0.0031308), color));
    }
    FragColor = vec4(color, 1);
}
