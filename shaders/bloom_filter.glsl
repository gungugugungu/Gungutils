@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs bloom_vs
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

@fs bloom_fs
layout(binding = 0) uniform texture2D u_texture2D;
layout(binding = 0) uniform sampler u_texture_smp;
layout(binding = 1) uniform texture2D u_depth2D;
layout(binding = 1) uniform sampler u_depth_smp;
#define texture2D sampler2D(u_texture2D, u_texture_smp)
#define depth2D sampler2D(u_depth2D, u_depth_smp)

layout(binding = 2) uniform bloom_filter_params {
    float threshold;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec3 color = texture(texture2D, uv).rgb;
    float d = texture(depth2D, uv).r;

    //frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    if (color.r+color.g+color.b >= threshold) {
        frag_color = vec4(color, 1.0);
    }
}
@end

@program bloom_filter bloom_vs bloom_fs