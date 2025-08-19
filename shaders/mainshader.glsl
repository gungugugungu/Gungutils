@ctype mat4 HMM_Mat4

@vs vs
in vec3 aPos;
in vec3 aNormal;
in vec2 aTexCoord;

layout(location = 0) out vec2 TexCoord;
layout(location = 1) out float v_opacity;
layout(location = 2) out vec3 vNormal;
layout(location = 3) flat out int venable_shading;

layout(binding = 0) uniform vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
    float opacity;
    int enable_shading; // 0 = disable, 1 = enable
};

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    v_opacity = opacity;

    // transform normal into world space
    mat3 normalMat = mat3(transpose(inverse(model)));
    vNormal = normalize(normalMat * aNormal);

    venable_shading = enable_shading;
}
@end

@fs fs
out vec4 FragColor;

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in float v_opacity;
layout(location = 2) in vec3 vNormal;
layout(location = 3) flat in int venable_shading;

layout(binding = 0) uniform texture2D _diffuse_tex2D;
layout(binding = 0) uniform sampler diffuse_tex_smp;
layout(binding = 1) uniform texture2D _specular_tex2D;
layout(binding = 1) uniform sampler specular_tex_smp;

layout(binding = 2) uniform model_fs_params {
    int has_diffuse_tex;
    int has_specular_tex;
    float specular; // in case of no specular texture, just render it with this setting instead of a map. 0f to 1f
};

#define diffuse_texture2D sampler2D(_diffuse_tex2D, diffuse_tex_smp)
#define specular_texture2D sampler2D(_specular_tex2D, specular_tex_smp)

float bayer4x4(vec2 fragXY) {
    // use integer pixel coordinates modulo 4
    ivec2 p = ivec2(floor(fragXY)) & ivec2(3, 3); // faster than mod for power-of-two
    int idx = p.y * 4 + p.x;

    // index order:
    //  0,  8,  2, 10
    // 12,  4, 14,  6
    //  3, 11,  1,  9
    // 15,  7, 13,  5
    int bayerVals[16] = int[16](
    0,  8,  2, 10,
    12,  4, 14,  6,
    3, 11,  1,  9,
    15,  7, 13,  5
    );
    return (float(bayerVals[idx]) + 0.5) / 16.0; // center within each bin
}

void main() {
    vec4 diffuse_tex_color = texture(diffuse_texture2D, TexCoord);
    vec4 specular_tex_color = texture(specular_texture2D, TexCoord);

    // directional light settings
    const vec3 LIGHT_DIR = normalize(vec3(-0.4, -1.0, -0.6));
    const vec3 LIGHT_COLOR = vec3(1.0, 1.0, 1.0);
    const vec3 AMBIENT_COLOR = vec3(0.5, 0.5, 0.5);
    const float AMBIENT_STRENGTH = 0.35;
    const float TOON_BANDS = 4.0;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-LIGHT_DIR); // from point toward light
    float ndl = max(dot(N, L), 0.0);

    // dither only the lighting bands.
    float bands = max(TOON_BANDS - 1.0, 1.0);
    float bandStep = 1.0 / bands;

    // Bayer in [-0.5, +0.5], then scale by band step for stable band crossing
    float dither = (bayer4x4(gl_FragCoord.xy) - 0.5) * bandStep;

    float ndlDithered = clamp(ndl + dither, 0.0, 1.0);
    float q = round(ndlDithered * bands) / bands;

    // combine ambient + toon diffuse (dithered)
    vec3 lighting = AMBIENT_COLOR * AMBIENT_STRENGTH + LIGHT_COLOR * q;

    vec3 finalRgb;
    if (venable_shading == 1) {
        finalRgb = clamp(diffuse_tex_color.rgb * lighting, 0.0, 1.0);
    } else {
        finalRgb = diffuse_tex_color.rgb;
    }

    FragColor = vec4(finalRgb, diffuse_tex_color.a * v_opacity);
}
@end

@program main vs fs