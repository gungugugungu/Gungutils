//
// Created by gungu on 9/11/25.
//

#ifndef SKYBOX_H
#define SKYBOX_H

sg_image skybox_img = {SG_INVALID_ID};
sg_sampler skybox_sampler = {SG_INVALID_ID};
sg_pipeline skybox_pipeline = {SG_INVALID_ID};

const float skybox_vertices[] = {
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
};

const uint32_t skybox_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

sg_buffer skybox_vbuf = {SG_INVALID_ID};
sg_buffer skybox_ibuf = {SG_INVALID_ID};

void initialize_skybox_buffers() {
    sg_buffer_desc vdesc = {};
    vdesc.size = sizeof(skybox_vertices)*sizeof(float);
    vdesc.usage.immutable = true;
    vdesc.data = SG_RANGE(skybox_vertices);
    vdesc.label = "skybox-vertices";
    skybox_vbuf = sg_make_buffer(&vdesc);

    sg_buffer_desc idesc = {};
    idesc.size = sizeof(skybox_indices)*sizeof(uint32_t);
    idesc.usage.immutable = true;
    idesc.usage.vertex_buffer = false;
    idesc.usage.index_buffer = true;
    idesc.data = SG_RANGE(skybox_indices);
    idesc.label = "skybox-indices";
    skybox_ibuf = sg_make_buffer(&idesc);
}

void load_skybox(const char* filename) { // function for loading equirectangular skyboxes
    if (skybox_sampler.id != SG_INVALID_ID) {
        sg_destroy_sampler(skybox_sampler);
        skybox_sampler = {SG_INVALID_ID};
    }
    if (skybox_img.id != SG_INVALID_ID) {
        sg_destroy_image(skybox_img);
        skybox_img = {SG_INVALID_ID};
    }

    int width, height, channels;
    float* data = stbi_loadf(filename, &width, &height, &channels, 4); // Force RGBA
    if (!data) {
        std::cerr << "Failed to load skybox image: " << filename << std::endl;
        return;
    }

    int face_size = 0;
    bool is_horizontal = (width == 6 * height);
    bool is_vertical = (height == 6 * width);
    bool is_equi = (width == 2 * height);
    if (!is_horizontal && !is_vertical && !is_equi) {
        std::cerr << "invalid cubemap dimensions: " << width << "x" << height << ", expected 6:1, 1:6 for strip, or 2:1 for equirectangular." << std::endl;
        stbi_image_free(data);
        return;
    }

    if (is_horizontal) {
        face_size = height;
    } else if (is_vertical) {
        face_size = width;
    } else if (is_equi) {
        face_size = height;
    }

    float* face_float[SG_CUBEFACE_NUM] = {nullptr};
    for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
        face_float[face] = new float[face_size * face_size * 4];
    }

    if (is_horizontal || is_vertical) {
        if (is_horizontal) {
            for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
                for (int y = 0; y < face_size; ++y) {
                    float* src_row = data + (y * width * 4) + (face * face_size * 4);
                    float* dst_row = face_float[face] + (y * face_size * 4);
                    memcpy(dst_row, src_row, face_size * 4 * sizeof(float));
                }
            }
        } else {
            for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
                float* src = data + (face * face_size * width * 4);
                memcpy(face_float[face], src, face_size * face_size * 4 * sizeof(float));
            }
        }
    } else if (is_equi) {
        for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
            for (int y = 0; y < face_size; ++y) {
                for (int x = 0; x < face_size; ++x) {
                    float u = 2.0f * (x + 0.5f) / static_cast<float>(face_size) - 1.0f;
                    float v = 2.0f * (y + 0.5f) / static_cast<float>(face_size) - 1.0f;
                    HMM_Vec3 dir;
                    switch (face) {
                        case 0: dir = HMM_V3(1.0f, -v, -u); break; // +x
                        case 1: dir = HMM_V3(-1.0f, -v, u); break; // -x
                        case 2: dir = HMM_V3(u, 1.0f, -v); break; // +y
                        case 3: dir = HMM_V3(u, -1.0f, v); break; // -y
                        case 4: dir = HMM_V3(u, -v, 1.0f); break; // +z
                        case 5: dir = HMM_V3(-u, -v, -1.0f); break; // -z
                    }
                    dir = HMM_NormV3(dir);
                    float theta = atan2f(dir.X, dir.Z);
                    float phi = asinf(dir.Y);
                    float tex_u = (theta / (2.0f * HMM_PI)) + 0.5f;
                    float tex_v = 1.0f - (phi / HMM_PI + 0.5f);

                    float tx = tex_u * (width - 1.0f);
                    float ty = tex_v * (height - 1.0f);
                    int x0 = static_cast<int>(floorf(tx));
                    float dx = tx - static_cast<float>(x0);
                    int y0 = static_cast<int>(floorf(ty));
                    float dy = ty - static_cast<float>(y0);

                    // Wrap x, clamp y
                    x0 = (x0 % width + width) % width;
                    int x1 = (x0 + 1) % width;
                    y0 = std::max(0, std::min(y0, height - 1));
                    int y1 = std::max(0, std::min(y0 + 1, height - 1));

                    auto get_col = [&](int ix, int iy) -> HMM_Vec4 {
                        int idx = (iy * width + ix) * 4;
                        return HMM_V4(data[idx], data[idx + 1], data[idx + 2], data[idx + 3]);
                    };

                    HMM_Vec4 c00 = get_col(x0, y0);
                    HMM_Vec4 c10 = get_col(x1, y0);
                    HMM_Vec4 c01 = get_col(x0, y1);
                    HMM_Vec4 c11 = get_col(x1, y1);

                    HMM_Vec4 c0 = HMM_AddV4(HMM_MulV4F(c00, 1.0f - dx), HMM_MulV4F(c10, dx));
                    HMM_Vec4 c1 = HMM_AddV4(HMM_MulV4F(c01, 1.0f - dx), HMM_MulV4F(c11, dx));
                    HMM_Vec4 col = HMM_AddV4(HMM_MulV4F(c0, 1.0f - dy), HMM_MulV4F(c1, dy));

                    int fidx = (y * face_size + x) * 4;
                    face_float[face][fidx + 0] = col.X;
                    face_float[face][fidx + 1] = col.Y;
                    face_float[face][fidx + 2] = col.Z;
                    face_float[face][fidx + 3] = col.W;
                }
            }
        }
    }

    unsigned char* face_uchar[SG_CUBEFACE_NUM] = {nullptr};
    for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
        face_uchar[face] = new unsigned char[face_size * face_size * 4];
        for (int i = 0; i < face_size * face_size * 4; ++i) {
            float val = face_float[face][i] * 255.0f;
            face_uchar[face][i] = static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, val)));
        }
        delete[] face_float[face];
    }

    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_CUBE;
    desc.width = face_size;
    desc.height = face_size;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.usage.immutable = true;
    desc.num_mipmaps = 1;
    for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
        desc.data.subimage[face][0].ptr = face_uchar[face];
        desc.data.subimage[face][0].size = face_size * face_size * 4;
    }
    desc.label = "skybox-cubemap";
    skybox_img = sg_make_image(&desc);

    if (skybox_img.id == SG_INVALID_ID) {
        std::cerr << "couldn't create skybox cubemap image." << std::endl;
    }

    sg_sampler_desc smp_desc = {};
    smp_desc.min_filter = SG_FILTER_LINEAR;
    smp_desc.mag_filter = SG_FILTER_LINEAR;
    smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    smp_desc.wrap_w = SG_WRAP_CLAMP_TO_EDGE;
    smp_desc.label = "skybox-sampler";
    skybox_sampler = sg_make_sampler(&smp_desc);

    if (skybox_sampler.id == SG_INVALID_ID) {
        std::cerr << "couldn't create skybox sampler." << std::endl;
    }

    for (int face = 0; face < SG_CUBEFACE_NUM; ++face) {
        delete[] face_uchar[face];
    }
    stbi_image_free(data);

    std::cout << "skybox loaded from " << filename << " (face size: " << face_size << ")" << std::endl;
}

#endif //SKYBOX_H
