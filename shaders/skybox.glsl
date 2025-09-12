@ctype mat4 HMM_Mat4

@vs skybox_vs
layout(location = 0) in vec3 aPos;

layout(binding = 0) uniform skybox_vs_params {
    mat4 view;
    mat4 projection;
};

out vec3 uvw;

void main() {
    mat3 view_rotation = mat3(view);
    mat4 view_no_translation = mat4(view_rotation);
    vec4 pos = projection * view_no_translation * vec4(aPos, 1.0);

    uvw = aPos;

    gl_Position = pos.xyww;
}
@end

@fs skybox_fs
out vec4 FragColor;

layout(binding = 0) uniform textureCube _tex2d;
layout(binding = 0) uniform sampler tex2d_smp;

in vec3 uvw;

void main() {
    vec3 color = texture(samplerCube(_tex2d, tex2d_smp), uvw).xyz;

    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0f);
}
@end

@program skybox skybox_vs skybox_fs