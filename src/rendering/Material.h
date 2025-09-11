//
// Created by gungu on 8/19/25.
//

#ifndef MATERIAL_H
#define MATERIAL_H

class Material {
public:
    bool has_color_texture = false;
    bool has_metallic_roughness_texture = false;
    bool has_normal_texture = false;
    bool has_emissive_texture = false;

    sg_image_desc base_color_image_desc = {};
    sg_sampler_desc base_color_sampler_desc = {};
    uint8_t* base_color_image_data = nullptr;
    size_t base_color_image_data_size = 0;

    // blue channel is metallic, green is roughness
    sg_image_desc metallic_roughness_image_desc = {};
    sg_sampler_desc metallic_roughness_sampler_desc = {};
    uint8_t* metallic_roughness_image_data = nullptr;
    size_t metallic_roughness_image_data_size = 0; // holy long variable name

    sg_image_desc normal_texture_desc = {};
    sg_sampler_desc normal_sampler_desc = {};
    uint8_t* normal_texture_data = nullptr;
    size_t normal_texture_data_size = 0;

    sg_image_desc emissive_image_desc = {};
    sg_sampler_desc emissive_sampler_desc = {};
    uint8_t* emissive_image_data = nullptr;
    size_t emissive_image_data_size = 0;

    sg_image base_color_image = { .id = SG_INVALID_ID };
    sg_sampler base_color_sampler = { .id = SG_INVALID_ID };
    sg_image metallic_roughness_image = { .id = SG_INVALID_ID };
    sg_sampler metallic_roughness_sampler = { .id = SG_INVALID_ID };
    sg_image normal_image = { .id = SG_INVALID_ID };
    sg_sampler normal_sampler = { .id = SG_INVALID_ID };
    sg_image emissive_image = { .id = SG_INVALID_ID };
    sg_sampler emissive_sampler = { .id = SG_INVALID_ID };

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
        custom_pipeline = sg_make_pipeline(&pipeline_desc);

        has_custom_shader = true;
    }

    ~Material() {
        if (base_color_image.id != SG_INVALID_ID) sg_destroy_image(base_color_image);
        if (base_color_sampler.id != SG_INVALID_ID) sg_destroy_sampler(base_color_sampler);
        if (metallic_roughness_image.id != SG_INVALID_ID) sg_destroy_image(metallic_roughness_image);
        if (metallic_roughness_sampler.id != SG_INVALID_ID) sg_destroy_sampler(metallic_roughness_sampler);
        if (normal_image.id != SG_INVALID_ID) sg_destroy_image(normal_image);
        if (normal_sampler.id != SG_INVALID_ID) sg_destroy_sampler(normal_sampler);
        if (emissive_image.id != SG_INVALID_ID) sg_destroy_image(emissive_image);
        if (emissive_sampler.id != SG_INVALID_ID) sg_destroy_sampler(emissive_sampler);
        if (has_custom_shader == true) {sg_destroy_shader(custom_shader); sg_destroy_pipeline(custom_pipeline);}
        delete[] base_color_image_data;
        delete[] metallic_roughness_image_data;
    }
};

#endif //MATERIAL_H
