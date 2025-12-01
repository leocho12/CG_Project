#version 330 core

in vec2 vTex;
out vec4 FragColor;

uniform vec3 uColor;          // 기본 색
uniform sampler2D uTex;       // 텍스처
uniform int uUseTexture;      // 0: 색만, 1: 텍스처 * 색

void main()
{
    vec4 col = vec4(uColor, 1.0);

    if (uUseTexture == 1)
    {
        col *= texture(uTex, vTex);
    }

    FragColor = col;
}
