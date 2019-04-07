#version 330 core

#include common.glsl
#include underwater.glsl

in vec3 pos;
in vec3 normal;
in vec4 color;
in vec2 uv;

out VertexData {
	vec3 normal;
	vec3 color;
	vec2 uv[5];
	vec4 texBlendFactor;
} vertexOut;

uniform mat4 mPV;
uniform mat4 mW;

void main() {
	vec3 wPos = (mW * vec4(pos, 1)).xyz;
	gl_Position = vec4(wPos, 1);
	gl_ClipDistance[0] = wPos.y * sign(subspace) + bReflection * 0.1 + bRefraction * 0.01;

    vertexOut.normal = (mW * vec4(normal, 0)).xyz;
    vertexOut.color = color.xyz;
    vertexOut.uv[0] = uv;
}
