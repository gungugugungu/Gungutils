@ctype mat4 HMM_Mat4

@vs shadow_vs
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aUV;

layout(binding=0) uniform shadow_vs_params {
    mat4 model;
    mat4 view;
    mat4 projection;
};
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
@end

@fs shadow_fs

out vec4 fraggy;

void main() {
    fraggy = vec4(0.5f, 0.5f, 1.0f, 1.0f);
}
@end

@program shadow shadow_vs shadow_fs