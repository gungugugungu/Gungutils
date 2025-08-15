//
// Created by gungu on 8/15/25.
//

#ifndef MESH_H
#define MESH_H

class Mesh {
public:
    HMM_Vec3 position{0,0,0};
    HMM_Quat rotation{0,0,0,1};
    HMM_Vec3 scale{1,1,1};
    float opacity = 1.0f;

    float*    vertices = nullptr;
    size_t    vertex_count = 0;
    uint32_t* indices  = nullptr;
    size_t    index_count = 0;

    sg_buffer_desc vertex_buffer_desc = {};
    sg_buffer_desc index_buffer_desc = {};

    uint16_t* indices16 = nullptr;
    bool use_uint16_indices = true;

    sg_image_desc texture_desc = {};
    sg_sampler_desc sampler_desc = {};
    bool has_texture = false;
    uint8_t* texture_data = nullptr;
    size_t texture_data_size = 0;

    Mesh() = default;

    ~Mesh() {
        delete[] vertices;
        delete[] indices;
        delete[] indices16;
        delete[] texture_data;
    }

    Mesh(const Mesh& other) : vertices(nullptr), indices(nullptr), indices16(nullptr), texture_data(nullptr) {
        *this = other;
    }

    Mesh& operator=(const Mesh& other) {
        if (this == &other) return *this;

        delete[] vertices;
        delete[] indices;
        delete[] indices16;
        delete[] texture_data;

        position = other.position;
        rotation = other.rotation;
        scale = other.scale;
        opacity = other.opacity;
        vertex_count = other.vertex_count;
        index_count = other.index_count;
        vertex_buffer_desc = other.vertex_buffer_desc;
        index_buffer_desc = other.index_buffer_desc;
        use_uint16_indices = other.use_uint16_indices;

        texture_desc = other.texture_desc;
        sampler_desc = other.sampler_desc;
        has_texture = other.has_texture;
        texture_data_size = other.texture_data_size;

        if (other.vertices && vertex_count > 0) {
            vertices = new float[vertex_count * 8];
            memcpy(vertices, other.vertices, vertex_count * 8 * sizeof(float));
            vertex_buffer_desc.data.ptr = vertices;
        } else {
            vertices = nullptr;
        }

        if (other.indices && index_count > 0) {
            indices = new uint32_t[index_count];
            memcpy(indices, other.indices, index_count * sizeof(uint32_t));

            if (!use_uint16_indices) {
                index_buffer_desc.data.ptr = indices;
            }
        } else {
            indices = nullptr;
        }

        if (other.indices16 && index_count > 0 && use_uint16_indices) {
            indices16 = new uint16_t[index_count];
            memcpy(indices16, other.indices16, index_count * sizeof(uint16_t));
            index_buffer_desc.data.ptr = indices16;
        } else {
            indices16 = nullptr;
        }

        if (other.texture_data && texture_data_size > 0) {
            texture_data = new uint8_t[texture_data_size];
            memcpy(texture_data, other.texture_data, texture_data_size);
            texture_desc.data.subimage[0][0].ptr = texture_data;
        } else {
            texture_data = nullptr;
        }

        return *this;
    }

    Mesh(Mesh&& o) noexcept {
        *this = std::move(o);
    }

    Mesh& operator=(Mesh&& o) noexcept {
        if (this == &o) return *this;

        delete[] vertices;
        delete[] indices;
        delete[] indices16;
        delete[] texture_data;

        vertices = o.vertices;
        vertex_count = o.vertex_count;
        indices = o.indices;
        index_count = o.index_count;
        indices16 = o.indices16;
        use_uint16_indices = o.use_uint16_indices;
        position = o.position;
        rotation = o.rotation;
        scale = o.scale;
        opacity = o.opacity;
        vertex_count = o.vertex_count;
        index_count = o.index_count;
        vertex_buffer_desc = o.vertex_buffer_desc;
        index_buffer_desc = o.index_buffer_desc;

        // Move texture data
        texture_desc = o.texture_desc;
        sampler_desc = o.sampler_desc;
        has_texture = o.has_texture;
        texture_data = o.texture_data;
        texture_data_size = o.texture_data_size;

        o.vertices = nullptr;
        o.indices = nullptr;
        o.indices16 = nullptr;
        o.texture_data = nullptr;

        return *this;
    }
};

#endif //MESH_H
