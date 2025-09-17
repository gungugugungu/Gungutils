@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs blur_vs
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

@fs blur_fs
layout(binding = 0) uniform texture2D u_texture2D;
layout(binding = 0) uniform sampler u_texture_smp;
#define texture2D sampler2D(u_texture2D, u_texture_smp)

layout(binding = 2) uniform bloom_blur_params {
    float strength;
    int type; // 0: horizontal blur, 1: vertical blur
};

in vec2 uv;
out vec4 frag_color;

const int kernelSize = 9;
const float kernel[9] = float[](
0.00390625, // 1/256
0.03125000, // 8/256
0.10937500, // 28/256
0.21875000, // 56/256
0.27343750, // 70/256
0.21875000, // 56/256
0.10937500, // 28/256
0.03125000, // 8/256
0.00390625  // 1/256
);

void main() {
    ivec2 texSize = textureSize(texture2D, 0);
    vec2 pixelSize = 1.0 / vec2(texSize);

    vec3 color = vec3(0.0);

    for (int i = 0; i < kernelSize; i++) {
        int offset = i - 4;

        vec2 offsetDir = (type == 0) ? vec2(float(offset), 0.0) : vec2(0.0, float(offset));

        vec2 sampleUV = uv + offsetDir * pixelSize;

        vec3 sampleColor = texture(texture2D, sampleUV).rgb;
        color += sampleColor * kernel[i];
    }

    frag_color = vec4(color * strength, 1.0);
}
@end

@program blur blur_vs blur_fs