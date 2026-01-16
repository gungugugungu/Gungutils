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
#define INV_PI 0.31830988618
#define NUM_SECTORS 8

void main() {
    vec3 st = texture(st_tex, uv).rgb;
    float A = st.r;
    float C = st.g;
    float B = st.b;

    float theta = 0.5 * atan(2.0 * B, A - C);

    float cos_t = cos(-theta);
    float sin_t = sin(-theta);
    float inv_aniso = 0.5;

    vec3 sum[NUM_SECTORS];
    vec3 sum_sq[NUM_SECTORS];
    float w_sum[NUM_SECTORS];

    for (int s = 0; s < NUM_SECTORS; s++) {
        sum[s] = vec3(0.0);
        sum_sq[s] = vec3(0.0);
        w_sum[s] = 0.0;
    }

    int kr = int(ceil(radius));
    float r_sq = radius * radius;
    float inv_r_sq = 1.0 / r_sq;

    for (int y = -kr; y <= kr; y++) {
        for (int x = -kr; x <= kr; x++) {
            float rx = (float(x) * cos_t - float(y) * sin_t) * inv_aniso;
            float ry = float(x) * sin_t + float(y) * cos_t;

            float dist_sq = rx * rx + ry * ry;
            if (dist_sq > r_sq) continue;

            float t_sq = dist_sq * inv_r_sq;
            float temp = 1.0 - t_sq;
            float weight = temp * temp * sharpness;

            float angle = atan(ry, rx);
            int sector = int(floor((angle + PI) * INV_PI * 0.5 * float(NUM_SECTORS))) & 7;

            vec3 color = texture(input_tex, uv + vec2(float(x), float(y)) * texel_size).rgb;

            sum[sector] += color * weight;
            sum_sq[sector] += color * color * weight;
            w_sum[sector] += weight;
        }
    }

    int min_s = 0;
    float min_var = 1e10;

    for (int s = 0; s < NUM_SECTORS; s++) {
        if (w_sum[s] > 0.001) {
            vec3 mean = sum[s] / w_sum[s];
            vec3 mean_sq = sum_sq[s] / w_sum[s];
            float var = dot(mean_sq - mean * mean, vec3(1.0));

            if (var < min_var) {
                min_var = var;
                min_s = s;
            }
        }
    }

    frag_color = vec4(sum[min_s] / w_sum[min_s], 1.0);
}
@end

@program kw_akw_program akw_vs akw_fs