@ctype mat4 HMM_Mat4

@vs particle_vs
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 texCoord;

layout(binding = 0) uniform particle_vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
};

out vec2 uv;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    uv = texCoord;
}
@end

@fs particle_fs
out vec4 FragColor;

layout(binding = 0) uniform texture2D _tex2d;
layout(binding = 0) uniform sampler tex2d_smp;

#define texture2D sampler2D(_tex2d, tex2d_smp)

in vec2 uv;

void main() {
    vec4 color = texture(texture2D, uv);

    FragColor = color;
}
@end

@program particle particle_vs particle_fs