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

    sg_view rendered_color_att_view;
    sg_view rendered_depth_att_view;
    sg_view rendered_color_tex_view;
    sg_view rendered_depth_tex_view;

    sg_pipeline post_pipeline;
    sg_bindings post_bindings;
    sg_buffer_desc quad_vb;
    sg_sampler_desc post_sampler;
    sg_sampler_desc post_depth_sampler;
    sg_sampler rendered_post_sampler;
    sg_sampler rendered_depth_sampler;

    pp_params_t uniforms;
};

#endif //POST_PROCESSING_H
