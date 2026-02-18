@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs postprocess_vs
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

@fs postprocess_fs
layout(binding = 0) uniform texture2D u_texture2D;
layout(binding = 0) uniform sampler u_texture_smp;
layout(binding = 1) uniform texture2D u_depth2D;
layout(binding = 1) uniform sampler u_depth_smp;
layout(binding = 2) uniform texture2D u_bloom2D;
layout(binding = 2) uniform sampler u_bloom_smp;
layout(binding = 3) uniform texture2D u_ao2D;
layout(binding = 3) uniform sampler u_ao_smp;
#define texture2D sampler2D(u_texture2D, u_texture_smp)
#define depth2D sampler2D(u_depth2D, u_depth_smp)
#define bloom2D sampler2D(u_bloom2D, u_bloom_smp)
#define ao2D sampler2D(u_ao2D, u_ao_smp)

layout(binding = 0) uniform pp_params {
    float vignette_strength;
    float vignette_radius;
    vec3 color_tint;
    float exposure;
    float contrast;
    float brightness;
    float saturation;
    float time;
    float panini_d;
    float panini_scale_h;
    float panini_scale_v;
};

in vec2 uv;
out vec4 frag_color;

vec2 InvPanini(vec2 p, float d) {
    float k = 1.0 + d;
    float S = p.x;

    float R   = sqrt(S * S + k * k);
    float psi = atan(-k, S);
    float theta = psi + acos(clamp(-S * d / R, -1.0, 1.0));

    float cos_theta = cos(theta);

    return vec2(tan(theta), p.y * (cos_theta + d) / k);
}

void main() {
    vec2 sample_uv = uv;
    if (panini_d > 0.0) {
        float d = panini_d;
        float k = 1.0 + d;
        vec2 ndc = 2.0 * uv - 1.0;

        float h_len = sqrt(1.0 + panini_scale_h * panini_scale_h);
        float panini_display_h = k * panini_scale_h / (1.0 + d * h_len);

        float panini_display_v = panini_scale_v;

        vec2 panini_pos = ndc * vec2(panini_display_h, panini_display_v);
        vec2 rect_pos   = InvPanini(panini_pos, d);

        sample_uv = (rect_pos / vec2(panini_scale_h, panini_scale_v)) * 0.5 + 0.5;
    }

    vec3 color = texture(texture2D, sample_uv).rgb;
    float d = texture(depth2D, sample_uv).r;

    if (d >= 0.9999) {
        frag_color = vec4(color, 1.0);
        return;
    }

    vec3 final_color = color;

    final_color *= exp2(exposure);
    final_color += vec3(brightness);
    final_color = (final_color - vec3(0.5)) * contrast + vec3(0.5);
    float luminance = dot(final_color, vec3(0.299, 0.587, 0.114));
    final_color = mix(vec3(luminance), final_color, saturation);
    final_color *= color_tint;

    float dist = length(uv - vec2(0.5));
    float norm_dist = dist / sqrt(0.5);
    float vignette_factor = max(0.0, 1.0 - vignette_strength * pow(norm_dist, vignette_radius));
    final_color *= vignette_factor;

    vec3 bloom_color = texture(bloom2D, sample_uv).xyz;
    vec3 ao_color = texture(ao2D, sample_uv).xyz;
    ao_color = vec3(abs(1-ao_color.r));

    frag_color = vec4(final_color + bloom_color - ao_color, 1.0);
}
@end

@program postprocess postprocess_vs postprocess_fs