#version 330 core
in vec2 v_UV;
uniform sampler2D u_Source;
uniform vec2 u_Step;
layout(location = 0) out vec4 FragColor;

void main()
{
    // Separable Gaussian, paired linear samples approximate a nine-tap kernel.
    vec3 color = texture(u_Source, v_UV).rgb * 0.2270270270;
    color += texture(u_Source, v_UV + u_Step * 1.3846153846).rgb * 0.3162162162;
    color += texture(u_Source, v_UV - u_Step * 1.3846153846).rgb * 0.3162162162;
    color += texture(u_Source, v_UV + u_Step * 3.2307692308).rgb * 0.0702702703;
    color += texture(u_Source, v_UV - u_Step * 3.2307692308).rgb * 0.0702702703;
    FragColor = vec4(color, 1.0);
}
