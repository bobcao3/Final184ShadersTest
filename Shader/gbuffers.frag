#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in VertexData {
    vec2 uv;
    vec3 color;
    vec3 n;
};

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

layout(binding = 1) uniform UBO {
	vec4 uniformColor;
    mat4 projection;
    mat4 modelview;
};

void main() {
    outAlbedo = uniformColor * vec4(color, 1.0);
    outNormal = vec4(n * 0.5 + 0.5, 0.0);
}
