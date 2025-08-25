//
// Created by gungu on 8/19/25.
//

#ifndef MATERIAL_H
#define MATERIAL_H

class Material {
public:
    bool has_diffuse_texture = false;
    bool has_specular_texture = false;
    sg_image_desc diffuse_texture_desc = {};
    sg_sampler_desc diffuse_sampler_desc = {};
    uint8_t* diffuse_texture_data = nullptr;
    size_t diffuse_texture_data_size = 0;
    sg_image_desc specular_texture_desc = {};
    sg_sampler_desc specular_sampler_desc = {};
    uint8_t* specular_texture_data = nullptr;
    size_t specular_texture_data_size = 0;

    sg_image diffuse_image = { .id = SG_INVALID_ID };
    sg_sampler diffuse_sampler = { .id = SG_INVALID_ID };
    sg_image specular_image = { .id = SG_INVALID_ID };
    sg_sampler specular_sampler = { .id = SG_INVALID_ID };

    float diffuse = 0.5f;
    float specular = 0.5f;

    bool has_custom_shader = false;
    sg_shader custom_shader = { .id = SG_INVALID_ID };
    sg_pipeline custom_pipeline = { .id = SG_INVALID_ID };

    void apply_custom_shader(sg_shader_desc shader_desc) {
        custom_shader = sg_make_shader(shader_desc);

        sg_pipeline_desc pipeline_desc = {};
        pipeline_desc.shader = custom_shader;
        pipeline_desc.color_count = 1;
        pipeline_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
        pipeline_desc.colors->blend.enabled = true;
        pipeline_desc.colors->blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pipeline_desc.colors->blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pipeline_desc.colors->blend.op_rgb = SG_BLENDOP_ADD;
        pipeline_desc.colors->blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
        pipeline_desc.colors->blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pipeline_desc.colors->blend.op_alpha = SG_BLENDOP_ADD;
        pipeline_desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3; // pos
        pipeline_desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT3; // normal
        pipeline_desc.layout.attrs[2].format = SG_VERTEXFORMAT_FLOAT2; // texture coord
        pipeline_desc.depth.compare = SG_COMPAREFUNC_LESS;
        pipeline_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
        pipeline_desc.index_type = SG_INDEXTYPE_UINT32;
        pipeline_desc.depth.write_enabled = true;
        pipeline_desc.cull_mode = SG_CULLMODE_FRONT;
        // NOTE TO SELF: I'm pretty sure labels are optional, but if something's buggy, that's the label
        custom_pipeline = sg_make_pipeline(&pipeline_desc);

        has_custom_shader = true;
    }

    ~Material() {
        if (diffuse_image.id != SG_INVALID_ID) sg_destroy_image(diffuse_image);
        if (diffuse_sampler.id != SG_INVALID_ID) sg_destroy_sampler(diffuse_sampler);
        if (specular_image.id != SG_INVALID_ID) sg_destroy_image(specular_image);
        if (specular_sampler.id != SG_INVALID_ID) sg_destroy_sampler(specular_sampler);
        if (has_custom_shader == true) {sg_destroy_shader(custom_shader); sg_destroy_pipeline(custom_pipeline);}
        delete[] diffuse_texture_data;
        delete[] specular_texture_data;
    }
};

#endif //MATERIAL_H
