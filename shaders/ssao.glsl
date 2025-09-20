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
    float time;
};

in vec2 uv;
out vec4 ao_output;

float linearize_reversed_depth(float d, float near, float far) {
    float denom = max((far - d * (far - near)), 1e-6);
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

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float hash3D(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

float interleavedGradientNoise(vec2 pos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(pos, magic.xy)));
}

mat2 rotationMatrix(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

vec3 hemispherePointUniform(int i, int numSamples, vec2 randomVec) {
    float goldenAngle = 2.399963229728653;
    float r = sqrt((float(i) + 0.5) / float(numSamples));
    float theta = float(i) * goldenAngle + randomVec.x * 6.28318530718;
    float phi = acos(1.0 - randomVec.y);

    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0 - r * r);

    return vec3(x, y, z);
}

void main() {
    float d = texture(depth2D, uv).r;

    if (d >= 0.9999) {
        ao_output = vec4(1.0);
        return;
    }

    vec3 P = reconstruct_view_pos(uv, d);
    vec3 N = estimate_normal(uv, d);

    vec2 pixelPos = uv * screen_size;
    float noiseAngle = interleavedGradientNoise(pixelPos + time * 0.1) * 6.28318530718;
    vec2 randomVec = vec2(hash(pixelPos + vec2(time)), hash(pixelPos + vec2(time * 1.3 + 17.0)));

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    mat3 TBN = mat3(tangent, bitangent, N);

    float occlusion = 0.0;
    int samples = clamp(ssao_samples, 8, 64);
    int validSamples = 0;

    for (int i = 0; i < samples; ++i) {
        vec3 sampleDir = hemispherePointUniform(i, samples, randomVec);

        mat2 rot = rotationMatrix(noiseAngle);
        sampleDir.xy = rot * sampleDir.xy;

        sampleDir = TBN * sampleDir;

        float scale = mix(0.1, 1.0, float(i) / float(samples));
        vec3 samplePos = P + sampleDir * ao_radius * scale;

        vec2 sampleUV = project_view_to_uv(samplePos);

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
        sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        float sampleDepth = texture(depth2D, sampleUV).r;
        float sampleZ = linearize_reversed_depth(sampleDepth, u_near, u_far);
        float samplePosZ = -samplePos.z;

        float depthDiff = sampleZ - samplePosZ;

        if (depthDiff > ao_bias) {
            float rangeCheck = smoothstep(0.0, ao_radius * 2.0, ao_radius / abs(depthDiff));
            occlusion += rangeCheck;
        }

        validSamples++;
    }

    if (validSamples > 0) {
        occlusion = occlusion / float(validSamples);
    }

    float output_float = clamp(1.0 - occlusion, 0.0, 1.0);
    ao_output = vec4(output_float, output_float, output_float, 1.0);
    float reversed_depth = linearize_reversed_depth(texture(depth2D, uv).r, u_near, u_far);
    ao_output = vec4(reversed_depth, reversed_depth, reversed_depth, 1.0);
}
@end

@program ssao_gen ssao_vs ssao_fs