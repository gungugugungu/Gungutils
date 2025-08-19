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
#define depth2D   sampler2D(u_depth2D, u_depth_smp)

layout(binding = 2) uniform fs_params {
    float vignette_strength;
    float vignette_radius;
    vec3 color_tint;
    float exposure;
    float contrast;
    float brightness;
    float saturation;
    float time;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    float u_near = 0.1;
    float u_far  = 200.0;

    vec3 color = texture(texture2D, uv).rgb;
    float d    = texture(depth2D, uv).r;

    float viewZ = (u_near * u_far) / max((u_far - d * (u_far - u_near)), 1e-6);

    float vis_near = 0.2;
    float vis_far  = 50.0;

    float mapped = clamp((viewZ - vis_near) / (vis_far - vis_near), 0.0, 1.0);
    mapped = smoothstep(0.0, 1.0, mapped);
    mapped = pow(mapped, 0.7);

    frag_color = vec4(color, 1.0);
}
@end

@program postprocess postprocess_vs postprocess_fs