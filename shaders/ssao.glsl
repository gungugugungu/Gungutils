@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs ssao_vs
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

@fs ssao_fs
layout(binding = 0) uniform texture2D u_depth2D;
layout(binding = 0) uniform sampler u_depth_smp;
#define depth2D sampler2D(u_depth2D, u_depth_smp)

layout(binding = 0) uniform ssao_params {
    float ao_radius;
    float ao_bias;
    int ssao_samples;
    vec2 proj;
    vec2 screen_size;
    float u_near;
    float u_far;
};

in vec2 uv;
out vec4 ao_output;

const vec3 noise8x8[64] = vec3[](
vec3( 0.5773, -0.5773,  0.0), vec3(-0.7071,  0.7071,  0.0), vec3(-0.9239,  0.3827,  0.0), vec3( 0.3827,  0.9239,  0.0),
vec3(-0.3827, -0.9239,  0.0), vec3( 0.9239, -0.3827,  0.0), vec3( 0.7071, -0.7071,  0.0), vec3(-0.5773, -0.5773,  0.0),
vec3( 0.0000,  1.0000,  0.0), vec3(-1.0000,  0.0000,  0.0), vec3( 0.7071,  0.7071,  0.0), vec3(-0.3827,  0.9239,  0.0),
vec3( 0.9239,  0.3827,  0.0), vec3( 0.3827, -0.9239,  0.0), vec3(-0.7071, -0.7071,  0.0), vec3( 0.5773,  0.5773,  0.0),
vec3(-0.8660, -0.5000,  0.0), vec3( 0.5000,  0.8660,  0.0), vec3( 0.9659, -0.2588,  0.0), vec3(-0.2588, -0.9659,  0.0),
vec3( 0.2588,  0.9659,  0.0), vec3(-0.9659,  0.2588,  0.0), vec3(-0.5000, -0.8660,  0.0), vec3( 0.8660,  0.5000,  0.0),
vec3( 0.1305,  0.9914,  0.0), vec3(-0.9914, -0.1305,  0.0), vec3( 0.7934,  0.6088,  0.0), vec3(-0.6088,  0.7934,  0.0),
vec3( 0.6088, -0.7934,  0.0), vec3(-0.7934, -0.6088,  0.0), vec3( 0.9914, -0.1305,  0.0), vec3(-0.1305, -0.9914,  0.0),
vec3( 0.4226,  0.9063,  0.0), vec3(-0.9063, -0.4226,  0.0), vec3( 0.9063,  0.4226,  0.0), vec3(-0.4226,  0.9063,  0.0),
vec3( 0.4226, -0.9063,  0.0), vec3(-0.9063,  0.4226,  0.0), vec3(-0.4226, -0.9063,  0.0), vec3( 0.9063, -0.4226,  0.0),
vec3( 0.7477,  0.6641,  0.0), vec3(-0.6641, -0.7477,  0.0), vec3( 0.6641,  0.7477,  0.0), vec3(-0.7477,  0.6641,  0.0),
vec3( 0.7477, -0.6641,  0.0), vec3(-0.6641,  0.7477,  0.0), vec3(-0.7477, -0.6641,  0.0), vec3( 0.6641, -0.7477,  0.0),
vec3( 0.8192,  0.5736,  0.0), vec3(-0.5736, -0.8192,  0.0), vec3( 0.5736,  0.8192,  0.0), vec3(-0.8192,  0.5736,  0.0),
vec3( 0.8192, -0.5736,  0.0), vec3(-0.5736,  0.8192,  0.0), vec3(-0.8192, -0.5736,  0.0), vec3( 0.5736, -0.8192,  0.0),
vec3( 0.9848,  0.1736,  0.0), vec3(-0.1736, -0.9848,  0.0), vec3( 0.1736,  0.9848,  0.0), vec3(-0.9848,  0.1736,  0.0),
vec3( 0.9848, -0.1736,  0.0), vec3(-0.1736,  0.9848,  0.0), vec3(-0.9848, -0.1736,  0.0), vec3( 0.1736, -0.9848,  0.0)
);

const vec3 kernel16[16] = vec3[](
vec3( 0.2024, 0.0699, 0.1620), vec3( 0.0515, 0.0929, 0.1659),
vec3( 0.1261, 0.2124, 0.0214), vec3(-0.2616, -0.0168, -0.0071),
vec3( 0.0257, -0.0598, 0.3198), vec3( 0.0209, 0.0026, -0.0689),
vec3(-0.0055, 0.0524, 0.0285), vec3( 0.0037, -0.0719, -0.0129),
vec3(-0.1338, -0.1982, -0.1630), vec3(-0.1185, 0.0398, 0.0059),
vec3( 0.0039, -0.2195, 0.0017), vec3(-0.0335, -0.1848, 0.1229),
vec3( 0.2663, -0.0058, -0.0343), vec3(-0.0199, 0.0223, -0.2024),
vec3( 0.0132, -0.0236, 0.2041), vec3(-0.1786, 0.1065, -0.0101)
);

float linearize_reversed_depth(float d, float near, float far) {
    float denom = max((near + d * (far - near)), 1e-6);
    float viewZ = (near * far) / denom;
    return viewZ;
}

vec3 reconstruct_view_pos(vec2 uvcoord, float depth_sample) {
    float viewZ = linearize_reversed_depth(depth_sample, u_near, u_far);
    vec2 ndc = uvcoord * 2.0 - 1.0;
    vec3 viewPos;
    viewPos.x = ndc.x * viewZ * proj.x;
    viewPos.y = ndc.y * viewZ * proj.y;
    viewPos.z = -viewZ;
    return viewPos;
}

vec3 estimate_normal(vec2 uvcoord, float center_depth) {
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

vec2 project_view_to_uv(vec3 viewPos) {
    vec2 ndc;
    ndc.x = viewPos.x / (-viewPos.z * proj.x);
    ndc.y = viewPos.y / (-viewPos.z * proj.y);
    return ndc * 0.5 + 0.5;
}

vec3 sample_noise(vec2 uv) {
    vec2 p = uv * screen_size;
    float n = sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453;
    float angle = fract(n) * 6.28318;
    return vec3(cos(angle), sin(angle), 0.0);
}

void main() {
    float d = texture(depth2D, uv).r;

    if (d <= 0.0001) {
        ao_output = vec4(1.0);
        return;
    }

    vec3 P = reconstruct_view_pos(uv, d);
    vec3 N = estimate_normal(uv, d);

    vec3 randomVec = sample_noise(uv);

    vec3 tangent = normalize(randomVec - N * dot(randomVec, N));
    vec3 bitangent = cross(N, tangent);
    mat3 TBN = mat3(tangent, bitangent, N);

    int samples = clamp(ssao_samples, 1, 16);
    float occlusion = 0.0;

    for (int i = 0; i < samples; ++i) {
        vec3 sampleVec = TBN * kernel16[i];
        if (dot(sampleVec, N) < 0.0) {
            sampleVec = -sampleVec;
        }

        vec3 samplePos = P + sampleVec * ao_radius;

        vec2 sampleUV = project_view_to_uv(samplePos);

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        float sampleDepthTex = texture(depth2D, sampleUV).r;
        float sampleDepthViewZ = linearize_reversed_depth(sampleDepthTex, u_near, u_far);
        float samplePosViewZ = -samplePos.z;

        float rangeCheck = smoothstep(0.0, 1.0, ao_radius / (abs(P.z - (-sampleDepthViewZ)) + 1e-4));

        if (sampleDepthViewZ < (samplePosViewZ - ao_bias)) {
            occlusion += rangeCheck;
        }
    }

    float occ = clamp(occlusion / float(samples), 0.0, 1.0);
    float ao = 1.0 - occ;

    ao_output = vec4(ao, ao, ao, 1.0);
}
@end

@program ssao_gen ssao_vs ssao_fs