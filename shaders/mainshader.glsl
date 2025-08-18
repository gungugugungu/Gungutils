
@ctype mat4 HMM_Mat4

@vs vs
in vec3 aPos;
in vec3 aNormal;
in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

layout(binding = 0) uniform vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
    float opacity;
};

layout(location = 1) out float v_opacity;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);
    TexCoord = aTexCoord;
    v_opacity = opacity;
}
@end

@fs fs
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

layout(binding = 0) uniform texture2D _palette2D;
layout(binding = 0) uniform sampler palette_smp;
layout(location = 1) in float v_opacity;

layout(binding = 1) uniform fs_params {
    vec3 view_pos;
    int num_lights;
    vec4 light_positions[32];
    vec4 light_directions[32];
    vec4 light_colors[32];
    vec4 light_data1[32];
    vec4 light_data2[32];
    ivec4 light_types[32];
};

layout(binding = 2) uniform material_params {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

vec3 calculateDirectionalLight(int i, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light_directions[i].xyz);

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 ambient_contrib = ambient * light_colors[i].xyz;
    vec3 diffuse_contrib = diff * diffuse * light_colors[i].xyz;
    vec3 specular_contrib = spec * specular * light_colors[i].xyz;

    return (ambient_contrib + diffuse_contrib + specular_contrib) * light_data1[i].x;
}

vec3 calculatePointLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light_positions[i].xyz - fragPos);

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    float distance = length(light_positions[i].xyz - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

    vec3 ambient_contrib = ambient * light_colors[i].xyz;
    vec3 diffuse_contrib = diff * diffuse * light_colors[i].xyz;
    vec3 specular_contrib = spec * specular * light_colors[i].xyz;

    ambient_contrib *= attenuation;
    diffuse_contrib *= attenuation;
    specular_contrib *= attenuation;

    return (ambient_contrib + diffuse_contrib + specular_contrib) * light_data1[i].x;
}

vec3 calculateSpotLight(int i, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light_positions[i].xyz - fragPos);

    float diff = max(dot(normal, lightDir), 0.0);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    float distance = length(light_positions[i].xyz - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

    float theta = dot(lightDir, normalize(-light_directions[i].xyz));
    float epsilon = light_data1[i].z - light_data1[i].w;
    float intensity_spot = clamp((theta - light_data1[i].w) / epsilon, 0.0, 1.0);

    vec3 ambient_contrib = ambient * light_colors[i].xyz;
    vec3 diffuse_contrib = diff * diffuse * light_colors[i].xyz;
    vec3 specular_contrib = spec * specular * light_colors[i].xyz;

    ambient_contrib *= attenuation * intensity_spot;
    diffuse_contrib *= attenuation * intensity_spot;
    specular_contrib *= attenuation * intensity_spot;

    return (ambient_contrib + diffuse_contrib + specular_contrib) * light_data1[i].x;
}

void main() {
    vec3 color = texture(sampler2D(_palette2D, palette_smp), TexCoord).rgb;
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(view_pos - FragPos);

    vec3 result = vec3(0.0);

    for(int i = 0; i < num_lights && i < 32; i++) {
        if(light_types[i].x == 0) {
            result += calculateDirectionalLight(i, normal, viewDir);
        } else if(light_types[i].x == 1) {
            result += calculatePointLight(i, normal, FragPos, viewDir);
        } else if(light_types[i].x == 2) {
            result += calculateSpotLight(i, normal, FragPos, viewDir);
        }
    }

    FragColor = vec4(result * color, v_opacity);
}
@end

@program main vs fs