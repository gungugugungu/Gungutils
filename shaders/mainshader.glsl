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
    int enable_shading; // 0 = disable, 1 = enable
};

layout(location = 1) out float v_opacity;
layout(location = 2) out vec3 vNormal;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    v_opacity = opacity;

    // transform normal into world space
    mat3 normalMat = mat3(transpose(inverse(model)));
    vNormal = normalize(normalMat * aNormal);
}
@end

@fs fs
out vec4 FragColor;

in vec2 TexCoord;

layout(binding = 0) uniform texture2D _palette2D;
layout(binding = 0) uniform sampler palette_smp;
layout(location = 1) in float v_opacity;
layout(location = 2) in vec3 vNormal;

#define palette2D sampler2D(_palette2D, palette_smp)

void main() {
    vec4 texColor = texture(palette2D, TexCoord);

    // directional light settings
    const vec3 LIGHT_DIR = normalize(vec3(-0.4, -1.0, -0.6));
    const vec3 LIGHT_COLOR = vec3(1.0, 0.98, 0.90);
    const vec3 AMBIENT_COLOR = vec3(0.12, 0.12, 0.12);
    const float AMBIENT_STRENGTH = 0.35;
    const float TOON_BANDS = 4.0;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-LIGHT_DIR); // from point toward light
    float ndl = max(dot(N, L), 0.0);

    float q = round(ndl * (TOON_BANDS - 1.0)) / (TOON_BANDS - 1.0);

    // combine ambient + toon diffuse modulated by texture/albedo
    vec3 lighting = AMBIENT_COLOR * AMBIENT_STRENGTH + LIGHT_COLOR * q;
    vec3 finalRgb = clamp(texColor.rgb * lighting, 0.0, 1.0);

    FragColor = vec4(finalRgb, texColor.a * v_opacity);
}
@end

@program main vs fs