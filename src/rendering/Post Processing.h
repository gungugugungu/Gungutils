//
// Created by gungu on 8/18/25.
//

#ifndef POST_PROCESSING_H
#define POST_PROCESSING_H

struct PostProcessState {
    sg_image_desc color_img;
    sg_image rendered_color_img;
    sg_image_desc depth_img;
    sg_image rendered_depth_img;
    sg_attachments pass_attachments;

    sg_pipeline post_pipeline;
    sg_bindings post_bindings;
    sg_buffer_desc quad_vb;
    sg_sampler_desc post_sampler;
    sg_sampler_desc post_depth_sampler;
    sg_sampler rendered_post_sampler;
    sg_sampler rendered_depth_sampler;

    // post-process uniforms
    struct PostProcessUniforms {
        float vignette_strength;
        float vignette_radius;
        HMM_Vec3 color_tint;
        float exposure;
        float contrast;
        float brightness;
        float saturation;
        float time; // for animated effects
        float _padding[2]; // align to 16 bytes
    } uniforms;
};

#endif //POST_PROCESSING_H
