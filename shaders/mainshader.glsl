@ctype mat4 HMM_Mat4

@vs vs
in vec3 aPos;
in vec3 aNormal;
in vec2 aTexCoord;

out vec2 TexCoord;

layout(binding = 0) uniform vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
};

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
@end

@fs fs
out vec4 FragColor;

in vec2 TexCoord;

layout(binding = 0) uniform texture2D _palette2D;
layout(binding = 0) uniform sampler palette_smp;
#define palette2D sampler2D(_palette2D, palette_smp)

void main() {
    FragColor = texture(palette2D, TexCoord);
}
@end

@program simple vs fs