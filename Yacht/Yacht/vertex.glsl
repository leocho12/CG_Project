#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTex;   // 텍스처 좌표

uniform mat4 uMVP;

out vec2 vTex;

void main()
{
    vTex = aTex;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
