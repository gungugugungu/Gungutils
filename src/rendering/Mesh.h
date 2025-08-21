//
// Created by gungu on 8/15/25.
//

#ifndef MESH_H
#define MESH_H

class Mesh {
public:
    float*    vertices = nullptr;
    size_t    vertex_count = 0;
    uint32_t* indices  = nullptr;
    size_t    index_count = 0;

    sg_buffer_desc vertex_buffer_desc = {};
    sg_buffer_desc index_buffer_desc = {};

    uint16_t* indices16 = nullptr;
    bool use_uint16_indices = true;

    bool enable_shading = true;

    Material* material;

    sg_buffer vertex_buffer = { .id = SG_INVALID_ID };
    sg_buffer index_buffer = { .id = SG_INVALID_ID };

    Mesh() = default;

    ~Mesh() {
        if (vertex_buffer.id != SG_INVALID_ID) sg_destroy_buffer(vertex_buffer);
        if (index_buffer.id != SG_INVALID_ID) sg_destroy_buffer(index_buffer);
        delete[] vertices;
        delete[] indices;
        delete[] indices16;
    }

    Mesh(const Mesh& other) : vertices(nullptr), indices(nullptr), indices16(nullptr) {
        *this = other;
    }

    Mesh& operator=(const Mesh& other) {
        if (this == &other) return *this;

        if (vertex_buffer.id != SG_INVALID_ID) sg_destroy_buffer(vertex_buffer);
        if (index_buffer.id != SG_INVALID_ID) sg_destroy_buffer(index_buffer);

        delete[] vertices;
        delete[] indices;
        delete[] indices16;

        vertex_count = other.vertex_count;
        index_count = other.index_count;
        vertex_buffer_desc = other.vertex_buffer_desc;
        index_buffer_desc = other.index_buffer_desc;
        use_uint16_indices = other.use_uint16_indices;

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

        vertex_buffer = sg_make_buffer(&vertex_buffer_desc);
        index_buffer = sg_make_buffer(&index_buffer_desc);

        return *this;
    }

    Mesh(Mesh&& o) noexcept {
        *this = std::move(o);
    }

    Mesh& operator=(Mesh&& o) noexcept {
        if (this == &o) return *this;

        if (vertex_buffer.id != SG_INVALID_ID) sg_destroy_buffer(vertex_buffer);
        if (index_buffer.id != SG_INVALID_ID) sg_destroy_buffer(index_buffer);

        delete[] vertices;
        delete[] indices;
        delete[] indices16;

        vertices = o.vertices;
        vertex_count = o.vertex_count;
        indices = o.indices;
        index_count = o.index_count;
        indices16 = o.indices16;
        use_uint16_indices = o.use_uint16_indices;
        vertex_count = o.vertex_count;
        index_count = o.index_count;
        vertex_buffer_desc = o.vertex_buffer_desc;
        index_buffer_desc = o.index_buffer_desc;

        vertex_buffer = o.vertex_buffer;
        index_buffer = o.index_buffer;

        o.vertices = nullptr;
        o.indices = nullptr;
        o.indices16 = nullptr;
        o.vertex_buffer = { .id = SG_INVALID_ID };
        o.index_buffer = { .id = SG_INVALID_ID };

        return *this;
    }
};

#endif //MESH_H
