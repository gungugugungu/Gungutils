#define SOKOL_IMPL
#define SOKOL_GLCORE
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <zlib.h>
#include <cstdlib>
#include <tuple>
#include <map>
#include <ranges>
#include <charconv>
#include <memory>
#include <functional>
#include <fstream>
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_fetch.h"
#include "sokol/sokol_time.h"
#define SOKOL_IMGUI_NO_SOKOL_APP
#include "sokol/util/sokol_imgui.h"
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
#include <reactphysics3d/reactphysics3d.h>
#include "meshoptimizer/src/meshoptimizer.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
// shaders
#include "shaders/mainshader.glsl.h"
#include "shaders/postprocess.glsl.h"
#include "shaders/surface.glsl.h"
#include "shaders/particles.glsl.h"
#include "shaders/billboard.glsl.h"
// sources
#include "rendering/Material.h"
#include "rendering/Mesh.h"
#include "rendering/Object.h"
#include "rendering/VisGroup.h"
#include "utils/Animation.h"
#include "rendering/Post Processing.h"
#include "rendering/Light.h"
#include "rendering/Surface.h"
#include "physics/PhysicsHolder.h"
#include "rendering/ParticleSystem.h"
#include "rendering/Billboard.h"
#include "ui/UIButton.h"
#include "utils/CharacterController.h"
#include "utils/FPSController.h"

using namespace std;

class AudioSource3D;
class Helper;
void render_editor();

float fps_over_time[225];
float vertex_count_over_time[225];
int all_vertex_count = 0;
float index_count_over_time[225];
int all_index_count = 0;

struct AppState {
    sg_pipeline pip{};
    sg_bindings bind{};
    SDL_Window* win;
    sg_pass_action pass_action{};
    HMM_Vec3 camera_pos;
    HMM_Vec3 camera_front;
    HMM_Vec3 camera_up;
    uint64_t last_time;
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
    std::vector<std::unique_ptr<PhysicsHolder>> physics_holders;
    vector<DirectionalLight> directional_lights;
    vector<PointLight> point_lights;
    vector<SpotLight> spot_lights;
    HMM_Vec3 ambient_light = {0.5f, 0.5f, 0.5f};

    sg_pipeline surf_pipeline;
    Surface window_surface{};
    vector<ParticleSystem> particle_systems;
};

AppState state;
PostProcessState post_state = {};
ssao_params_t ssao_params;
int texture_index = 0;
int mesh_index = 0;
int num_elements = 0;
bool loaded_is_palette = false;
sg_pipeline particle_pipeline{.id = SG_INVALID_ID};
sg_buffer quad_vb{.id = SG_INVALID_ID};
sg_buffer quad_ib{.id = SG_INVALID_ID};

struct TimeState {
    Uint64 freq = 0;
    Uint64 last = 0;
    float dt = 1.0f / 60.0f; // safe initial value
    float fps = 60.0f;
};

TimeState time_state;

void Time_Init(TimeState& t) {
    t.freq = SDL_GetPerformanceFrequency();
    t.last = SDL_GetPerformanceCounter();
}

void Time_BeginFrame(TimeState& t) {
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 diff = now - t.last;
    t.last = now;

    double dt = (double)diff / (double)t.freq;

    // keep it valid and friendly for ImGui
    if (!std::isfinite(dt) || dt <= 0.0) {
        dt = 1.0 / 60.0;
    } else if (dt > 0.25) {
        // clamp huge spikes
        dt = 0.016;
    }

    t.dt = static_cast<float>(dt);
    t.fps = 1.0f / t.dt;
}

void load_font(stbtt_fontinfo *font_info, const char *filename) {
    FILE* font_file = fopen(filename, "rb");
    fseek(font_file, 0, SEEK_END);
    size_t font_size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);

    unsigned char* font_buffer = new unsigned char[font_size];
    fread(font_buffer, 1, font_size, font_file);
    fclose(font_file);

    stbtt_InitFont(font_info, font_buffer, 0);
    cout << "loaded font " << filename << endl;
}

struct Plane {
    HMM_Vec3 normal;
    float d;
};

Plane frustum_planes[6];

void init_post_processing() {
    int width, height;
    SDL_GetWindowSize(state.win, &width, &height);

    post_state.color_img = {};
    post_state.color_img.width = width;
    post_state.color_img.height = height;
    post_state.color_img.pixel_format = SG_PIXELFORMAT_RGBA8;
    post_state.color_img.sample_count = 1;
    post_state.color_img.usage.color_attachment = true;
    post_state.color_img.label = "color-render-target";

    post_state.depth_img = {};
    post_state.depth_img.width = width;
    post_state.depth_img.height = height;
    post_state.depth_img.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    post_state.depth_img.sample_count = 1;
    post_state.depth_img.usage.depth_stencil_attachment = true;
    post_state.depth_img.label = "depth-render-target";

    post_state.rendered_color_img = {SG_INVALID_ID};
    post_state.rendered_depth_img = {SG_INVALID_ID};
    post_state.rendered_color_att_view = {SG_INVALID_ID};
    post_state.rendered_depth_att_view = {SG_INVALID_ID};
    post_state.rendered_color_tex_view = {SG_INVALID_ID};
    post_state.rendered_depth_tex_view = {SG_INVALID_ID};

    sg_shader post_shader = sg_make_shader(postprocess_shader_desc(sg_query_backend()));

    sg_pipeline_desc post_pip_desc = {};
    post_pip_desc.shader = post_shader;
    post_pip_desc.layout.attrs[ATTR_postprocess_position].format = SG_VERTEXFORMAT_FLOAT3;
    post_pip_desc.layout.attrs[ATTR_postprocess_texcoord].format = SG_VERTEXFORMAT_FLOAT2;
    post_pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    post_pip_desc.color_count = 1;
    post_pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    post_pip_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    post_pip_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    post_pip_desc.depth.write_enabled = true;
    post_pip_desc.cull_mode = SG_CULLMODE_NONE;
    post_pip_desc.label = "post-process-pipeline";
    post_state.post_pipeline = sg_make_pipeline(&post_pip_desc);

    sg_sampler_desc smp_desc = {};
    smp_desc.min_filter = SG_FILTER_LINEAR;
    smp_desc.mag_filter = SG_FILTER_LINEAR;
    smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    post_state.rendered_post_sampler = sg_make_sampler(&smp_desc);

    sg_sampler_desc smp_depth_desc = {};
    smp_depth_desc.min_filter = SG_FILTER_NEAREST;
    smp_depth_desc.mag_filter = SG_FILTER_NEAREST;
    smp_depth_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp_depth_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    post_state.rendered_depth_sampler = sg_make_sampler(&smp_depth_desc);

    post_state.uniforms.vignette_strength = 0.5f;
    post_state.uniforms.vignette_radius = 0.8f;
    post_state.uniforms.color_tint = HMM_V3(1.0f, 1.0f, 1.0f);
    post_state.uniforms.exposure = 1.0f;
    post_state.uniforms.contrast = 1.0f;
    post_state.uniforms.brightness = 0.0f;
    post_state.uniforms.saturation = 1.0f;

    ssao_params = ssao_params_t{};
    ssao_params.ao_radius = 0.5f;
    ssao_params.ao_bias = 0.02f;
    ssao_params.ao_strength = 1.0f;
    ssao_params.ao_power = 1.75f;
    ssao_params.ssao_samples = 32;
}

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

vector<VisGroup> vis_groups;
vector<Object> visualizer_objects;

sg_image validate_and_make_image(sg_image_desc *d, const char *name) {
    sg_image invalid_image{};
    invalid_image.id = SG_INVALID_ID;
    if (!d) { printf("validate: null desc for %s\n", name); return invalid_image;}
    if (d->width <= 0 || d->height <= 0) {
        printf("validate: %s has invalid dims w=%d h=%d\n", name, d->width, d->height);
    }
    if (d->pixel_format != SG_PIXELFORMAT_RGBA8) {
        printf("validate: %s pixel_format = %d (expected RGBA8)\n", name, (int)d->pixel_format);
    }
    size_t expected = (size_t)d->width * (size_t)d->height * 4;
    size_t given = d->data.subimage[0][0].size;
    const void *ptr = d->data.subimage[0][0].ptr;
    if (!ptr || given == 0) {
        printf("validate: %s has null ptr or zero size (ptr=%p size=%zu)\n", name, ptr, given);
    } else if (given != expected) {
        printf("validate: %s size mismatch (expected %zu, given %zu)\n", name, expected, given);
    }
    sg_image_desc local_desc = *d;
    return sg_make_image(&local_desc);
};

void prepare_mesh_buffers(Object& object) {
    auto prepare_single_mesh = [](Mesh& mesh) {
        if (!mesh.vertices || mesh.vertex_count == 0) {
            std::cerr << "ERROR: Invalid vertex data!" << std::endl;
            exit(-1);
        }

        if (!mesh.indices || mesh.index_count == 0) {
            std::cerr << "ERROR: Invalid index data!" << std::endl;
            exit(-1);
        }

        std::cout << "=== MESH BUFFER DEBUG ===" << std::endl;
        std::cout << "Vertex count: " << mesh.vertex_count << std::endl;
        std::cout << "Index count: " << mesh.index_count << std::endl;
        std::cout << "Buffer size: " << (mesh.vertex_count * 8 * sizeof(float)) << " bytes" << std::endl;
        std::cout << "Expected layout: 8 floats per vertex (pos=3, norm=3, uv=2)" << std::endl;

        for (size_t i = 0; i < mesh.vertex_count; ++i) {
            float px = mesh.vertices[i*8 + 0];
            float py = mesh.vertices[i*8 + 1];
            float pz = mesh.vertices[i*8 + 2];
            if (!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(pz)) {
                std::cerr << "ERROR: Invalid vertex position at " << i << std::endl;
                exit(-1);
            }
        }

        for (size_t i = 0; i < mesh.index_count; ++i) {
            if (mesh.indices[i] >= mesh.vertex_count) {
                std::cout << "ERROR: Index " << i << " value " << mesh.indices[i] << " >= vertex_count " << mesh.vertex_count << std::endl;
                break;
            }
        }
        std::cout << "=========================" << std::endl;

        const size_t index_count  = mesh.index_count;
        const size_t vertex_count = mesh.vertex_count;
        const size_t vertex_size  = 8 * sizeof(float);
        const float* vertices_src = mesh.vertices;
        const uint32_t* indices_src = mesh.indices;

        uint32_t* tmp_indices_1 = new uint32_t[index_count];
        uint32_t* tmp_indices_2 = new uint32_t[index_count];

        meshopt_optimizeVertexCache(tmp_indices_1, indices_src, index_count, vertex_count);

        meshopt_optimizeOverdraw(tmp_indices_2, tmp_indices_1, index_count, vertices_src, vertex_count, vertex_size, 1.05f);

        unsigned int* remap = new unsigned int[vertex_count];
        size_t new_vertex_count = meshopt_generateVertexRemap(remap, tmp_indices_2, index_count, vertices_src, vertex_count, vertex_size);

        float* vertices_remapped = new float[new_vertex_count * 8];
        meshopt_remapVertexBuffer(vertices_remapped, vertices_src, vertex_count, vertex_size, remap);

        uint32_t* indices_remapped = new uint32_t[index_count];
        meshopt_remapIndexBuffer(indices_remapped, tmp_indices_2, index_count, remap);

        delete[] tmp_indices_1;
        delete[] tmp_indices_2;
        delete[] remap;

        bool can_use_uint16 = true;
        for (size_t i = 0; i < index_count; ++i) {
            if (indices_remapped[i] > 0xFFFFu) { can_use_uint16 = false; break; }
        }

        delete[] mesh.vertices;
        delete[] mesh.indices;
        delete[] mesh.indices16;
        mesh.indices16 = nullptr;

        mesh.vertices = vertices_remapped;
        mesh.vertex_count = new_vertex_count;

        mesh.index_buffer_desc = {};
        mesh.index_buffer_desc.usage.immutable = true;
        mesh.index_buffer_desc.usage.vertex_buffer = false;
        mesh.index_buffer_desc.usage.index_buffer = true;

        mesh.use_uint16_indices = false;
        mesh.indices = indices_remapped;
        mesh.index_buffer_desc.size = index_count * sizeof(uint32_t);
        mesh.index_buffer_desc.data.ptr = mesh.indices;
        mesh.index_buffer_desc.data.size = mesh.index_buffer_desc.size;

        mesh.vertex_buffer_desc = {};
        mesh.vertex_buffer_desc.usage.immutable = true;
        mesh.vertex_buffer_desc.usage.vertex_buffer = true;
        mesh.vertex_buffer_desc.size = mesh.vertex_count * 8 * sizeof(float);
        mesh.vertex_buffer_desc.data.ptr = mesh.vertices;
        mesh.vertex_buffer_desc.data.size = mesh.vertex_buffer_desc.size;

        if (!mesh.material->has_diffuse_texture) {
            mesh.material->diffuse_sampler_desc.min_filter = SG_FILTER_LINEAR;
            mesh.material->diffuse_sampler_desc.mag_filter = SG_FILTER_LINEAR;
            mesh.material->diffuse_sampler_desc.wrap_u = SG_WRAP_REPEAT;
            mesh.material->diffuse_sampler_desc.wrap_v = SG_WRAP_REPEAT;
        }

        mesh.vertex_buffer = sg_make_buffer(&mesh.vertex_buffer_desc);
        mesh.index_buffer = sg_make_buffer(&mesh.index_buffer_desc);

        Material* material = mesh.material;
        if (material->has_diffuse_texture) {
            material->diffuse_image = validate_and_make_image(&material->diffuse_texture_desc, "diffuse");
            if (material->diffuse_image.id == SG_INVALID_ID) {
                cout << "Oh no failed to create diffuse image ohhh noooo" << endl;
                material->has_diffuse_texture = false;
            } else {
                material->diffuse_sampler = sg_make_sampler(&material->diffuse_sampler_desc);
            }
        }
        if (material->has_specular_texture) {
            material->specular_image = validate_and_make_image(&material->specular_texture_desc, "specular");
            if (material->specular_image.id == SG_INVALID_ID) {
                cout << "Oh no failed to create specualr image ohhh noooo" << endl;
                material->has_specular_texture = false;
            } else {
                material->specular_sampler = sg_make_sampler(&material->specular_sampler_desc);
            }
        }

        std::cout << "meshoptimizer: original verts=" << vertex_count << " -> new verts=" << mesh.vertex_count << ", indices=" << index_count << ", 16bit=" << (mesh.use_uint16_indices ? "yes" : "no") << std::endl;
    };

    if (object.mesh) {
        prepare_single_mesh(*object.mesh);
        uint64_t time = stm_now();
        object.initialize_bounds();
        cout << "Bounds initialized in " << stm_ms(stm_since(time)) << " ms" << endl;
    }

    for (Mesh* shape_key : object.shape_keys) {
        prepare_single_mesh(*shape_key);
    }
}

bool is_object_in_frustum(const Object& obj) {
    if (obj.bounding_rect.X == 0.0f && obj.bounding_rect.Y == 0.0f && obj.bounding_rect.Z == 0.0f) return true;

    float eff_dx = obj.bounding_rect.X * fabsf(obj.scale.X);
    float eff_dy = obj.bounding_rect.Y * fabsf(obj.scale.Y);
    float eff_dz = obj.bounding_rect.Z * fabsf(obj.scale.Z);

    float radius = 0.5f * sqrtf(eff_dx*eff_dx + eff_dy*eff_dy + eff_dz*eff_dz);

    HMM_Vec3 center = obj.position;

    for (int i = 0; i < 6; i++) {
        float dist = HMM_DotV3(frustum_planes[i].normal, center) + frustum_planes[i].d;
        if (dist < -radius) return false;
    }

    return true;
}

bool is_object_obstructed(const Object& obj, const HMM_Mat4& view_matrix, const HMM_Mat4& proj_matrix) {
    if (obj.bounding_rect.X == 0.0f && obj.bounding_rect.Y == 0.0f && obj.bounding_rect.Z == 0.0f) return false;

    float eff_dx = obj.bounding_rect.X * fabsf(obj.scale.X);
    float eff_dy = obj.bounding_rect.Y * fabsf(obj.scale.Y);
    float eff_dz = obj.bounding_rect.Z * fabsf(obj.scale.Z);

    HMM_Vec3 half_extents = {eff_dx * 0.5f, eff_dy * 0.5f, eff_dz * 0.5f};
    HMM_Vec3 center = obj.position;

    HMM_Vec3 corners[8] = {
        HMM_AddV3(center, HMM_V3(-half_extents.X, -half_extents.Y, -half_extents.Z)),
        HMM_AddV3(center, HMM_V3( half_extents.X, -half_extents.Y, -half_extents.Z)),
        HMM_AddV3(center, HMM_V3(-half_extents.X,  half_extents.Y, -half_extents.Z)),
        HMM_AddV3(center, HMM_V3( half_extents.X,  half_extents.Y, -half_extents.Z)),
        HMM_AddV3(center, HMM_V3(-half_extents.X, -half_extents.Y,  half_extents.Z)),
        HMM_AddV3(center, HMM_V3( half_extents.X, -half_extents.Y,  half_extents.Z)),
        HMM_AddV3(center, HMM_V3(-half_extents.X,  half_extents.Y,  half_extents.Z)),
        HMM_AddV3(center, HMM_V3( half_extents.X,  half_extents.Y,  half_extents.Z))
    };

    HMM_Mat4 view_proj = HMM_MulM4(proj_matrix, view_matrix);

    float min_screen_x = FLT_MAX, max_screen_x = -FLT_MAX;
    float min_screen_y = FLT_MAX, max_screen_y = -FLT_MAX;
    float closest_z = FLT_MAX;

    for (int i = 0; i < 8; i++) {
        HMM_Vec4 world_pos = HMM_V4V(corners[i], 1.0f);
        HMM_Vec4 clip_pos = HMM_MulM4V4(view_proj, world_pos);

        if (clip_pos.W <= 0.0f) continue;

        HMM_Vec3 ndc = HMM_V3(clip_pos.X / clip_pos.W, clip_pos.Y / clip_pos.W, clip_pos.Z / clip_pos.W);

        if (ndc.X < -1.0f || ndc.X > 1.0f || ndc.Y < -1.0f || ndc.Y > 1.0f) continue;

        min_screen_x = fminf(min_screen_x, ndc.X);
        max_screen_x = fmaxf(max_screen_x, ndc.X);
        min_screen_y = fminf(min_screen_y, ndc.Y);
        max_screen_y = fmaxf(max_screen_y, ndc.Y);
        closest_z = fminf(closest_z, ndc.Z);
    }

    if (min_screen_x == FLT_MAX) return false;

    int width, height;
    SDL_GetWindowSize(state.win, &width, &height);

    int screen_min_x = (int)((min_screen_x * 0.5f + 0.5f) * width);
    int screen_max_x = (int)((max_screen_x * 0.5f + 0.5f) * width);
    int screen_min_y = (int)((min_screen_y * 0.5f + 0.5f) * height);
    int screen_max_y = (int)((max_screen_y * 0.5f + 0.5f) * height);

    screen_min_x = fmaxf(0, fminf(width - 1, screen_min_x));
    screen_max_x = fmaxf(0, fminf(width - 1, screen_max_x));
    screen_min_y = fmaxf(0, fminf(height - 1, screen_min_y));
    screen_max_y = fmaxf(0, fminf(height - 1, screen_max_y));

    int sample_step = fmaxf(1, (screen_max_x - screen_min_x) / 8);

    for (int y = screen_min_y; y <= screen_max_y; y += sample_step) {
        for (int x = screen_min_x; x <= screen_max_x; x += sample_step) {
            float depth_buffer_value = 1.0f;

            if (closest_z < depth_buffer_value - 0.001f) {
                return false;
            }
        }
    }

    return true;
}

void render_meshes() {
    all_vertex_count = 0;
    all_index_count = 0;
    struct Instance {
        Object obj;
        float group_opacity;
    };
    std::vector<Instance> instances;

    for (auto& visgroup : vis_groups) {
        if (!visgroup.enabled) continue;
        float group_opacity = visgroup.opacity;
        for (size_t i = 0; i < visgroup.objects.size(); i++) {
            Object obj = visgroup.objects[i];
            if (obj.mesh != nullptr && is_object_in_frustum(obj)) {
                if (!is_object_obstructed(obj, vs_params.view, vs_params.projection)) {
                    instances.push_back({obj, group_opacity});
                }
            }
        }
    }

    for (size_t i = 0; i < visualizer_objects.size(); ++i) {
        Object obj = visualizer_objects[i];
        if (obj.mesh != nullptr && is_object_in_frustum(obj)) {
            if (!is_object_obstructed(obj, vs_params.view, vs_params.projection)) {
                instances.push_back({ obj, 1.0f});
            }
        }
    }

    if (instances.empty()) return;

    struct MeshGroup {
        Mesh* mesh;
        std::vector<Instance> items;
        sg_view diffuse_view;
        sg_view specular_view;
        bool views_created = false;
    };
    std::vector<MeshGroup> groups;

    for (auto &inst : instances) {
        Mesh* mesh = inst.obj.mesh;
        if (!mesh || !mesh->vertices) continue;

        bool placed = false;
        for (auto &g : groups) {
            if (g.mesh->vertex_count == mesh->vertex_count) {
                size_t bytes = (size_t)mesh->vertex_count * 8 * sizeof(float);
                if (bytes > 0 && g.mesh->vertices && mesh->vertices) {
                    if (memcmp(g.mesh->vertices, mesh->vertices, bytes) == 0) {
                        g.items.push_back(inst);
                        placed = true;
                        break;
                    }
                }
            }
        }
        if (!placed) {
            MeshGroup mg;
            mg.mesh = mesh;
            mg.items.push_back(inst);
            mg.views_created = false;
            groups.push_back(std::move(mg));
        }
    }

    struct lighting_params {
        int light_types_packed[13][4];
        HMM_Vec4 light_positions[50];
        HMM_Vec4 light_directions[50];
        HMM_Vec4 light_colors[50];
        HMM_Vec4 light_att_params[50];
        int light_amount;
        float padding[3];
        HMM_Vec4 ambient_color;
    } lights = {};

    int light_idx = 0;
    for (const auto& dl : state.directional_lights) {
        if (light_idx >= 50) break;
        lights.light_types_packed[light_idx / 4][light_idx % 4] = 0;
        lights.light_positions[light_idx] = { 0.0f, 0.0f, 0.0f, 0.0f };
        lights.light_directions[light_idx] = { dl.direction.X, dl.direction.Y, dl.direction.Z, 0.0f };
        lights.light_colors[light_idx] = { dl.color.X, dl.color.Y, dl.color.Z, dl.intensity };
        lights.light_att_params[light_idx] = { 0.0f, 0.0f, 0.0f, 0.0f };
        light_idx++;
    }

    for (const auto& pl : state.point_lights) {
        if (light_idx >= 50) break;
        lights.light_types_packed[light_idx / 4][light_idx % 4] = 1;
        lights.light_positions[light_idx] = { pl.position.X, pl.position.Y, pl.position.Z, 0.0f };
        lights.light_directions[light_idx] = { 0.0f, 0.0f, 0.0f, 0.0f };
        lights.light_colors[light_idx] = { pl.color.X, pl.color.Y, pl.color.Z, pl.intensity };
        lights.light_att_params[light_idx] = { pl.radius, 0.0f, 0.0f, 0.0f };
        light_idx++;
    }

    for (const auto& sl : state.spot_lights) {
        if (light_idx >= 50) break;
        lights.light_types_packed[light_idx / 4][light_idx % 4] = 2;
        lights.light_positions[light_idx] = { sl.position.X, sl.position.Y, sl.position.Z, 0.0f };
        lights.light_directions[light_idx] = { sl.direction.X, sl.direction.Y, sl.direction.Z, 0.0f };
        lights.light_colors[light_idx] = { sl.color.X, sl.color.Y, sl.color.Z, sl.intensity };
        lights.light_att_params[light_idx] = { 0.0f, sl.inner_cone_angle, sl.outer_cone_angle, 0.0f };
        light_idx++;
    }

    lights.light_amount = light_idx;
    lights.ambient_color = {state.ambient_light.X, state.ambient_light.Y, state.ambient_light.Y, 0.0f};

    for (auto &g : groups) {
        Mesh* mesh = g.mesh;
        if (!mesh || g.items.empty()) continue;

        sg_buffer vb = mesh->vertex_buffer;
        sg_buffer ib = mesh->index_buffer;
        if (vb.id == SG_INVALID_ID || ib.id == SG_INVALID_ID) continue;

        Material* mat = mesh->material;
        if (!g.views_created && mat) {
            g.diffuse_view = { .id = SG_INVALID_ID };
            g.specular_view = { .id = SG_INVALID_ID };

            if (mat->has_diffuse_texture && mat->diffuse_image.id != SG_INVALID_ID) {
                sg_view_desc diffuse_view_desc = {};
                diffuse_view_desc.texture.image = mat->diffuse_image;
                g.diffuse_view = sg_make_view(&diffuse_view_desc);
            }

            if (mat->has_specular_texture && mat->specular_image.id != SG_INVALID_ID) {
                sg_view_desc specular_view_desc = {};
                specular_view_desc.texture.image = mat->specular_image;
                g.specular_view = sg_make_view(&specular_view_desc);
            }
            g.views_created = true;
        }

        if (mesh->material->has_custom_shader) {
            sg_apply_pipeline(mesh->material->custom_pipeline);
        } else {
            sg_apply_pipeline(state.pip);
        }

        state.bind.vertex_buffers[0] = vb;
        state.bind.index_buffer = ib;

        for (const auto& inst : g.items) {
            const Object& obj = inst.obj;
            // in case of different textures this should be in the loop (although that'll basically never happen)
            if (g.diffuse_view.id != SG_INVALID_ID) {
                state.bind.views[0] = g.diffuse_view;
                state.bind.samplers[0] = mat->diffuse_sampler;
            } else {
                state.bind.views[0] = { .id = SG_INVALID_ID };
                state.bind.samplers[0] = { .id = SG_INVALID_ID };
            }

            if (g.specular_view.id != SG_INVALID_ID) {
                state.bind.views[1] = g.specular_view;
                state.bind.samplers[1] = mat->specular_sampler;
            } else {
                state.bind.views[1] = { .id = SG_INVALID_ID };
                state.bind.samplers[1] = { .id = SG_INVALID_ID };
            }
            sg_apply_bindings(&state.bind);

            HMM_Mat4 translate_mat = HMM_Translate(obj.position);
            HMM_Mat4 rot_mat = HMM_QToM4(obj.rotation);
            HMM_Mat4 scale_mat = HMM_Scale(obj.scale);
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));

            vs_params.model = model;
            vs_params.opacity = obj.opacity * inst.group_opacity;
            vs_params.enable_shading = mesh->enable_shading ? 1 : 0;
            sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

            struct model_fs_params_t {
                int has_diffuse_tex;
                int has_specular_tex;
                float specular;
                float shininess;
                float camera_pos_x;
                float camera_pos_y;
                float camera_pos_z;
                float camera_pos_w;
            } model_fs_params;

            model_fs_params.has_diffuse_tex = (mat && mat->has_diffuse_texture) ? 1 : 0;
            model_fs_params.has_specular_tex = (mat && mat->has_specular_texture) ? 1 : 0;
            model_fs_params.specular = (mat) ? mat->specular : 0.0f;
            model_fs_params.shininess = 32.0f;
            model_fs_params.camera_pos_x = state.camera_pos.X;
            model_fs_params.camera_pos_y = state.camera_pos.Y;
            model_fs_params.camera_pos_z = state.camera_pos.Z;
            model_fs_params.camera_pos_w = 0.0f;
            sg_apply_uniforms(2, SG_RANGE(model_fs_params));

            sg_apply_uniforms(3, SG_RANGE(lights));

            sg_draw(0, mesh->index_count, 1);

            all_vertex_count += mesh->vertex_count;
            all_index_count += mesh->index_count;
        }
    }

    for (auto &g : groups) {
        if (g.views_created) {
            if (g.diffuse_view.id != SG_INVALID_ID) {
                sg_destroy_view(g.diffuse_view);
            }
            if (g.specular_view.id != SG_INVALID_ID) {
                sg_destroy_view(g.specular_view);
            }
        }
    }
}

void render_state_surf() {
    if (state.window_surface.pixels.empty() || state.window_surface.pixels[0].empty()) {
        cout << "the window surface is empty which I don't know how you did like what the fuck man?" << endl;
        return;
    }

    sg_apply_pipeline(state.surf_pipeline);

    int height = state.window_surface.pixels.size();
    int width = state.window_surface.pixels[0].size();

    sg_image_desc surf_image{};
    surf_image.width = width;
    surf_image.height = height;
    surf_image.pixel_format = SG_PIXELFORMAT_RGBA8;
    surf_image.usage.immutable = true;
    surf_image.data = state.window_surface.get_sokol_image_data();
    sg_image surf_img = sg_make_image(&surf_image);

    sg_sampler_desc sampler_desc{};
    sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sampler_desc.min_filter = SG_FILTER_LINEAR;
    sampler_desc.mag_filter = SG_FILTER_LINEAR;
    sampler_desc.label = "surf_sampler";
    sg_sampler surf_sampler = sg_make_sampler(&sampler_desc);

    sg_view_desc surf_view_desc{};
    surf_view_desc.texture.image = surf_img;
    surf_view_desc.label = "surf_view";
    sg_view surf_view = sg_make_view(&surf_view_desc);

    state.bind.vertex_buffers[0] = {.id = SG_INVALID_ID};
    state.bind.views[0] = surf_view;
    state.bind.samplers[0] = surf_sampler;
    sg_apply_bindings(&state.bind);

    sg_draw(0, 3, 1);

    sg_destroy_view(surf_view);
    sg_destroy_sampler(surf_sampler);
    sg_destroy_image(surf_img);
}

void render_first_pass() {
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);

    if (post_state.color_img.width != w_width || post_state.color_img.height != w_height) {
        if (post_state.rendered_color_img.id != SG_INVALID_ID) {
            sg_destroy_view(post_state.rendered_color_att_view);
            sg_destroy_image(post_state.rendered_color_img);
        }
        if (post_state.rendered_depth_img.id != SG_INVALID_ID) {
            sg_destroy_view(post_state.rendered_depth_att_view);
            sg_destroy_image(post_state.rendered_depth_img);
        }
        if (post_state.rendered_color_tex_view.id != SG_INVALID_ID) {
            sg_destroy_view(post_state.rendered_color_tex_view);
        }
        if (post_state.rendered_depth_tex_view.id != SG_INVALID_ID) {
            sg_destroy_view(post_state.rendered_depth_tex_view);
        }

        post_state.color_img.width = w_width;
        post_state.color_img.height = w_height;
        post_state.depth_img.width = w_width;
        post_state.depth_img.height = w_height;
        post_state.color_img.usage.color_attachment = true;
        post_state.depth_img.usage.depth_stencil_attachment = true;

        post_state.rendered_color_img = sg_make_image(&post_state.color_img);
        post_state.rendered_depth_img = sg_make_image(&post_state.depth_img);

        sg_view_desc color_att_view_desc = {};
        color_att_view_desc.color_attachment.image = post_state.rendered_color_img;
        post_state.rendered_color_att_view = sg_make_view(&color_att_view_desc);

        sg_view_desc depth_att_view_desc = {};
        depth_att_view_desc.depth_stencil_attachment.image = post_state.rendered_depth_img;
        post_state.rendered_depth_att_view = sg_make_view(&depth_att_view_desc);

        sg_view_desc color_tex_view_desc = {};
        color_tex_view_desc.texture.image = post_state.rendered_color_img;
        post_state.rendered_color_tex_view = sg_make_view(&color_tex_view_desc);

        sg_view_desc depth_tex_view_desc = {};
        depth_tex_view_desc.texture.image = post_state.rendered_depth_img;
        post_state.rendered_depth_tex_view = sg_make_view(&depth_tex_view_desc);
    } else if (post_state.rendered_color_img.id == SG_INVALID_ID) {
        post_state.rendered_color_img = sg_make_image(&post_state.color_img);
        post_state.rendered_depth_img = sg_make_image(&post_state.depth_img);

        sg_view_desc color_att_view_desc2 = {};
        color_att_view_desc2.color_attachment.image = post_state.rendered_color_img;
        post_state.rendered_color_att_view = sg_make_view(&color_att_view_desc2);

        sg_view_desc depth_att_view_desc2 = {};
        depth_att_view_desc2.depth_stencil_attachment.image = post_state.rendered_depth_img;
        post_state.rendered_depth_att_view = sg_make_view(&depth_att_view_desc2);

        sg_view_desc color_tex_view_desc2 = {};
        color_tex_view_desc2.texture.image = post_state.rendered_color_img;
        post_state.rendered_color_tex_view = sg_make_view(&color_tex_view_desc2);

        sg_view_desc depth_tex_view_desc2 = {};
        depth_tex_view_desc2.texture.image = post_state.rendered_depth_img;
        post_state.rendered_depth_tex_view = sg_make_view(&depth_tex_view_desc2);
    }

    if (post_state.rendered_color_img.id == SG_INVALID_ID || post_state.rendered_depth_img.id == SG_INVALID_ID) {
        cout << "Could create offscreen image, sorry" << endl;
        return;
    }

    sg_pass_action offscreen_pass_action = {};
    offscreen_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    offscreen_pass_action.colors[0].clear_value = {state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f};
    offscreen_pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    offscreen_pass_action.depth.clear_value = 1.0f;

    sg_pass pass = {};
    pass.action = offscreen_pass_action;
    pass.attachments.colors[0] = post_state.rendered_color_att_view;
    pass.attachments.depth_stencil = post_state.rendered_depth_att_view;
    pass.label = "offscreen-pass";
    sg_begin_pass(&pass);

    render_meshes();

    _draw_all_billboards(state.camera_pos);

    for (auto& psys : state.particle_systems) {
        psys.draw_particles(particle_pipeline, time_state.dt, vs_params.projection, vs_params.view, state.camera_pos);
    }

    sg_end_pass();
}

void render_second_pass() {
    if (post_state.rendered_color_img.id == SG_INVALID_ID) {
        printf("ERROR: No valid color image from first pass!\n");
        return;
    }

    sg_pass_action swapchain_pass_action = {};
    swapchain_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    swapchain_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);

    sg_swapchain swapchain = {};
    swapchain.width = w_width;
    swapchain.height = w_height;
    swapchain.sample_count = 1;
    swapchain.color_format = SG_PIXELFORMAT_RGBA8;
    swapchain.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    swapchain.gl.framebuffer = 0;

    sg_pass pass = {};
    pass.action = swapchain_pass_action;
    pass.swapchain = swapchain;
    pass.label = "swapchain-pass";
    sg_begin_pass(&pass);

    sg_apply_pipeline(post_state.post_pipeline);

    post_state.uniforms.time = (float)stm_sec(stm_now());

    post_state.post_bindings.vertex_buffers[0] = {.id = SG_INVALID_ID};
    post_state.post_bindings.views[0] = post_state.rendered_color_tex_view;
    post_state.post_bindings.samplers[0] = post_state.rendered_post_sampler;
    post_state.post_bindings.views[1] = post_state.rendered_depth_tex_view;
    post_state.post_bindings.samplers[1] = post_state.rendered_depth_sampler;

    sg_apply_bindings(&post_state.post_bindings);
    sg_apply_uniforms(2, SG_RANGE(post_state.uniforms));
    sg_apply_uniforms(3, SG_RANGE(ssao_params));

    sg_draw(0, 3, 1);

    render_state_surf();

    render_editor();

    sg_end_pass();
}

// using the Möller-Trumbore by the way
bool ray_triangle_intersect(const HMM_Vec3& ray_origin, const HMM_Vec3& ray_direction, const HMM_Vec3& v0, const HMM_Vec3& v1, const HMM_Vec3& v2, float& distance) {
    const float EPSILON = 0.0000001f;

    HMM_Vec3 edge1 = HMM_SubV3(v1, v0);
    HMM_Vec3 edge2 = HMM_SubV3(v2, v0);
    HMM_Vec3 h = HMM_Cross(ray_direction, edge2);
    float a = HMM_DotV3(edge1, h);

    if (a > -EPSILON && a < EPSILON) {
        return false;
    }

    float f = 1.0f / a;
    HMM_Vec3 s = HMM_SubV3(ray_origin, v0);
    float u = f * HMM_DotV3(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    HMM_Vec3 q = HMM_Cross(s, edge1);
    float v = f * HMM_DotV3(ray_direction, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = f * HMM_DotV3(edge2, q);

    if (t > EPSILON) {
        distance = t;
        return true;
    }

    return false;
}

// helper function for raycasting
HMM_Vec3 transform_vertex(const HMM_Vec3& vertex, const Object& obj) {
    HMM_Mat4 scale_mat = HMM_Scale(obj.scale);
    HMM_Mat4 rot_mat = HMM_QToM4(obj.rotation);
    HMM_Mat4 translate_mat = HMM_Translate(obj.position);
    HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));

    HMM_Vec4 transformed = HMM_MulM4V4(model, HMM_V4V(vertex, 1.0f));
    return HMM_V3(transformed.X, transformed.Y, transformed.Z);
}

struct RaycastResult {
    bool hit;
    HMM_Vec3 point;
    const Object* obj;
};

RaycastResult raycast_from_screen(float screen_x, float screen_y) {
    int window_width, window_height;
    SDL_GetWindowSize(state.win, &window_width, &window_height);
    float ndc_x = (2.0f * screen_x) / window_width - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y) / window_height;

    HMM_Mat4 view = HMM_LookAt_RH(state.camera_pos, HMM_AddV3(state.camera_pos, state.camera_front), state.camera_up);
    HMM_Mat4 projection = HMM_Perspective_RH_NO(state.fov * (HMM_PI32 / 180.0f), (float)window_width / (float)window_height, 0.1f, 100.0f);
    HMM_Mat4 view_proj = HMM_MulM4(projection, view);
    HMM_Mat4 inv_view_proj = HMM_InvGeneralM4(view_proj);
    HMM_Vec4 near_point_ndc = HMM_V4(ndc_x, ndc_y, -1.0f, 1.0f);
    HMM_Vec4 far_point_ndc = HMM_V4(ndc_x, ndc_y, 1.0f, 1.0f);
    HMM_Vec4 near_point_world = HMM_MulM4V4(inv_view_proj, near_point_ndc);
    HMM_Vec4 far_point_world = HMM_MulM4V4(inv_view_proj, far_point_ndc);
    near_point_world = HMM_DivV4F(near_point_world, near_point_world.W);
    far_point_world = HMM_DivV4F(far_point_world, far_point_world.W);

    HMM_Vec3 ray_origin = HMM_V3(near_point_world.X, near_point_world.Y, near_point_world.Z);
    HMM_Vec3 ray_end = HMM_V3(far_point_world.X, far_point_world.Y, far_point_world.Z);
    HMM_Vec3 ray_direction = HMM_NormV3(HMM_SubV3(ray_end, ray_origin));

    float closest_distance = FLT_MAX;
    HMM_Vec3 closest_point = HMM_V3(0, 0, 0);
    const Object* hit_obj = nullptr;
    bool hit_found = false;

    for (const auto& visgroup : vis_groups) {
        if (!visgroup.enabled) continue;

        for (const auto& obj : visgroup.objects) {
            Mesh* mesh = obj.mesh;
            if (!mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count == 0) {
                continue;
            }

            for (size_t i = 0; i < mesh->index_count; i += 3) {
                if (i + 2 >= mesh->index_count) break;

                uint32_t idx0, idx1, idx2;
                if (mesh->use_uint16_indices && mesh->indices16) {
                    idx0 = mesh->indices16[i];
                    idx1 = mesh->indices16[i + 1];
                    idx2 = mesh->indices16[i + 2];
                } else {
                    idx0 = mesh->indices[i];
                    idx1 = mesh->indices[i + 1];
                    idx2 = mesh->indices[i + 2];
                }

                if (idx0 >= mesh->vertex_count || idx1 >= mesh->vertex_count || idx2 >= mesh->vertex_count) {
                    continue;
                }

                HMM_Vec3 v0 = HMM_V3(mesh->vertices[idx0 * 8], mesh->vertices[idx0 * 8 + 1], mesh->vertices[idx0 * 8 + 2]);
                HMM_Vec3 v1 = HMM_V3(mesh->vertices[idx1 * 8], mesh->vertices[idx1 * 8 + 1], mesh->vertices[idx1 * 8 + 2]);
                HMM_Vec3 v2 = HMM_V3(mesh->vertices[idx2 * 8], mesh->vertices[idx2 * 8 + 1], mesh->vertices[idx2 * 8 + 2]);

                v0 = transform_vertex(v0, obj);
                v1 = transform_vertex(v1, obj);
                v2 = transform_vertex(v2, obj);

                float distance;
                if (ray_triangle_intersect(ray_origin, ray_direction, v0, v1, v2, distance)) {
                    if (distance < closest_distance) {
                        closest_distance = distance;
                        closest_point = HMM_AddV3(ray_origin, HMM_MulV3F(ray_direction, distance));
                        hit_obj = &obj;
                        hit_found = true;
                    }
                }
            }
        }
    }

    if (hit_found) {
        std::cout << "raycast at: (" << closest_point.X << ", " << closest_point.Y << ", " << closest_point.Z << ")" << std::endl;
        return {true, closest_point, hit_obj};
    } else {
        std::cout << "raycast missed" << std::endl;
        return {false, HMM_V3(0, 0, 0), nullptr};
    }
}

// helper function for gltf loading
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

vector<Object> load_gltf(const std::string& filename) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    bool res = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    vector<Object> objects;

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!err.empty()) std::cout << "ERR: " << err << std::endl;
    if (!res) {
        std::cout << "Failed to load GLTF: " << filename << std::endl;
        return vector<Object>();
    }

    auto loadTexture = [&](int textureIndex) -> std::pair<uint8_t*, sg_image_desc> {
        cout << "loading texture" << endl;
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

        auto flip_tex_vertically_local = [&](unsigned char* data, int width, int height, int channels) -> void {
            cout << "flipping texture vertically" << endl;
            int row_size = width * channels;
            unsigned char* temp_row = new unsigned char[row_size];
            for (int y = 0; y < height / 2; ++y) {
                unsigned char* top_row = data + y * row_size;
                unsigned char* bottom_row = data + (height - 1 - y) * row_size;
                memcpy(temp_row, top_row, row_size);
                memcpy(top_row, bottom_row, row_size);
                memcpy(bottom_row, temp_row, row_size);
            }
            delete[] temp_row;
        };

        auto expand_to_rgba = [&](const unsigned char* src, int w, int h, int channels)->std::pair<uint8_t*, size_t> {
            cout << "expanding to rgba" << endl;
            const size_t out_pixels = (size_t)w * (size_t)h;
            size_t out_size = out_pixels * 4;
            uint8_t* out = new uint8_t[out_size];
            for (size_t i = 0; i < out_pixels; ++i) {
                const unsigned char* s = src + i * channels;
                uint8_t r = (channels >= 1) ? s[0] : 0;
                uint8_t g = (channels >= 2) ? s[1] : 0;
                uint8_t b = (channels >= 3) ? s[2] : 0;
                uint8_t a = (channels == 4) ? s[3] : 255;
                uint8_t* d = out + i * 4;
                d[0] = r; d[1] = g; d[2] = b; d[3] = a;
            }
            return {out, out_size};
        };

        if (!image.image.empty()) {
            int channels = image.component > 0 ? image.component : 4;
            int w = image.width;
            int h = image.height;
            if (w > 0 && h > 0) {
                if (channels == 4) {
                    size_t data_size = (size_t)w * (size_t)h * 4;
                    texture_data = new uint8_t[data_size];
                    memcpy(texture_data, image.image.data(), data_size);
                    flip_tex_vertically_local(texture_data, w, h, 4);
                    img_desc.width = w;
                    img_desc.height = h;
                    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                    img_desc.data.subimage[0][0].ptr = texture_data;
                    img_desc.data.subimage[0][0].size = data_size;
                    cout << "set 4 channel texture data" << endl;
                } else {
                    const unsigned char* src = image.image.data();
                    auto [expanded, expanded_size] = expand_to_rgba(src, w, h, channels);
                    flip_tex_vertically_local(expanded, w, h, 4);
                    texture_data = expanded;
                    img_desc.width = w;
                    img_desc.height = h;
                    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                    img_desc.data.subimage[0][0].ptr = texture_data;
                    img_desc.data.subimage[0][0].size = expanded_size;
                    cout << "setting other channel number data?" << endl;
                }
            } else {
                return {nullptr, {}};
            }
        } else if (!image.uri.empty()) {
            std::string base_dir = filename.substr(0, filename.find_last_of("/\\") + 1);
            std::string full_path = base_dir + image.uri;

            cout << "loading texture from path" << endl;

            int img_width = 0, img_height = 0, num_channels = 0;
            const int desired_channels = 4;
            stbi_uc* pixels = stbi_load(full_path.c_str(), &img_width, &img_height, &num_channels, desired_channels);
            if (pixels) {
                img_desc.width = img_width;
                img_desc.height = img_height;
                img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                size_t data_size = (size_t)img_width * (size_t)img_height * desired_channels;
                texture_data = new uint8_t[data_size];
                memcpy(texture_data, pixels, data_size);
                flip_tex_vertically_local(texture_data, img_width, img_height, desired_channels);
                img_desc.data.subimage[0][0].ptr = texture_data;
                img_desc.data.subimage[0][0].size = data_size;
                stbi_image_free(pixels);
            }
        } else if (image.bufferView >= 0) {
            const auto& bufferView = model.bufferViews[image.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            cout << "something to do with bufferviews idk" << endl;

            const uint8_t* data = buffer.data.data() + bufferView.byteOffset;
            size_t data_size = bufferView.byteLength;

            int img_width = 0, img_height = 0, num_channels = 0;
            const int desired_channels = 4;
            stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(data_size),
                                                  &img_width, &img_height, &num_channels, desired_channels);

            if (pixels) {
                img_desc.width = img_width;
                img_desc.height = img_height;
                img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;

                size_t pixel_data_size = (size_t)img_width * (size_t)img_height * desired_channels;
                texture_data = new uint8_t[pixel_data_size];
                memcpy(texture_data, pixels, pixel_data_size);
                flip_tex_vertically_local(texture_data, img_width, img_height, desired_channels);
                img_desc.data.subimage[0][0].ptr = texture_data;
                img_desc.data.subimage[0][0].size = pixel_data_size;

                stbi_image_free(pixels);
            }
        }

        cout << "returning texture data" << endl;
        return {texture_data, img_desc};
    };

    const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
    for (int node_idx : scene.nodes) {
        const tinygltf::Node& node = model.nodes[node_idx];

        if (node.mesh < 0) continue;

        const tinygltf::Mesh& gltf_mesh = model.meshes[node.mesh];
        const tinygltf::Primitive& primitive = gltf_mesh.primitives[0];

        size_t vcount = 0;
        float* base_vertices = nullptr;
        uint32_t* base_indices = nullptr;
        size_t icount = 0;

        auto pos_it = primitive.attributes.find("POSITION");
        if (pos_it == primitive.attributes.end()) continue;
        const tinygltf::Accessor& pos_acc = model.accessors[pos_it->second];
        vcount = pos_acc.count;
        const tinygltf::BufferView& pos_bv = model.bufferViews[pos_acc.bufferView];
        const tinygltf::Buffer& pos_buf = model.buffers[pos_bv.buffer];
        const unsigned char* pos_data = pos_buf.data.data() + pos_acc.byteOffset + pos_bv.byteOffset;
        size_t pos_stride = pos_bv.byteStride ? pos_bv.byteStride : 12;  // vec3 float

        auto norm_it = primitive.attributes.find("NORMAL");
        const unsigned char* norm_data = nullptr;
        size_t norm_stride = 12;
        if (norm_it != primitive.attributes.end()) {
            const tinygltf::Accessor& norm_acc = model.accessors[norm_it->second];
            const tinygltf::BufferView& norm_bv = model.bufferViews[norm_acc.bufferView];
            const tinygltf::Buffer& norm_buf = model.buffers[norm_bv.buffer];
            norm_data = norm_buf.data.data() + norm_acc.byteOffset + norm_bv.byteOffset;
            norm_stride = norm_bv.byteStride ? norm_bv.byteStride : 12;
        }

        auto tex_it = primitive.attributes.find("TEXCOORD_0");
        const unsigned char* tex_data = nullptr;
        size_t tex_stride = 8;
        if (tex_it != primitive.attributes.end()) {
            const tinygltf::Accessor& tex_acc = model.accessors[tex_it->second];
            const tinygltf::BufferView& tex_bv = model.bufferViews[tex_acc.bufferView];
            const tinygltf::Buffer& tex_buf = model.buffers[tex_bv.buffer];
            tex_data = tex_buf.data.data() + tex_acc.byteOffset + tex_bv.byteOffset;
            tex_stride = tex_bv.byteStride ? tex_bv.byteStride : 8;  // vec2 float
        }

        base_vertices = new float[vcount * 8];
        for (size_t v = 0; v < vcount; ++v) {
            const float* pos = reinterpret_cast<const float*>(pos_data + v * pos_stride);
            base_vertices[v * 8 + 0] = pos[0];
            base_vertices[v * 8 + 1] = pos[1];
            base_vertices[v * 8 + 2] = pos[2];

            if (norm_data) {
                const float* norm = reinterpret_cast<const float*>(norm_data + v * norm_stride);
                base_vertices[v * 8 + 3] = norm[0];
                base_vertices[v * 8 + 4] = norm[1];
                base_vertices[v * 8 + 5] = norm[2];
            } else {
                base_vertices[v * 8 + 3] = 0.0f;
                base_vertices[v * 8 + 4] = 1.0f;
                base_vertices[v * 8 + 5] = 0.0f;
            }

            if (tex_data) {
                const float* tex = reinterpret_cast<const float*>(tex_data + v * tex_stride);
                base_vertices[v * 8 + 6] = tex[0];
                base_vertices[v * 8 + 7] = tex[1];
            } else {
                base_vertices[v * 8 + 6] = 0.0f;
                base_vertices[v * 8 + 7] = 0.0f;
            }
        }

        if (primitive.indices >= 0) {
            const tinygltf::Accessor& ind_acc = model.accessors[primitive.indices];
            icount = ind_acc.count;
            const tinygltf::BufferView& ind_bv = model.bufferViews[ind_acc.bufferView];
            const tinygltf::Buffer& ind_buf = model.buffers[ind_bv.buffer];
            const unsigned char* ind_data = ind_buf.data.data() + ind_acc.byteOffset + ind_bv.byteOffset;
            size_t ind_stride = ind_bv.byteStride ? ind_bv.byteStride : (ind_acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 4);

            base_indices = new uint32_t[icount];
            if (ind_acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                for (size_t j = 0; j < icount; ++j) {
                    base_indices[j] = *reinterpret_cast<const uint16_t*>(ind_data + j * ind_stride);
                }
            } else if (ind_acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                for (size_t j = 0; j < icount; ++j) {
                    base_indices[j] = *reinterpret_cast<const uint32_t*>(ind_data + j * ind_stride);
                }
            } else {
                delete[] base_vertices;
                continue;
            }
        } else {
            icount = vcount;
            base_indices = new uint32_t[icount];
            for (size_t j = 0; j < icount; ++j) base_indices[j] = static_cast<uint32_t>(j);
        }

        Mesh* base_mesh = new Mesh();
        base_mesh->vertices = base_vertices;
        base_mesh->vertex_count = vcount;
        base_mesh->indices = base_indices;
        base_mesh->index_count = icount;

        Material* mat = new Material();
        if (primitive.material >= 0 && primitive.material < model.materials.size()) {
            const auto& material = model.materials[primitive.material];

            if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                auto [texture_data, img_desc] = loadTexture(material.pbrMetallicRoughness.baseColorTexture.index);
                cout << material.pbrMetallicRoughness.baseColorTexture.index << endl;
                uint8_t* specular_texture_data;
                sg_image_desc specular_img_desc;
                if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
                    auto [u_specular_texture_data, u_specular_img_desc] = loadTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
                    specular_texture_data = u_specular_texture_data;
                    specular_img_desc = u_specular_img_desc;
                }

                if (texture_data && img_desc.width > 0) {
                    mat->diffuse_texture_data = texture_data;
                    mat->diffuse_texture_desc = img_desc;
                    mat->diffuse_texture_data_size = img_desc.data.subimage[0][0].size;
                    mat->has_diffuse_texture = true;

                    mat->diffuse_sampler_desc.min_filter = SG_FILTER_LINEAR;
                    mat->diffuse_sampler_desc.mag_filter = SG_FILTER_LINEAR;
                    mat->diffuse_sampler_desc.wrap_u = SG_WRAP_REPEAT;
                    mat->diffuse_sampler_desc.wrap_v = SG_WRAP_REPEAT;
                }
                if (specular_texture_data && specular_img_desc.width > 0) {
                    mat->specular_texture_data = specular_texture_data;
                    mat->specular_texture_desc = specular_img_desc;
                    mat->specular_texture_data_size = specular_img_desc.data.subimage[0][0].size;
                    mat->has_specular_texture = true;

                    mat->specular_sampler_desc.min_filter = SG_FILTER_LINEAR;
                    mat->specular_sampler_desc.mag_filter = SG_FILTER_LINEAR;
                    mat->specular_sampler_desc.wrap_u = SG_WRAP_REPEAT;
                    mat->specular_sampler_desc.wrap_v = SG_WRAP_REPEAT;
                }
            }
        }

        base_mesh->material = mat;

        Object obj;
        obj.mesh = nullptr;
        obj.position = node.translation.size() == 3 ? HMM_V3(static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]), static_cast<float>(node.translation[2])) : HMM_V3(0, 0, 0);
        obj.scale = node.scale.size() == 3 ? HMM_V3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2])) : HMM_V3(1, 1, 1);
        obj.rotation = node.rotation.size() == 4 ? HMM_Q(static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]), static_cast<float>(node.rotation[3])) : HMM_Q(0, 0, 0, 1);

        obj.shape_keys.push_back(base_mesh);

        for (size_t target_idx = 0; target_idx < primitive.targets.size(); ++target_idx) {
            const std::map<std::string, int>& target = primitive.targets[target_idx];

            float* morphed_vertices = new float[vcount * 8];
            memcpy(morphed_vertices, base_vertices, vcount * 8 * sizeof(float));

            auto dpos_it = target.find("POSITION");
            if (dpos_it != target.end()) {
                const tinygltf::Accessor& dpos_acc = model.accessors[dpos_it->second];
                const tinygltf::BufferView& dpos_bv = model.bufferViews[dpos_acc.bufferView];
                const tinygltf::Buffer& dpos_buf = model.buffers[dpos_bv.buffer];
                const unsigned char* dpos_data = dpos_buf.data.data() + dpos_acc.byteOffset + dpos_bv.byteOffset;
                size_t dpos_stride = dpos_bv.byteStride ? dpos_bv.byteStride : 12;
                for (size_t v = 0; v < vcount; ++v) {
                    const float* dpos = reinterpret_cast<const float*>(dpos_data + v * dpos_stride);
                    morphed_vertices[v * 8 + 0] += dpos[0];
                    morphed_vertices[v * 8 + 1] += dpos[1];
                    morphed_vertices[v * 8 + 2] += dpos[2];
                }
            }

            auto dnorm_it = target.find("NORMAL");
            if (dnorm_it != target.end()) {
                const tinygltf::Accessor& dnorm_acc = model.accessors[dnorm_it->second];
                const tinygltf::BufferView& dnorm_bv = model.bufferViews[dnorm_acc.bufferView];
                const tinygltf::Buffer& dnorm_buf = model.buffers[dnorm_bv.buffer];
                const unsigned char* dnorm_data = dnorm_buf.data.data() + dnorm_acc.byteOffset + dnorm_bv.byteOffset;
                size_t dnorm_stride = dnorm_bv.byteStride ? dnorm_bv.byteStride : 12;
                for (size_t v = 0; v < vcount; ++v) {
                    const float* dnorm = reinterpret_cast<const float*>(dnorm_data + v * dnorm_stride);
                    morphed_vertices[v * 8 + 3] += dnorm[0];
                    morphed_vertices[v * 8 + 4] += dnorm[1];
                    morphed_vertices[v * 8 + 5] += dnorm[2];
                    HMM_Vec3 norm = HMM_V3(morphed_vertices[v * 8 + 3], morphed_vertices[v * 8 + 4], morphed_vertices[v * 8 + 5]);
                    if (HMM_LenV3(norm) > 0.0f) norm = HMM_NormV3(norm);
                    morphed_vertices[v * 8 + 3] = norm.X;
                    morphed_vertices[v * 8 + 4] = norm.Y;
                    morphed_vertices[v * 8 + 5] = norm.Z;
                }
            }

            Mesh* morph_mesh = new Mesh();
            morph_mesh->vertices = morphed_vertices;
            morph_mesh->vertex_count = vcount;
            morph_mesh->indices = new uint32_t[icount];
            memcpy(morph_mesh->indices, base_indices, icount * sizeof(uint32_t));
            morph_mesh->index_count = icount;
            morph_mesh->material = mat;

            obj.shape_keys.push_back(morph_mesh);
        }

        obj.select_shape_key(0);

        prepare_mesh_buffers(obj);
        objects.push_back(obj);
    }
    return objects;
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
        Object visualizer_object;
        int visualizer_obj_index = -1;
        FMOD_GUID guid;
        int script_id = -1;

        void initialize(FMOD::Studio::EventDescription* desc, HMM_Vec3 pos) {
            FMOD_RESULT result = desc->createInstance(&event_instance);
            print_fmod_error(result);
            state.audio_sources.push_back(this);
            position = pos;

            auto loaded = load_gltf("speaker.glb");
            if (!loaded.empty()) {
                visualizer_objects.push_back(loaded[0]);
                visualizer_obj_index = visualizer_objects.size() - 1;
                Object vo = visualizer_objects[visualizer_obj_index];
                vo.mesh->enable_shading = false;
                visualizer_object = vo;
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
            if (visualizer_obj_index >= 0 && visualizer_obj_index < visualizer_objects.size()) {
                visualizer_objects[visualizer_obj_index].position = position;
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

            if (visualizer_obj_index >= 0 && visualizer_obj_index < visualizer_objects.size()) {
                delete visualizer_objects[visualizer_obj_index].mesh;
                visualizer_objects.erase(visualizer_objects.begin() + visualizer_obj_index);
                for (auto* as : state.audio_sources) {
                    if (as->visualizer_obj_index > visualizer_obj_index) {
                        as->visualizer_obj_index--;
                    }
                }
            }

            event_instance->release();
        }
};

class Helper {
public:
    HMM_Vec3 position;
    string name;
    Object visualizer_obj;
    int visualizer_obj_index = -1;
    bool operator==(const Helper& other) const { return this == &other; }

    void initialize(const string& name, HMM_Vec3 pos) {
        this->name = name;
        position = pos;

        auto loaded = load_gltf("helper.glb");
        if (!loaded.empty()) {
            visualizer_objects.push_back(loaded[0]);
            visualizer_obj_index = visualizer_objects.size() - 1;
            Object vo = visualizer_objects[visualizer_obj_index];
            vo.mesh->enable_shading = false;
            visualizer_obj = vo;
        }

        state.helpers.push_back(this);
    }


    void update_visualizer_position() {
        if (visualizer_obj_index >= 0 && visualizer_obj_index < visualizer_objects.size()) {
            visualizer_objects[visualizer_obj_index].position = position;
        }
    }

    void set_position(HMM_Vec3 pos) {
        position = pos;
        update_visualizer_position();
    }

    void remove() {
        state.helpers.erase(std::remove(state.helpers.begin(), state.helpers.end(), this),state.helpers.end());

        if (visualizer_obj_index >= 0 && visualizer_obj_index < visualizer_objects.size()) {
            delete visualizer_objects[visualizer_obj_index].mesh;
            visualizer_objects.erase(visualizer_objects.begin() + visualizer_obj_index);
            for (auto* hpr : state.helpers) {
                if (hpr->visualizer_obj_index > visualizer_obj_index) {
                    hpr->visualizer_obj_index--;
                }
            }
        }
    }
};

bool is_point_within_helpers(Helper* helper1, Helper* helper2, HMM_Vec3 point) {
    HMM_Vec3 pos1 = helper1->position;
    HMM_Vec3 pos2 = helper2->position;
    float min_x = HMM_MIN(pos1.X, pos2.X);
    float max_x = HMM_MAX(pos1.X, pos2.X);
    float min_y = HMM_MIN(pos1.Y, pos2.Y);
    float max_y = HMM_MAX(pos1.Y, pos2.Y);
    float min_z = HMM_MIN(pos1.Z, pos2.Z);
    float max_z = HMM_MAX(pos1.Z, pos2.Z);
    return (point.X >= min_x && point.X <= max_x) &&
    (point.Y >= min_y && point.Y <= max_y) &&
    (point.Z >= min_z && point.Z <= max_z);
}

void make_audiosource_by_index(int index) {
    AudioSource3D* audio_source = new AudioSource3D();
    audio_source->initialize(state.event_descriptions[index], {0.0f, 0.0f, 0.0f});
}

void clear_scene() {
    for (auto* as : state.audio_sources) {
        as->remove();
    }
    state.audio_sources.clear();

    for (auto& visgroup : vis_groups) {
        /*for (auto& obj : visgroup.objects) {
            obj.shape_keys.clear();
        }*/
        visgroup.objects.clear();
    }
    vis_groups.clear();

    for (auto* hpr : state.helpers) {
        hpr->remove();
    }
    state.helpers.clear();

    state.directional_lights.clear();
    state.point_lights.clear();
    state.spot_lights.clear();

    visualizer_objects.clear();
}

void save_scene(const string& path) {
    nlohmann::json j;

    j["visgroups"] = nlohmann::json::array();
    for (auto& visgroup : vis_groups) {
        nlohmann::json vg_json;
        vg_json["name"] = visgroup.name;
        vg_json["enabled"] = visgroup.enabled;
        vg_json["opacity"] = visgroup.opacity;

        vg_json["objects"] = nlohmann::json::array();
        for (const auto& obj : visgroup.objects) {
            nlohmann::json obj_json;

            obj_json["pos"] = {obj.position.X, obj.position.Y, obj.position.Z};
            obj_json["rot"] = {obj.rotation.X, obj.rotation.Y, obj.rotation.Z, obj.rotation.W};
            obj_json["scl"] = {obj.scale.X, obj.scale.Y, obj.scale.Z};
            obj_json["opacity"] = obj.opacity;

            Mesh* mesh = obj.mesh;
            if (mesh) {
                obj_json["mesh"] = nlohmann::json::object();
                auto& mesh_json = obj_json["mesh"];

                mesh_json["shading"] = mesh->enable_shading;

                if (mesh->vertex_count > 0 && mesh->vertices) {
                    mesh_json["vert_cnt"] = mesh->vertex_count;
                    std::vector<float> verts(mesh->vertices, mesh->vertices + mesh->vertex_count * 8);
                    mesh_json["verts"] = verts;
                }

                if (mesh->index_count > 0 && mesh->indices) {
                    mesh_json["idx_cnt"] = mesh->index_count;
                    std::vector<uint32_t> indices(mesh->indices, mesh->indices + mesh->index_count);
                    mesh_json["indices"] = indices;
                }

                if (mesh->material) {
                    mesh_json["mat"] = nlohmann::json::object();
                    auto& mat_json = mesh_json["mat"];

                    Material* material = mesh->material;
                    mat_json["diff"] = material->diffuse;
                    mat_json["spec"] = material->specular;

                    if (material->has_diffuse_texture && material->diffuse_texture_data && material->diffuse_texture_data_size > 0) {
                        mat_json["diff_tex"] = nlohmann::json::object();
                        auto& diff_tex = mat_json["diff_tex"];
                        diff_tex["w"] = material->diffuse_texture_desc.width;
                        diff_tex["h"] = material->diffuse_texture_desc.height;
                        diff_tex["fmt"] = static_cast<int>(material->diffuse_texture_desc.pixel_format);

                        std::vector<uint8_t> tex_data(material->diffuse_texture_data,
                                                     material->diffuse_texture_data + material->diffuse_texture_data_size);
                        diff_tex["data"] = tex_data;

                        diff_tex["samp"] = {
                            static_cast<int>(material->diffuse_sampler_desc.min_filter),
                            static_cast<int>(material->diffuse_sampler_desc.mag_filter),
                            static_cast<int>(material->diffuse_sampler_desc.wrap_u),
                            static_cast<int>(material->diffuse_sampler_desc.wrap_v)
                        };
                    }

                    if (material->has_specular_texture && material->specular_texture_data && material->specular_texture_data_size > 0) {
                        mat_json["spec_tex"] = nlohmann::json::object();
                        auto& spec_tex = mat_json["spec_tex"];
                        spec_tex["w"] = material->specular_texture_desc.width;
                        spec_tex["h"] = material->specular_texture_desc.height;
                        spec_tex["fmt"] = static_cast<int>(material->specular_texture_desc.pixel_format);

                        std::vector<uint8_t> tex_data(material->specular_texture_data,
                                                     material->specular_texture_data + material->specular_texture_data_size);
                        spec_tex["data"] = tex_data;

                        spec_tex["samp"] = {
                            static_cast<int>(material->specular_sampler_desc.min_filter),
                            static_cast<int>(material->specular_sampler_desc.mag_filter),
                            static_cast<int>(material->specular_sampler_desc.wrap_u),
                            static_cast<int>(material->specular_sampler_desc.wrap_v)
                        };
                    }
                }
            }

            obj_json["scrid"] = obj.script_id;

            vg_json["objects"].push_back(obj_json);
        }
        j["visgroups"].push_back(vg_json);
    }

    j["audio_src"] = nlohmann::json::array();
    for (const auto& as : state.audio_sources) {
        j["audio_src"].push_back({
            {as->position.X, as->position.Y, as->position.Z},
            {as->guid.Data1, as->guid.Data2, as->guid.Data3,
             std::vector<uint8_t>(as->guid.Data4, as->guid.Data4 + 8)},
            as->script_id
        });
    }

    j["helpers"] = nlohmann::json::array();
    for (const Helper* hpr : state.helpers) {
        j["helpers"].push_back({
            {hpr->position.X, hpr->position.Y, hpr->position.Z},
            hpr->name
        });
    }

    j["dir_lights"] = nlohmann::json::array();
    for (const DirectionalLight& light : state.directional_lights) {
        j["dir_lights"].push_back({
            {light.direction.X, light.direction.Y, light.direction.Z},
            light.intensity,
            {light.color.X, light.color.Y, light.color.Z},
            light.script_id
        });
    }

    j["pt_lights"] = nlohmann::json::array();
    for (const PointLight& light : state.point_lights) {
        j["pt_lights"].push_back({
            {light.position.X, light.position.Y, light.position.Z},
            light.intensity,
            {light.color.X, light.color.Y, light.color.Z},
            light.radius,
            light.script_id
        });
    }

    j["spot_lights"] = nlohmann::json::array();
    for (const SpotLight& light : state.spot_lights) {
        j["spot_lights"].push_back({
            {light.position.X, light.position.Y, light.position.Z},
            {light.direction.X, light.direction.Y, light.direction.Z},
            light.intensity,
            {light.color.X, light.color.Y, light.color.Z},
            light.inner_cone_angle,
            light.outer_cone_angle,
            light.script_id
        });
    }

    j["ambient"] = {state.ambient_light.X, state.ambient_light.Y, state.ambient_light.Z};

    std::string json_str = j.dump();

    uLongf compressed_size = compressBound(json_str.length());
    std::vector<Bytef> compressed_data(compressed_size);

    int result = compress(compressed_data.data(), &compressed_size, reinterpret_cast<const Bytef*>(json_str.c_str()), json_str.length());

    if (result == Z_OK) {
        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_size);
            file.close();
            std::cout << "Compressed scene saved to: " << path << std::endl;
            std::cout << "Original size: " << json_str.length() << " bytes" << std::endl;
            std::cout << "Compressed size: " << compressed_size << " bytes" << std::endl;
            std::cout << "Compression ratio: " << (float)compressed_size / json_str.length() * 100 << "%" << std::endl;
        }
    }
}

void load_scene(const string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open compressed file: " << path << std::endl;
        return;
    }

    std::streamsize compressed_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<Bytef> compressed_data(compressed_size);
    if (!file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_size)) {
        std::cerr << "Failed to read compressed data" << std::endl;
        return;
    }
    file.close();

    uLongf uncompressed_size = compressed_size * 1000;
    std::vector<Bytef> uncompressed_data(uncompressed_size);

    int result = uncompress(uncompressed_data.data(), &uncompressed_size, compressed_data.data(), compressed_size);

    while (result == Z_BUF_ERROR) {
        uncompressed_size *= 2;
        uncompressed_data.resize(uncompressed_size);
        result = uncompress(uncompressed_data.data(), &uncompressed_size, compressed_data.data(), compressed_size);
    }

    if (result != Z_OK) {
        std::cerr << "Decompression failed with error: " << result << std::endl;
        return;
    }

    clear_scene();

    std::string json_str(reinterpret_cast<char*>(uncompressed_data.data()), uncompressed_size);
    nlohmann::json j = nlohmann::json::parse(json_str);

    if (j.contains("visgroups")) {
        for (const auto& vg_json : j["visgroups"]) {
            vector<Object> objects;
            VisGroup new_visgroup(vg_json["name"], objects);
            new_visgroup.enabled = vg_json["enabled"];
            new_visgroup.opacity = vg_json["opacity"];

            if (vg_json.contains("objects")) {
                for (const auto& obj_json : vg_json["objects"]) {
                    Object obj;

                    if (obj_json.contains("pos")) {
                        auto pos = obj_json["pos"];
                        obj.position = HMM_V3(pos[0], pos[1], pos[2]);
                    }

                    if (obj_json.contains("rot")) {
                        auto rot = obj_json["rot"];
                        obj.rotation = HMM_Quat{rot[0], rot[1], rot[2], rot[3]};
                    }

                    if (obj_json.contains("scl")) {
                        auto scale = obj_json["scl"];
                        obj.scale = HMM_V3(scale[0], scale[1], scale[2]);
                    }

                    if (obj_json.contains("opacity")) {
                        obj.opacity = obj_json["opacity"];
                    }

                    if (obj_json.contains("scrid")) {
                        obj.script_id = obj_json["scrid"];
                    }

                    if (obj_json.contains("mesh")) {
                        const auto& mesh_json = obj_json["mesh"];
                        Mesh* mesh = new Mesh();
                        obj.mesh = mesh;

                        if (mesh_json.contains("shading")) {
                            mesh->enable_shading = mesh_json["shading"];
                        }

                        if (mesh_json.contains("vert_cnt") && mesh_json.contains("verts")) {
                            mesh->vertex_count = mesh_json["vert_cnt"];
                            mesh->vertices = new float[mesh->vertex_count * 8];

                            auto verts = mesh_json["verts"];
                            for (size_t i = 0; i < mesh->vertex_count * 8; i++) {
                                mesh->vertices[i] = verts[i];
                            }
                        }

                        if (mesh_json.contains("idx_cnt") && mesh_json.contains("indices")) {
                            mesh->index_count = mesh_json["idx_cnt"];
                            mesh->indices = new uint32_t[mesh->index_count];

                            auto indices = mesh_json["indices"];
                            for (size_t i = 0; i < mesh->index_count; i++) {
                                mesh->indices[i] = indices[i];
                            }
                        }

                        if (mesh_json.contains("mat")) {
                            const auto& mat_json = mesh_json["mat"];
                            Material* material = new Material();
                            mesh->material = material;

                            if (mat_json.contains("diff")) {
                                material->diffuse = mat_json["diff"];
                            }
                            if (mat_json.contains("spec")) {
                                material->specular = mat_json["spec"];
                            }

                            if (mat_json.contains("diff_tex")) {
                                const auto& diff_tex = mat_json["diff_tex"];
                                material->has_diffuse_texture = true;

                                material->diffuse_texture_desc.width = diff_tex["w"];
                                material->diffuse_texture_desc.height = diff_tex["h"];
                                material->diffuse_texture_desc.pixel_format = static_cast<sg_pixel_format>(diff_tex["fmt"]);

                                std::vector<uint8_t> tex_data = diff_tex["data"];
                                material->diffuse_texture_data_size = tex_data.size();
                                material->diffuse_texture_data = new uint8_t[material->diffuse_texture_data_size];
                                std::copy(tex_data.begin(), tex_data.end(), material->diffuse_texture_data);

                                material->diffuse_texture_desc.data.subimage[0][0].ptr = material->diffuse_texture_data;
                                material->diffuse_texture_desc.data.subimage[0][0].size = material->diffuse_texture_data_size;

                                auto samp = diff_tex["samp"];
                                material->diffuse_sampler_desc.min_filter = static_cast<sg_filter>(samp[0]);
                                material->diffuse_sampler_desc.mag_filter = static_cast<sg_filter>(samp[1]);
                                material->diffuse_sampler_desc.wrap_u = static_cast<sg_wrap>(samp[2]);
                                material->diffuse_sampler_desc.wrap_v = static_cast<sg_wrap>(samp[3]);
                            }

                            if (mat_json.contains("spec_tex")) {
                                const auto& spec_tex = mat_json["spec_tex"];
                                material->has_specular_texture = true;

                                material->specular_texture_desc.width = spec_tex["w"];
                                material->specular_texture_desc.height = spec_tex["h"];
                                material->specular_texture_desc.pixel_format = static_cast<sg_pixel_format>(spec_tex["fmt"]);

                                std::vector<uint8_t> tex_data = spec_tex["data"];
                                material->specular_texture_data_size = tex_data.size();
                                material->specular_texture_data = new uint8_t[material->specular_texture_data_size];
                                std::copy(tex_data.begin(), tex_data.end(), material->specular_texture_data);

                                material->specular_texture_desc.data.subimage[0][0].ptr = material->specular_texture_data;
                                material->specular_texture_desc.data.subimage[0][0].size = material->specular_texture_data_size;

                                auto samp = spec_tex["samp"];
                                material->specular_sampler_desc.min_filter = static_cast<sg_filter>(samp[0]);
                                material->specular_sampler_desc.mag_filter = static_cast<sg_filter>(samp[1]);
                                material->specular_sampler_desc.wrap_u = static_cast<sg_wrap>(samp[2]);
                                material->specular_sampler_desc.wrap_v = static_cast<sg_wrap>(samp[3]);
                            }
                        }

                        if (obj_json.contains("shape_keys")) {
                            for (const auto& sk_json : obj_json["shape_keys"]) {
                                Mesh* shape_key_mesh = new Mesh();

                                if (sk_json.contains("shading")) {
                                    shape_key_mesh->enable_shading = sk_json["shading"];
                                }

                                if (sk_json.contains("vert_cnt") && sk_json.contains("verts")) {
                                    shape_key_mesh->vertex_count = sk_json["vert_cnt"];
                                    shape_key_mesh->vertices = new float[shape_key_mesh->vertex_count * 8];

                                    auto verts = sk_json["verts"];
                                    for (size_t i = 0; i < shape_key_mesh->vertex_count * 8; i++) {
                                        shape_key_mesh->vertices[i] = verts[i];
                                    }
                                }

                                if (sk_json.contains("idx_cnt") && sk_json.contains("indices")) {
                                    shape_key_mesh->index_count = sk_json["idx_cnt"];
                                    shape_key_mesh->indices = new uint32_t[shape_key_mesh->index_count];

                                    auto indices = sk_json["indices"];
                                    for (size_t i = 0; i < shape_key_mesh->index_count; i++) {
                                        shape_key_mesh->indices[i] = indices[i];
                                    }
                                }

                                shape_key_mesh->material = obj.mesh ? obj.mesh->material : nullptr;

                                obj.shape_keys.push_back(shape_key_mesh);
                            }
                        }

                        prepare_mesh_buffers(obj);
                    }

                    new_visgroup.objects.push_back(std::move(obj));
                }
            }
            vis_groups.push_back(std::move(new_visgroup));
        }
    }

    if (j.contains("audio_src")) {
        for (const auto& as_data : j["audio_src"]) {
            AudioSource3D* audio_source = new AudioSource3D();

            auto pos = as_data[0];
            audio_source->position = HMM_V3(pos[0], pos[1], pos[2]);

            auto guid_data = as_data[1];
            FMOD_GUID guid;
            guid.Data1 = guid_data[0];
            guid.Data2 = guid_data[1];
            guid.Data3 = guid_data[2];
            std::vector<uint8_t> data4 = guid_data[3];
            for (int i = 0; i < 8; i++) {
                guid.Data4[i] = data4[i];
            }
            audio_source->guid = guid;

            auto script_id = as_data[2];
            audio_source->script_id = script_id;

            for (auto& ed : state.event_descriptions) {
                FMOD_GUID current_id;
                ed->getID(&current_id);
                if (current_id.Data1 == audio_source->guid.Data1 &&
                    current_id.Data2 == audio_source->guid.Data2 &&
                    current_id.Data3 == audio_source->guid.Data3 &&
                    memcmp(current_id.Data4, audio_source->guid.Data4, 8) == 0) {
                    audio_source->initialize(ed, audio_source->position);
                    break;
                }
            }
        }
    }

    if (j.contains("helpers")) {
        for (const auto& helper_data : j["helpers"]) {
            Helper* helper = new Helper();
            auto pos = helper_data[0];
            helper->position = HMM_V3(pos[0], pos[1], pos[2]);
            helper->name = helper_data[1];
            helper->initialize(helper->name, helper->position);
        }
    }

    if (j.contains("dir_lights")) {
        for (const auto& light_data : j["dir_lights"]) {
            DirectionalLight light{};
            auto dir = light_data[0];
            light.direction = HMM_V3(dir[0], dir[1], dir[2]);
            light.intensity = light_data[1];
            auto color = light_data[2];
            light.color = HMM_V3(color[0], color[1], color[2]);
            state.directional_lights.push_back(light);
            auto script_id = light_data[3];
            light.script_id = script_id;
        }
    }

    if (j.contains("pt_lights")) {
        for (const auto& light_data : j["pt_lights"]) {
            PointLight light{};
            auto pos = light_data[0];
            light.position = HMM_V3(pos[0], pos[1], pos[2]);
            light.intensity = light_data[1];
            auto color = light_data[2];
            light.color = HMM_V3(color[0], color[1], color[2]);
            light.radius = light_data[3];
            state.point_lights.push_back(light);
            auto script_id = light_data[4];
            light.script_id = script_id;
        }
    }

    if (j.contains("spot_lights")) {
        for (const auto& light_data : j["spot_lights"]) {
            SpotLight light{};
            auto pos = light_data[0];
            light.position = HMM_V3(pos[0], pos[1], pos[2]);
            auto dir = light_data[1];
            light.direction = HMM_V3(dir[0], dir[1], dir[2]);
            light.intensity = light_data[2];
            auto color = light_data[3];
            light.color = HMM_V3(color[0], color[1], color[2]);
            light.inner_cone_angle = light_data[4];
            light.outer_cone_angle = light_data[5];
            state.spot_lights.push_back(light);
            auto script_id = light_data[6];
            light.script_id = script_id;
        }
    }

    if (j.contains("ambient")) {
        auto ambient = j["ambient"];
        state.ambient_light = HMM_V3(ambient[0], ambient[1], ambient[2]);
    }

    std::cout << "Compressed scene loaded from: " << path << std::endl;
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

static int selected_object_index = -1;
static int selected_mesh_visgroup = -1;
static int selected_as_index = -1;
static int selected_visgroup_index = -1;
string currently_entered_path = "";
static std::map<int, HMM_Vec3> mesh_euler_rotations;
static int last_selected_mesh = -1;
static int selected_selectable_visgroup_index = -1;
static int selected_dir_light_index = -1;
static int selected_point_light_index = -1;
static int selected_spot_light_index = -1;

sg_image editor_display_image;
sg_sampler editor_display_sampler;
sg_image editor_specular_display_image;
sg_sampler editor_specular_display_sampler;

void render_editor() {
    std::vector<sg_view> temp_editor_views;
    if (state.editor_open) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("General settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::Button("SAVE SCENE")) {
            const char* filter_patterns[] = {"*.gmap"};
            const char* file_path = tinyfd_saveFileDialog("Select GMap File", "", 1, filter_patterns, "Gungutils Map Files");

            if (file_path) {
                save_scene(file_path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("LOAD SCENE")) {
            const char* filter_patterns[] = {"*.gmap"};
            const char* file_path = tinyfd_openFileDialog("Select GMap File", "", 1, filter_patterns, "Gungutils Map Files", 0);

            if (file_path) {
                load_scene(file_path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("CLEAR SCENE")) {
            clear_scene();
        }
        ImGui::SameLine();
        if (ImGui::Button("QUIT")) {
            state.running = false;
        }
        ImGui::SameLine();
        ImGui::Text("FPS: %.2f", time_state.fps);

        ImGui::Separator();
        if (ImGui::CollapsingHeader("VISGROUPS")) {
            ImGui::BeginChild("VISGROUPS", ImVec2(150, 75), true);
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
            ImGui::BeginChild("VISGROUP SETTINGS", ImVec2(150, 75), true);
            if (selected_visgroup_index >= 0 && selected_visgroup_index < vis_groups.size()) {
                static char visgroup_name_buffer[256];
                static int last_selected_visgroup = -1;
                VisGroup* selected_visgroup = &vis_groups[selected_visgroup_index];

                if (last_selected_visgroup != selected_visgroup_index) {
                    strncpy(visgroup_name_buffer, selected_visgroup->name.c_str(), sizeof(visgroup_name_buffer) - 1);
                    visgroup_name_buffer[sizeof(visgroup_name_buffer) - 1] = '\0';
                    last_selected_visgroup = selected_visgroup_index;
                }

                if (ImGui::InputText("NAME", visgroup_name_buffer, sizeof(visgroup_name_buffer))) {
                    selected_visgroup->name = string(visgroup_name_buffer);
                }
                ImGui::Checkbox("ENABLED", &selected_visgroup->enabled);
                ImGui::DragFloat("OPACITY", &selected_visgroup->opacity, 0.01f, 0.0f, 1.0f);
                if (ImGui::Button("DELETE")) {
                    for (auto& obj : selected_visgroup->objects) {
                        vis_groups[0].objects.push_back(obj);
                    }
                    vis_groups.erase(vis_groups.begin() + selected_visgroup_index);
                    selected_visgroup_index = -1;
                    selected_object_index = -1;
                    selected_mesh_visgroup = -1;
                }
            } else {
                ImGui::Text("NO VISGROUP SELECTED");
            }
            ImGui::EndChild();
            if (ImGui::Button("ADD VISGROUP")) {
                vis_groups.emplace_back("New VisGroup", vector<Object>());
            }
        }

        // Mesh editor
        if (ImGui::CollapsingHeader("OBJECT")) {
            ImGui::BeginChild("OBJECT", ImVec2(256, 300), true);

            for (int v = 0; v < vis_groups.size(); v++) {
                VisGroup visgroup = vis_groups[v];
                for (int i = 0; i < visgroup.objects.size(); i++) {
                    Object obj = visgroup.objects[i];
                    Mesh* mesh = obj.mesh;
                    string label = "OBJECT " + to_string(v) + ":" +  to_string(i) + " with VC: " + to_string(mesh->vertex_count);

                    bool is_selected = (selected_object_index == i);
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        selected_object_index = i;
                        selected_mesh_visgroup = v;
                        if (vis_groups[v].objects[i].mesh->material->has_diffuse_texture) {
                            editor_display_image = sg_make_image(vis_groups[v].objects[i].mesh->material->diffuse_texture_desc);
                            editor_display_sampler = sg_make_sampler(vis_groups[v].objects[i].mesh->material->diffuse_sampler_desc);
                        }
                        if (vis_groups[v].objects[i].mesh->material->has_specular_texture) {
                            editor_specular_display_image = sg_make_image(vis_groups[v].objects[i].mesh->material->specular_texture_desc);
                            editor_specular_display_sampler = sg_make_sampler(vis_groups[v].objects[i].mesh->material->specular_sampler_desc);
                        }
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("OBJECT SETTINGS", ImVec2(300, 300), true);
            if (selected_object_index != -1) {
                Object* selected_object = &vis_groups[selected_mesh_visgroup].objects[selected_object_index];

                ImGui::PushItemWidth(200);

                ImGui::DragFloat3("POSITION", &selected_object->position.X, 0.01f);

                if (last_selected_mesh != selected_object_index) {
                    mesh_euler_rotations[selected_object_index] = QuatToEulerDegrees(selected_object->rotation);
                    last_selected_mesh = selected_object_index;
                }

                HMM_Vec3& euler_rotation = mesh_euler_rotations[selected_object_index];

                if (ImGui::DragFloat3("ROTATION", &euler_rotation.X, 0.5f)) {
                    selected_object->rotation = EulerDegreesToQuat(euler_rotation);
                }
                HMM_Vec3 current_euler = QuatToEulerDegrees(selected_object->rotation);
                if (abs(current_euler.X - euler_rotation.X) > 0.1f || abs(current_euler.Y - euler_rotation.Y) > 0.1f || abs(current_euler.Z - euler_rotation.Z) > 0.1f) {euler_rotation = current_euler;}

                ImGui::DragFloat3("SCALE", &selected_object->scale.X, 0.01f);

                ImGui::SliderFloat("OPACITY", &selected_object->opacity, 0.0f, 1.0f);

                ImGui::Checkbox("SHADING", &selected_object->mesh->enable_shading);

                ImGui::InputInt("SCRIPT ID", &selected_object->script_id);

                ImGui::PopItemWidth();

                if (ImGui::BeginCombo("VISGROUP", vis_groups[selected_mesh_visgroup].name.c_str())) {
                    for (int i = 0; i < vis_groups.size(); i++) {
                        bool is_selected = (selected_selectable_visgroup_index == i);
                        if (ImGui::Selectable(vis_groups[i].name.c_str(), is_selected)) {
                            selected_selectable_visgroup_index = i;
                            if (selected_object_index >= 0 && selected_object_index < vis_groups[selected_mesh_visgroup].objects.size()) {
                                Object object_to_move = std::move(vis_groups[selected_mesh_visgroup].objects[selected_object_index]);
                                vis_groups[selected_mesh_visgroup].objects.erase(vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index);
                                vis_groups[i].objects.push_back(std::move(object_to_move));
                                selected_object_index = -1;
                            }
                        }

                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Separator();
                if (ImGui::Button("LOOK AT")) {
                    HMM_Vec3 target = selected_object->position;
                    HMM_Vec3 direction = HMM_NormV3(HMM_SubV3(target, state.camera_pos));
                    state.camera_front = direction;
                    state.yaw = atan2f(direction.Z, direction.X) * 180.0f / HMM_PI;
                    state.pitch = asinf(direction.Y) * 180.0f / HMM_PI;
                }
                if (ImGui::Button("DUPLICATE")) {
                    if (selected_object_index >= 0 && selected_object_index < vis_groups[selected_mesh_visgroup].objects.size()) {
                        const Object selected_object = vis_groups[selected_mesh_visgroup].objects[selected_object_index];
                        Object new_object;
                        new_object.mesh = selected_object.mesh;

                        new_object.position = selected_object.position;
                        new_object.rotation = selected_object.rotation;
                        new_object.scale = selected_object.scale;
                        new_object.opacity = selected_object.opacity;

                        vis_groups[0].objects.push_back(new_object);
                    }
                }
                ImGui::Separator();
                if (selected_object->shape_keys.size() > 1) {
                    if (ImGui::CollapsingHeader("SHAPE KEYS")) {
                        for (int i = 0; i < selected_object->shape_keys.size(); i++) {
                            ImGui::PushID(i);
                            if (ImGui::Button(to_string(i).c_str())) {
                                selected_object->select_shape_key(i);
                            }
                            ImGui::PopID();
                        }
                    }
                }
                if (ImGui::CollapsingHeader("TEXTURES")) {
                    if (selected_object->mesh->material->has_diffuse_texture) {
                        sg_view_desc editor_view_desc = {};
                        editor_view_desc.texture.image = selected_object->mesh->material->diffuse_image;
                        sg_view editor_display_view = sg_make_view(&editor_view_desc);
                        if (editor_display_view.id == SG_INVALID_ID) {
                            ImGui::Text("Failed to create diffuse view!");
                        } else {
                            ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_display_view, selected_object->mesh->material->diffuse_sampler);
                            ImGui::Image(imtex_id, ImVec2(128, 128));
                            temp_editor_views.push_back(editor_display_view);
                        }
                    }
                    if (selected_object->mesh->material->has_specular_texture) {
                        sg_view_desc editor_specular_view_desc = {};
                        editor_specular_view_desc.texture.image = selected_object->mesh->material->specular_image;
                        sg_view editor_specular_display_view = sg_make_view(&editor_specular_view_desc);
                        if (editor_specular_display_view.id == SG_INVALID_ID) {
                            ImGui::Text("Failed to create specular view!");
                        } else {
                            ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_specular_display_view, selected_object->mesh->material->specular_sampler);
                            ImGui::Image(imtex_id, ImVec2(128, 128));
                            temp_editor_views.push_back(editor_specular_display_view);
                        }
                    }
                }
                if (ImGui::CollapsingHeader("DANGER ZONE")) {
                    if (ImGui::Button("RE-PREPARE BUFFERS")) {
                        prepare_mesh_buffers(*selected_object);
                    }
                    ImGui::SameLine();
                    ImGui::Text("This wastes memory");
                    if (ImGui::Button("DELETE")) {
                        vis_groups[selected_mesh_visgroup].objects.erase(vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index);
                        selected_object_index = -1;
                        selected_mesh_visgroup = -1;
                    }
                }
            } else {
                ImGui::Text("NO OBJECT SELECTED");
            }
            ImGui::EndChild();

            if (ImGui::Button("LOAD GLTF")) {
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
                    vector<Object> loaded_objects = load_gltf(file_path);
                    for (auto& obj : loaded_objects) {
                        vis_groups[0].objects.push_back(obj);
                    }
                }
            }
            /*ImGui::SameLine();
            if (ImGui::Button("LOAD OBJ")) {
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
                        Object loaded_object{};
                        loaded_object.mesh = new Mesh();
                        loaded_object.mesh->vertices = verts;
                        loaded_object.mesh->vertex_count = vertex_count;
                        loaded_object.mesh->indices = indices;
                        loaded_object.mesh->index_count = index_count;
                        loaded_object.position = HMM_V3(0.0f, 0.0f, 0.0f);

                        prepare_mesh_buffers(*loaded_object.mesh);
                        vis_groups[0].objects.push_back(loaded_object);
                        std::cout << "Loaded OBJ" << std::endl;
                    }
                }
            }*/
        }

        // Audio sources
        if (ImGui::CollapsingHeader("AUDIO SOURCES")) {
            static int selected_as_index = -1;

            ImGui::BeginChild("SOURCES", ImVec2(256, 150), true);

            for (int i = 0; i < state.audio_sources.size(); i++) {
                string label = "AUDIO SOURCE " + to_string(i);

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
            ImGui::BeginChild("AUDIO SOURCE SETTINGS", ImVec2(300, 150), true);
            if (selected_as_index >= 0 && selected_as_index < state.audio_sources.size()) {
                auto& selected_as = state.audio_sources[selected_as_index];

                ImGui::PushItemWidth(200);

                string label = "POSITION";
                if (ImGui::DragFloat3(label.c_str(), &selected_as->position.X, 0.01f)) {
                    selected_as->set_position(selected_as->position);
                }

                ImGui::Separator();

                label = "PLAY";
                if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                    selected_as->play();
                }

                ImGui::SameLine();
                label = "STOP";
                if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                    selected_as->stop();
                }

                label = "PAUSE";
                if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                    selected_as->pause();
                }

                ImGui::SameLine();
                label = "UNPAUSE";
                if (ImGui::Button(label.c_str(), ImVec2(75, 25))) {
                    selected_as->unpause();
                }

                ImGui::InputInt("SCRIPT ID", &selected_as->script_id);

                ImGui::PopItemWidth();

                ImGui::Separator();
                if (ImGui::Button("DELETE")) {
                    selected_as->remove();
                    selected_as_index = -1;
                }
            } else {
                ImGui::Text("NO SOURCE SELECTED");
            }
            ImGui::EndChild();

            static int selected_event_index = -1;

            ImGui::BeginChild("EVENTS", ImVec2(512, 150), true);

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

            if (selected_event_index >= 0 && selected_event_index < state.event_descriptions.size()) {
                if (ImGui::Button("ADD SOURCE")) {
                    make_audiosource_by_index(selected_event_index);
                }
            }
        }

        if (ImGui::CollapsingHeader("HELPERS")) {
            static int selected_helper_index = -1;

            ImGui::BeginChild("HELPER LIST", ImVec2(256, 150), true);
            for (int i = 0; i < state.helpers.size(); i++) {
                string label = "HELPER " + state.helpers[i]->name;

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
            ImGui::BeginChild("HELPER SETTINGS", ImVec2(300, 150), true);
            if (selected_helper_index >= 0 && selected_helper_index < state.helpers.size()) {
                auto& selected_helper = state.helpers[selected_helper_index];

                ImGui::PushItemWidth(200);

                static char name_buffer[256];
                static int last_selected_helper = -1;

                if (last_selected_helper != selected_helper_index) {
                    strncpy(name_buffer, selected_helper->name.c_str(), sizeof(name_buffer) - 1);
                    name_buffer[sizeof(name_buffer) - 1] = '\0';
                    last_selected_helper = selected_helper_index;
                }

                if (ImGui::InputText("NAME", name_buffer, sizeof(name_buffer))) {
                    selected_helper->name = std::string(name_buffer);
                }

                if (ImGui::DragFloat3("POSITION", &selected_helper->position.X, 0.01f)) {
                    selected_helper->set_position(selected_helper->position);
                }

                ImGui::PopItemWidth();
                ImGui::Separator();
                if (ImGui::Button("DELETE")) {
                    selected_helper->remove();
                    selected_helper_index = -1;
                }
            } else {
                ImGui::Text("NO HELPER SELECTED");
            }
            ImGui::EndChild();
            if (ImGui::Button("ADD HELPER")) {
                Helper* helper = new Helper();
                helper->initialize("New Helper", {0.0f, 0.0f, 0.0f});
            }
        }
        if (ImGui::CollapsingHeader("LIGHTS")) {
            ImGui::BeginTabBar("LIGHT TYPES", ImGuiTabBarFlags_None);

            if (ImGui::BeginTabItem("DIRECTIONAL")) {
                ImGui::BeginChild("DIRECTIONAL LIGHT SELECTION", ImVec2(300, 150), true);
                for (int i = 0; i < state.directional_lights.size(); i++) {
                    string label = "DIRECTIONAL LIGHT " + to_string(i);

                    bool is_selected = (selected_dir_light_index == i);
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        selected_dir_light_index = i;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("DIRECTIONAL LIGHT SETTINGS", ImVec2(300, 150), true);
                if (selected_dir_light_index >= 0 && selected_dir_light_index < state.directional_lights.size()) {
                    auto& selected_dir_light = state.directional_lights[selected_dir_light_index];
                    ImGui::PushItemWidth(200);
                    ImGui::SliderFloat3("DIRECTION", &selected_dir_light.direction.X, -1.0f, 1.0f, "%.1f");
                    ImGui::ColorEdit3("COLOR", &selected_dir_light.color.X);
                    ImGui::DragFloat("INTENSITY", &selected_dir_light.intensity, 0.01f);
                    ImGui::InputInt("SCRIPT ID", &selected_dir_light.script_id);
                    if (ImGui::Button("DELETE")) {
                        state.directional_lights.erase(state.directional_lights.begin() + selected_dir_light_index);
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::EndChild();
                if (ImGui::Button("ADD")) {
                    DirectionalLight* new_light = new DirectionalLight();
                    state.directional_lights.push_back(*new_light);
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("POINT")) {
                ImGui::BeginChild("POINT LIGHT SELECTION", ImVec2(300, 150), true);
                for (int i = 0; i < state.point_lights.size(); i++) {
                    string label = "POINT LIGHT " + to_string(i);

                    bool is_selected = (selected_point_light_index == i);
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        selected_point_light_index = i;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("POINT LIGHT SETTINGS", ImVec2(300, 150), true);
                if (selected_point_light_index >= 0 && selected_point_light_index < state.point_lights.size()) {
                    auto& selected_point_light = state.point_lights[selected_point_light_index];
                    ImGui::PushItemWidth(200);
                    ImGui::DragFloat3("POSITION", &selected_point_light.position.X, 0.1f);
                    ImGui::ColorEdit3("COLOR", &selected_point_light.color.X);
                    ImGui::DragFloat("RADIUS", &selected_point_light.radius, 0.01f);
                    ImGui::DragFloat("INTENSITY", &selected_point_light.intensity, 0.01f);
                    ImGui::InputInt("SCRIPT ID", &selected_point_light.script_id);
                    if (ImGui::Button("DELETE")) {
                        state.point_lights.erase(state.point_lights.begin() + selected_point_light_index);
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::EndChild();

                if (ImGui::Button("ADD")) {
                    PointLight* new_light = new PointLight();
                    state.point_lights.push_back(*new_light);
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("SPOT")) {
                ImGui::BeginChild("SPOT LIGHT SELECTION", ImVec2(300, 150), true);
                for (int i = 0; i < state.spot_lights.size(); i++) {
                    string label = "SPOT LIGHT " + to_string(i);

                    bool is_selected = (selected_spot_light_index == i);
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        selected_spot_light_index = i;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("SPOT LIGHT SETTINGS", ImVec2(300, 150), true);
                if (selected_spot_light_index >= 0 && selected_spot_light_index < state.spot_lights.size()) {
                    auto& selected_spot_light = state.spot_lights[selected_spot_light_index];
                    ImGui::PushItemWidth(200);
                    ImGui::DragFloat3("POSITION", &selected_spot_light.position.X, 0.1f);
                    ImGui::SliderFloat3("DIRECTION", &selected_spot_light.direction.X, -1.0f, 1.0f, "%.1f");
                    ImGui::ColorEdit3("COLOR", &selected_spot_light.color.X);
                    ImGui::DragFloat("INNER CONE ANGLE", &selected_spot_light.inner_cone_angle, 0.01f);
                    ImGui::DragFloat("OUTER CONE ANGLE", &selected_spot_light.outer_cone_angle, 0.01f);
                    ImGui::DragFloat("INTENSITY", &selected_spot_light.intensity, 0.01f);
                    ImGui::InputInt("SCRIPT ID", &selected_spot_light.script_id);
                    if (ImGui::Button("DELETE")) {
                        state.spot_lights.erase(state.spot_lights.begin() + selected_spot_light_index);
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::EndChild();

                if (ImGui::Button("ADD")) {
                    SpotLight* new_light = new SpotLight();
                    state.spot_lights.push_back(*new_light);
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("RESET")) {
            state.camera_pos = HMM_V3(0.0f, 0.0f, 0.0f);
            state.camera_front = HMM_V3(0.0f, 0.0f, 1.0f);
            state.camera_up = HMM_V3(0.0f, 1.0f, 0.0f);
            state.yaw = 0.0f;
            state.pitch = 0.0f;
        }
        ImGui::SameLine();
        ImGui::InputFloat3("CAMERA POS", &state.camera_pos.X);
        ImGui::SliderFloat("FOV", &state.fov, 1.0f, 179.0, "%.0f");
        ImGui::ColorEdit3("AMBIENT COLOR", &state.ambient_light.X);

        ImGui::Separator();
        if (ImGui::CollapsingHeader("PROFILER")) {
            ImGui::Text("DT: %f", time_state.dt);
            int vertex_count = 0;
            int index_count = 0;
            int light_count = 0;
            for (auto& vis_group : vis_groups) {
                for (auto& object : vis_group.objects) {
                    vertex_count += object.mesh->vertex_count;
                    index_count += object.mesh->index_count;
                }
            }
            for (auto& light : state.directional_lights) {
                light_count++;
            }
            for (auto& light : state.point_lights) {
                light_count++;
            }
            for (auto& light : state.spot_lights) {
                light_count++;
            }
            ImGui::Text("VERTEX COUNT: %d", vertex_count);
            ImGui::Text("INDEX COUNT: %d", index_count);
            ImGui::Text("LIGHT COUNT: %d", light_count);
            ImPlot::SetNextAxesToFit();
            if (ImPlot::BeginPlot("PERFORMANCE PLOT")) {
                ImPlot::PlotLine("FPS", fps_over_time, 225);

                ImPlot::EndPlot();
            }
            ImPlot::SetNextAxesToFit();
            if (ImPlot::BeginPlot("MEMORY PLOT")) {
                ImPlot::PlotLine("VERTEX COUNT", vertex_count_over_time, 225);
                ImPlot::PlotLine("INDEX COUNT", index_count_over_time, 225);

                ImPlot::EndPlot();
            }
        }

        ImGui::End();
    } /*else {
        ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        ImGui::Text("Press 0 to open the dev UI");
        ImGui::Text("ALSO, VERY IMPORTANT!! WHEN YOU CLEAR THE SCENE, ADD CREATE A VISGROUP SO GLBS GETS LOADED");
        ImGui::End();
    }*/ // This is just for the DEMO, feel free to remove it later.
    simgui_render();
    for (auto& view : temp_editor_views) {
        sg_destroy_view(view);
    }
}

vector<Object*> get_objects_by_script_id(int id) {
    vector<Object*> objects;
    for (auto& visgroup : vis_groups) {
        for (auto& object : visgroup.objects) {
            if (object.script_id == id) {
                objects.push_back(&object);
            }
        }
    }
    return objects;
}

vector<Light*> get_lights_by_script_id(int id) {
    vector<Light*> lights;
    for (auto& light : state.directional_lights) {
        if (light.script_id == id) {
            lights.push_back(&light);
        }
    }
    for (auto& light : state.point_lights) {
        if (light.script_id == id) {
            lights.push_back(&light);
        }
    }
    for (auto& light : state.spot_lights) {
        if (light.script_id == id) {
            lights.push_back(&light);
        }
    }
    return lights;
}

vector<AudioSource3D*> get_audio_sources_by_script_id(int id) {
    vector<AudioSource3D*> audio_sources;
    for (auto& audio_source : state.audio_sources) {
        if (audio_source->script_id == id) {
            audio_sources.push_back(audio_source);
        }
    }
    return audio_sources;
}

void _init() {
    VisGroup* default_visgroup = new VisGroup("default", {});
    vis_groups.push_back(*default_visgroup);
    stbi_set_flip_vertically_on_load(true);
    stbi_set_flip_vertically_on_load_thread(true);

    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    state.window_surface.clear(w_width, w_height);

    // ImGui
    simgui_desc_t imgui_desc = {};
    simgui_setup(imgui_desc);
    ImPlot::CreateContext();
    //ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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
    state.fov = 75.0f;

    sfetch_desc_t fetch_desc = {};
    fetch_desc.max_requests = 1;
    fetch_desc.num_channels = 1;
    fetch_desc.num_lanes = 1;
    sfetch_setup(&fetch_desc);

    for (int i = 0; i < 225; i++) {
        fps_over_time[i] = 0.0f;
    }
    for (int i = 0; i < 225; i++) {
        vertex_count_over_time[i] = 0;
    }
    for (int i = 0; i < 225; i++) {
        index_count_over_time[i] = 0;
    }

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
    pip_desc.index_type = SG_INDEXTYPE_UINT32;
    pip_desc.depth.write_enabled = true;
    pip_desc.cull_mode = SG_CULLMODE_FRONT; // really fucky
    pip_desc.label = "main-pipeline";
    state.pip = sg_make_pipeline(&pip_desc);

    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f };
    state.pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    state.pass_action.depth.clear_value = 1.0f;

    init_post_processing();

    // 2d rendering pipeline
    sg_shader surf_shader = sg_make_shader(surface_shader_desc(sg_query_backend()));
    sg_pipeline_desc surface_pipeline_desc = {};
    surface_pipeline_desc.shader = surf_shader;
    surface_pipeline_desc.layout.attrs[ATTR_surface_position].format = SG_VERTEXFORMAT_FLOAT3;
    surface_pipeline_desc.layout.attrs[ATTR_surface_texcoord].format = SG_VERTEXFORMAT_FLOAT2;
    surface_pipeline_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    surface_pipeline_desc.color_count = 1;
    surface_pipeline_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    surface_pipeline_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    surface_pipeline_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    surface_pipeline_desc.depth.write_enabled = true;
    surface_pipeline_desc.cull_mode = SG_CULLMODE_NONE;
    surface_pipeline_desc.colors->blend.enabled = true;
    surface_pipeline_desc.colors->blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    surface_pipeline_desc.colors->blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    surface_pipeline_desc.colors->blend.op_rgb = SG_BLENDOP_ADD;
    surface_pipeline_desc.colors->blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
    surface_pipeline_desc.colors->blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    surface_pipeline_desc.colors->blend.op_alpha = SG_BLENDOP_ADD;
    surface_pipeline_desc.label = "surface-pipeline";
    state.surf_pipeline = sg_make_pipeline(&surface_pipeline_desc);

    // particle pipeline
    sg_shader particle_shader = sg_make_shader(particle_shader_desc(sg_query_backend()));
    sg_pipeline_desc particle_pipeline_desc = {};
    particle_pipeline_desc.shader = particle_shader;
    particle_pipeline_desc.layout.attrs[ATTR_particle_aPos].format = SG_VERTEXFORMAT_FLOAT3;
    particle_pipeline_desc.layout.attrs[ATTR_particle_texCoord].format = SG_VERTEXFORMAT_FLOAT2;
    particle_pipeline_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    particle_pipeline_desc.color_count = 1;
    particle_pipeline_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    particle_pipeline_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    particle_pipeline_desc.depth.compare = SG_COMPAREFUNC_LESS;
    particle_pipeline_desc.depth.write_enabled = true;
    particle_pipeline_desc.cull_mode = SG_CULLMODE_NONE;
    particle_pipeline_desc.index_type = SG_INDEXTYPE_UINT32;
    particle_pipeline_desc.colors->blend.enabled = true;
    particle_pipeline_desc.colors->blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    particle_pipeline_desc.colors->blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    particle_pipeline_desc.colors->blend.op_rgb = SG_BLENDOP_ADD;
    particle_pipeline_desc.colors->blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
    particle_pipeline_desc.colors->blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    particle_pipeline_desc.colors->blend.op_alpha = SG_BLENDOP_ADD;
    particle_pipeline_desc.label = "particle-pipeline";
    particle_pipeline = sg_make_pipeline(&particle_pipeline_desc);

    sg_buffer_desc quad_vb_desc = {};
    quad_vb_desc.usage.vertex_buffer = true;
    quad_vb_desc.size = sizeof(quad_vertices);
    quad_vb_desc.data = SG_RANGE(quad_vertices);
    quad_vb = sg_make_buffer(&quad_vb_desc);
    billboard_vb = quad_vb;

    sg_buffer_desc quad_ib_desc = {};
    quad_ib_desc.usage.vertex_buffer = false;
    quad_ib_desc.usage.index_buffer = true;
    quad_ib_desc.usage.immutable = true;
    quad_ib_desc.size = sizeof(quad_indices);
    quad_ib_desc.data = SG_RANGE(quad_indices);
    quad_ib = sg_make_buffer(&quad_ib_desc);
    billboard_ib = quad_ib;

    sg_shader billboard_shader = sg_make_shader(billboard_shader_desc(sg_query_backend()));
    sg_pipeline_desc billboard_pipeline_desc = {};
    billboard_pipeline_desc.shader = billboard_shader;
    billboard_pipeline_desc.layout.attrs[ATTR_billboard_aPos].format = SG_VERTEXFORMAT_FLOAT3;
    billboard_pipeline_desc.layout.attrs[ATTR_billboard_texCoord].format = SG_VERTEXFORMAT_FLOAT2;
    billboard_pipeline_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    billboard_pipeline_desc.color_count = 1;
    billboard_pipeline_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    billboard_pipeline_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    billboard_pipeline_desc.depth.compare = SG_COMPAREFUNC_LESS;
    billboard_pipeline_desc.depth.write_enabled = true;
    billboard_pipeline_desc.cull_mode = SG_CULLMODE_NONE;
    billboard_pipeline_desc.index_type = SG_INDEXTYPE_UINT32;
    billboard_pipeline_desc.colors->blend.enabled = true;
    billboard_pipeline_desc.colors->blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    billboard_pipeline_desc.colors->blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    billboard_pipeline_desc.colors->blend.op_rgb = SG_BLENDOP_ADD;
    billboard_pipeline_desc.colors->blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
    billboard_pipeline_desc.colors->blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    billboard_pipeline_desc.colors->blend.op_alpha = SG_BLENDOP_ADD;
    billboard_pipeline_desc.label = "billboard-pipeline";
    billboard_pipeline = sg_make_pipeline(&billboard_pipeline_desc);
}

void _frame() {
    Time_BeginFrame(time_state);
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);

    for (int i = 0; i < 225; i++) {
        if (i > 0) {
            fps_over_time[i - 1] = fps_over_time[i];
        }
    }
    fps_over_time[224] = time_state.fps;

    for (int i = 0; i < 225; i++) {
        if (i > 0) {
            vertex_count_over_time[i - 1] = vertex_count_over_time[i];
        }
    }
    vertex_count_over_time[224] = all_vertex_count;

    for (int i = 0; i < 225; i++) {
        if (i > 0) {
            index_count_over_time[i - 1] = index_count_over_time[i];
        }
    }
    index_count_over_time[224] = all_index_count;

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

    // imgui
    simgui_frame_desc_t imgui_frame_desc = {};
    imgui_frame_desc.width = w_width;
    imgui_frame_desc.height = w_height;
    imgui_frame_desc.delta_time = time_state.dt;
    imgui_frame_desc.dpi_scale = 1.0f;
    simgui_new_frame(imgui_frame_desc);

    // Physics
    if (!state.editor_open) world->update(time_state.dt);
    for (auto& holder : state.physics_holders) {
        holder->update();
    }

    sfetch_dowork();
    state.pass_action.colors[0].clear_value = { state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f };

    float aspect = static_cast<float>(w_width)/static_cast<float>(w_height);
    HMM_Mat4 view = HMM_LookAt_RH(state.camera_pos, HMM_AddV3(state.camera_pos, state.camera_front), state.camera_up);
    HMM_Mat4 projection = HMM_Perspective_RH_NO(state.fov * (HMM_PI32 / 180.0f), aspect, 0.1f, 1050.0f);

    HMM_Mat4 clip = HMM_MulM4(projection, view);

    HMM_Vec4 row0 = HMM_V4(clip.Elements[0][0], clip.Elements[1][0], clip.Elements[2][0], clip.Elements[3][0]);
    HMM_Vec4 row1 = HMM_V4(clip.Elements[0][1], clip.Elements[1][1], clip.Elements[2][1], clip.Elements[3][1]);
    HMM_Vec4 row2 = HMM_V4(clip.Elements[0][2], clip.Elements[1][2], clip.Elements[2][2], clip.Elements[3][2]);
    HMM_Vec4 row3 = HMM_V4(clip.Elements[0][3], clip.Elements[1][3], clip.Elements[2][3], clip.Elements[3][3]);

    // left plane
    HMM_Vec4 left = HMM_AddV4(row3, row0);
    float len_left = HMM_LenV3(left.XYZ);
    if (len_left > 0.0001f) {
        frustum_planes[0].normal = HMM_DivV3F(left.XYZ, len_left);
        frustum_planes[0].d = left.W / len_left;
    } else {
        frustum_planes[0] = {{0,0,0}, 0};
    }

    // right plane
    HMM_Vec4 right = HMM_SubV4(row3, row0);
    float len_right = HMM_LenV3(right.XYZ);
    if (len_right > 0.0001f) {
        frustum_planes[1].normal = HMM_DivV3F(right.XYZ, len_right);
        frustum_planes[1].d = right.W / len_right;
    } else {
        frustum_planes[1] = {{0,0,0}, 0};
    }

    // bottom plane
    HMM_Vec4 bottom = HMM_AddV4(row3, row1);
    float len_bottom = HMM_LenV3(bottom.XYZ);
    if (len_bottom > 0.0001f) {
        frustum_planes[2].normal = HMM_DivV3F(bottom.XYZ, len_bottom);
        frustum_planes[2].d = bottom.W / len_bottom;
    } else {
        frustum_planes[2] = {{0,0,0}, 0};
    }

    // top plane
    HMM_Vec4 top = HMM_SubV4(row3, row1);
    float len_top = HMM_LenV3(top.XYZ);
    if (len_top > 0.0001f) {
        frustum_planes[3].normal = HMM_DivV3F(top.XYZ, len_top);
        frustum_planes[3].d = top.W / len_top;
    } else {
        frustum_planes[3] = {{0,0,0}, 0};
    }

    // near plane
    HMM_Vec4 near_p = HMM_AddV4(row3, row2);
    float len_near = HMM_LenV3(near_p.XYZ);
    if (len_near > 0.0001f) {
        frustum_planes[4].normal = HMM_DivV3F(near_p.XYZ, len_near);
        frustum_planes[4].d = near_p.W / len_near;
    } else {
        frustum_planes[4] = {{0,0,0}, 0};
    }

    // far plane
    HMM_Vec4 far_p = HMM_SubV4(row3, row2);
    float len_far = HMM_LenV3(far_p.XYZ);
    if (len_far > 0.0001f) {
        frustum_planes[5].normal = HMM_DivV3F(far_p.XYZ, len_far);
        frustum_planes[5].d = far_p.W / len_far;
    } else {
        frustum_planes[5] = {{0,0,0}, 0};
    }

    vs_params = {.view = view, .projection = projection};
    billboard_vs_params = {.view = view, .projection = projection};

    HMM_Vec2 ssao_proj{};
    ssao_proj.Y = tanf(state.fov * 0.5f);
    ssao_proj.X = ssao_proj.Y * (static_cast<float>(w_width) / static_cast<float>(w_height));
    ssao_params.proj = ssao_proj;
    ssao_params.screen_size = HMM_Vec2{ static_cast<float>(w_width), static_cast<float>(w_height) };
    ssao_params.u_near = 0.1f;
    ssao_params.u_far = 1050.0f;

    render_first_pass();
    render_second_pass();

    // input
    if (state.editor_open) {
        float camera_speed = 5.f * time_state.dt;
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
            if (state.editor_open && !ImGui::GetIO().WantCaptureMouse) {
                RaycastResult result = raycast_from_screen(e->button.x, e->button.y);
                if (result.hit) {
                    selected_object_index = -1;
                    selected_mesh_visgroup = -1;

                    bool found = false;
                    for (int vg_idx = 0; vg_idx < vis_groups.size(); ++vg_idx) {
                        auto& visgroup = vis_groups[vg_idx];
                        for (int obj_idx = 0; obj_idx < visgroup.objects.size(); ++obj_idx) {
                            if (&visgroup.objects[obj_idx] == result.obj) {
                                selected_object_index = obj_idx;
                                selected_mesh_visgroup = vg_idx;

                                auto* mesh = visgroup.objects[obj_idx].mesh;
                                if (mesh->material->has_diffuse_texture) {
                                    editor_display_image = sg_make_image(&mesh->material->diffuse_texture_desc);
                                    editor_display_sampler = sg_make_sampler(&mesh->material->diffuse_sampler_desc);
                                }
                                if (mesh->material->has_specular_texture) {
                                    editor_specular_display_image = sg_make_image(&mesh->material->specular_texture_desc);
                                    editor_specular_display_sampler = sg_make_sampler(&mesh->material->specular_sampler_desc);
                                }

                                found = true;
                                break;
                            }
                        }
                        if (found) {
                            break;
                        }
                    }
                }
            }
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
        ImGuiIO& io = ImGui::GetIO();
        if (e->key.key == SDL_KMOD_CTRL) io.KeyCtrl = true;
        if (e->key.key == SDL_KMOD_SHIFT) io.KeyShift = true;
        if (e->key.key == SDL_KMOD_ALT) io.KeyAlt = true;
        if (e->key.key == SDL_KMOD_GUI) io.KeySuper = true;
        ImGuiKey imgui_key = ImGui_ImplSDL3_KeyEventToImGuiKey(e->key.key, e->key.scancode);
        simgui_add_key_event(static_cast<int>(imgui_key), true);

        if (!io.WantCaptureKeyboard) {
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
        if (e->key.key == SDLK_SPACE && state.editor_open) {
            world->update(time_state.dt);
        }
    }
    if (e->type == SDL_EVENT_KEY_UP) {
        ImGuiIO& io = ImGui::GetIO();
        if (e->key.key == SDL_KMOD_CTRL) io.KeyCtrl = false;
        if (e->key.key == SDL_KMOD_SHIFT) io.KeyShift = false;
        if (e->key.key == SDL_KMOD_ALT) io.KeyAlt = false;
        if (e->key.key == SDL_KMOD_GUI) io.KeySuper = false;
        state.inputs[e->key.key] = false;
        ImGuiKey imgui_key = ImGui_ImplSDL3_KeyEventToImGuiKey(e->key.key, e->key.scancode);
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
            sg_destroy_view(state.bind.views[texture_index]);
            sg_image old_img = sg_query_view_image(state.bind.views[texture_index]);
            if (old_img.id != SG_INVALID_ID) {
                sg_destroy_image(old_img);
            }

            sg_image_desc img_desc = {};
            img_desc.width = img_width;
            img_desc.height = img_height;
            img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
            img_desc.data.subimage[0][0].ptr = pixels;
            img_desc.data.subimage[0][0].size = img_width * img_height * 4;

            sg_image new_img = sg_make_image(&img_desc);
            sg_view_desc tex_view_desc = {};
            tex_view_desc.texture.image = new_img;
            state.bind.views[texture_index] = sg_make_view(&tex_view_desc);

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
    srand (static_cast <unsigned> (time(0)));
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO);
    SDL_Rect display_bounds;
    SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display_bounds);
    state.win = SDL_CreateWindow("Gungutils", display_bounds.x, display_bounds.y, SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FULLSCREEN);
    SDL_GLContext ctx = SDL_GL_CreateContext(state.win);
    SDL_StartTextInput(state.win);
    sg_desc desc = {};
    sg_environment env = {};
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    env.defaults.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    env.defaults.sample_count = 1;
    desc.buffer_pool_size = 4096;
    desc.image_pool_size = 8192;
    desc.sampler_pool_size = 8192;
    desc.shader_pool_size = 512;
    desc.pipeline_pool_size = 1024;
    desc.view_pool_size = 8192;
    desc.environment = env;
    sg_setup(&desc);
    stm_setup();
    Time_Init(time_state);
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
    ImPlot::DestroyContext();
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