@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs postprocess_vs
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

out vec2 uv;

void main() {
    // fullscreen triangle
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

@fs postprocess_fs
layout(binding = 0) uniform texture2D u_texture2D;
layout(binding = 0) uniform sampler u_texture_smp;
layout(binding = 1) uniform texture2D u_depth2D;
layout(binding = 1) uniform sampler u_depth_smp;
#define texture2D sampler2D(u_texture2D, u_texture_smp)
#define depth2D sampler2D(u_depth2D, u_depth_smp)

layout(binding = 2) uniform fs_params { // add post processing into the shader
    float vignette_strength;
    float vignette_radius;
    vec3 color_tint;
    float exposure;
    float contrast;
    float brightness;
    float saturation;
    float time;
};

layout(binding = 3) uniform ssao_params {
    float ao_radius; // radius of occlusion sampling
    float ao_bias; // bias to avoid self-occlusion (e.g. 0.02)
    float ao_strength; // how strongly ao darkens (0..1)
    float ao_power; // final power curve for aesthetic
    int ssao_samples; // number of samples (e.g. 8..32)
    vec2 proj; // proj.x = tan(fov0.5) * aspect, proj.y = tan(fov0.5)
    vec2 screen_size; // framebuffer size in pixels
    float u_near; // camera near
    float u_far; // camera far
};

in vec2 uv;
out vec4 frag_color;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float linearize_reversed_depth(float d, float near, float far){
    float denom = max((far - d * (far - near)), 1e-6);
    float viewZ = (near * far) / denom;
    return viewZ;
}

vec3 reconstruct_view_pos(vec2 uvcoord, float depth_sample){
    float viewZ = linearize_reversed_depth(depth_sample, u_near, u_far);
    vec2 ndc = uvcoord * 2.0 - 1.0;
    vec3 viewPos;
    viewPos.x = ndc.x * viewZ * proj.x;
    viewPos.y = ndc.y * viewZ * proj.y;
    viewPos.z = -viewZ;
    return viewPos;
}

vec3 estimate_normal(vec2 uvcoord, float center_depth){
    vec2 px = 1.0 / screen_size;
    float depth_r = texture(depth2D, uvcoord + vec2(px.x, 0.0)).r;
    float depth_u = texture(depth2D, uvcoord + vec2(0.0, px.y)).r;
    vec3 p = reconstruct_view_pos(uvcoord, center_depth);
    vec3 pr = reconstruct_view_pos(uvcoord + vec2(px.x, 0.0), depth_r);
    vec3 pu = reconstruct_view_pos(uvcoord + vec2(0.0, px.y), depth_u);
    vec3 vx = pr - p;
    vec3 vy = pu - p;
    vec3 n = normalize(cross(vx, vy));
    if(length(n) < 1e-3) return vec3(0.0, 0.0, 1.0);
    return n;
}

vec2 project_view_to_uv(vec3 viewPos){
    vec2 ndc;
    ndc.x = viewPos.x / (-viewPos.z * proj.x);
    ndc.y = viewPos.y / (-viewPos.z * proj.y);
    return ndc * 0.5 + 0.5;
}

const int MAX_SAMPLES = 32;
const vec3 kernel16[16] = vec3[](
vec3( 0.5381, 0.1856, 0.4319),
vec3( 0.1379, 0.2486, 0.4430),
vec3( 0.3371, 0.5679, 0.0057),
vec3(-0.6999, -0.0451, -0.0019),
vec3( 0.0689, -0.1598, 0.8547),
vec3( 0.0560, 0.0069, -0.1843),
vec3(-0.0146, 0.1402, 0.0762),
vec3( 0.0100, -0.1924, -0.0344),
vec3(-0.3577, -0.5301, -0.4358),
vec3(-0.3169, 0.1063, 0.0158),
vec3( 0.0103, -0.5869, 0.0046),
vec3(-0.0897, -0.4940, 0.3287),
vec3( 0.7119, -0.0154, -0.0918),
vec3(-0.0533, 0.0596, -0.5411),
vec3( 0.0352, -0.0631, 0.5460),
vec3(-0.4776, 0.2847, -0.0271)
);

int outline_size = 2;
float outline_trigger_dist = 0.5;

void main() {
    vec3 color = texture(texture2D, uv).rgb;
    float d = texture(depth2D, uv).r;
    if (d >= 0.9999) {
        frag_color = vec4(color, 1.0);
        return;
    }

    vec3 P = reconstruct_view_pos(uv, d);// view-space position
    vec3 N = estimate_normal(uv, d);

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    int samples = clamp(ssao_samples, 1, 32);
    float occlusion = 0.0;
    float invScreenX = 1.0 / screen_size.x;
    float invScreenY = 1.0 / screen_size.y;

    for (int i = 0; i < samples; ++i){
        vec3 sampleDir;
        if (i < 16){
            sampleDir = kernel16[i];
        } else {
            float a = rand(uv + float(i) * 0.13) * 6.28318530718;
            float z = rand(uv + float(i) * 0.37);
            float r = sqrt(max(0.0, 1.0 - z*z));
            sampleDir = vec3(r * cos(a), r * sin(a), z);
        }

        vec3 sampleVec = tangent * sampleDir.x + bitangent * sampleDir.y + N * abs(sampleDir.z);
        float scale = float(i) / float(samples);
        scale = mix(0.1, 1.0, scale * scale);// push more samples nearer
        vec3 samplePos = P + sampleVec * ao_radius * scale;

        vec2 sampleUV = project_view_to_uv(samplePos);

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        float sampleDepthTex = texture(depth2D, sampleUV).r;
        float sampleDepthViewZ = linearize_reversed_depth(sampleDepthTex, u_near, u_far);

        float samplePosViewZ = -samplePos.z;// positive distance

        float rangeCheck = smoothstep(0.0, 1.0, ao_radius / (abs(P.z - (-sampleDepthViewZ)) + 1e-4));
        if (sampleDepthViewZ < (samplePosViewZ - ao_bias)) {
            occlusion += rangeCheck;
        }
    }

    float occ = clamp(occlusion / float(samples), 0.0, 1.0);

    float ao = 1.0 - occ;
    ao = pow(mix(1.0, ao, ao_strength), ao_power);

    vec3 final_color = color * ao; // ambient-occlusion done by here

    float highest_depth = 0.0;
    float lowest_depth = 1.0;
    vec2 texel = 1.0 / screen_size;

    float center_raw = texture(depth2D, uv).r;
    float center_depth = linearize_reversed_depth(center_raw, u_near, u_far);

    float max_diff = 0.0;

    for (int y=-outline_size; y<=outline_size; y++) {
        for (int x=-outline_size; x<=outline_size; x++) {
            vec2 offset = uv + vec2(x, y) * texel;
            float sample_raw = texture(depth2D, offset).r;
            float sample_depth = linearize_reversed_depth(sample_raw, u_near, u_far);

            float diff = abs(center_depth - sample_depth);
            max_diff = max(max_diff, diff);
        }
    }

    if (max_diff > outline_trigger_dist) {
        final_color = vec3(0.0, 0.0, 0.0); // outline
    }

    frag_color = vec4(final_color, 1.0);
}
@end

@program postprocess postprocess_vs postprocess_fs