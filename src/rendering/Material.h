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

    ~Material() {
        if (diffuse_image.id != SG_INVALID_ID) sg_destroy_image(diffuse_image);
        if (diffuse_sampler.id != SG_INVALID_ID) sg_destroy_sampler(diffuse_sampler);
        if (specular_image.id != SG_INVALID_ID) sg_destroy_image(specular_image);
        if (specular_sampler.id != SG_INVALID_ID) sg_destroy_sampler(specular_sampler);
        delete[] diffuse_texture_data;
        delete[] specular_texture_data;
    }
};

#endif //MATERIAL_H
