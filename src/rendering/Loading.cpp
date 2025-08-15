//
// Created by gungu on 8/15/25.
//

#include "Loading.h"
#include "HandmadeMath/HandmadeMath.h"
#include "src/rendering/Mesh.h"

std::vector<Mesh> load_gltf(const std::string& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!warn.empty()) std::cout << "WARN: " << warn << "\n";
    if (!ok) {
        std::cerr << "ERROR: " << err << "\n";
        return {};
    }

    std::vector<Mesh> meshes;

    // Helper function to load texture from GLTF model
    auto loadTexture = [&](int textureIndex) -> std::pair<uint8_t*, sg_image_desc> {
        if (textureIndex < 0 || textureIndex >= model.textures.size()) {
            return {nullptr, {}};
        }

        const auto& texture = model.textures[textureIndex];
        if (texture.source < 0 || texture.source >= model.images.size()) {
            return {nullptr, {}};
        }

        const auto& image = model.images[texture.source];

        sg_image_desc img_desc = {};
        uint8_t* texture_data = nullptr;

        if (!image.image.empty()) {
            // image data is already decoded
            img_desc.width = image.width;
            img_desc.height = image.height;
            img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;

            size_t data_size = image.image.size();
            texture_data = new uint8_t[data_size];
            memcpy(texture_data, image.image.data(), data_size);

            img_desc.data.subimage[0][0].ptr = texture_data;
            img_desc.data.subimage[0][0].size = data_size;
        } else if (!image.uri.empty()) {
            // load from file (relative to GLTF file)
            std::string base_dir = path.substr(0, path.find_last_of("/\\") + 1);
            std::string full_path = base_dir + image.uri;

            int img_width, img_height, num_channels;
            const int desired_channels = 4;
            stbi_uc* pixels = stbi_load(full_path.c_str(), &img_width, &img_height, &num_channels, desired_channels);

            if (pixels) {
                img_desc.width = img_width;
                img_desc.height = img_height;
                img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;

                size_t data_size = img_width * img_height * desired_channels;
                texture_data = new uint8_t[data_size];
                memcpy(texture_data, pixels, data_size);

                img_desc.data.subimage[0][0].ptr = texture_data;
                img_desc.data.subimage[0][0].size = data_size;

                stbi_image_free(pixels);
            }
        } else if (image.bufferView >= 0) {
            const auto& bufferView = model.bufferViews[image.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            const uint8_t* data = buffer.data.data() + bufferView.byteOffset;
            size_t data_size = bufferView.byteLength;

            int img_width, img_height, num_channels;
            const int desired_channels = 4;
            stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(data_size),
                                                  &img_width, &img_height, &num_channels, desired_channels);

            if (pixels) {
                img_desc.width = img_width;
                img_desc.height = img_height;
                img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;

                size_t pixel_data_size = img_width * img_height * desired_channels;
                texture_data = new uint8_t[pixel_data_size];
                memcpy(texture_data, pixels, pixel_data_size);

                img_desc.data.subimage[0][0].ptr = texture_data;
                img_desc.data.subimage[0][0].size = pixel_data_size;

                stbi_image_free(pixels);
            }
        }

        return {texture_data, img_desc};
    };

    std::function<void(int, const HMM_Mat4&)> processNode = [&](int nodeIndex, const HMM_Mat4& parentTransform) {
        if (nodeIndex < 0 || nodeIndex >= model.nodes.size()) return;
        const auto& node = model.nodes[nodeIndex];

        HMM_Mat4 localTransform = HMM_M4D(1.0f);

        if (node.matrix.size() == 16) {
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    localTransform.Elements[i][j] = (float)node.matrix[j * 4 + i];
                }
            }
        } else {
            HMM_Vec3 translation = {0, 0, 0};
            HMM_Quat rotation = {0, 0, 0, 1}; // Identity quaternion
            HMM_Vec3 scale = {1, 1, 1};

            if (node.translation.size() == 3) {
                translation = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
            }

            if (node.rotation.size() == 4) {
                rotation = {(float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]};
            }

            if (node.scale.size() == 3) {
                scale = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
            }

            // transformation matrix: T * R * S
            HMM_Mat4 T = HMM_Translate(translation);
            HMM_Mat4 R = HMM_QToM4(rotation);
            HMM_Mat4 S = HMM_Scale(scale);
            localTransform = HMM_MulM4(T, HMM_MulM4(R, S));
        }

        HMM_Mat4 worldTransform = HMM_MulM4(parentTransform, localTransform);

        if (node.mesh >= 0 && node.mesh < model.meshes.size()) {
            const auto& meshDef = model.meshes[node.mesh];
            for (const auto& prim : meshDef.primitives) {
                // Check if required attributes exist
                if (prim.attributes.find("POSITION") == prim.attributes.end() ||
                    prim.attributes.find("NORMAL") == prim.attributes.end() ||
                    prim.attributes.find("TEXCOORD_0") == prim.attributes.end()) {
                    std::cout << "Warning: Primitive missing required attributes, skipping..." << std::endl;
                    continue;
                }

                const auto& posAcc = model.accessors[prim.attributes.at("POSITION")];
                const auto& normAcc = model.accessors[prim.attributes.at("NORMAL")];
                const auto& uvAcc = model.accessors[prim.attributes.at("TEXCOORD_0")];
                size_t vcount = posAcc.count;

                auto* verts = new float[vcount * 8];

                const auto& posView = model.bufferViews[posAcc.bufferView];
                const auto& normView = model.bufferViews[normAcc.bufferView];
                const auto& uvView = model.bufferViews[uvAcc.bufferView];

                const auto* posBuf = reinterpret_cast<const float*>(
                    model.buffers[posView.buffer].data.data() + posView.byteOffset + posAcc.byteOffset);
                const auto* normBuf = reinterpret_cast<const float*>(
                    model.buffers[normView.buffer].data.data() + normView.byteOffset + normAcc.byteOffset);
                const auto* uvBuf = reinterpret_cast<const float*>(
                    model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uvAcc.byteOffset);

                size_t posStride = posView.byteStride ? posView.byteStride / sizeof(float) : 3;
                size_t normStride = normView.byteStride ? normView.byteStride / sizeof(float) : 3;
                size_t uvStride = uvView.byteStride ? uvView.byteStride / sizeof(float) : 2;

                for (size_t i = 0; i < vcount; i++) {
                    // position
                    verts[i*8 + 0] = posBuf[i*posStride + 0];
                    verts[i*8 + 1] = posBuf[i*posStride + 1];
                    verts[i*8 + 2] = posBuf[i*posStride + 2];
                    // normal
                    verts[i*8 + 3] = normBuf[i*normStride + 0];
                    verts[i*8 + 4] = normBuf[i*normStride + 1];
                    verts[i*8 + 5] = normBuf[i*normStride + 2];
                    // texture coordinates
                    verts[i*8 + 6] = uvBuf[i*uvStride + 0];
                    verts[i*8 + 7] = uvBuf[i*uvStride + 1];
                }

                const auto& idxAcc = model.accessors[prim.indices];
                size_t icount = idxAcc.count;
                auto* idxs = new uint32_t[icount];

                const auto& idxView = model.bufferViews[idxAcc.bufferView];
                const uint8_t* idxBuf = model.buffers[idxView.buffer].data.data() +
                                      idxView.byteOffset + idxAcc.byteOffset;

                if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(idxBuf);
                    for (size_t i = 0; i < icount; i++) {
                        idxs[i] = indices16[i];
                    }
                } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(idxBuf);
                    for (size_t i = 0; i < icount; i++) {
                        idxs[i] = indices32[i];
                    }
                } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* indices8 = reinterpret_cast<const uint8_t*>(idxBuf);
                    for (size_t i = 0; i < icount; i++) {
                        idxs[i] = indices8[i];
                    }
                }

                Mesh mesh;
                mesh.vertices = verts;
                mesh.vertex_count = vcount;
                mesh.indices = idxs;
                mesh.index_count = icount;

                decompose_matrix(worldTransform, mesh.position, mesh.rotation, mesh.scale);

                if (prim.material >= 0 && prim.material < model.materials.size()) {
                    const auto& material = model.materials[prim.material];

                    if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                        auto [texture_data, img_desc] = loadTexture(material.pbrMetallicRoughness.baseColorTexture.index);

                        if (texture_data && img_desc.width > 0) {
                            mesh.texture_data = texture_data;
                            mesh.texture_desc = img_desc;
                            mesh.texture_data_size = img_desc.data.subimage[0][0].size;
                            mesh.has_texture = true;

                            mesh.sampler_desc.min_filter = SG_FILTER_LINEAR;
                            mesh.sampler_desc.mag_filter = SG_FILTER_LINEAR;
                            mesh.sampler_desc.wrap_u = SG_WRAP_REPEAT;
                            mesh.sampler_desc.wrap_v = SG_WRAP_REPEAT;
                        }
                    }
                }

                // if no texture was loaded, set up for default palette.png
                if (!mesh.has_texture) {
                    mesh.has_texture = false;
                    mesh.sampler_desc.min_filter = SG_FILTER_LINEAR;
                    mesh.sampler_desc.mag_filter = SG_FILTER_LINEAR;
                    mesh.sampler_desc.wrap_u = SG_WRAP_REPEAT;
                    mesh.sampler_desc.wrap_v = SG_WRAP_REPEAT;
                }

                meshes.push_back(std::move(mesh));
            }
        }

        for (int childIndex : node.children) {
            processNode(childIndex, worldTransform);
        }
    };

    std::vector<bool> isChild(model.nodes.size(), false);
    for (const auto& node : model.nodes) {
        for (int child : node.children) {
            if (child >= 0 && child < model.nodes.size()) {
                isChild[child] = true;
            }
        }
    }

    HMM_Mat4 identity = HMM_M4D(1.0f);
    for (int i = 0; i < model.nodes.size(); i++) {
        if (!isChild[i]) {
            processNode(i, identity);
        }
    }

    if (meshes.empty()) {
        for (const auto& meshDef : model.meshes) {
            for (const auto& prim : meshDef.primitives) {
                if (prim.attributes.find("POSITION") == prim.attributes.end() ||
                    prim.attributes.find("NORMAL") == prim.attributes.end() ||
                    prim.attributes.find("TEXCOORD_0") == prim.attributes.end()) {
                    continue;
                }

                const auto& posAcc = model.accessors[prim.attributes.at("POSITION")];
                const auto& normAcc = model.accessors[prim.attributes.at("NORMAL")];
                const auto& uvAcc = model.accessors[prim.attributes.at("TEXCOORD_0")];
                size_t vcount = posAcc.count;

                auto* verts = new float[vcount * 8];

                const auto& posView = model.bufferViews[posAcc.bufferView];
                const auto& normView = model.bufferViews[normAcc.bufferView];
                const auto& uvView = model.bufferViews[uvAcc.bufferView];

                const auto* posBuf = reinterpret_cast<const float*>(
                    model.buffers[posView.buffer].data.data() + posView.byteOffset + posAcc.byteOffset);
                const auto* normBuf = reinterpret_cast<const float*>(
                    model.buffers[normView.buffer].data.data() + normView.byteOffset + normAcc.byteOffset);
                const auto* uvBuf = reinterpret_cast<const float*>(
                    model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uvAcc.byteOffset);

                size_t posStride = posView.byteStride ? posView.byteStride / sizeof(float) : 3;
                size_t normStride = normView.byteStride ? normView.byteStride / sizeof(float) : 3;
                size_t uvStride = uvView.byteStride ? uvView.byteStride / sizeof(float) : 2;

                for (size_t i = 0; i < vcount; i++) {
                    verts[i*8 + 0] = posBuf[i*posStride + 0];
                    verts[i*8 + 1] = posBuf[i*posStride + 1];
                    verts[i*8 + 2] = posBuf[i*posStride + 2];
                    verts[i*8 + 3] = normBuf[i*normStride + 0];
                    verts[i*8 + 4] = normBuf[i*normStride + 1];
                    verts[i*8 + 5] = normBuf[i*normStride + 2];
                    verts[i*8 + 6] = uvBuf[i*uvStride + 0];
                    verts[i*8 + 7] = uvBuf[i*uvStride + 1];
                }

                const auto& idxAcc = model.accessors[prim.indices];
                size_t icount = idxAcc.count;
                auto* idxs = new uint32_t[icount];

                const auto& idxView = model.bufferViews[idxAcc.bufferView];
                const uint8_t* idxBuf = model.buffers[idxView.buffer].data.data()+idxView.byteOffset + idxAcc.byteOffset;

                if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(idxBuf);
                    for (size_t i = 0; i < icount; i++) {
                        idxs[i] = indices16[i];
                    }
                } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(idxBuf);
                    for (size_t i = 0; i < icount; i++) {
                        idxs[i] = indices32[i];
                    }
                } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* indices8 = reinterpret_cast<const uint8_t*>(idxBuf);
                    for (size_t i = 0; i < icount; i++) {
                        idxs[i] = indices8[i];
                    }
                }

                Mesh mesh;
                mesh.vertices = verts;
                mesh.vertex_count = vcount;
                mesh.indices = idxs;
                mesh.index_count = icount;
                mesh.position = {0, 0, 0};
                mesh.rotation = {0, 0, 0, 1};
                mesh.scale = {1, 1, 1};

                // load texture if material has one
                if (prim.material >= 0 && prim.material < model.materials.size()) {
                    const auto& material = model.materials[prim.material];

                    if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                        auto [texture_data, img_desc] = loadTexture(material.pbrMetallicRoughness.baseColorTexture.index);

                        if (texture_data && img_desc.width > 0) {
                            mesh.texture_data = texture_data;
                            mesh.texture_desc = img_desc;
                            mesh.texture_data_size = img_desc.data.subimage[0][0].size;
                            mesh.has_texture = true;

                            mesh.sampler_desc.min_filter = SG_FILTER_LINEAR;
                            mesh.sampler_desc.mag_filter = SG_FILTER_LINEAR;
                            mesh.sampler_desc.wrap_u = SG_WRAP_REPEAT;
                            mesh.sampler_desc.wrap_v = SG_WRAP_REPEAT;
                        }
                    }
                }

                if (!mesh.has_texture) {
                    mesh.has_texture = false;
                    mesh.sampler_desc.min_filter = SG_FILTER_LINEAR;
                    mesh.sampler_desc.mag_filter = SG_FILTER_LINEAR;
                    mesh.sampler_desc.wrap_u = SG_WRAP_REPEAT;
                    mesh.sampler_desc.wrap_v = SG_WRAP_REPEAT;
                }

                meshes.push_back(std::move(mesh));
            }
        }
    }

    return meshes;
}

bool load_obj(
    const std::string& filename,
    float** out_vertices,
    uint32_t* out_vertex_count,
    uint32_t** out_indices,
    uint32_t* out_index_count
) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    *out_vertices = nullptr;
    *out_vertex_count = 0;
    *out_indices = nullptr;
    *out_index_count = 0;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), nullptr, true);
    if (!warn.empty()) std::cout << "WARN: " << warn << "\n";
    if (!err.empty())  std::cerr << "ERR: " << err << "\n";
    if (!ok)          return false;

    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::map<std::tuple<int, int, int>, uint32_t> vertex_index_map;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                auto key = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);

                if (auto it = vertex_index_map.find(key); it != vertex_index_map.end()) {
                    indices.push_back(it->second);
                } else {
                    float px = attrib.vertices[3 * idx.vertex_index + 0];
                    float py = attrib.vertices[3 * idx.vertex_index + 1];
                    float pz = attrib.vertices[3 * idx.vertex_index + 2];

                    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
                    if (idx.normal_index >= 0) {
                        nx = attrib.normals[3 * idx.normal_index + 0];
                        ny = attrib.normals[3 * idx.normal_index + 1];
                        nz = attrib.normals[3 * idx.normal_index + 2];
                    }

                    float u = 0.0f, v_ = 0.0f;
                    if (idx.texcoord_index >= 0) {
                        u = attrib.texcoords[2 * idx.texcoord_index + 0];
                        v_ = attrib.texcoords[2 * idx.texcoord_index + 1];
                    }

                    vertices.insert(vertices.end(), {px, py, pz, nx, ny, nz, u, v_});
                    uint32_t new_index = static_cast<uint32_t>(vertex_index_map.size());
                    vertex_index_map[key] = new_index;
                    indices.push_back(new_index);
                }
            }
            index_offset += fv;
        }
    }

    const size_t vertex_data_size = vertices.size() * sizeof(float);
    *out_vertices = new float[vertices.size()];
    memcpy(*out_vertices, vertices.data(), vertex_data_size);
    *out_vertex_count = static_cast<uint32_t>(vertices.size() / 8);

    const size_t index_data_size = indices.size() * sizeof(uint32_t);
    *out_indices = new uint32_t[indices.size()];
    memcpy(*out_indices, indices.data(), index_data_size);
    *out_index_count = static_cast<uint32_t>(indices.size());

    return true;
}