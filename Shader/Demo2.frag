#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in VertexData {
    vec2 inUV;
    vec3 color;
};

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform UBO {
	vec4 uniformColor;
    mat4 projection;
    mat4 modelview;
};

layout(set = 1, binding = 0) uniform sampler s;
layout(set = 1, binding = 1) uniform texture2D t;

void main() {
	vec4 sampledColor = texture(sampler2D(t, s), inUV);
    outColor = uniformColor * sampledColor * vec4(color, 1.0);
}
