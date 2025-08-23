@ctype mat4 HMM_Mat4

@vs vs
in vec3 aPos;
in vec3 aNormal;
in vec2 aTexCoord;

layout(location = 0) out vec2 TexCoord;
layout(location = 1) out float v_opacity;
layout(location = 2) out vec3 vNormal;
layout(location = 3) flat out int venable_shading;
layout(location = 4) out vec3 vWorldPos;

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

    vWorldPos = (model * vec4(aPos, 1.0)).xyz;
}
@end

@fs fs
out vec4 FragColor;

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in float v_opacity;
layout(location = 2) in vec3 vNormal;
layout(location = 3) flat in int venable_shading;
layout(location = 4) in vec3 vWorldPos;

layout(binding = 0) uniform texture2D _diffuse_tex2D;
layout(binding = 0) uniform sampler diffuse_tex_smp;
layout(binding = 1) uniform texture2D _specular_tex2D;
layout(binding = 1) uniform sampler specular_tex_smp;

layout(binding = 2) uniform model_fs_params {
    int has_diffuse_tex;
    int has_specular_tex;
    float specular;
    float shininess;
    vec4 camera_pos; // 16 bytes (x,y,z,w(=unused))
};

layout(binding = 3) uniform lighting_params {
    ivec4 light_types_packed[13];
    vec4 light_positions[50];
    vec4 light_directions[50];
    vec4 light_colors[50];
    vec4 light_att_params[50];
    int light_amount;
    float padding1;
    float padding2;
    float padding3;
    vec4 ambient_color;
};

#define diffuse_texture2D sampler2D(_diffuse_tex2D, diffuse_tex_smp)
#define specular_texture2D sampler2D(_specular_tex2D, specular_tex_smp)

float bayer4x4(vec2 fragXY) {
    ivec2 p = ivec2(floor(fragXY)) & ivec2(3, 3);
    int idx = p.y * 4 + p.x;

    int bayerVals[16] = int[16](
    0,  8,  2, 10,
    12,  4, 14,  6,
    3, 11,  1,  9,
    15,  7, 13,  5
    );
    return (float(bayerVals[idx]) + 0.5) / 16.0;
}

void main() {
    vec4 base_color = (has_diffuse_tex == 1) ? texture(diffuse_texture2D, TexCoord) : vec4(1.0);
    vec4 specular_tex_color = (has_specular_tex == 1) ? texture(specular_texture2D, TexCoord) : vec4(0.0);
    float spec_strength = (has_specular_tex == 1) ? specular_tex_color.r : specular;

    const float AMBIENT_STRENGTH = 0.35;
    const float TOON_BANDS = 4.0;

    vec3 ambient_col = ambient_color.xyz;
    vec3 ambientTerm = ambient_col * AMBIENT_STRENGTH * base_color.rgb;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(camera_pos.xyz - vWorldPos);

    vec3 diffuseTerm = vec3(0.0);
    vec3 specularTerm = vec3(0.0);

    float bands = max(TOON_BANDS - 1.0, 1.0);
    float bandStep = 1.0 / bands;
    float dither = (bayer4x4(gl_FragCoord.xy) - 0.5) * bandStep;

    for (int i = 0; i < light_amount; i++) {
        int idx = i >> 2;
        int comp = i & 3;
        int type = light_types_packed[idx][comp];

        vec3 color = light_colors[i].xyz;
        float intensity = light_colors[i].w;
        vec3 lightColor = color * intensity;

        vec3 L;
        float atten = 1.0;

        if (type == 0) {
            vec3 lightDir = light_directions[i].xyz;
            L = normalize(-lightDir);
        } else {
            vec3 lightPos = light_positions[i].xyz;
            vec3 toLight = lightPos - vWorldPos;
            float dist = length(toLight);
            if (dist <= 0.0) continue;
            L = toLight / dist;

            float atten_dist = 1.0;
            float radius = light_att_params[i].x;
            if (type == 1) {
                if (dist > radius) continue;
                float d = dist / radius;
                atten_dist = (1.0 - d * d) * (1.0 - d * d);
            } else if (type == 2) {
                vec3 lightDir = light_directions[i].xyz;
                vec3 spotDir = normalize(lightDir);
                float cosTheta = dot(-L, spotDir);

                float cosInner = cos(light_att_params[i].y);
                float cosOuter = cos(light_att_params[i].z);
                float spot_atten = 0.0;
                if (cosTheta > cosInner) {
                    spot_atten = 1.0;
                } else if (cosTheta > cosOuter) {
                    spot_atten = (cosTheta - cosOuter) / (cosInner - cosOuter);
                }
                atten = atten_dist * spot_atten;
            } else {
                atten = atten_dist;
            }
        }

        float ndl = max(dot(N, L), 0.0);
        float diffuse_factor = ndl * atten;

        float ndlDithered = clamp(diffuse_factor + dither, 0.0, 1.0);
        float q = round(ndlDithered * bands) / bands;

        diffuseTerm += lightColor * q * base_color.rgb;

        float spec = 0.0;
        if (ndl > 0.0 && spec_strength > 0.0) {
            vec3 H = normalize(L + V);
            spec = pow(max(dot(N, H), 0.0), shininess) * spec_strength;
            spec = bayer4x4(gl_FragCoord.xy) * spec;
        }
        specularTerm += lightColor * spec * atten;
    }

    vec3 finalRgb = base_color.rgb;
    if (venable_shading == 1) {
        finalRgb = ambientTerm + diffuseTerm + specularTerm;
    }

    FragColor = vec4(clamp(finalRgb, 0.0, 1.0), base_color.a * v_opacity);
}
@end

@program main vs fs