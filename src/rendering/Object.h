//
// Created by gungu on 8/19/25.
//

#ifndef OBJECT_H
#define OBJECT_H

class Object {
public:
    HMM_Vec3 position{0,0,0};
    HMM_Quat rotation{0,0,0,1};
    HMM_Vec3 scale{1,1,1};
    float opacity = 1.0f;

    Mesh* mesh;

    std::vector<Mesh*> shape_keys;

    HMM_Vec3 bounding_rect{0.0f ,0.0f, 0.0f};

    int script_id = -1;

    void select_shape_key(int index) {
        mesh = shape_keys[index];
    }

    void initialize_bounds() {
        bounding_rect = {0.0f, 0.0f, 0.0f};

        std::vector<Mesh*> meshes_to_check;
        if (shape_keys.empty()) {
            if (mesh) meshes_to_check.push_back(mesh);
        } else {
            meshes_to_check = shape_keys;
        }

        if (meshes_to_check.empty()) return;

        float max_volume = -1.0f;
        HMM_Vec3 max_size = {0.0f, 0.0f, 0.0f};

        for (auto m : meshes_to_check) {
            if (!m || m->vertex_count == 0) continue;

            float min_x = std::numeric_limits<float>::max();
            float max_x = std::numeric_limits<float>::lowest();
            float min_y = min_x;
            float max_y = max_x;
            float min_z = min_x;
            float max_z = max_x;

            for (size_t i = 0; i < m->vertex_count; ++i) {
                float x = m->vertices[i * 8 + 0];
                float y = m->vertices[i * 8 + 1];
                float z = m->vertices[i * 8 + 2];

                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (z < min_z) min_z = z;
                if (z > max_z) max_z = z;
            }

            float dx = max_x - min_x;
            float dy = max_y - min_y;
            float dz = max_z - min_z;
            float volume = dx * dy * dz;

            if (volume > max_volume) {
                max_volume = volume;
                max_size = {dx, dy, dz};
            }
        }

        if (max_volume >= 0.0f) {
            bounding_rect = max_size;
        }
    }
};

#endif //OBJECT_H
