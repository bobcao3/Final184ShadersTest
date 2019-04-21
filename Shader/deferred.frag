#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in VertexData {
    vec2 uv;
};

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler s;
layout(set = 1, binding = 1) uniform texture2D t_albedo;
layout(set = 1, binding = 2) uniform texture2D t_normals;
layout(set = 1, binding = 3) uniform texture2D t_depth;

void main() {
    outColor = vec4(texture(sampler2D(t_albedo, s), uv).rgb, 0.0);
}
