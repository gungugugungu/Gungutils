@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

// ---- STRUCTURE TENSOR COMPUTAION ----
@vs st_vs
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

out vec2 uv;

void main() {
    const vec2 pos[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
    );
    vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    uv = 0.5 * (p + 1.0);
}
@end

@fs st_fs
layout(binding = 0) uniform texture2D u_input;
layout(binding = 0) uniform sampler u_input_smp;
#define input_tex sampler2D(u_input, u_input_smp)

layout(binding = 0) uniform st_params {
    vec2 texel_size;
};

in vec2 uv;
out vec4 frag_color;

float luminance(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    // Sobel operator for gradient
    float Ix =
    -1.0 * luminance(texture(input_tex, uv + vec2(-1.0, -1.0) * texel_size).rgb) +
    -2.0 * luminance(texture(input_tex, uv + vec2(-1.0,  0.0) * texel_size).rgb) +
    -1.0 * luminance(texture(input_tex, uv + vec2(-1.0,  1.0) * texel_size).rgb) +
    1.0 * luminance(texture(input_tex, uv + vec2( 1.0, -1.0) * texel_size).rgb) +
    2.0 * luminance(texture(input_tex, uv + vec2( 1.0,  0.0) * texel_size).rgb) +
    1.0 * luminance(texture(input_tex, uv + vec2( 1.0,  1.0) * texel_size).rgb);

    float Iy =
    -1.0 * luminance(texture(input_tex, uv + vec2(-1.0, -1.0) * texel_size).rgb) +
    -2.0 * luminance(texture(input_tex, uv + vec2( 0.0, -1.0) * texel_size).rgb) +
    -1.0 * luminance(texture(input_tex, uv + vec2( 1.0, -1.0) * texel_size).rgb) +
    1.0 * luminance(texture(input_tex, uv + vec2(-1.0,  1.0) * texel_size).rgb) +
    2.0 * luminance(texture(input_tex, uv + vec2( 0.0,  1.0) * texel_size).rgb) +
    1.0 * luminance(texture(input_tex, uv + vec2( 1.0,  1.0) * texel_size).rgb);

    float Ix2 = Ix * Ix;
    float Iy2 = Iy * Iy;
    float Ixy = Ix * Iy;

    // R = Ix^2, G = Iy^2, B = Ix*Iy
    frag_color = vec4(Ix2, Iy2, Ixy, 1.0);
}
@end

@program kw_st_program st_vs st_fs

// ---- ANISOTROPIC KUWAHARA FILTER ----
@vs akw_vs
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

out vec2 uv;

void main() {
    const vec2 pos[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
    );
    vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    uv = 0.5 * (p + 1.0);
}
@end

@fs akw_fs
layout(binding = 0) uniform texture2D u_input;
layout(binding = 0) uniform sampler u_input_smp;
#define input_tex sampler2D(u_input, u_input_smp)

layout(binding = 1) uniform texture2D u_structure_tensor;
layout(binding = 1) uniform sampler u_structure_tensor_smp;
#define st_tex sampler2D(u_structure_tensor, u_structure_tensor_smp)

layout(binding = 0) uniform akw_params {
    vec2 texel_size;
    float radius;
    float sharpness;
};

in vec2 uv;
out vec4 frag_color;

#define PI 3.14159265359
#define NUM_SECTORS 8

void main() {
    vec3 st = texture(st_tex, uv).rgb;
    float A = st.r;
    float C = st.g;
    float B = st.b;

    // dominant orientation from structure tensor
    float theta;
    if (abs(B) > 0.001) {
        float trace = A + C;
        float det = A * C - B * B;
        float lambda_max = 0.5 * (trace + sqrt(trace * trace - 4.0 * det));

        vec2 evec = normalize(vec2(B, lambda_max - A));
        theta = atan(evec.y, evec.x);
    } else {
        theta = (A > C) ? 0.0 : PI * 0.5;
    }

    float anisotropy = 2.0;

    vec3 sector_mean[NUM_SECTORS];
    float sector_var[NUM_SECTORS];

    for (int s = 0; s < NUM_SECTORS; s++) {
        sector_mean[s] = vec3(0.0);
        sector_var[s] = 0.0;
    }

    float sector_weight[NUM_SECTORS];
    for (int s = 0; s < NUM_SECTORS; s++) {
        sector_weight[s] = 0.0;
    }

    int kernel_radius = int(radius);

    for (int y = -kernel_radius; y <= kernel_radius; y++) {
        for (int x = -kernel_radius; x <= kernel_radius; x++) {
            vec2 offset = vec2(float(x), float(y));

            float cos_t = cos(-theta);
            float sin_t = sin(-theta);
            vec2 rotated = vec2(
            offset.x * cos_t - offset.y * sin_t,
            offset.x * sin_t + offset.y * cos_t
            );

            rotated.x /= anisotropy;

            float dist = length(rotated);
            if (dist > radius) continue;

            float angle = atan(rotated.y, rotated.x);
            int sector = int(floor((angle + PI) / (2.0 * PI) * float(NUM_SECTORS))) % NUM_SECTORS;

            float weight = exp(-dist * dist * sharpness);

            vec2 sample_uv = uv + offset * texel_size;
            vec3 color = texture(input_tex, sample_uv).rgb;

            sector_mean[sector] += color * weight;
            sector_weight[sector] += weight;
        }
    }

    for (int s = 0; s < NUM_SECTORS; s++) {
        if (sector_weight[s] > 0.0) {
            sector_mean[s] /= sector_weight[s];
        }
    }

    for (int y = -kernel_radius; y <= kernel_radius; y++) {
        for (int x = -kernel_radius; x <= kernel_radius; x++) {
            vec2 offset = vec2(float(x), float(y));

            float cos_t = cos(-theta);
            float sin_t = sin(-theta);
            vec2 rotated = vec2(
            offset.x * cos_t - offset.y * sin_t,
            offset.x * sin_t + offset.y * cos_t
            );

            rotated.x /= anisotropy;
            float dist = length(rotated);
            if (dist > radius) continue;

            float angle = atan(rotated.y, rotated.x);
            int sector = int(floor((angle + PI) / (2.0 * PI) * float(NUM_SECTORS))) % NUM_SECTORS;

            float weight = exp(-dist * dist * sharpness);

            vec2 sample_uv = uv + offset * texel_size;
            vec3 color = texture(input_tex, sample_uv).rgb;

            vec3 diff = color - sector_mean[sector];
            sector_var[sector] += dot(diff, diff) * weight;
        }
    }

    for (int s = 0; s < NUM_SECTORS; s++) {
        if (sector_weight[s] > 0.0) {
            sector_var[s] /= sector_weight[s];
        }
    }

    // minimum deviation
    int min_sector = 0;
    float min_var = sector_var[0];
    for (int s = 1; s < NUM_SECTORS; s++) {
        if (sector_var[s] < min_var) {
            min_var = sector_var[s];
            min_sector = s;
        }
    }

    frag_color = vec4(sector_mean[min_sector], 1.0);
}
@end

@program kw_akw_program akw_vs akw_fs