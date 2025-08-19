//
// Created by gungu on 8/15/25.
//

#ifndef MESH_H
#define MESH_H

class Mesh {
public:
    float*    vertices = nullptr; // 3 floats for position, 3 floats for normals, and 2 floats for vertex coords
    size_t    vertex_count = 0;
    uint32_t* indices  = nullptr;
    size_t    index_count = 0;

    sg_buffer_desc vertex_buffer_desc = {};
    sg_buffer_desc index_buffer_desc = {};

    uint16_t* indices16 = nullptr;
    bool use_uint16_indices = true;

    bool enable_shading = true;

    Material* material;

    Mesh() = default;

    ~Mesh() {
        delete[] vertices;
        delete[] indices;
        delete[] indices16;
    }

    Mesh(const Mesh& other) : vertices(nullptr), indices(nullptr), indices16(nullptr) {
        *this = other;
    }

    Mesh& operator=(const Mesh& other) {
        if (this == &other) return *this;

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

        o.vertices = nullptr;
        o.indices = nullptr;
        o.indices16 = nullptr;

        return *this;
    }
};

#endif //MESH_H
