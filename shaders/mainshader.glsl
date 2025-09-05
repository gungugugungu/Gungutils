@ctype mat4 HMM_Mat4

@vs vs
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 0) out vec2 TexCoord;
layout(location = 1) out float v_opacity;
layout(location = 2) out vec3 vNormal;
layout(location = 3) flat out int venable_shading;
layout(location = 4) out vec3 vWorldPos;
layout(location = 5) out vec3 vTangent;
layout(location = 6) out vec3 vBitangent;
layout(location = 7) out vec4 vShadowCoord;

layout(binding = 0) uniform vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
    float opacity;
    int enable_shading;
    mat4 light_space;
};

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    v_opacity = opacity;
    mat3 normalMat = mat3(transpose(inverse(model)));
    vNormal = normalize(normalMat * aNormal);
    vec3 tangent = normalize(normalMat * aTangent);
    vec3 bitangent = cross(vNormal, tangent);
    venable_shading = enable_shading;
    vWorldPos = (model * vec4(aPos, 1.0)).xyz;
    vTangent = tangent;
    vBitangent = bitangent;
    vShadowCoord = light_space * vec4(vWorldPos, 1.0);
}

@end

@fs fs
out vec4 FragColor;
layout(location = 0) in vec2 TexCoord;
layout(location = 1) in float v_opacity;
layout(location = 2) in vec3 vNormal;
layout(location = 3) flat in int venable_shading;
layout(location = 4) in vec3 vWorldPos;
layout(location = 5) in vec3 vTangent;
layout(location = 6) in vec3 vBitangent;
layout(location = 7) in vec4 vShadowCoord;
layout(binding = 0) uniform texture2D _diffuse_tex2D;
layout(binding = 0) uniform sampler diffuse_tex_smp;
layout(binding = 1) uniform texture2D _specular_tex2D;
layout(binding = 1) uniform sampler specular_tex_smp;
layout(binding = 2) uniform texture2D _normal_tex2D;
layout(binding = 2) uniform sampler normal_tex_smp;
layout(binding = 3) uniform texture2D _shadow_tex2D;
layout(binding = 3) uniform sampler shadow_tex_smp;

layout(binding = 2) uniform model_fs_params {
    float shininess;
    vec4 camera_pos;
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
    mat4 light_space;
};

#define diffuse_texture2D sampler2D(_diffuse_tex2D, diffuse_tex_smp)
#define specular_texture2D sampler2D(_specular_tex2D, specular_tex_smp)
#define normal_texture2D sampler2D(_normal_tex2D, normal_tex_smp)
#define shadow_texture2D sampler2D(_shadow_tex2D, shadow_tex_smp)

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

float calculateShadow(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    float closestDepth = texture(shadow_texture2D, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(vNormal, light_directions[0].xyz)), 0.005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_texture2D, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadow_texture2D, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec4 base_color = texture(diffuse_texture2D, TexCoord);
    vec4 specular_tex_color = texture(specular_texture2D, TexCoord);
    vec4 normal_tex_color = texture(normal_texture2D, TexCoord);
    float spec_strength = specular_tex_color.r;
    vec3 tangentNormal = normal_tex_color.xyz * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    vec3 N = normalize(TBN * tangentNormal);
    const float AMBIENT_STRENGTH = 0.35;
    const float TOON_BANDS = 4.0;

    vec3 ambient_col = ambient_color.xyz;
    vec3 ambientTerm = ambient_col * AMBIENT_STRENGTH * base_color.rgb;

    vec3 V = normalize(camera_pos.xyz - vWorldPos);

    vec3 diffuseTerm = vec3(0.0);
    vec3 specularTerm = vec3(0.0);

    float bands = max(TOON_BANDS - 1.0, 1.0);
    float bandStep = 1.0 / bands;
    float dither = (bayer4x4(gl_FragCoord.xy) - 0.5) * bandStep;
    float shadow = 0.0;
    if (light_amount > 0 && light_types_packed[0][0] == 0) {
        shadow = calculateShadow(vShadowCoord);
    }
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
        float thisShadow = (i == 0 && type == 0) ? (1.0 - shadow) : 1.0;
        diffuseTerm += lightColor * q * base_color.rgb * thisShadow;
        float spec = 0.0;
        if (ndl > 0.0 && spec_strength > 0.0) {
            vec3 H = normalize(L + V);
            spec = pow(max(dot(N, H), 0.0), shininess) * spec_strength;
            spec = bayer4x4(gl_FragCoord.xy) * spec;
        }
        specularTerm += lightColor * spec * atten * thisShadow;
    }

    vec3 finalRgb = base_color.rgb;
    if (venable_shading == 1) {
        finalRgb = ambientTerm + diffuseTerm + specularTerm;
    }

    FragColor = vec4(clamp(finalRgb, 0.0, 1.0), base_color.a * v_opacity);
}
@end

@program main vs fs