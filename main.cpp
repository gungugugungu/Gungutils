#define SOKOL_IMPL
#define SOKOL_GLCORE
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <tuple>
#include <map>
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_fetch.h"
#include "sokol/sokol_time.h"
#ifdef B32
#pragma message("B32 is defined as: " B32)
#undef B32
#endif
#include "HandmadeMath/HandmadeMath.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
// shaders
#include <ranges>

#include "shaders/mainshader.glsl.h"

using namespace std;

struct AppState {
    sg_pipeline pip{};
    sg_bindings bind{};
    sg_pass_action pass_action{};
    std::array<uint8_t, 512 * 1024> file_buffer{};
    HMM_Vec3 cube_positions[10];
    HMM_Vec3 camera_pos;
    HMM_Vec3 camera_front;
    HMM_Vec3 camera_up;
    uint64_t last_time;
    uint64_t delta_time;
    bool mouse_btn;
    bool first_mouse;
    float last_x;
    float last_y;
    float yaw;
    float pitch;
    float fov;
    bool inputs[348];
};

AppState state;
int texture_index = 0;
int mesh_index = 0;
int num_elements = 0;

void fetch_callback(const sfetch_response_t* response);

class Mesh {
public:
    HMM_Vec3 position{0,0,0};
    HMM_Quat rotation{0,0,0,1};
    HMM_Vec3 scale{1,1,1};

    float*    vertices = nullptr; // 3 floats for pos, 3 floats for normals and 2 floats for tex coords
    size_t    vertex_count = 0;
    uint32_t* indices  = nullptr;
    size_t    index_count = 0;

    sg_buffer vertex_buffer = {0};
    sg_buffer index_buffer = {0};

    Mesh() = default;
    ~Mesh() {
        delete[] vertices;
        delete[] indices;
    }
    // disable copy, allow move
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& o) noexcept {
        *this = std::move(o);
    }
    Mesh& operator=(Mesh&& o) noexcept {
        std::swap(vertices,    o.vertices);
        std::swap(vertex_count,o.vertex_count);
        std::swap(indices,     o.indices);
        std::swap(index_count, o.index_count);
        std::swap(vertex_buffer, o.vertex_buffer);
        std::swap(index_buffer, o.index_buffer);
        position = o.position;
        rotation = o.rotation;
        scale    = o.scale;
        return *this;
    }
};

vector<Mesh> all_meshes;

// I've been doing this non-stop for a week 12 hours a day
// and this is still not working
void Mesh_To_Buffers(Mesh& mesh) {
    // god damn I'm stuck in debugging hell like wtf is this bullshit
    std::cout << "=== Stupid Mesh Debug info ===" << std::endl;
    std::cout << "Vertex count: " << mesh.vertex_count << std::endl;
    std::cout << "Index count: " << mesh.index_count << std::endl;
    std::cout << "Vertices pointer: " << (void*)mesh.vertices << std::endl;
    std::cout << "Indices pointer: " << (void*)mesh.indices << std::endl;

    if (mesh.vertices != nullptr && mesh.vertex_count > 0) {
        std::cout << "First few vertices:" << std::endl;
        for (size_t i = 0; i < std::min((size_t)3, mesh.vertex_count); i++) {
            std::cout << "  Vertex " << i << ": pos("
                      << mesh.vertices[i*8+0] << ", " << mesh.vertices[i*8+1] << ", " << mesh.vertices[i*8+2]
                      << ") tex(" << mesh.vertices[i*8+6] << ", " << mesh.vertices[i*8+7] << ")" << std::endl;
        }
    }

    // vbuf
    sg_buffer_desc vbuf_desc = {};
    vbuf_desc.size = mesh.vertex_count * 8 * sizeof(float);
    vbuf_desc.data.ptr = mesh.vertices;
    vbuf_desc.data.size = vbuf_desc.size;
    vbuf_desc.usage.immutable = true;
    cout << "vbuf size: " << vbuf_desc.size << endl;
    string label = "mesh"+to_string(all_meshes.size())+"-vertices";
    vbuf_desc.label = label.c_str();
    mesh.vertex_buffer = sg_make_buffer(&vbuf_desc);

    auto* indices16 = new uint16_t[mesh.index_count];
    bool can_use_uint16 = true;
    for (size_t i = 0; i < mesh.index_count; i++) {
        if (mesh.indices[i] > 65535) {
            can_use_uint16 = false;
            break;
        }
        indices16[i] = static_cast<uint16_t>(mesh.indices[i]);
    }

    sg_buffer_desc ibuf_desc = {};
    if (can_use_uint16) {
        ibuf_desc.size = mesh.index_count * sizeof(uint16_t);
        ibuf_desc.data.ptr = indices16;
        ibuf_desc.data.size = ibuf_desc.size;
    } else {
        ibuf_desc.size = mesh.index_count * sizeof(uint32_t);
        ibuf_desc.data.ptr = mesh.indices;
        ibuf_desc.data.size = ibuf_desc.size;
        std::cout << "Warning: Using uint32 indices" << std::endl;
    }

    // ibuf
    ibuf_desc.usage.vertex_buffer = true;
    ibuf_desc.usage.index_buffer = true;
    ibuf_desc.usage.immutable = true;
    label = "mesh"+to_string(all_meshes.size())+"-indices";
    ibuf_desc.label = label.c_str();
    mesh.index_buffer = sg_make_buffer(&ibuf_desc);

    all_meshes.push_back(std::move(mesh));
    std::cout << "=== No More Stupid Debug Info ===" << std::endl;
}

void decompose_matrix(const HMM_Mat4& m, HMM_Vec3& translation, HMM_Quat& rotation, HMM_Vec3& scale) {
    translation = { m.Elements[3][0], m.Elements[3][1], m.Elements[3][2] };

    HMM_Vec3 col0 = { m.Elements[0][0], m.Elements[0][1], m.Elements[0][2] };
    HMM_Vec3 col1 = { m.Elements[1][0], m.Elements[1][1], m.Elements[1][2] };
    HMM_Vec3 col2 = { m.Elements[2][0], m.Elements[2][1], m.Elements[2][2] };

    scale.X = HMM_LenV3(col0);
    scale.Y = HMM_LenV3(col1);
    scale.Z = HMM_LenV3(col2);

    if (scale.X < 0.0001f) scale.X = 1.0f;
    if (scale.Y < 0.0001f) scale.Y = 1.0f;
    if (scale.Z < 0.0001f) scale.Z = 1.0f;

    col0 = col0 * (1.0f / scale.X);
    col1 = col1 * (1.0f / scale.Y);
    col2 = col2 * (1.0f / scale.Z);

    HMM_Mat4 rot_mat = HMM_M4D(1.0f);
    rot_mat.Elements[0][0] = col0.X; rot_mat.Elements[0][1] = col1.X; rot_mat.Elements[0][2] = col2.X;
    rot_mat.Elements[1][0] = col0.Y; rot_mat.Elements[1][1] = col1.Y; rot_mat.Elements[1][2] = col2.Y;
    rot_mat.Elements[2][0] = col0.Z; rot_mat.Elements[2][1] = col1.Z; rot_mat.Elements[2][2] = col2.Z;

    rotation = HMM_M4ToQ_RH(rot_mat);
}

std::vector<Mesh> load_gltf(const std::string& path) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!warn.empty())  std::cout << "WARN: " << warn << "\n";
    if (!ok) {
        std::cerr << "ERROR: " << err << "\n";
        return {};
    }

    std::vector<Mesh> meshes;

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
            HMM_Quat rotation = {0, 0, 0, 1};
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

            HMM_Mat4 T = HMM_Translate(translation);
            HMM_Mat4 R = HMM_QToM4(rotation);
            HMM_Mat4 S = HMM_Scale(scale);
            localTransform = HMM_MulM4(HMM_MulM4(T, R), S);
        }
        HMM_Mat4 worldTransform = HMM_MulM4(parentTransform, localTransform);
        if (node.mesh >= 0 && node.mesh < model.meshes.size()) {
            const auto& meshDef = model.meshes[node.mesh];
            for (const auto& prim : meshDef.primitives) {
                // vertex count
                const auto& posAcc  = model.accessors[prim.attributes.at("POSITION")];
                const auto& normAcc = model.accessors[prim.attributes.at("NORMAL")];
                const auto& uvAcc   = model.accessors[prim.attributes.at("TEXCOORD_0")];
                size_t vcount = posAcc.count;

                auto* verts = new float[vcount * 8];
                const auto* posBuf = reinterpret_cast<const float*>(model.buffers[model.bufferViews[posAcc.bufferView].buffer].data.data()+model.bufferViews[posAcc.bufferView].byteOffset+posAcc.byteOffset);
                const auto* normBuf = reinterpret_cast<const float*>(model.buffers[model.bufferViews[normAcc.bufferView].buffer].data.data()+model.bufferViews[normAcc.bufferView].byteOffset+normAcc.byteOffset);
                const auto* uvBuf = reinterpret_cast<const float*>(model.buffers[model.bufferViews[uvAcc.bufferView].buffer].data.data()+model.bufferViews[uvAcc.bufferView].byteOffset+uvAcc.byteOffset);

                for (size_t i = 0; i < vcount; i++) {
                    // position
                    verts[i*8 + 0] = posBuf[i*3 + 0];
                    verts[i*8 + 1] = posBuf[i*3 + 1];
                    verts[i*8 + 2] = posBuf[i*3 + 2];
                    // normal
                    verts[i*8 + 3] = normBuf[i*3 + 0];
                    verts[i*8 + 4] = normBuf[i*3 + 1];
                    verts[i*8 + 5] = normBuf[i*3 + 2];
                    // texcoord
                    verts[i*8 + 6] = uvBuf[i*2 + 0];
                    verts[i*8 + 7] = uvBuf[i*2 + 1];
                }

                // indices
                const auto& idxAcc = model.accessors[prim.indices];
                size_t icount = idxAcc.count;
                auto* idxs = new uint32_t[icount];
                const auto* idxBuf = reinterpret_cast<const uint16_t*>(
                    model.buffers[ model.bufferViews[idxAcc.bufferView].buffer ].data.data()
                    + model.bufferViews[idxAcc.bufferView].byteOffset
                    + idxAcc.byteOffset
                );
                for (size_t i = 0; i < icount; i++) {
                    idxs[i] = idxBuf[i];
                }

                Mesh mesh;
                mesh.vertices = verts;
                mesh.vertex_count = vcount;
                mesh.indices = idxs;
                mesh.index_count = icount;
                // extract transforms
                decompose_matrix(worldTransform, mesh.position, mesh.rotation, mesh.scale);

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
                // vertex count
                const auto& posAcc  = model.accessors[prim.attributes.at("POSITION")];
                const auto& normAcc = model.accessors[prim.attributes.at("NORMAL")];
                const auto& uvAcc   = model.accessors[prim.attributes.at("TEXCOORD_0")];
                size_t vcount = posAcc.count;

                auto* verts = new float[vcount * 8];
                const auto* posBuf = reinterpret_cast<const float*>(model.buffers[model.bufferViews[posAcc.bufferView].buffer].data.data()+model.bufferViews[posAcc.bufferView].byteOffset+posAcc.byteOffset);
                const auto* normBuf = reinterpret_cast<const float*>(model.buffers[model.bufferViews[normAcc.bufferView].buffer].data.data()+model.bufferViews[normAcc.bufferView].byteOffset+normAcc.byteOffset);
                const auto* uvBuf = reinterpret_cast<const float*>(model.buffers[model.bufferViews[uvAcc.bufferView].buffer].data.data()+model.bufferViews[uvAcc.bufferView].byteOffset+uvAcc.byteOffset);

                for (size_t i = 0; i < vcount; i++) {
                    // position
                    verts[i*8 + 0] = posBuf[i*3 + 0];
                    verts[i*8 + 1] = posBuf[i*3 + 1];
                    verts[i*8 + 2] = posBuf[i*3 + 2];
                    // normal
                    verts[i*8 + 3] = normBuf[i*3 + 0];
                    verts[i*8 + 4] = normBuf[i*3 + 1];
                    verts[i*8 + 5] = normBuf[i*3 + 2];
                    // texcoord
                    verts[i*8 + 6] = uvBuf[i*2 + 0];
                    verts[i*8 + 7] = uvBuf[i*2 + 1];
                }

                // indices
                const auto& idxAcc = model.accessors[prim.indices];
                size_t icount = idxAcc.count;
                auto* idxs = new uint32_t[icount];
                const auto* idxBuf = reinterpret_cast<const uint16_t*>(
                    model.buffers[ model.bufferViews[idxAcc.bufferView].buffer ].data.data()
                    + model.bufferViews[idxAcc.bufferView].byteOffset
                    + idxAcc.byteOffset
                );
                for (size_t i = 0; i < icount; i++) {
                    idxs[i] = idxBuf[i];
                }

                Mesh mesh;
                mesh.vertices = verts;
                mesh.vertex_count = vcount;
                mesh.indices = idxs;
                mesh.index_count = icount;
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

void init() {
    sg_desc desc = {};
    desc.environment = sglue_environment();
    sg_setup(&desc);
    stm_setup();

    stbi_set_flip_vertically_on_load(true);

    sapp_show_mouse(false);

    state.camera_pos = HMM_V3(0.0f, 0.0f, 3.0f);
    state.camera_front = HMM_V3(0.0f, 0.0f, -1.0f);
    state.camera_up = HMM_V3(0.0f, 1.0f, 0.0f);
    state.yaw = -90.0f;
    state.pitch = 0.0f;
    state.first_mouse = true;
    state.last_time = stm_now();
    state.fov = 45.0f;

    sfetch_desc_t fetch_desc = {};
    fetch_desc.max_requests = 2;
    fetch_desc.num_channels = 1;
    fetch_desc.num_lanes = 1;
    sfetch_setup(&fetch_desc);

    // create sampler
    sg_sampler_desc sampler_desc = {};
    sampler_desc.min_filter = SG_FILTER_LINEAR;
    sampler_desc.mag_filter = SG_FILTER_LINEAR;
    sampler_desc.wrap_u = SG_WRAP_REPEAT;
    sampler_desc.wrap_v = SG_WRAP_REPEAT;
    state.bind.samplers[0] = sg_make_sampler(&sampler_desc);

    /*float* verts = nullptr;
    uint32_t vertex_count = 0;
    uint32_t* indices = nullptr;
    uint32_t index_count = 0;*/

    vector<Mesh> loaded_meshes = load_gltf("test2.glb");
    for (auto& mesh : loaded_meshes) {
        Mesh_To_Buffers(mesh);
    }
    /*if (load_obj("test.obj", &verts, &vertex_count, &indices, &index_count)) {
        Mesh loaded_mesh = Mesh();
        loaded_mesh.vertices = verts;
        loaded_mesh.vertex_count = vertex_count;
        loaded_mesh.indices = indices;
        loaded_mesh.index_count = index_count;
        loaded_mesh.position = HMM_V3(0.0f, 0.0f, 0.0f);

        Mesh_To_Buffers(loaded_mesh);
        std::cout << "Loaded OBJ" << std::endl;
    }*/

    sg_shader shd = sg_make_shader(simple_shader_desc(sg_query_backend()));

    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = shd;
    pip_desc.color_count = 1;
    pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    pip_desc.layout.attrs[ATTR_simple_aPos].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_simple_aNormal].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_simple_aTexCoord].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth.write_enabled = true;
    pip_desc.cull_mode = SG_CULLMODE_FRONT;
    pip_desc.label = "main-pipeline";
    state.pip = sg_make_pipeline(&pip_desc);

    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.2f, 0.3f, 0.3f, 1.0f };
    state.pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    state.pass_action.depth.clear_value = 1.0f;

    sfetch_request_t request = {};
    request.path = "palette.png";
    request.callback = fetch_callback;
    request.buffer.ptr = state.file_buffer.data();
    request.buffer.size = state.file_buffer.size();
    sfetch_send(&request);
}

void frame(void) {
    state.delta_time = stm_laptime(&state.last_time);
    sfetch_dowork();
    sg_pass pass = {};
    pass.action = state.pass_action;
    pass.swapchain = sglue_swapchain();

    // input
    float camera_speed = 5.f * (float) stm_sec(state.delta_time);
    if (state.inputs[SAPP_KEYCODE_W] == true) {
        HMM_Vec3 offset = HMM_MulV3F(state.camera_front, camera_speed);
        state.camera_pos = HMM_AddV3(state.camera_pos, offset);
    }
    if (state.inputs[SAPP_KEYCODE_S] == true) {
        HMM_Vec3 offset = HMM_MulV3F(state.camera_front, camera_speed);
        state.camera_pos = HMM_SubV3(state.camera_pos, offset);
    }
    if (state.inputs[SAPP_KEYCODE_A] == true) {
        HMM_Vec3 offset = HMM_MulV3F(HMM_NormV3(HMM_Cross(state.camera_front, state.camera_up)), camera_speed);
        state.camera_pos = HMM_SubV3(state.camera_pos, offset);
    }
    if (state.inputs[SAPP_KEYCODE_D] == true) {
        HMM_Vec3 offset = HMM_MulV3F(HMM_NormV3(HMM_Cross(state.camera_front, state.camera_up)), camera_speed);
        state.camera_pos = HMM_AddV3(state.camera_pos, offset);
    }

    // note that we're translating the scene in the reverse direction of where we want to move -- said zeromake
    HMM_Mat4 view = HMM_LookAt_RH(state.camera_pos, HMM_AddV3(state.camera_pos, state.camera_front), state.camera_up);
    HMM_Mat4 projection = HMM_Perspective_RH_NO(state.fov, (float)sapp_width() / (float)sapp_height(), 0.1f, 100.0f);

    sg_begin_pass(&pass);
    sg_apply_pipeline(state.pip);

    vs_params_t vs_params = {
        .view = view,
        .projection = projection
    };

    for (auto& mesh : all_meshes) {
        state.bind.vertex_buffers[0] = mesh.vertex_buffer;
        state.bind.index_buffer = mesh.index_buffer;
        sg_apply_bindings(&state.bind);
        /*std::cout << "Model vertices: " << mesh.vertex_count << std::endl;
        for (int i = 0; i < mesh.vertex_count; i++) {
            std::cout << "----------------------------------------" << std::endl;
            std::cout << "Positions: " << mesh.vertices[i*8 + 0] << " " << mesh.vertices[i*8 + 1] << " " << mesh.vertices[i*8 + 2] << std::endl;
            std::cout << "Normals: " << mesh.vertices[i*8 + 3] << " " << mesh.vertices[i*8 + 4] << " " << mesh.vertices[i*8 + 5] << std::endl;
            std::cout << "Texture coordinates: " << mesh.vertices[i*8 + 6] << " " << mesh.vertices[i*8 + 7] << std::endl;
            std::cout << "Camera pos: " << state.camera_pos.X << ", " << state.camera_pos.Y << ", " << state.camera_pos.Z << std::endl;
            std::cout << "Mesh pos: " << mesh.position.X << ", " << mesh.position.Y << ", " << mesh.position.Z << std::endl;
            std::cout << "Index count: " << mesh.index_count << std::endl;
        }*/

        HMM_Mat4 model = HMM_Translate(mesh.position);
        HMM_Mat4 rot_mat = HMM_QToM4(mesh.rotation);
        HMM_Mat4 scale_mat = HMM_Scale(mesh.scale);
        model = HMM_MulM4(model, HMM_MulM4(scale_mat, rot_mat));
        vs_params.model = model;
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

        sg_draw(0, mesh.index_count, 1);
    }

    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
    sfetch_shutdown();
}

void event(const sapp_event* e) {
        if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        state.mouse_btn = true;
    } else if (e->type == SAPP_EVENTTYPE_MOUSE_UP) {
        state.mouse_btn = false;
    } else if (e->type == SAPP_EVENTTYPE_KEY_DOWN) {
        state.inputs[e->key_code] = true;
        if (e->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_request_quit();
        }

        if (e->key_code == SAPP_KEYCODE_SPACE) {
            bool mouse_shown = sapp_mouse_shown();
            sapp_show_mouse(!mouse_shown);
        }

    } else if (e->type == SAPP_EVENTTYPE_KEY_UP) {
        state.inputs[e->key_code] = false;
    }  else if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE && state.mouse_btn) {
        if(state.first_mouse) {
            state.last_x = e->mouse_x;
            state.last_y = e->mouse_y;
            state.first_mouse = false;
        }

        float xoffset = e->mouse_x - state.last_x;
        float yoffset = state.last_y - e->mouse_y;
        state.last_x = e->mouse_x;
        state.last_y = e->mouse_y;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        state.yaw   += xoffset;
        state.pitch += yoffset;

        if(state.pitch > 89.0f) {
            state.pitch = 89.0f;
        }
        else if(state.pitch < -89.0f) {
            state.pitch = -89.0f;
        }

        HMM_Vec3 direction;
        direction.X = cosf(state.yaw * HMM_PI / 180.0f) * cosf(state.pitch * HMM_PI / 180.0f);
        direction.Y = sinf(state.pitch * HMM_PI / 180.0f);
        direction.Z = sinf(state.yaw * HMM_PI / 180.0f) * cosf(state.pitch * HMM_PI / 180.0f);
        state.camera_front = HMM_NormV3(direction);
    }
    else if (e->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        if (state.fov >= 1.0f && state.fov <= 45.0f) {
            state.fov -= e->scroll_y;
        }
        if (state.fov <= 1.0f)
            state.fov = 1.0f;
        else if (state.fov >= 45.0f)
            state.fov = 45.0f;
    }
}

void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        int img_width, img_height, num_channels;
        const int desired_channels = 4;
        stbi_uc* pixels = stbi_load_from_memory(
            static_cast<const stbi_uc*>(response->data.ptr),
            static_cast<int>(response->data.size),
            &img_width, &img_height,
            &num_channels, desired_channels);

        if (pixels) {
            // yay, memory safety!
            sg_destroy_image(state.bind.images[texture_index]);

            sg_image_desc img_desc = {};
            img_desc.width = img_width;
            img_desc.height = img_height;
            img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
            img_desc.data.subimage[0][0].ptr = pixels;
            img_desc.data.subimage[0][0].size = img_width * img_height * 4;
            state.bind.images[texture_index] = sg_make_image(&img_desc);

            stbi_image_free(pixels);
        }
    } else if (response->failed) {
        state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
        state.pass_action.colors[0].clear_value = { 1.0f, 0.0f, 0.0f, 1.0f };
        std::cout << "ohhh no, failed to fetch the texture =(" << std::endl;
    }
    texture_index++;
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = event;
    desc.width = 800;
    desc.height = 600;
    desc.high_dpi = true;
    desc.window_title = "Gungutils";
    return desc;
}