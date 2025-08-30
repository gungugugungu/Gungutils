@ctype mat4 HMM_Mat4

@vs billboard_vs
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 texCoord;

layout(binding = 0) uniform billboard_vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
};

layout(location = 0) out vec2 uv;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    uv = texCoord;
}
@end

@fs billboard_fs
out vec4 FragColor;

layout(binding = 0) uniform texture2D _tex2d;
layout(binding = 0) uniform sampler tex2d_smp;

#define texy sampler2D(_tex2d, tex2d_smp)

layout(location = 0) in vec2 uv;

void main() {
    vec4 color = texture(texy, uv);

    FragColor = color;
}
@end

@program billboard billboard_vs billboard_fs