@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs surface_vs
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 uv;

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

@fs surface_fs
layout(binding = 0) uniform texture2D u_texture2D;
layout(binding = 0) uniform sampler u_texture_smp;
#define texture2D sampler2D(u_texture2D, u_texture_smp)

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture(texture2D, uv);
}
@end

@program surface surface_vs surface_fs