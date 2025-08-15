//
// Created by gungu on 8/15/25.
//

#ifndef LOADING_H
#define LOADING_H

std::vector<Mesh> load_gltf(const std::string& path);
bool load_obj(
    const std::string& filename,
    float** out_vertices,
    uint32_t* out_vertex_count,
    uint32_t** out_indices,
    uint32_t* out_index_count
);

#endif //LOADING_H
