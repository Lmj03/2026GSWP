#version 330 core
in vec4 v_Color;
in vec2 v_UV;
uniform sampler2D u_Coverage;
layout(location = 0) out vec4 FragColor;
void main()
{
    FragColor = vec4(v_Color.rgb, v_Color.a * texture(u_Coverage, v_UV).r);
}

