@ctype mat4 HMM_Mat4
@vs shadow_vs
layout(location=0) in vec3 aPos;
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
void main() {

}
@end

@program shadow shadow_vs shadow_fs