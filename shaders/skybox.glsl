@ctype mat4 HMM_Mat4

@vs skybox_vs
layout(location = 0) in vec3 aPos;

layout(binding = 0) uniform skybox_vs_params {
    mat4 view;
    mat4 projection;
};

out vec3 uvw;

void main() {
    uvw = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
@end

@fs skybox_fs
out vec4 FragColor;

layout(binding = 0) uniform textureCube _tex2d;
layout(binding = 0) uniform sampler tex2d_smp;

in vec3 uvw;

void main() {
    FragColor = texture(samplerCube(_tex2d, tex2d_smp), uvw);
}
@end

@program skybox skybox_vs skybox_fs