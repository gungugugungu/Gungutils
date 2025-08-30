//
// Created by gungu on 8/30/25.
//

#ifndef BILLBOARD_H
#define BILLBOARD_H
#include "Surface.h"
#include "HandmadeMath/HandmadeMath.h"

sg_pipeline billboard_pipeline{.id = SG_INVALID_ID};
sg_buffer billboard_vb{.id = SG_INVALID_ID};
sg_buffer billboard_ib{.id = SG_INVALID_ID};
billboard_vs_params_t billboard_vs_params;

struct BillboardInfo {
    Surface* surface;
    HMM_Vec3 position;
    float size;
    bool y_only_rotation;
    bool rotate_to_camera = false;
    HMM_Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

vector<BillboardInfo> billboards;

void draw_billboard(Surface* surface, HMM_Vec3 position, float size = 1.0f, bool y_only_rotation = false) {
    billboards.emplace_back(surface, position, size, y_only_rotation, true);
}

void draw_decal(Surface* surface, HMM_Vec3 position, HMM_Quat rotation, float size = 1.0f) {
    billboards.emplace_back(surface, position, size, false, false, rotation);
}

void _draw_all_billboards(HMM_Vec3 camera_pos) {
    for (auto& billboard : billboards) {
        sg_apply_pipeline(billboard_pipeline);
        sg_bindings bind = {};

        sg_image_desc image_desc = {};
        image_desc.width = billboard.surface->pixels[0].size();
        image_desc.height = billboard.surface->pixels.size();
        image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        image_desc.usage.immutable = true;
        image_desc.data = billboard.surface->get_sokol_image_data_unflipped();
        image_desc.label = "billboard-image";
        sg_image image = sg_make_image(&image_desc);

        sg_sampler_desc sampler_desc = {};
        sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.min_filter = SG_FILTER_LINEAR;
        sampler_desc.mag_filter = SG_FILTER_LINEAR;
        sampler_desc.label = "billboard-sampler";
        sg_sampler sampler = sg_make_sampler(&sampler_desc);

        sg_view_desc view_desc = {};
        view_desc.texture.image = image;
        view_desc.label = "billboard-image-view";
        sg_view image_view = sg_make_view(&view_desc);

        bind.vertex_buffers[0] = billboard_vb;
        bind.index_buffer = billboard_ib;
        bind.views[0] = image_view;
        bind.samplers[0] = sampler;
        sg_apply_bindings(&bind);

        HMM_Mat4 billboard_mat;
        if (billboard.rotate_to_camera) {
            if (billboard.y_only_rotation) {
                HMM_Vec3 to_camera_xz = HMM_NormV3({camera_pos.X - billboard.position.X, 0.0f, camera_pos.Z - billboard.position.Z});
                HMM_Vec3 up = {0.0f, 1.0f, 0.0f};
                HMM_Vec3 right = HMM_NormV3(HMM_Cross(up, to_camera_xz));

                billboard_mat = {
                    right.X, right.Y, right.Z, 0.0f,
                    up.X, up.Y, up.Z, 0.0f,
                    -to_camera_xz.X, -to_camera_xz.Y, -to_camera_xz.Z, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f
                };
            } else {
                HMM_Vec3 to_camera = HMM_NormV3(HMM_SubV3(camera_pos, billboard.position));
                HMM_Vec3 up = {0.0f, 1.0f, 0.0f};
                HMM_Vec3 right = HMM_NormV3(HMM_Cross(up, to_camera));
                up = HMM_Cross(to_camera, right);

                billboard_mat = {
                    right.X, right.Y, right.Z, 0.0f,
                    up.X, up.Y, up.Z, 0.0f,
                    -to_camera.X, -to_camera.Y, -to_camera.Z, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f
                };
            }
        } else {
            billboard_mat = HMM_QToM4(billboard.rotation);
        }

        HMM_Mat4 translate_mat = HMM_Translate(billboard.position);
        HMM_Mat4 scale_mat = HMM_Scale({billboard.size, billboard.size, billboard.size});
        HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(billboard_mat, scale_mat));

        billboard_vs_params.model = model;

        sg_apply_uniforms(UB_billboard_vs_params, SG_RANGE(billboard_vs_params));

        sg_draw(0, 6, 1);

        sg_destroy_view(image_view);
        sg_destroy_sampler(sampler);
        sg_destroy_image(image);
    }
    billboards.clear();
}

#endif //BILLBOARD_H
