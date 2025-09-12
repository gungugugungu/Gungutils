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
    tangent = normalize(tangent - dot(tangent, vNormal) * vNormal);
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
layout(binding = 0) uniform texture2D _base_color_tex2D;
layout(binding = 0) uniform sampler base_color_tex_smp;
layout(binding = 1) uniform texture2D _metallic_roughness_tex2D;
layout(binding = 1) uniform sampler metallic_roughness_tex_smp;
layout(binding = 2) uniform texture2D _normal_tex2D;
layout(binding = 2) uniform sampler normal_tex_smp;
layout(binding = 3) uniform texture2D _emissive_tex2D;
layout(binding = 3) uniform sampler emissive_tex_smp;
layout(binding = 4) uniform texture2D _shadow_tex2D;
layout(binding = 4) uniform sampler shadow_tex_smp;
layout(binding = 5) uniform textureCube _skybox_tex2D;
layout(binding = 5) uniform sampler skybox_tex_smp;

layout(binding = 2) uniform model_fs_params {
    float shininess;
    vec4 camera_pos;
};

layout(binding = 3) uniform lighting_params {
    ivec4 light_types_packed[38];
    vec4 light_positions[150];
    vec4 light_directions[150];
    vec4 light_colors[150];
    vec4 light_att_params[150];
    int light_amount;
    float padding1;
    float padding2;
    float padding3;
    vec4 ambient_color;
    mat4 light_space;
};

#define base_color_texture2D sampler2D(_base_color_tex2D, base_color_tex_smp)
#define metallic_roughness_texture2D sampler2D(_metallic_roughness_tex2D, metallic_roughness_tex_smp)
#define normal_texture2D sampler2D(_normal_tex2D, normal_tex_smp)
#define emissive_tex2D sampler2D(_emissive_tex2D, emissive_tex_smp)
#define shadow_texture2D sampler2DShadow(_shadow_tex2D, shadow_tex_smp)

const float PI = 3.14159265359;

float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 0.001);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float calculateShadow(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(vNormal, -light_directions[0].xyz)), 0.005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_texture2D, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float lit = texture(shadow_texture2D, vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth - bias));
            shadow += 1.0 - lit;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec4 base_color = texture(base_color_texture2D, TexCoord);
    vec3 albedo = base_color.rgb;
    float alpha = base_color.a;
    vec3 mr = texture(metallic_roughness_texture2D, TexCoord).rgb;
    float ao = mr.r;
    float roughness = max(mr.g, 0.04);
    float metallic = max(mr.b, 0.04);
    vec3 normal_tex_color = texture(normal_texture2D, TexCoord).xyz;
    vec3 tangentNormal = normal_tex_color * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    vec3 N = normalize(TBN * tangentNormal);
    vec3 emissive_tex_color = texture(emissive_tex2D, TexCoord).xyz;

    const float AMBIENT_STRENGTH = 0.35;
    vec3 ambient_col = ambient_color.xyz;
    vec3 ambientTerm = ambient_col * AMBIENT_STRENGTH * albedo * ao;

    vec3 V = normalize(camera_pos.xyz - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

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
                atten = atten_dist;
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

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float thisShadow = (i == 0 && type == 0) ? (1.0 - shadow) : 1.0;

        if (NdotL > 0.0) {
            float NdotV = max(dot(N, V), 0.000001);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);

            float D = DistributionGGX(NdotH, roughness);
            float G = GeometrySmith(NdotV, NdotL, roughness);
            vec3 F = fresnelSchlick(HdotV, F0);

            float denom = 4.0 * NdotL * NdotV + 0.001;
            vec3 specular = D * G * F / max(denom, 0.001);

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            vec3 diffuse = (kD * albedo / PI);

            Lo += (diffuse + specular) * lightColor * NdotL * atten * thisShadow;
        }
    }

    vec3 finalRgb = albedo;
    if (venable_shading == 1) {
        finalRgb = ambientTerm + Lo;
    }

    FragColor = vec4(clamp(finalRgb, 0.0, 1.0), alpha * v_opacity)+vec4(emissive_tex_color, 0.0f);
}
@end

@program main vs fs