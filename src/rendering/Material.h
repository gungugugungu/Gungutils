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

    float diffuse = 0.5f;
    float specular = 0.5f;
};

#endif //MATERIAL_H
