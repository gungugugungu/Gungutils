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
    //frag_color = vec4(Ix2, Iy2, Ixy, 1.0);
    frag_color = vec4(texture(input_tex, uv));
}
@end

@program kw_st_program st_vs st_fs