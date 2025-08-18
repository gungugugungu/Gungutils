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
    float opacity;
};

layout(location = 1) out float v_opacity;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    v_opacity = opacity;
}
@end

@fs fs
out vec4 FragColor;

in vec2 TexCoord;

layout(binding = 0) uniform texture2D _palette2D;
layout(binding = 0) uniform sampler palette_smp;
layout(location = 1) in float v_opacity;
#define palette2D sampler2D(_palette2D, palette_smp)

void main() {
    vec4 texColor = texture(palette2D, TexCoord);
    FragColor = vec4(texColor.rgb, texColor.a*v_opacity);
}
@end

@program main vs fs