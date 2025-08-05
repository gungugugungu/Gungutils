#pragma once
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
#include <ranges>
#include <charconv>
#include <memory>
#include "imgui/imgui.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_fetch.h"
#include "sokol/sokol_time.h"
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol/util/sokol_imgui.h"
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
#include "SDL3/SDL.h"
#include "FModStudio/api/core/inc/fmod.hpp"
#include "FModStudio/api/studio/inc/fmod_studio.hpp"
#include "FModStudio/api/core/inc/fmod_errors.h"
#include "libtinyfiledialogs/tinyfiledialogs.h"
#include "json/include/nlohmann/json.hpp"
// shaders
#include "shaders/mainshader.glsl.h"

using namespace std;

class AudioSource3D;
class Helper;

struct AppState {
    sg_pipeline pip{};
    sg_bindings bind{};
    SDL_Window* win;
    sg_pass_action pass_action{};
    std::array<uint8_t, 512 * 1024> file_buffer{};
    HMM_Vec3 cube_positions[10];
    HMM_Vec3 camera_pos;
    HMM_Vec3 camera_front;
    HMM_Vec3 camera_up;
    uint64_t last_time;
    uint64_t delta_time;
    HMM_Vec3 background_color;
    bool lmb;
    bool rmb;
    float yaw;
    float pitch;
    float fov;
    map<SDL_Keycode, bool> inputs;
    bool running = true;
    FMOD::Studio::System* fmod_system = NULL;
    bool editor_open = false;
    FMOD::Studio::Bank* bank = nullptr;
    std::vector<FMOD::Studio::EventDescription*> event_descriptions;
    vector<AudioSource3D*> audio_sources;
    vector<Helper*> helpers;
};

AppState state;
int texture_index = 0;
int mesh_index = 0;
int num_elements = 0;

void fetch_callback(const sfetch_response_t* response);

HMM_Vec3 QuatToEulerDegrees(const HMM_Quat& q) {
    HMM_Vec3 euler;

    float sinr_cosp = 2 * (q.W * q.X + q.Y * q.Z);
    float cosr_cosp = 1 - 2 * (q.X * q.X + q.Y * q.Y);
    euler.X = atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2 * (q.W * q.Y - q.Z * q.X);
    if (abs(sinp) >= 1)
        euler.Y = copysignf(HMM_PI / 2, sinp);
    else
        euler.Y = asinf(sinp);

    float siny_cosp = 2 * (q.W * q.Z + q.X * q.Y);
    float cosy_cosp = 1 - 2 * (q.Y * q.Y + q.Z * q.Z);
    euler.Z = atan2f(siny_cosp, cosy_cosp);

    euler.X = euler.X * 180.0f / HMM_PI;
    euler.Y = euler.Y * 180.0f / HMM_PI;
    euler.Z = euler.Z * 180.0f / HMM_PI;

    return euler;
}

HMM_Quat EulerDegreesToQuat(const HMM_Vec3& euler) {
    float roll = euler.X * HMM_PI / 180.0f;
    float pitch = euler.Y * HMM_PI / 180.0f;
    float yaw = euler.Z * HMM_PI / 180.0f;

    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);

    HMM_Quat q;
    q.W = cr * cp * cy + sr * sp * sy;
    q.X = sr * cp * cy - cr * sp * sy;
    q.Y = cr * sp * cy + sr * cp * sy;
    q.Z = cr * cp * sy - sr * sp * cy;

    return q;
}

vs_params_t vs_params;

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

        position = other.position;
        rotation = other.rotation;
        scale = other.scale;
        opacity = other.opacity;
        vertex_count = other.vertex_count;
        index_count = other.index_count;
        vertex_buffer_desc = other.vertex_buffer_desc;
        index_buffer_desc = other.index_buffer_desc;
        use_uint16_indices = other.use_uint16_indices;

        // Deep copy vertex data
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
        position = o.position;
        rotation = o.rotation;
        scale = o.scale;
        opacity = o.opacity;
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

class VisGroup;

vector<VisGroup> vis_groups;

class VisGroup {
public:
    string name;
    vector<Mesh> meshes;
    bool enabled = true;
    float opacity = 1.0f;
    VisGroup(string name, vector<Mesh> new_meshes) {
        this->name = name;
        this->meshes = std::move(new_meshes);
    }
};

vector<Mesh> visualizer_meshes;

void Mesh_To_Buffers(Mesh& mesh, bool dontadd = false) {
    std::cout << "=== Preparing Mesh Buffer Descriptors ===" << std::endl;
    std::cout << "Vertex count: " << mesh.vertex_count << std::endl;
    std::cout << "Index count: " << mesh.index_count << std::endl;

    if (!mesh.vertices || mesh.vertex_count == 0) {
        std::cerr << "ERROR: Invalid vertex data!" << std::endl;
        return;
    }

    if (!mesh.indices || mesh.index_count == 0) {
        std::cerr << "ERROR: Invalid index data!" << std::endl;
        return;
    }

    mesh.vertex_buffer_desc = {};
    mesh.vertex_buffer_desc.size = mesh.vertex_count * 8 * sizeof(float);
    mesh.vertex_buffer_desc.data.ptr = mesh.vertices;
    mesh.vertex_buffer_desc.data.size = mesh.vertex_buffer_desc.size;
    mesh.vertex_buffer_desc.usage.immutable = true;
    mesh.vertex_buffer_desc.usage.vertex_buffer = true;

    mesh.use_uint16_indices = true;
    for (size_t i = 0; i < mesh.index_count; i++) {
        if (mesh.indices[i] > 65535) {
            mesh.use_uint16_indices = false;
            break;
        }
    }

    if (mesh.use_uint16_indices) {
        mesh.indices16 = new uint16_t[mesh.index_count];
        for (size_t i = 0; i < mesh.index_count; i++) {
            mesh.indices16[i] = static_cast<uint16_t>(mesh.indices[i]);
        }
    }

    mesh.index_buffer_desc = {};
    mesh.index_buffer_desc.usage.immutable = true;
    mesh.index_buffer_desc.usage.vertex_buffer = false;
    mesh.index_buffer_desc.usage.index_buffer = true;

    if (mesh.use_uint16_indices) {
        mesh.index_buffer_desc.size = mesh.index_count * sizeof(uint16_t);
        mesh.index_buffer_desc.data.ptr = mesh.indices16;
        mesh.index_buffer_desc.data.size = mesh.index_buffer_desc.size;
    } else {
        mesh.index_buffer_desc.size = mesh.index_count * sizeof(uint32_t);
        mesh.index_buffer_desc.data.ptr = mesh.indices;
        mesh.index_buffer_desc.data.size = mesh.index_buffer_desc.size;
    }

    if (!dontadd) {
        vis_groups[0].meshes.push_back(std::move(mesh));
    }
}

void render_meshes_streaming() {
    for (auto& visgroup : vis_groups) {
        if (visgroup.enabled) {
            for (auto& mesh : visgroup.meshes) {
                sg_buffer vertex_buffer = sg_make_buffer(&mesh.vertex_buffer_desc);
                sg_buffer index_buffer = sg_make_buffer(&mesh.index_buffer_desc);

                if (vertex_buffer.id == SG_INVALID_ID || index_buffer.id == SG_INVALID_ID) {
                    if (vertex_buffer.id != SG_INVALID_ID) sg_destroy_buffer(vertex_buffer);
                    if (index_buffer.id != SG_INVALID_ID) sg_destroy_buffer(index_buffer);
                    continue;
                }

                state.bind.vertex_buffers[0] = vertex_buffer;
                state.bind.index_buffer = index_buffer;

                sg_apply_bindings(&state.bind);

                HMM_Mat4 scale_mat = HMM_Scale(mesh.scale);
                HMM_Mat4 rot_mat = HMM_QToM4(mesh.rotation);
                HMM_Mat4 translate_mat = HMM_Translate(mesh.position);
                HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));
                vs_params.model = model;
                vs_params.opacity = mesh.opacity*visgroup.opacity;
                sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

                sg_draw(0, mesh.index_count, 1);

                sg_destroy_buffer(vertex_buffer);
                sg_destroy_buffer(index_buffer);
            }
        }
    }

    if (state.editor_open) {
        for (auto& mesh : visualizer_meshes) {
            sg_buffer vb = sg_make_buffer(&mesh.vertex_buffer_desc);
            sg_buffer ib = sg_make_buffer(&mesh.index_buffer_desc);
            if (vb.id == SG_INVALID_ID || ib.id == SG_INVALID_ID) {
                if (vb.id != SG_INVALID_ID) sg_destroy_buffer(vb);
                if (ib.id != SG_INVALID_ID) sg_destroy_buffer(ib);
                continue;
            }

            state.bind.vertex_buffers[0] = vb;
            state.bind.index_buffer = ib;
            sg_apply_bindings(&state.bind);

            HMM_Mat4 scale_mat = HMM_Scale(mesh.scale);
            HMM_Mat4 rot_mat = HMM_QToM4(mesh.rotation);
            HMM_Mat4 translate_mat = HMM_Translate(mesh.position);
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));

            vs_params.model = model;
            vs_params.opacity = mesh.opacity;
            sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

            sg_draw(0, mesh.index_count, 1);

            sg_destroy_buffer(vb);
            sg_destroy_buffer(ib);
        }
    }
}

void render_meshes_batched_streaming(size_t batch_size = 10) {
    std::vector<sg_buffer> vertex_buffers;
    std::vector<sg_buffer> index_buffers;
    vertex_buffers.reserve(batch_size);
    index_buffers.reserve(batch_size);

    for (auto& visgroup : vis_groups) {
        if (visgroup.enabled) {
            for (size_t start = 0; start < visgroup.meshes.size(); start += batch_size) {
                size_t end = std::min(start + batch_size, visgroup.meshes.size());

                vertex_buffers.clear();
                index_buffers.clear();

                for (size_t i = start; i < end; i++) {
                    auto& mesh = visgroup.meshes[i];
                    sg_buffer vb = sg_make_buffer(&mesh.vertex_buffer_desc);
                    sg_buffer ib = sg_make_buffer(&mesh.index_buffer_desc);

                    if (vb.id != SG_INVALID_ID && ib.id != SG_INVALID_ID) {
                        vertex_buffers.push_back(vb);
                        index_buffers.push_back(ib);
                    } else {
                        if (vb.id != SG_INVALID_ID) sg_destroy_buffer(vb);
                        if (ib.id != SG_INVALID_ID) sg_destroy_buffer(ib);
                        std::cerr << "Failed to create buffers for mesh " << i << std::endl;
                    }
                }

                for (size_t i = 0; i < vertex_buffers.size(); i++) {
                    size_t mesh_idx = start + i;
                    auto& mesh = visgroup.meshes[mesh_idx];

                    state.bind.vertex_buffers[0] = vertex_buffers[i];
                    state.bind.index_buffer = index_buffers[i];

                    sg_apply_bindings(&state.bind);

                    HMM_Mat4 scale_mat = HMM_Scale(mesh.scale);
                    HMM_Mat4 rot_mat = HMM_QToM4(mesh.rotation);
                    HMM_Mat4 translate_mat = HMM_Translate(mesh.position);
                    HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));
                    vs_params.model = model;
                    vs_params.opacity = mesh.opacity*visgroup.opacity;
                    sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

                    sg_draw(0, mesh.index_count, 1);
                }

                for (auto& vb : vertex_buffers) {
                    sg_destroy_buffer(vb);
                }
                for (auto& ib : index_buffers) {
                    sg_destroy_buffer(ib);
                }
            }
        }
    }

    if (state.editor_open) {
        for (auto& mesh : visualizer_meshes) {
            sg_buffer vb = sg_make_buffer(&mesh.vertex_buffer_desc);
            sg_buffer ib = sg_make_buffer(&mesh.index_buffer_desc);
            if (vb.id == SG_INVALID_ID || ib.id == SG_INVALID_ID) {
                if (vb.id != SG_INVALID_ID) sg_destroy_buffer(vb);
                if (ib.id != SG_INVALID_ID) sg_destroy_buffer(ib);
                continue;
            }
            state.bind.vertex_buffers[0] = vb;
            state.bind.index_buffer = ib;
            sg_apply_bindings(&state.bind);

            HMM_Mat4 scale_mat = HMM_Scale(mesh.scale);
            HMM_Mat4 rot_mat = HMM_QToM4(mesh.rotation);
            HMM_Mat4 translate_mat = HMM_Translate(mesh.position);
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));

            vs_params.model = model;
            vs_params.opacity = mesh.opacity;
            sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

            sg_draw(0, mesh.index_count, 1);

            sg_destroy_buffer(vb);
            sg_destroy_buffer(ib);
        }
    }
}

void decompose_matrix(const HMM_Mat4& m, HMM_Vec3& translation, HMM_Quat& rotation, HMM_Vec3& scale) {
    translation = { m.Elements[3][0], m.Elements[3][1], m.Elements[3][2] };

    HMM_Vec3 col0 = { m.Elements[0][0], m.Elements[0][1], m.Elements[0][2] };
    HMM_Vec3 col1 = { m.Elements[1][0], m.Elements[1][1], m.Elements[1][2] };
    HMM_Vec3 col2 = { m.Elements[2][0], m.Elements[2][1], m.Elements[2][2] };

    scale.X = HMM_LenV3(col0);
    scale.Y = HMM_LenV3(col1);
    scale.Z = HMM_LenV3(col2);

    float det = m.Elements[0][0] * (m.Elements[1][1] * m.Elements[2][2] - m.Elements[2][1] * m.Elements[1][2]) -
                m.Elements[0][1] * (m.Elements[1][0] * m.Elements[2][2] - m.Elements[1][2] * m.Elements[2][0]) +
                m.Elements[0][2] * (m.Elements[1][0] * m.Elements[2][1] - m.Elements[1][1] * m.Elements[2][0]);

    if (det < 0) {
        scale.X = -scale.X;
    }

    if (scale.X < 0.0001f) scale.X = (scale.X < 0) ? -1.0f : 1.0f;
    if (scale.Y < 0.0001f) scale.Y = 1.0f;
    if (scale.Z < 0.0001f) scale.Z = 1.0f;

    col0 = col0 * (1.0f / scale.X);
    col1 = col1 * (1.0f / scale.Y);
    col2 = col2 * (1.0f / scale.Z);

    HMM_Mat4 rot_mat = HMM_M4D(1.0f);
    rot_mat.Elements[0][0] = col0.X; rot_mat.Elements[0][1] = col0.Y; rot_mat.Elements[0][2] = col0.Z;
    rot_mat.Elements[1][0] = col1.X; rot_mat.Elements[1][1] = col1.Y; rot_mat.Elements[1][2] = col1.Z;
    rot_mat.Elements[2][0] = col2.X; rot_mat.Elements[2][1] = col2.Y; rot_mat.Elements[2][2] = col2.Z;

    rotation = HMM_M4ToQ_RH(rot_mat);
}

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
;
    return true;
}

void print_fmod_error(FMOD_RESULT result) {
    if (result != FMOD_OK)
    {
        printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
        exit(-1);
    }
}

class AudioSource3D {
    public:
        FMOD::Studio::EventInstance* event_instance;
        HMM_Vec3 position;
        Mesh* visualizer_mesh = nullptr;
        int visualizer_mesh_index = -1;
        FMOD_GUID guid;

        void initalize(FMOD::Studio::EventDescription* desc, HMM_Vec3 pos) {
            FMOD_RESULT result = desc->createInstance(&event_instance);
            print_fmod_error(result);
            state.audio_sources.push_back(this);
            position = pos;

            // init visualizer mesh
            visualizer_mesh = new Mesh();
            float* verts = nullptr;
            uint32_t vertex_count = 0;
            uint32_t* indices = nullptr;
            uint32_t index_count = 0;

            if (load_obj("speaker.obj", &verts, &vertex_count, &indices, &index_count)) {
                visualizer_mesh->vertices = verts;
                visualizer_mesh->vertex_count = vertex_count;
                visualizer_mesh->indices = indices;
                visualizer_mesh->index_count = index_count;
                visualizer_mesh->position = position;

                visualizer_mesh_index = visualizer_meshes.size();
                Mesh_To_Buffers(*visualizer_mesh, true);
                visualizer_meshes.push_back(std::move(*visualizer_mesh));

                delete visualizer_mesh;
                visualizer_mesh = nullptr;
            }
            desc->getID(&guid);
        }

        void play() {
            FMOD_3D_ATTRIBUTES attributes;
            attributes.position = {-position.X, position.Y, -position.Z};
            attributes.velocity = {0.0f, 0.0f, 0.0f};
            attributes.up = {0.0f, 1.0f, 0.0f};
            attributes.forward = {0.0f, 0.0f, 1.0f};
            FMOD_RESULT result = event_instance->set3DAttributes(&attributes);
            print_fmod_error(result);
            event_instance->start();
        }

        void update_visualizer_position() {
            if (visualizer_mesh_index >= 0 && visualizer_mesh_index < visualizer_meshes.size()) {
                visualizer_meshes[visualizer_mesh_index].position = position;
            }
        }

        void stop() {
            event_instance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
        }

        void pause() {
            event_instance->setPaused(true);
        }

        void unpause() {
            event_instance->setPaused(false);
        }

        void set_volume(float volume) {
            event_instance->setVolume(volume);
        }

        void set_position(HMM_Vec3 pos) {
            position = pos;
            FMOD_3D_ATTRIBUTES attributes;
            attributes.position = {-position.X, position.Y, -position.Z};
            attributes.velocity = {0.0f, 0.0f, 0.0f};
            attributes.up = {0.0f, 1.0f, 0.0f};
            attributes.forward = {0.0f, 0.0f, 1.0f};
            FMOD_RESULT result = event_instance->set3DAttributes(&attributes);
            print_fmod_error(result);

            update_visualizer_position();
        }

        void remove() {
            state.audio_sources.erase(std::remove(state.audio_sources.begin(), state.audio_sources.end(), this),state.audio_sources.end());

            if (visualizer_mesh_index >= 0 && visualizer_mesh_index < visualizer_meshes.size()) {
                visualizer_meshes.erase(visualizer_meshes.begin() + visualizer_mesh_index);
                for (auto* as : state.audio_sources) {
                    if (as->visualizer_mesh_index > visualizer_mesh_index) {
                        as->visualizer_mesh_index--;
                    }
                }
            }

            event_instance->release();
            delete visualizer_mesh;
        }
};

class Helper {
public:
    HMM_Vec3 position;
    string name;
    Mesh* visualizer_mesh;
    int visualizer_mesh_index = -1;
    bool operator==(const Helper& other) const { return this == &other; }

    void initalize(const string& name, HMM_Vec3 pos) {
        this->name = name;
        position = pos;
        visualizer_mesh = new Mesh();
        float* verts = nullptr;
        uint32_t vertex_count = 0;
        uint32_t* indices = nullptr;
        uint32_t index_count = 0;

        if (load_obj("helper.obj", &verts, &vertex_count, &indices, &index_count)) {
            visualizer_mesh->vertices = verts;
            visualizer_mesh->vertex_count = vertex_count;
            visualizer_mesh->indices = indices;
            visualizer_mesh->index_count = index_count;
            visualizer_mesh->position = position;

            visualizer_mesh_index = visualizer_meshes.size();
            Mesh_To_Buffers(*visualizer_mesh, true);
            visualizer_meshes.push_back(std::move(*visualizer_mesh));

            delete visualizer_mesh;
            visualizer_mesh = nullptr;
        }
        state.helpers.push_back(this);
    }


    void update_visualizer_position() {
        if (visualizer_mesh_index >= 0 && visualizer_mesh_index < visualizer_meshes.size()) {
            visualizer_meshes[visualizer_mesh_index].position = position;
        }
    }

    void set_position(HMM_Vec3 pos) {
        position = pos;
        update_visualizer_position();
    }

    void remove() {
        state.helpers.erase(std::remove(state.helpers.begin(), state.helpers.end(), this),state.helpers.end());

        if (visualizer_mesh_index >= 0 && visualizer_mesh_index < visualizer_meshes.size()) {
            visualizer_meshes.erase(visualizer_meshes.begin() + visualizer_mesh_index);
            for (auto* hpr : state.helpers) {
                if (hpr->visualizer_mesh_index > visualizer_mesh_index) {
                    hpr->visualizer_mesh_index--;
                }
            }
        }
        delete visualizer_mesh;
    }
};

void make_audiosource_by_index(int index) {
    AudioSource3D* audio_source = new AudioSource3D();
    audio_source->initalize(state.event_descriptions[index], {0.0f, 0.0f, 0.0f});
}

void clear_scene() {
    for (auto* as : state.audio_sources) {
        as->remove();
    }
    state.audio_sources.clear();

    for (auto& visgroup : vis_groups) {
        visgroup.meshes.clear();
    }
    vis_groups.clear();

    for (auto* hpr : state.helpers) {
        hpr->remove();
    }
    state.helpers.clear();

    visualizer_meshes.clear();
}

void save_scene(const string& path) {
    nlohmann::json j;

    // Save meshes
    j["visgroups"] = nlohmann::json::array();
    for (auto& visgroup : vis_groups) {
        nlohmann::json vg_json;
        vg_json["name"] = visgroup.name;
        vg_json["enabled"] = visgroup.enabled;
        vg_json["opacity"] = visgroup.opacity;

        vg_json["meshes"] = nlohmann::json::array();
        for (const auto& mesh_ptr : visgroup.meshes) {
            const auto& mesh = mesh_ptr;
            nlohmann::json mesh_json;

            mesh_json["position"] = {mesh.position.X, mesh.position.Y, mesh.position.Z};
            mesh_json["rotation"] = {mesh.rotation.X, mesh.rotation.Y, mesh.rotation.Z, mesh.rotation.W};
            mesh_json["scale"] = {mesh.scale.X, mesh.scale.Y, mesh.scale.Z};
            mesh_json["opacity"] = mesh.opacity;

            if (mesh.vertex_count > 0 && mesh.vertices) {
                mesh_json["vertex_count"] = mesh.vertex_count;
                mesh_json["vertices"] = nlohmann::json::array();
                for (size_t v = 0; v < mesh.vertex_count * 8; v++) {
                    mesh_json["vertices"].push_back(mesh.vertices[v]);
                }
            }

            if (mesh.index_count > 0 && mesh.indices) {
                mesh_json["index_count"] = mesh.index_count;
                mesh_json["indices"] = nlohmann::json::array();
                for (size_t idx = 0; idx < mesh.index_count; idx++) {
                    mesh_json["indices"].push_back(mesh.indices[idx]);
                }
            }

            vg_json["meshes"].push_back(mesh_json);
        }

        j["visgroups"].push_back(vg_json);
    }

    // Save audio sources
    j["audio_sources"] = nlohmann::json::array();
    for (size_t i = 0; i < state.audio_sources.size(); i++) {
        const auto& as = state.audio_sources[i];
        nlohmann::json as_json;

        as_json["position"] = {as->position.X, as->position.Y, as->position.Z};

        as_json["guid"] = {
            as->guid.Data1,
            as->guid.Data2,
            as->guid.Data3,
            nlohmann::json::array()
        };

        for (int b = 0; b < 8; b++) {
            as_json["guid"][3].push_back(as->guid.Data4[b]);
        }

        j["audio_sources"].push_back(as_json);
    }

    j["helpers"] = nlohmann::json::array();
    for (const Helper* hpr : state.helpers) {
        nlohmann::json helper_json;

        helper_json["position"] = nlohmann::json::array({hpr->position.X, hpr->position.Y, hpr->position.Z});
        helper_json["name"] = hpr->name;

        j["helpers"].push_back(helper_json);
    }

    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
        std::cout << "Scene saved to: " << path << std::endl;
    } else {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
    }
}

void load_scene(const string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << path << std::endl;
        return;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    clear_scene();

    if (j.contains("visgroups")) {
        for (const auto& vg_json : j["visgroups"]) {
            VisGroup new_visgroup = {"placeholder name", {}};
            new_visgroup.name = vg_json["name"];
            new_visgroup.enabled = vg_json["enabled"];
            new_visgroup.opacity = vg_json["opacity"];

            if (vg_json.contains("meshes")) {
                for (const auto& mesh_json : vg_json["meshes"]) {
                    Mesh mesh;

                    if (mesh_json.contains("position")) {
                        auto pos = mesh_json["position"];
                        mesh.position = HMM_V3(pos[0], pos[1], pos[2]);
                    }

                    if (mesh_json.contains("rotation")) {
                        auto rot = mesh_json["rotation"];
                        mesh.rotation = HMM_Quat{rot[0], rot[1], rot[2], rot[3]};
                    }

                    if (mesh_json.contains("scale")) {
                        auto scale = mesh_json["scale"];
                        mesh.scale = HMM_V3(scale[0], scale[1], scale[2]);
                    }

                    if (mesh_json.contains("opacity")) {
                        mesh.opacity = mesh_json["opacity"];
                    }

                    if (mesh_json.contains("vertex_count") &&
                        mesh_json.contains("index_count") &&
                        mesh_json.contains("vertices") &&
                        mesh_json.contains("indices")) {

                        mesh.vertex_count = mesh_json["vertex_count"];
                        mesh.index_count = mesh_json["index_count"];

                        mesh.vertices = new float[mesh.vertex_count * 8];
                        mesh.indices = new uint32_t[mesh.index_count];

                        for (size_t i = 0; i < mesh.vertex_count * 8; i++) {
                            mesh.vertices[i] = mesh_json["vertices"][i];
                        }

                        for (size_t i = 0; i < mesh.index_count; i++) {
                            mesh.indices[i] = mesh_json["indices"][i];
                        }

                        Mesh_To_Buffers(mesh, true);
                    }

                    new_visgroup.meshes.push_back(std::move(mesh));
                }
            }

            vis_groups.push_back(std::move(new_visgroup));
        }
    }

    for (auto* as : state.audio_sources) {
        as->remove();
    }
    state.audio_sources.clear();

    if (j.contains("audio_sources")) {
        for (const auto& as_json : j["audio_sources"]) {
            AudioSource3D* audio_source = new AudioSource3D();

            if (as_json.contains("position")) {
                auto pos = as_json["position"];
                audio_source->position = HMM_V3(pos[0], pos[1], pos[2]);
            }

            if (as_json.contains("guid")) {
                auto guid_json = as_json["guid"];
                FMOD_GUID guid;
                guid.Data1 = guid_json[0];
                guid.Data2 = guid_json[1];
                guid.Data3 = guid_json[2];
                for (int i = 0; i < 8; i++) {
                    guid.Data4[i] = guid_json[3][i];
                }
                audio_source->guid = guid;
            }

            for (auto& ed : state.event_descriptions) {
                FMOD_GUID current_id;
                ed->getID(&current_id);
                if (current_id.Data1 == audio_source->guid.Data1 && current_id.Data2 == audio_source->guid.Data2 && current_id.Data3 == audio_source->guid.Data3 && memcmp(current_id.Data4, audio_source->guid.Data4, 8) == 0) {
                    audio_source->initalize(ed, audio_source->position);
                    break;
                }
            }
        }
    }

    if (j.contains("helpers")) {
        for (const auto& helper_json : j["helpers"]) {
            Helper* helper = new Helper();
            if (helper_json.contains("position")) {
                auto pos = helper_json["position"];
                helper->position = HMM_V3(pos[0], pos[1], pos[2]);
            }

            if (helper_json.contains("name")) {
                auto name = helper_json["name"];
                helper->name = static_cast<string>(name);
            }
            helper->initalize(helper->name, helper->position);
        }
    }

    std::cout << "Scene loaded from: " << path << std::endl;
}

Helper* get_helper_by_name(const string& name) { // DO NOT NAME HELPERS THE SAME NAME
    for (auto& hpr : state.helpers) {
        if (hpr->name == name) {
            return hpr;
        }
    }
    return nullptr;
}

extern void (*init_callback)();
extern void (*frame_callback)();
extern void (*event_callback)(SDL_Event* e);
ImGuiKey ImGui_ImplSDL3_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode);

static int selected_mesh_index = -1;
static int selected_mesh_visgroup = -1;
static int selected_as_index = -1;
static int selected_visgroup_index = -1;
string currently_entered_path = "";
static std::map<int, HMM_Vec3> mesh_euler_rotations;
static int last_selected_mesh = -1;
static int selected_selectable_visgroup_index = -1;

void _init() {
    VisGroup* default_visgroup = new VisGroup("default", {});
    vis_groups.push_back(*default_visgroup);
    stbi_set_flip_vertically_on_load(true);

    simgui_desc_t imgui_desc = {};
    simgui_setup(imgui_desc);

    // FMOD
    FMOD_RESULT result;
    result = FMOD::Studio::System::create(&state.fmod_system);
    print_fmod_error(result);
    FMOD::System* sys;
    result = state.fmod_system->getCoreSystem(&sys);
    print_fmod_error(result);
    result = sys->setSoftwareFormat(0, FMOD_SPEAKERMODE_STEREO, 0);
    print_fmod_error(result);
    result = state.fmod_system->initialize(512, FMOD_STUDIO_INIT_NORMAL | FMOD_STUDIO_INIT_LIVEUPDATE, FMOD_INIT_NORMAL, 0);
    print_fmod_error(result);
    result = state.fmod_system->setNumListeners(1);
    print_fmod_error(result);

    state.camera_pos = HMM_V3(0.0f, 0.0f, 0.0f);
    state.camera_front = HMM_V3(0.0f, 0.0f, -1.0f);
    state.camera_up = HMM_V3(0.0f, 1.0f, 0.0f);
    state.yaw = -90.0f;
    state.pitch = 0.0f;
    state.last_time = stm_now();
    state.fov = 95.0f;

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

    sg_shader shd = sg_make_shader(main_shader_desc(sg_query_backend()));

    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = shd;
    pip_desc.color_count = 1;
    pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    pip_desc.colors->blend.enabled = true;
    pip_desc.colors->blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors->blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors->blend.op_rgb = SG_BLENDOP_ADD;
    pip_desc.colors->blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors->blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors->blend.op_alpha = SG_BLENDOP_ADD;
    pip_desc.layout.attrs[ATTR_main_aPos].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_main_aNormal].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_main_aTexCoord].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS;
    pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth.write_enabled = true;
    pip_desc.cull_mode = SG_CULLMODE_FRONT; // really fucky, only use it if you avoided issues with it in scenes
    pip_desc.label = "main-pipeline";
    state.pip = sg_make_pipeline(&pip_desc);

    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f };
    state.pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    state.pass_action.depth.clear_value = 1.0f;

    sfetch_request_t request = {};
    request.path = "palette.png";
    request.callback = fetch_callback;
    request.buffer.ptr = state.file_buffer.data();
    request.buffer.size = state.file_buffer.size();
    sfetch_send(&request);
}

void _frame() {
    // FMOD
    FMOD_RESULT result;
    state.fmod_system->update();
    FMOD_3D_ATTRIBUTES camera_attributes;
    camera_attributes.position = { -state.camera_pos.X, state.camera_pos.Y, -state.camera_pos.Z };
    camera_attributes.velocity = { 0.0f, 0.0f, 0.0f };
    camera_attributes.forward = { state.camera_front.X, state.camera_front.Y, state.camera_front.Z };
    camera_attributes.up = { state.camera_up.X, state.camera_up.Y, state.camera_up.Z };
    result = state.fmod_system->setListenerAttributes(0, &camera_attributes);
    print_fmod_error(result);

    state.delta_time = stm_laptime(&state.last_time);
    sfetch_dowork();
    state.pass_action.colors[0].clear_value = { state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f };
    sg_pass pass = {};
    pass.action = state.pass_action;
    sg_swapchain swapchain = {};
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    swapchain.width = w_width;
    swapchain.height = w_height;
    swapchain.sample_count = 1;
    swapchain.color_format = SG_PIXELFORMAT_RGBA8;
    swapchain.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    swapchain.gl.framebuffer = 0;
    pass.swapchain = swapchain;
    sg_begin_pass(&pass);
    sg_apply_pipeline(state.pip);

    // imgui
    simgui_frame_desc_t imgui_frame_desc = {};
    imgui_frame_desc.width = w_width;
    imgui_frame_desc.height = w_height;
    imgui_frame_desc.delta_time = state.delta_time;
    imgui_frame_desc.dpi_scale = 1.0f;
    simgui_new_frame(imgui_frame_desc);

    // note that we're translating the scene in the reverse direction of where we want to move -- said zeromake
    HMM_Mat4 view = HMM_LookAt_RH(state.camera_pos, HMM_AddV3(state.camera_pos, state.camera_front), state.camera_up);
    HMM_Mat4 projection = HMM_Perspective_RH_NO(state.fov, static_cast<float>((int)w_width)/static_cast<float>((int)w_height), 0.1f, 100.0f);

    vs_params = {
        .view = view,
        .projection = projection
    };

    render_meshes_batched_streaming();

    // input
    if (state.editor_open) {
        float camera_speed = 5.f * (float) stm_sec(state.delta_time);
        if (state.inputs[SDLK_W] == true) {
            HMM_Vec3 offset = HMM_MulV3F(state.camera_front, camera_speed);
            state.camera_pos = HMM_AddV3(state.camera_pos, offset);
        }
        if (state.inputs[SDLK_S] == true) {
            HMM_Vec3 offset = HMM_MulV3F(state.camera_front, camera_speed);
            state.camera_pos = HMM_SubV3(state.camera_pos, offset);
        }
        if (state.inputs[SDLK_A] == true) {
            HMM_Vec3 offset = HMM_MulV3F(HMM_NormV3(HMM_Cross(state.camera_front, state.camera_up)), camera_speed);
            state.camera_pos = HMM_SubV3(state.camera_pos, offset);
        }
        if (state.inputs[SDLK_D] == true) {
            HMM_Vec3 offset = HMM_MulV3F(HMM_NormV3(HMM_Cross(state.camera_front, state.camera_up)), camera_speed);
            state.camera_pos = HMM_AddV3(state.camera_pos, offset);
        }
    }

    // imgui and editor
    if (state.editor_open) {
        // Mesh editor
        ImGui::Begin("Object list");

        static int selected_mesh_index = -1;

        ImGui::BeginChild("Objects", ImVec2(256, 300), true);

        for (int v = 0; v < vis_groups.size(); v++) {
            VisGroup visgroup = vis_groups[v];
            for (int i = 0; i < visgroup.meshes.size(); i++) {
                auto& mesh = visgroup.meshes[i];
                string label = "Mesh " + to_string(v) + ":" +  to_string(i) + " with VC: " + to_string(mesh.vertex_count);

                bool is_selected = (selected_mesh_index == i);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    selected_mesh_index = i;
                    selected_mesh_visgroup = v;
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Obj settings", ImVec2(300, 300), true);
        if (selected_mesh_index != -1) {
            ImGui::Text("Object settings");
            ImGui::Separator();

            auto& selected_mesh = vis_groups[selected_mesh_visgroup].meshes[selected_mesh_index];

            ImGui::Text("Selected: Mesh %d", selected_mesh_index);

            ImGui::PushItemWidth(200);

            ImGui::DragFloat3("Positon", &selected_mesh.position.X, 0.01f);

            if (last_selected_mesh != selected_mesh_index) {
                mesh_euler_rotations[selected_mesh_index] = QuatToEulerDegrees(selected_mesh.rotation);
                last_selected_mesh = selected_mesh_index;
            }

            HMM_Vec3& euler_rotation = mesh_euler_rotations[selected_mesh_index];

            if (ImGui::DragFloat3("Rotation", &euler_rotation.X, 0.5f)) {
                selected_mesh.rotation = EulerDegreesToQuat(euler_rotation);
            }
            HMM_Vec3 current_euler = QuatToEulerDegrees(selected_mesh.rotation);
            if (abs(current_euler.X - euler_rotation.X) > 0.1f || abs(current_euler.Y - euler_rotation.Y) > 0.1f || abs(current_euler.Z - euler_rotation.Z) > 0.1f) {euler_rotation = current_euler;}

            ImGui::DragFloat3("Scale", &selected_mesh.scale.X, 0.01f);

            ImGui::SliderFloat("Opacity", &selected_mesh.opacity, 0.0f, 1.0f);

            ImGui::PopItemWidth();

            if (ImGui::BeginCombo("VisGroup", vis_groups[selected_mesh_visgroup].name.c_str())) {
                for (int i = 0; i < vis_groups.size(); i++) {
                    bool is_selected = (selected_selectable_visgroup_index == i);
                    if (ImGui::Selectable(vis_groups[i].name.c_str(), is_selected)) {
                        selected_selectable_visgroup_index = i;
                        if (selected_mesh_index >= 0 && selected_mesh_index < vis_groups[selected_mesh_visgroup].meshes.size()) {
                            Mesh mesh_to_move = std::move(vis_groups[selected_mesh_visgroup].meshes[selected_mesh_index]);
                            vis_groups[selected_mesh_visgroup].meshes.erase(vis_groups[selected_mesh_visgroup].meshes.begin() + selected_mesh_index);
                            vis_groups[i].meshes.push_back(std::move(mesh_to_move));
                            selected_mesh_index = -1;
                        }
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            if (ImGui::Button("Duplicate")) {
                if (selected_mesh_index >= 0 && selected_mesh_index < vis_groups[selected_mesh_visgroup].meshes.size()) {
                    const auto& selected_mesh = vis_groups[selected_mesh_visgroup].meshes[selected_mesh_index];
                    Mesh new_mesh;

                    new_mesh.position = selected_mesh.position;
                    new_mesh.rotation = selected_mesh.rotation;
                    new_mesh.scale = selected_mesh.scale;
                    new_mesh.opacity = selected_mesh.opacity;

                    if (selected_mesh.vertex_count > 0 && selected_mesh.vertices) {
                        new_mesh.vertex_count = selected_mesh.vertex_count;
                        new_mesh.vertices = new float[new_mesh.vertex_count * 8];
                        memcpy(new_mesh.vertices, selected_mesh.vertices,new_mesh.vertex_count * 8 * sizeof(float));
                    }

                    if (selected_mesh.index_count > 0 && selected_mesh.indices) {
                        new_mesh.index_count = selected_mesh.index_count;
                        new_mesh.indices = new uint32_t[new_mesh.index_count];
                        memcpy(new_mesh.indices, selected_mesh.indices,
                               new_mesh.index_count * sizeof(uint32_t));
                    }

                    Mesh_To_Buffers(new_mesh);
                }
            }
            if (ImGui::Button("Delete Mesh")) {
                vis_groups[selected_mesh_visgroup].meshes.erase(vis_groups[selected_mesh_visgroup].meshes.begin() + selected_mesh_index);
                selected_mesh_index = -1;
                selected_mesh_visgroup = -1;
            }
        } else {
            ImGui::Text("No mesh selected");
        }
        ImGui::EndChild();

        if (ImGui::Button("Load GLTF")) {
            const char* filter_patterns[] = {"*.glb", "*.gltf"};
            const char* file_path = tinyfd_openFileDialog(
                "Select GLTF File",
                "",
                2,
                filter_patterns,
                "GLTF Files",
                0
            );

            if (file_path) {
                vector<Mesh> loaded_meshes = load_gltf(file_path);
                for (auto& mesh : loaded_meshes) {
                    Mesh_To_Buffers(mesh);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load OBJ")) {
            const char* filter_patterns[] = {"*.obj"};
            const char* file_path = tinyfd_openFileDialog(
                "Select OBJ File",
                "",
                1,
                filter_patterns,
                "OBJ Files",
                0
            );

            if (file_path) {
                float* verts;
                uint32_t vertex_count;
                unsigned int* indices;
                uint32_t index_count;

                if (load_obj(file_path, &verts, &vertex_count, &indices, &index_count)) {
                    Mesh loaded_mesh = Mesh();
                    loaded_mesh.vertices = verts;
                    loaded_mesh.vertex_count = vertex_count;
                    loaded_mesh.indices = indices;
                    loaded_mesh.index_count = index_count;
                    loaded_mesh.position = HMM_V3(0.0f, 0.0f, 0.0f);

                    Mesh_To_Buffers(loaded_mesh);
                    std::cout << "Loaded OBJ" << std::endl;
                }
            }
        }

        ImGui::End();

        // Audio sources
        ImGui::Begin("Audio sources");

        static int selected_as_index = -1;

        ImGui::BeginChild("Sources", ImVec2(256, 150), true);

        for (int i = 0; i < state.audio_sources.size(); i++) {
            string label = "Audio source " + to_string(i);

            bool is_selected = (selected_as_index == i);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_as_index = i;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Src settings", ImVec2(300, 150), true);
        if (selected_as_index >= 0 && selected_as_index < state.audio_sources.size()) {
            ImGui::Text("Settings");
            ImGui::Separator();

            auto& selected_as = state.audio_sources[selected_as_index];

            ImGui::Text("Selected: Mesh %d", selected_as_index);

            ImGui::PushItemWidth(200);

            string label = "Position";
            if (ImGui::DragFloat3(label.c_str(), &selected_as->position.X, 0.01f)) {
                selected_as->set_position(selected_as->position);
            }

            ImGui::Separator();

            label = "Play";
            if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                selected_as->play();
            }

            ImGui::SameLine();
            label = "Stop";
            if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                selected_as->stop();
            }

            label = "Pause";
            if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                selected_as->pause();
            }

            ImGui::SameLine();
            label = "Unpause";
            if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                selected_as->unpause();
            }

            ImGui::PopItemWidth();
            ImGui::Separator();
            if (ImGui::Button("Delete")) {
                selected_as->remove();
                selected_as_index = -1;
            }
        } else {
            ImGui::Text("No source selected");
        }
        ImGui::EndChild();

        static int selected_event_index = -1;

        ImGui::BeginChild("Events", ImVec2(512, 150), true);

        for (int i = 0; i < state.event_descriptions.size(); i++) {
            FMOD_GUID eyedeeznuts;
            FMOD_RESULT result = state.event_descriptions[i]->getID(&eyedeeznuts);
            print_fmod_error(result);

            string label = to_string(i) + ". event, GUID: " + to_string(eyedeeznuts.Data1) + "-" + to_string(eyedeeznuts.Data2) + "-" + to_string(eyedeeznuts.Data3) + "-" + to_string(*eyedeeznuts.Data4);

            bool is_selected = (selected_event_index == i);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_event_index = i;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndChild();

        if (ImGui::Button("Add Source")) {
            make_audiosource_by_index(selected_event_index);
        }

        ImGui::End();

        ImGui::Begin("Helpers");

        static int selected_helper_index = -1;

        ImGui::BeginChild("Helper list", ImVec2(256, 150), true);
        for (int i = 0; i < state.helpers.size(); i++) {
            string label = "Helper " + state.helpers[i]->name;

            bool is_selected = (selected_helper_index == i);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_helper_index = i;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Helper settings", ImVec2(300, 150), true);
        if (selected_helper_index >= 0 && selected_helper_index < state.helpers.size()) {
            ImGui::Text("Settings");
            ImGui::Separator();

            auto& selected_helper = state.helpers[selected_helper_index];

            string label = "Selected: Helper "+selected_helper->name;
            ImGui::Text(label.c_str());

            ImGui::PushItemWidth(200);

            static char name_buffer[256];
            static int last_selected_helper = -1;

            if (last_selected_helper != selected_helper_index) {
                strncpy(name_buffer, selected_helper->name.c_str(), sizeof(name_buffer) - 1);
                name_buffer[sizeof(name_buffer) - 1] = '\0';
                last_selected_helper = selected_helper_index;
            }

            if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
                selected_helper->name = std::string(name_buffer);
            }

            if (ImGui::DragFloat3("Position", &selected_helper->position.X, 0.01f)) {
                selected_helper->set_position(selected_helper->position);
            }

            ImGui::PopItemWidth();
            ImGui::Separator();
            if (ImGui::Button("Delete")) {
                selected_helper->remove();
                selected_helper_index = -1;
            }
        } else {
            ImGui::Text("No helper selected");
        }
        ImGui::EndChild();
        if (ImGui::Button("Add Helper")) {
            Helper* helper = new Helper();
            helper->initalize("New Helper", {0.0f, 0.0f, 0.0f});
        }

        ImGui::End();

        ImGui::Begin("General settings");
        if (ImGui::Button("Save Scene")) {
            const char* filter_patterns[] = {"*.gmap"};
            const char* file_path = tinyfd_saveFileDialog("Select GMap File", "", 1, filter_patterns, "Gungutils Map Files");

            if (file_path) {
                save_scene(file_path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            const char* filter_patterns[] = {"*.gmap"};
            const char* file_path = tinyfd_openFileDialog("Select GMap File", "", 1, filter_patterns, "Gungutils Map Files", 0);

            if (file_path) {
                load_scene(file_path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Scene")) {
            clear_scene();
        }

        ImGui::Separator();
        ImGui::BeginChild("VisGroups", ImVec2(150, 75), true);
        for (int i = 0; i < vis_groups.size(); i++) {
            string label = vis_groups[i].name;

            bool is_selected = (selected_visgroup_index == i);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_visgroup_index = i;
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("VisGroup settings", ImVec2(150, 75), true);
        if (selected_visgroup_index >= 0 && selected_visgroup_index < vis_groups.size()) {
            static char visgroup_name_buffer[256];
            static int last_selected_visgroup = -1;
            VisGroup* selected_visgroup = &vis_groups[selected_visgroup_index];

            if (last_selected_visgroup != selected_visgroup_index) {
                strncpy(visgroup_name_buffer, selected_visgroup->name.c_str(), sizeof(visgroup_name_buffer) - 1);
                visgroup_name_buffer[sizeof(visgroup_name_buffer) - 1] = '\0';
                last_selected_visgroup = selected_visgroup_index;
            }

            if (ImGui::InputText("Name", visgroup_name_buffer, sizeof(visgroup_name_buffer))) {
                selected_visgroup->name = string(visgroup_name_buffer);
            }
            ImGui::Checkbox("Enabled", &selected_visgroup->enabled);
            ImGui::DragFloat("Opacity", &selected_visgroup->opacity, 0.01f, 0.0f, 1.0f);
            if (ImGui::Button("Delete VisGroup")) {
                for (auto& mesh : selected_visgroup->meshes) {
                    vis_groups[0].meshes.push_back(std::move(mesh));
                }
                vis_groups.erase(vis_groups.begin() + selected_visgroup_index);
                selected_visgroup_index = -1;
                selected_mesh_index = -1;
                selected_mesh_visgroup = -1;
            }
        } else {
            ImGui::Text("No VisGroup selected");
        }
        ImGui::EndChild();
        if (ImGui::Button("Add VisGroup")) {
            VisGroup* new_visgroup = new VisGroup("New VisGroup", {});
            vis_groups.push_back(*new_visgroup);
        }
        ImGui::End();
    }
    simgui_render();

    sg_end_pass();
    sg_commit();
}

void _event(SDL_Event* e) {
    if (e->type == SDL_EVENT_QUIT) state.running = false;
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        simgui_add_mouse_pos_event(e->motion.x, e->motion.y);
        if (state.editor_open and state.rmb == true) {
            float sensitivity = 0.1f;

            state.yaw += e->motion.xrel * sensitivity;
            state.pitch += -e->motion.yrel * sensitivity;

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
            HMM_Vec3 world_up = HMM_V3(0.0f, 1.0f, 0.0f);
            HMM_Vec3 camera_right = HMM_NormV3(HMM_Cross(state.camera_front, world_up));
            state.camera_up = HMM_NormV3(HMM_Cross(camera_right, state.camera_front));
        }
    }
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        switch (e->button.button) {
            case SDL_BUTTON_LEFT:
                simgui_add_mouse_button_event(0, true);
                break;
            case SDL_BUTTON_RIGHT:
                simgui_add_mouse_button_event(1, true);
                break;
            case SDL_BUTTON_MIDDLE:
                simgui_add_mouse_button_event(2, true);
                break;
            default:
                simgui_add_mouse_button_event(0, true);
                break;
        }
        if (e->button.button == SDL_BUTTON_LEFT) {
            state.lmb = true;
        }
        if (e->button.button == SDL_BUTTON_RIGHT) {
            state.rmb = true;
        }
    }
    if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        switch (e->button.button) {
            case SDL_BUTTON_LEFT:
                simgui_add_mouse_button_event(0, false);
                break;
            case SDL_BUTTON_RIGHT:
                simgui_add_mouse_button_event(1, false);
                break;
            case SDL_BUTTON_MIDDLE:
                simgui_add_mouse_button_event(2, false);
                break;
            default:
                simgui_add_mouse_button_event(0, false);
                break;
        }
        if (e->button.button == SDL_BUTTON_LEFT) {
            state.lmb = false;
        }
        if (e->button.button == SDL_BUTTON_RIGHT) {
            state.rmb = false;
        }
    }
    if (e->type == SDL_EVENT_MOUSE_WHEEL) {
        simgui_add_mouse_wheel_event(static_cast<float>(e->wheel.x), e->wheel.y);
    }
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        simgui_add_input_characters_utf8(e->text.text);
    }
    if (e->type == SDL_EVENT_KEY_DOWN) {
        ImGuiKey imgui_key = ImGui_ImplSDL3_KeyEventToImGuiKey(e->key.key, e->key.scancode);
        simgui_add_key_event(static_cast<int>(imgui_key), true);

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            state.inputs[e->key.key] = true;
        }
        if (e->key.key == SDLK_0) {
            state.editor_open = !state.editor_open;
            if (state.editor_open) {
                SDL_ShowCursor();
            } else {
                SDL_HideCursor();
            }
        }
    }
    if (e->type == SDL_EVENT_KEY_UP) {
        state.inputs[e->key.key] = false;
        ImGuiKey imgui_key = ImGui_ImplSDL3_KeyEventToImGuiKey(e->key.key, e->key.scancode);
        //printf("Key down: scancode=%d, keycode=%d, imgui_key=%d\n", e->key.scancode, e->key.key, imgui_key);
        simgui_add_key_event(static_cast<int>(imgui_key), false);
    }
    if (e->type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
        simgui_add_focus_event(true);
    }
    if (e->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        simgui_add_focus_event(false);
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

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO);
    SDL_Rect display_bounds;
    SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display_bounds);
    state.win = SDL_CreateWindow("Gungutils", display_bounds.w, display_bounds.h, SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FULLSCREEN);
    SDL_GLContext ctx = SDL_GL_CreateContext(state.win);
    SDL_StartTextInput(state.win);
    sg_desc desc = {};
    sg_environment env = {};
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    env.defaults.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    env.defaults.sample_count = 1;
    desc.environment = env;
    sg_setup(&desc);
    stm_setup();
    _init();
    //init_callback();

    while (state.running) {
        static bool first_frame = true;
        if (first_frame) {
            init_callback();
            first_frame = false;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            _event(&e);
            event_callback(&e);
        }
        if (!state.editor_open) frame_callback();
        _frame();
        SDL_GL_SwapWindow(state.win);
    }

    state.fmod_system->release();
    simgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(state.win);
    SDL_Quit();
    sg_shutdown();
    return 1;
}

ImGuiKey ImGui_ImplSDL3_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode)
{
    // Keypad doesn't have individual key values in SDL3
    switch (scancode)
    {
        case SDL_SCANCODE_KP_0: return ImGuiKey_Keypad0;
        case SDL_SCANCODE_KP_1: return ImGuiKey_Keypad1;
        case SDL_SCANCODE_KP_2: return ImGuiKey_Keypad2;
        case SDL_SCANCODE_KP_3: return ImGuiKey_Keypad3;
        case SDL_SCANCODE_KP_4: return ImGuiKey_Keypad4;
        case SDL_SCANCODE_KP_5: return ImGuiKey_Keypad5;
        case SDL_SCANCODE_KP_6: return ImGuiKey_Keypad6;
        case SDL_SCANCODE_KP_7: return ImGuiKey_Keypad7;
        case SDL_SCANCODE_KP_8: return ImGuiKey_Keypad8;
        case SDL_SCANCODE_KP_9: return ImGuiKey_Keypad9;
        case SDL_SCANCODE_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDL_SCANCODE_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDL_SCANCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDL_SCANCODE_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDL_SCANCODE_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDL_SCANCODE_KP_EQUALS: return ImGuiKey_KeypadEqual;
        default: break;
    }
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        //case SDLK_APOSTROPHE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        //case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        //case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        //case SDLK_EQUALS: return ImGuiKey_Equal;
        //case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        //case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        //case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        //case SDLK_GRAVE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_A: return ImGuiKey_A;
        case SDLK_B: return ImGuiKey_B;
        case SDLK_C: return ImGuiKey_C;
        case SDLK_D: return ImGuiKey_D;
        case SDLK_E: return ImGuiKey_E;
        case SDLK_F: return ImGuiKey_F;
        case SDLK_G: return ImGuiKey_G;
        case SDLK_H: return ImGuiKey_H;
        case SDLK_I: return ImGuiKey_I;
        case SDLK_J: return ImGuiKey_J;
        case SDLK_K: return ImGuiKey_K;
        case SDLK_L: return ImGuiKey_L;
        case SDLK_M: return ImGuiKey_M;
        case SDLK_N: return ImGuiKey_N;
        case SDLK_O: return ImGuiKey_O;
        case SDLK_P: return ImGuiKey_P;
        case SDLK_Q: return ImGuiKey_Q;
        case SDLK_R: return ImGuiKey_R;
        case SDLK_S: return ImGuiKey_S;
        case SDLK_T: return ImGuiKey_T;
        case SDLK_U: return ImGuiKey_U;
        case SDLK_V: return ImGuiKey_V;
        case SDLK_W: return ImGuiKey_W;
        case SDLK_X: return ImGuiKey_X;
        case SDLK_Y: return ImGuiKey_Y;
        case SDLK_Z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        case SDLK_F13: return ImGuiKey_F13;
        case SDLK_F14: return ImGuiKey_F14;
        case SDLK_F15: return ImGuiKey_F15;
        case SDLK_F16: return ImGuiKey_F16;
        case SDLK_F17: return ImGuiKey_F17;
        case SDLK_F18: return ImGuiKey_F18;
        case SDLK_F19: return ImGuiKey_F19;
        case SDLK_F20: return ImGuiKey_F20;
        case SDLK_F21: return ImGuiKey_F21;
        case SDLK_F22: return ImGuiKey_F22;
        case SDLK_F23: return ImGuiKey_F23;
        case SDLK_F24: return ImGuiKey_F24;
        case SDLK_AC_BACK: return ImGuiKey_AppBack;
        case SDLK_AC_FORWARD: return ImGuiKey_AppForward;
        default: break;
    }

    // Fallback to scancode
    switch (scancode)
    {
    case SDL_SCANCODE_GRAVE: return ImGuiKey_GraveAccent;
    case SDL_SCANCODE_MINUS: return ImGuiKey_Minus;
    case SDL_SCANCODE_EQUALS: return ImGuiKey_Equal;
    case SDL_SCANCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDL_SCANCODE_NONUSBACKSLASH: return ImGuiKey_Oem102;
    case SDL_SCANCODE_BACKSLASH: return ImGuiKey_Backslash;
    case SDL_SCANCODE_SEMICOLON: return ImGuiKey_Semicolon;
    case SDL_SCANCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
    case SDL_SCANCODE_COMMA: return ImGuiKey_Comma;
    case SDL_SCANCODE_PERIOD: return ImGuiKey_Period;
    case SDL_SCANCODE_SLASH: return ImGuiKey_Slash;
    default: break;
    }
    return ImGuiKey_None;
}