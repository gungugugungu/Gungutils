@ctype mat4 HMM_Mat4
@ctype vec2 HMM_Vec2
@ctype vec3 HMM_Vec3
@ctype vec4 HMM_Vec4

@vs postprocess_vs
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

out vec2 uv;

void main() {
    // Fullscreen triangle
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
// Use consistent binding pattern like mainshader.glsl
layout(binding = 0) uniform texture2D u_texture2D;
layout(binding = 0) uniform sampler u_texture_smp;
#define texture2D sampler2D(u_texture2D, u_texture_smp)

// Move uniform block to binding = 1 to avoid conflict
layout(binding = 1) uniform fs_params {
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

// Helper functions for color grading
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Vignette effect
vec3 apply_vignette(vec3 color, vec2 uv, float strength, float radius) {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(uv, center);
    float vignette = smoothstep(radius, radius - 0.3, dist);
    return mix(color * (1.0 - strength), color, vignette);
}

// Tone mapping (simple Reinhard)
vec3 tonemap_reinhard(vec3 color) {
    return color / (1.0 + color);
}

// ACES filmic tone mapping (approximation)
vec3 tonemap_aces(vec3 color) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(texture2D, uv).rgb;

    /*// Apply exposure
    color *= exposure;

    // Apply contrast (simple method)
    color = ((color - 0.5) * contrast) + 0.5;

    // Apply brightness
    color += brightness;

    // Apply saturation
    vec3 hsv = rgb2hsv(color);
    hsv.y *= saturation;
    color = hsv2rgb(hsv);

    // Apply color tint
    color *= color_tint;

    // Apply vignette
    color = apply_vignette(color, uv, vignette_strength, vignette_radius);

    // Tone mapping (choose one)
    color = tonemap_aces(color);
    // color = tonemap_reinhard(color);

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    // Optional: Add some film grain or noise
    // vec2 noise_uv = uv + sin(time * 1000.0) * 0.001;
    // float noise = fract(sin(dot(noise_uv, vec2(12.9898, 78.233))) * 43758.5453);
    // color += (noise - 0.5) * 0.02;

    // Clamp final color
    color = clamp(color, 0.0, 1.0);*/

    frag_color = vec4(color, 1.0);
}
@end

@program postprocess postprocess_vs postprocess_fs