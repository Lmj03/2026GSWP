#version 330 core
layout(location = 0) in vec2 a_Corner;
layout(location = 1) in vec4 a_AB;
layout(location = 2) in vec4 a_CD;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec3 a_Material; // emission, texture slot (-1 for solid), ellipse
out vec4 v_Color;
out vec2 v_UV;
out float v_Emission;
flat out int v_TextureSlot;
flat out int v_Ellipse;
uniform vec2 u_LogicalSize;

void main()
{
    vec2 position = a_Corner.y < 0.5 ? (a_Corner.x < 0.5 ? a_AB.xy : a_AB.zw)
                                     : (a_Corner.x < 0.5 ? a_CD.zw : a_CD.xy);
    gl_Position = vec4(position.x * 2.0 / u_LogicalSize.x - 1.0,
                       1.0 - position.y * 2.0 / u_LogicalSize.y,
                       0.0,
                       1.0);
    v_Color = a_Color;
    v_UV = a_Corner;
    v_Emission = a_Material.x;
    v_TextureSlot = int(a_Material.y);
    v_Ellipse = int(a_Material.z);
}
