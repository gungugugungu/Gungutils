#define SOKOL_IMPL
#define SOKOL_GLCORE
#define SOKOL_IMGUI_NO_SOKOL_APP
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
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
#include <typeindex>
#include <unordered_map>
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "ImGuizmo/ImGuizmo.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_fetch.h"
#include "sokol/sokol_time.h"
#include "sokol/util/sokol_imgui.h"
#include "sokol/sokol_log.h"
#include "HandmadeMath/HandmadeMath.h"
#include "tinygltf/tiny_gltf.h"
#include "tinyobjloader/tiny_obj_loader.h"
#include "SDL3/SDL.h"
#include "FModStudio/api/core/inc/fmod.hpp"
#include "FModStudio/api/studio/inc/fmod_studio.hpp"
#include "FModStudio/api/core/inc/fmod_errors.h"
#include "libtinyfiledialogs/tinyfiledialogs.h"
#include "json/include/nlohmann/json.hpp"
#include <reactphysics3d/reactphysics3d.h>
#include "meshoptimizer/src/meshoptimizer.h"
#include "stb/stb_image_resize2.h"
#include "stb/stb_truetype.h"
// shaders
#include "shaders/mainshader.glsl.h"
#include "shaders/postprocess.glsl.h"
#include "shaders/surface.glsl.h"
#include "shaders/particles.glsl.h"
#include "shaders/billboard.glsl.h"
#include "shaders/shadow.glsl.h"
#include "shaders/skybox.glsl.h"
#include "shaders/bloom_filter.glsl.h"
#include "shaders/blur.glsl.h"
#include "shaders/ssao.glsl.h"
#include "shaders/kuwahara.glsl.h"
// sources
#include "utils/Log.h"
#include "rendering/Material.h"
#include "rendering/Mesh.h"
#include "rendering/Object.h"
#include "rendering/VisGroup.h"
#include "rendering/Skybox.h"
#include "utils/Animation.h"
#include "rendering/Post Processing.h"
#include "rendering/Light.h"
#include "rendering/Surface.h"
#include "physics/PhysicsComponent.h"
#include "rendering/ParticleSystem.h"
#include "rendering/Billboard.h"
#include "ui/UIButton.h"
#include "utils/CharacterController.h"
#include "utils/FPSController.h"

extern "C" {
#ifdef _WIN32
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
#endif
}

using namespace std;

class AudioSource3D;
class Helper;
void render_editor();

float fps_over_time[225];
float vertex_count_over_time[225];
int all_vertex_count = 0;
float index_count_over_time[225];
int all_index_count = 0;

stbtt_fontinfo font;

sg_image shadow_depth_img = {SG_INVALID_ID};
sg_view shadow_depth_att_view = {SG_INVALID_ID};
sg_view shadow_depth_tex_view = {SG_INVALID_ID};
sg_sampler shadow_sampler = {SG_INVALID_ID};
sg_pipeline shadow_pip = {SG_INVALID_ID};
int shadow_map_size = 2048;
float shadow_ortho_size = 50.0f;
float shadow_near = 0.1f;
float shadow_far = 100.0f;

const float camera_near = 0.1f;
const float camera_far = 1050.0f;

const int max_light_amount = 50;

sg_image diffuse_img = {SG_INVALID_ID};
sg_image specular_img = {SG_INVALID_ID};
sg_image normal_img = {SG_INVALID_ID};
sg_image emissive_img = {SG_INVALID_ID};

Surface as_visualizer;
Surface light_visualizer;
Surface hpr_visualizer;

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
    DirectionalLight directional_light;
    vector<PointLight> point_lights;
    vector<SpotLight> spot_lights;
    HMM_Vec3 ambient_light = {0.5f, 0.5f, 0.5f};
    sg_pipeline surf_pipeline;
    Surface window_surface{};
    vector<ParticleSystem> particle_systems;
    vector<VisGroup> vis_groups;
};

AppState state;
AppState old_state;
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
    float speed_multiplier = 1.0f;
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
    iprint("loaded font " + to_string(*filename));
}

struct Plane {
    HMM_Vec3 normal;
    float d;
};

Plane frustum_planes[6];
Plane light_frustum_planes[6];

void init_shadowmaps() {
    sg_image_desc shadow_img_desc = {};
    shadow_img_desc.usage.depth_stencil_attachment = true;
    shadow_img_desc.width = shadow_map_size;
    shadow_img_desc.height = shadow_map_size;
    shadow_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    shadow_img_desc.sample_count = 1;
    shadow_img_desc.label = "shadow-depth-target";
    shadow_depth_img = sg_make_image(&shadow_img_desc);

    sg_view_desc shadow_att_desc = {};
    shadow_att_desc.depth_stencil_attachment.image = shadow_depth_img;
    shadow_depth_att_view = sg_make_view(&shadow_att_desc);

    sg_view_desc shadow_tex_desc = {};
    shadow_tex_desc.texture.image = shadow_depth_img;
    shadow_depth_tex_view = sg_make_view(&shadow_tex_desc);

    sg_sampler_desc shadow_smp_desc = {};
    shadow_smp_desc.min_filter = SG_FILTER_LINEAR;
    shadow_smp_desc.mag_filter = SG_FILTER_LINEAR;
    shadow_smp_desc.wrap_u = SG_WRAP_CLAMP_TO_BORDER;
    shadow_smp_desc.wrap_v = SG_WRAP_CLAMP_TO_BORDER;
    shadow_smp_desc.border_color = SG_BORDERCOLOR_OPAQUE_WHITE;
    shadow_smp_desc.compare = SG_COMPAREFUNC_LESS_EQUAL;
    shadow_sampler = sg_make_sampler(&shadow_smp_desc);

    sg_shader shadow_shd = sg_make_shader(shadow_shader_desc(sg_query_backend()));
    sg_pipeline_desc shadow_pip_desc = {};
    shadow_pip_desc.shader = shadow_shd;
    shadow_pip_desc.layout.attrs[ATTR_shadow_aPos].format = SG_VERTEXFORMAT_FLOAT3;
    shadow_pip_desc.layout.attrs[ATTR_shadow_aNormal].format = SG_VERTEXFORMAT_FLOAT3;
    shadow_pip_desc.layout.attrs[ATTR_shadow_aUV].format = SG_VERTEXFORMAT_FLOAT2;
    shadow_pip_desc.index_type = SG_INDEXTYPE_UINT32;
    shadow_pip_desc.color_count = 1;
    shadow_pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    shadow_pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    shadow_pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    shadow_pip_desc.depth.write_enabled = true;
    shadow_pip_desc.cull_mode = SG_CULLMODE_FRONT;
    shadow_pip_desc.label = "shadow-pipeline";
    shadow_pip = sg_make_pipeline(&shadow_pip_desc);
}

sg_image bloom_img;
sg_image bloom_depth_img;
sg_view bloom_att_view = {SG_INVALID_ID};
sg_view bloom_depth_att_view = {SG_INVALID_ID};
sg_view bloom_tex_view = {SG_INVALID_ID};
sg_view bloom_depth_tex_view = {SG_INVALID_ID};
sg_sampler bloom_smp;
sg_sampler bloom_depth_smp;
bloom_filter_params_t bloom_params;
sg_pipeline bloom_filter_pip;
sg_pipeline blur_pip;

void init_blur_filter() {
    sg_pipeline_desc blur_pip_desc = {};
    blur_pip_desc.shader = sg_make_shader(blur_shader_desc(sg_query_backend()));
    blur_pip_desc.layout.attrs[ATTR_blur_position].format = SG_VERTEXFORMAT_FLOAT3;
    blur_pip_desc.layout.attrs[ATTR_blur_texcoord].format = SG_VERTEXFORMAT_FLOAT2;
    blur_pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    blur_pip_desc.color_count = 1;
    blur_pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    blur_pip_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    blur_pip_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    blur_pip_desc.depth.write_enabled = false;
    blur_pip_desc.cull_mode = SG_CULLMODE_NONE;
    blur_pip_desc.label = "bloom_blur_pipeline";
    blur_pip = sg_make_pipeline(&blur_pip_desc);
}

void blur_image(sg_image input_image, float strength, int passes) {
    struct {
        float strength;
        int type;
    } blur_params;
    blur_params.strength = strength;

    int width = sg_query_image_width(input_image);
    int height = sg_query_image_height(input_image);

    sg_image_desc temp_img_desc = {};
    temp_img_desc.width = width;
    temp_img_desc.height = height;
    temp_img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    temp_img_desc.sample_count = 1;
    temp_img_desc.usage.color_attachment = true;
    temp_img_desc.label = "blur-temp-target";
    sg_image temp_img = sg_make_image(&temp_img_desc);

    sg_view_desc temp_att_desc = {};
    temp_att_desc.color_attachment.image = temp_img;
    sg_view temp_att_view = sg_make_view(&temp_att_desc);

    sg_view_desc temp_tex_desc = {};
    temp_tex_desc.texture.image = temp_img;
    sg_view temp_tex_view = sg_make_view(&temp_tex_desc);

    sg_view_desc input_att_desc = {};
    input_att_desc.color_attachment.image = input_image;
    sg_view input_att_view = sg_make_view(&input_att_desc);

    sg_view_desc input_tex_desc = {};
    input_tex_desc.texture.image = input_image;
    sg_view input_tex_view = sg_make_view(&input_tex_desc);

    sg_sampler_desc blur_smp_desc = {};
    blur_smp_desc.min_filter = SG_FILTER_LINEAR;
    blur_smp_desc.mag_filter = SG_FILTER_LINEAR;
    blur_smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    blur_smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sg_sampler blur_smp = sg_make_sampler(&blur_smp_desc);

    sg_pass_action blur_pass_action = {};
    blur_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    blur_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

    sg_bindings binds_input_to_temp = {};
    binds_input_to_temp.vertex_buffers[0] = {.id = SG_INVALID_ID};
    binds_input_to_temp.views[0] = input_tex_view;
    binds_input_to_temp.samplers[0] = blur_smp;

    sg_bindings binds_temp_to_input = {};
    binds_temp_to_input.vertex_buffers[0] = {.id = SG_INVALID_ID};
    binds_temp_to_input.views[0] = temp_tex_view;
    binds_temp_to_input.samplers[0] = blur_smp;

    if (temp_att_view.id != SG_INVALID_ID && temp_tex_view.id != SG_INVALID_ID && input_att_view.id != SG_INVALID_ID && input_tex_view.id != SG_INVALID_ID && blur_pip.id != SG_INVALID_ID && temp_img.id != SG_INVALID_ID && blur_smp.id != SG_INVALID_ID) {
        for (int i = 0; i < passes; i++) {
            sg_pass horiz_pass = {};
            horiz_pass.action = blur_pass_action;
            horiz_pass.attachments.colors[0] = temp_att_view;
            horiz_pass.label = "blur-horizontal";
            sg_begin_pass(&horiz_pass);
            sg_apply_pipeline(blur_pip);
            sg_apply_bindings(&binds_input_to_temp);
            blur_params.type = 0; // horizontal
            sg_apply_uniforms(2, SG_RANGE(blur_params));
            sg_draw(0, 3, 1);
            sg_end_pass();

            sg_pass vert_pass = {};
            vert_pass.action = blur_pass_action;
            vert_pass.attachments.colors[0] = input_att_view;
            vert_pass.label = "blur-vertical";
            sg_begin_pass(&vert_pass);
            sg_apply_pipeline(blur_pip);
            sg_apply_bindings(&binds_temp_to_input);
            blur_params.type = 1; // vertical
            sg_apply_uniforms(2, SG_RANGE(blur_params));
            sg_draw(0, 3, 1);
            sg_end_pass();
        }
    } else {
        eprint("invalid buffers during image blur");
    }

    sg_destroy_view(temp_att_view);
    sg_destroy_view(temp_tex_view);
    sg_destroy_view(input_att_view);
    sg_destroy_view(input_tex_view);
    sg_destroy_sampler(blur_smp);
    sg_destroy_image(temp_img);
}

void init_bloom() {
    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

    sg_image_desc bloom_img_desc = {};
    bloom_img_desc.width = w_width;
    bloom_img_desc.height = w_height;
    bloom_img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    bloom_img_desc.sample_count = 1;
    bloom_img_desc.usage.color_attachment = true;
    bloom_img_desc.label = "bloom-render-target";
    bloom_img = sg_make_image(&bloom_img_desc);

    sg_image_desc bloom_depth_img_desc = {};
    bloom_depth_img_desc.width = w_width;
    bloom_depth_img_desc.height = w_height;
    bloom_depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    bloom_depth_img_desc.sample_count = 1;
    bloom_depth_img_desc.usage.depth_stencil_attachment = true;
    bloom_depth_img_desc.label = "bloom-depth-render-target";
    bloom_depth_img = sg_make_image(&bloom_depth_img_desc);

    sg_sampler_desc bloom_smp_desc = {};
    bloom_smp_desc.min_filter = SG_FILTER_LINEAR;
    bloom_smp_desc.mag_filter = SG_FILTER_LINEAR;
    bloom_smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    bloom_smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    bloom_smp = sg_make_sampler(&bloom_smp_desc);
    bloom_depth_smp = sg_make_sampler(&bloom_smp_desc);

    sg_view_desc bloom_att_desc = {};
    bloom_att_desc.color_attachment.image = bloom_img;
    bloom_att_view = sg_make_view(&bloom_att_desc);

    sg_view_desc bloom_depth_att_desc = {};
    bloom_depth_att_desc.depth_stencil_attachment.image = bloom_depth_img;
    bloom_depth_att_view = sg_make_view(&bloom_depth_att_desc);

    sg_view_desc bloom_tex_desc = {};
    bloom_tex_desc.texture.image = bloom_img;
    bloom_tex_view = sg_make_view(&bloom_tex_desc);

    sg_view_desc bloom_depth_tex_desc = {};
    bloom_depth_tex_desc.texture.image = bloom_depth_img;
    bloom_depth_tex_view = sg_make_view(&bloom_depth_tex_desc);

    sg_shader bloom_filter_shader = sg_make_shader(bloom_filter_shader_desc(sg_query_backend()));
    sg_pipeline_desc bloom_pip_desc = {};
    bloom_pip_desc.shader = bloom_filter_shader;
    bloom_pip_desc.layout.attrs[ATTR_bloom_filter_position].format = SG_VERTEXFORMAT_FLOAT3;
    bloom_pip_desc.layout.attrs[ATTR_bloom_filter_texcoord].format = SG_VERTEXFORMAT_FLOAT2;
    bloom_pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    bloom_pip_desc.color_count = 1;
    bloom_pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    bloom_pip_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    bloom_pip_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    bloom_pip_desc.depth.write_enabled = true;
    bloom_pip_desc.cull_mode = SG_CULLMODE_NONE;
    bloom_pip_desc.label = "bloom_pipeline";
    bloom_filter_pip = sg_make_pipeline(&bloom_pip_desc);

    bloom_params.threshold = 1.5f;
}

sg_image ssao_image;
sg_view ssao_att_view;
sg_view ssao_tex_view;
sg_sampler ssao_smp;
sg_pipeline ssao_pip;

void init_ssao() {
    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

    sg_image_desc ssao_img_desc = {};
    ssao_img_desc.width = w_width;
    ssao_img_desc.height = w_height;
    ssao_img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    ssao_img_desc.sample_count = 1;
    ssao_img_desc.usage.color_attachment = true;
    ssao_img_desc.label = "ssao-render-target";
    ssao_image = sg_make_image(&ssao_img_desc);

    sg_view_desc ssao_att_desc = {};
    ssao_att_desc.color_attachment.image = ssao_image;
    ssao_att_view = sg_make_view(&ssao_att_desc);

    sg_view_desc ssao_tex_desc = {};
    ssao_tex_desc.texture.image = ssao_image;
    ssao_tex_view = sg_make_view(&ssao_tex_desc);

    sg_sampler_desc ssao_smp_desc = {};
    ssao_smp_desc.min_filter = SG_FILTER_LINEAR;
    ssao_smp_desc.mag_filter = SG_FILTER_LINEAR;
    ssao_smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    ssao_smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    ssao_smp = sg_make_sampler(&ssao_smp_desc);

    sg_shader ssao_shd = sg_make_shader(ssao_gen_shader_desc(sg_query_backend()));
    sg_pipeline_desc ssao_pip_desc = {};
    ssao_pip_desc.shader = ssao_shd;
    ssao_pip_desc.layout.attrs[ATTR_ssao_gen_position].format = SG_VERTEXFORMAT_FLOAT3;
    ssao_pip_desc.layout.attrs[ATTR_ssao_gen_texcoord].format = SG_VERTEXFORMAT_FLOAT2;
    ssao_pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    ssao_pip_desc.color_count = 1;
    ssao_pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    ssao_pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    ssao_pip_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    ssao_pip_desc.depth.write_enabled = true;
    ssao_pip_desc.cull_mode = SG_CULLMODE_NONE;
    ssao_pip_desc.label = "ssao-gen-pipeline";
    ssao_pip = sg_make_pipeline(&ssao_pip_desc);

    ssao_params.ao_radius = 0.5f;
    ssao_params.ao_bias = 0.025f;
    ssao_params.ssao_samples = 12;
}

sg_image kw_output_1;
sg_view kw_output_1_att_view;
sg_view kw_output_1_tex_view;
sg_image kw_output_2;
sg_view kw_output_2_att_view;
sg_view kw_output_2_tex_view;
sg_image kw_output_3;
sg_view kw_output_3_att_view;
sg_view kw_output_3_tex_view;
sg_sampler kw_smp;
sg_pipeline kw_pip_1; // structure tensor computatation
sg_pipeline kw_pip_2; // structure tensor smoothing
sg_pipeline kw_pip_3; // actual kw
st_params_t st_params = {}; // just for texel size, seemed like the safest option

void init_kw() {
    int width, height;
    SDL_GetWindowSizeInPixels(state.win, &width, &height);

    sg_image_desc kw_img_desc_1 = {};
    kw_img_desc_1.width = width;
    kw_img_desc_1.height = height;
    kw_img_desc_1.pixel_format = SG_PIXELFORMAT_RGBA8;
    kw_img_desc_1.usage.color_attachment = true;
    kw_img_desc_1.label = "kw_output_1";
    kw_output_1 = sg_make_image(&kw_img_desc_1);

    sg_view_desc kw_view_desc_1 = {};
    kw_view_desc_1.color_attachment.image = kw_output_1;
    kw_output_1_att_view = sg_make_view(&kw_view_desc_1);

    sg_view_desc kw_tex_view_desc_1 = {};
    kw_tex_view_desc_1.texture.image = kw_output_1;
    kw_output_1_tex_view = sg_make_view(&kw_tex_view_desc_1);

    sg_image_desc kw_img_desc_2 = {};
    kw_img_desc_2.width = width;
    kw_img_desc_2.height = height;
    kw_img_desc_2.pixel_format = SG_PIXELFORMAT_RGBA8;
    kw_img_desc_2.usage.color_attachment = true;
    kw_img_desc_2.label = "kw_output_2";
    kw_output_2 = sg_make_image(&kw_img_desc_2);

    sg_view_desc kw_view_desc_2 = {};
    kw_view_desc_2.color_attachment.image = kw_output_2;
    kw_output_2_att_view = sg_make_view(&kw_view_desc_2);

    sg_view_desc kw_tex_view_desc_2 = {};
    kw_tex_view_desc_2.texture.image = kw_output_2;
    kw_output_2_tex_view = sg_make_view(&kw_tex_view_desc_2);

    sg_image_desc kw_img_desc_3 = {};
    kw_img_desc_3.width = width;
    kw_img_desc_3.height = height;
    kw_img_desc_3.pixel_format = SG_PIXELFORMAT_RGBA8;
    kw_img_desc_3.usage.color_attachment = true;
    kw_img_desc_3.label = "kw_output_3";
    kw_output_3 = sg_make_image(&kw_img_desc_3);

    sg_view_desc kw_view_desc_3 = {};
    kw_view_desc_3.color_attachment.image = kw_output_3;
    kw_output_3_att_view = sg_make_view(&kw_view_desc_3);

    sg_view_desc kw_tex_view_desc_3 = {};
    kw_tex_view_desc_3.texture.image = kw_output_3;
    kw_output_3_tex_view = sg_make_view(&kw_tex_view_desc_3);

    sg_sampler_desc kw_smp_desc = {};
    kw_smp_desc.min_filter = SG_FILTER_LINEAR;
    kw_smp_desc.mag_filter = SG_FILTER_LINEAR;
    kw_smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    kw_smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    kw_smp = sg_make_sampler(&kw_smp_desc);

    sg_pipeline_desc kw_pip_desc_1 = {};
    kw_pip_desc_1.shader = sg_make_shader(kw_st_program_shader_desc(sg_query_backend()));
    kw_pip_desc_1.layout.attrs[ATTR_kw_st_program_position].format = SG_VERTEXFORMAT_FLOAT3;
    kw_pip_desc_1.layout.attrs[ATTR_kw_st_program_texcoord].format = SG_VERTEXFORMAT_FLOAT2;
    kw_pip_desc_1.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    kw_pip_desc_1.color_count = 1;
    kw_pip_desc_1.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    kw_pip_desc_1.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    kw_pip_desc_1.depth.compare = SG_COMPAREFUNC_ALWAYS;
    kw_pip_desc_1.depth.write_enabled = true;
    kw_pip_desc_1.cull_mode = SG_CULLMODE_NONE;
    kw_pip_desc_1.label = "kuwahara_stage_1";
    kw_pip_1 = sg_make_pipeline(&kw_pip_desc_1);

    st_params.texel_size = {1.0f/width, 1.0f/height};
}

void render_kw_pass() {
    int width, height;
    SDL_GetWindowSizeInPixels(state.win, &width, &height);

    sg_pass_action pass_1_action = {};
    pass_1_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_1_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};
    pass_1_action.depth.load_action = SG_LOADACTION_CLEAR;
    pass_1_action.depth.clear_value = 0.0f;

    sg_image_desc _temp_depth_image_desc{};
    _temp_depth_image_desc.width = width;
    _temp_depth_image_desc.height = height;
    _temp_depth_image_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    _temp_depth_image_desc.usage.depth_stencil_attachment = true;
    sg_image temp_depth = sg_make_image(&_temp_depth_image_desc);
    sg_view_desc _temp_view_desc{};
    _temp_view_desc.depth_stencil_attachment.image = temp_depth;
    sg_view temp_depth_view = sg_make_view(&_temp_view_desc);

    sg_pass pass_1 = {};
    pass_1.action = pass_1_action;
    pass_1.attachments.colors[0] = kw_output_1_att_view;
    pass_1.attachments.depth_stencil = temp_depth_view;

    sg_bindings kw_binds = {};
    kw_binds.vertex_buffers[0] = {.id = SG_INVALID_ID};
    kw_binds.views[0] = post_state.rendered_color_tex_view;
    kw_binds.samplers[0] = kw_smp;

    sg_begin_pass(&pass_1);
    sg_apply_bindings(kw_binds);
    sg_apply_pipeline(kw_pip_1);
    sg_apply_uniforms(0, SG_RANGE(st_params));
    sg_draw(0, 3, 1);
    sg_end_pass();

    sg_destroy_view(temp_depth_view);
    sg_destroy_image(temp_depth);
}

void init_post_processing() {
    int width, height;
    SDL_GetWindowSizeInPixels(state.win, &width, &height);

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
    post_pip_desc.label = "post_process_pipeline";
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
    size_t given = d->data.mip_levels[0].size;
    const void *ptr = d->data.mip_levels[0].ptr;
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
        std::cout << "=========================" << std::endl;
        if (!mesh.vertices || mesh.vertex_count == 0) {
            eprint("Invalid vertex data!");
            exit(-1);
        }

        if (!mesh.indices || mesh.index_count == 0) {
            eprint("Invalid index data!");
            exit(-1);
        }

        iprint("Vertex count: " + to_string(mesh.vertex_count));
        iprint("Index count: " + to_string(mesh.index_count));

        for (size_t i = 0; i < mesh.vertex_count; ++i) {
            float px = mesh.vertices[i*8 + 0];
            float py = mesh.vertices[i*8 + 1];
            float pz = mesh.vertices[i*8 + 2];
            if (!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(pz)) {
                eprint("ERROR: Invalid vertex position at " + to_string(i));
                exit(-1);
            }
        }

        for (size_t i = 0; i < mesh.index_count; ++i) {
            if (mesh.indices[i] >= mesh.vertex_count) {
                eprint("ERROR: Index " + to_string(i) + " value " + to_string(mesh.indices[i]) + " >= vertex_count " + to_string(mesh.vertex_count));
                break;
            }
        }

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

        mesh.vertex_buffer = sg_make_buffer(&mesh.vertex_buffer_desc);
        mesh.index_buffer = sg_make_buffer(&mesh.index_buffer_desc);

        Material* material = mesh.material;

        material->base_color_sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        material->base_color_sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        material->base_color_sampler_desc.min_filter = SG_FILTER_LINEAR;
        material->base_color_sampler_desc.mag_filter = SG_FILTER_LINEAR;
        material->base_color_sampler = sg_make_sampler(&material->base_color_sampler_desc);

        material->metallic_roughness_sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        material->metallic_roughness_sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        material->metallic_roughness_sampler_desc.min_filter = SG_FILTER_LINEAR;
        material->metallic_roughness_sampler_desc.mag_filter = SG_FILTER_LINEAR;
        material->metallic_roughness_sampler = sg_make_sampler(&material->metallic_roughness_sampler_desc);

        material->normal_sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        material->normal_sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        material->normal_sampler_desc.min_filter = SG_FILTER_LINEAR;
        material->normal_sampler_desc.mag_filter = SG_FILTER_LINEAR;
        material->normal_sampler = sg_make_sampler(&material->normal_sampler_desc);

        material->emissive_sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        material->emissive_sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        material->emissive_sampler_desc.min_filter = SG_FILTER_LINEAR;
        material->emissive_sampler_desc.mag_filter = SG_FILTER_LINEAR;
        material->emissive_sampler = sg_make_sampler(&material->emissive_sampler_desc);

        if (material->has_color_texture) {
            material->base_color_image = validate_and_make_image(&material->base_color_image_desc, "diffuse");
            if (material->base_color_image.id == SG_INVALID_ID) {
                eprint("failed to create diffuse image");
                material->has_color_texture = false;
            }
        }

        if (material->has_metallic_roughness_texture) {
            material->metallic_roughness_image = validate_and_make_image(&material->metallic_roughness_image_desc, "specular");
            if (material->metallic_roughness_image.id == SG_INVALID_ID) {
                eprint("failed to create specular image");
                material->has_metallic_roughness_texture = false;
            }
        }

        if (material->has_normal_texture) {
            material->normal_image = validate_and_make_image(&material->normal_texture_desc, "normal");
            if (material->normal_image.id == SG_INVALID_ID) {
                eprint("failed to create normal image");
                material->has_normal_texture = false;
            }
        }

        if (material->has_emissive_texture) {
            material->emissive_image = validate_and_make_image(&material->emissive_image_desc, "emissive");
            if (material->emissive_image.id == SG_INVALID_ID) {
                eprint("failed to create emissive image");
                material->has_emissive_texture = false;
            }
        }

        iprint("original verts=" + to_string(vertex_count) + " -> new verts=" + to_string(mesh.vertex_count) + ", indices=" + to_string(index_count));
    };

    if (object.mesh) {
        prepare_single_mesh(*object.mesh);
        object.initialize_bounds();
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

std::pair<HMM_Vec3, HMM_Vec3> get_scene_bounds() { // TODO: occlusion culling for real this time
    struct ObjSnapshot {
        HMM_Vec3 position;
        HMM_Vec3 bounding_rect;
        HMM_Vec3 scale;
    };

    std::vector<ObjSnapshot> snaps;

    for (const auto& visgroup : state.vis_groups) {
        if (!visgroup.enabled) continue;
        for (const auto& obj : visgroup.objects) {
            if (obj.enable_shading) {
                if (obj.mesh == nullptr) continue;

                ObjSnapshot s;
                s.position = obj.position;
                s.bounding_rect = obj.bounding_rect;
                s.scale = obj.scale;

                auto finite_vec = [](const HMM_Vec3 &v) {
                    return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z);
                };
                if (!finite_vec(s.position) || !finite_vec(s.bounding_rect) || !finite_vec(s.scale)) {
                    continue;
                }

                snaps.push_back(s);
            }
        }
    }

    HMM_Vec3 min_bounds = {FLT_MAX, FLT_MAX, FLT_MAX};
    HMM_Vec3 max_bounds = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (const auto& s : snaps) {
        HMM_Vec3 half_ext = HMM_MulV3F(s.bounding_rect, 0.5f);
        half_ext.X *= fabsf(s.scale.X);
        half_ext.Y *= fabsf(s.scale.Y);
        half_ext.Z *= fabsf(s.scale.Z);

        HMM_Vec3 obj_min = HMM_SubV3(s.position, half_ext);
        HMM_Vec3 obj_max = HMM_AddV3(s.position, half_ext);

        min_bounds.X = std::min(min_bounds.X, obj_min.X);
        min_bounds.Y = std::min(min_bounds.Y, obj_min.Y);
        min_bounds.Z = std::min(min_bounds.Z, obj_min.Z);
        max_bounds.X = std::max(max_bounds.X, obj_max.X);
        max_bounds.Y = std::max(max_bounds.Y, obj_max.Y);
        max_bounds.Z = std::max(max_bounds.Z, obj_max.Z);
    }

    if (min_bounds.X == FLT_MAX) {
        min_bounds = HMM_V3(-10.0f, -10.0f, -10.0f);
        max_bounds = HMM_V3(10.0f, 10.0f, 10.0f);
    }
    return {min_bounds, max_bounds};
}

bool is_object_in_light_frustum(const Object& obj) {
    if (obj.bounding_rect.X == 0.0f && obj.bounding_rect.Y == 0.0f && obj.bounding_rect.Z == 0.0f) return true;
    float eff_dx = obj.bounding_rect.X * fabsf(obj.scale.X);
    float eff_dy = obj.bounding_rect.Y * fabsf(obj.scale.Y);
    float eff_dz = obj.bounding_rect.Z * fabsf(obj.scale.Z);
    float radius = 0.5f * sqrtf(eff_dx*eff_dx + eff_dy*eff_dy + eff_dz*eff_dz);
    HMM_Vec3 center = obj.position;
    for (int i = 0; i < 6; i++) {
        float dist = HMM_DotV3(light_frustum_planes[i].normal, center) + light_frustum_planes[i].d;
        if (dist < -radius) return false;
    }
    return true;
}

void render_shadow_meshes(const HMM_Mat4& light_view, const HMM_Mat4& light_proj) {
    struct Instance {
        Object obj;
    };
    std::vector<Instance> instances;
    for (auto& visgroup : state.vis_groups) {
        if (!visgroup.enabled) continue;
        for (size_t i = 0; i < visgroup.objects.size(); i++) {
            Object obj = visgroup.objects[i];
            if (obj.mesh != nullptr && is_object_in_light_frustum(obj)) {
                instances.push_back({obj});
            }
        }
    }
    if (instances.empty()) return;

    struct MeshGroup {
        Mesh* mesh;
        std::vector<Instance> items;
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
            groups.push_back(std::move(mg));
        }
    }

    state.bind.views[0] = {SG_INVALID_ID};
    state.bind.samplers[0] = {SG_INVALID_ID};

    for (auto &g : groups) {
        Mesh* mesh = g.mesh;
        if (!mesh || g.items.empty()) continue;
        sg_buffer vb = mesh->vertex_buffer;
        sg_buffer ib = mesh->index_buffer;
        if (vb.id == SG_INVALID_ID || ib.id == SG_INVALID_ID) continue;

        state.bind.vertex_buffers[0] = vb;
        state.bind.index_buffer = ib;
        sg_apply_bindings(&state.bind);

        for (const auto& inst : g.items) {
            const Object& obj = inst.obj;
            HMM_Mat4 translate_mat = HMM_Translate(obj.position);
            HMM_Mat4 rot_mat = HMM_QToM4(obj.rotation);
            HMM_Mat4 scale_mat = HMM_Scale(obj.scale);
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));

            vs_params_t shadow_params = {};
            shadow_params.model = model;
            shadow_params.view = light_view;
            shadow_params.projection = light_proj;
            sg_apply_uniforms(0, SG_RANGE(shadow_params));

            // originally for the skyboxes, but I think I'll just keep it.
            if (inst.obj.enable_shading) {
                sg_draw(0, mesh->index_count, 1);
            }
        }
    }
}

void render_meshes() {
    sg_view_desc view_desc{};
    view_desc.label = "skybox_view";
    view_desc.texture.image = skybox_img;

    sg_view skybox_view = sg_make_view(&view_desc);

    all_vertex_count = 0;
    all_index_count = 0;
    struct Instance {
        Object obj;
        float group_opacity;
    };
    std::vector<Instance> instances;

    for (auto& visgroup : state.vis_groups) {
        if (!visgroup.enabled) continue;
        float group_opacity = visgroup.opacity;
        for (size_t i = 0; i < visgroup.objects.size(); i++) {
            Object obj = visgroup.objects[i];
            if (obj.mesh != nullptr && is_object_in_frustum(obj)) {
                instances.push_back({obj, group_opacity});
            }
        }
    }

    if (instances.empty()) return;

    struct MeshGroup {
        Mesh* mesh;
        std::vector<Instance> items;
        sg_view diffuse_view;
        sg_view specular_view;
        sg_view normal_view;
        sg_view emissive_view;
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
        HMM_Vec4 light_positions[max_light_amount];
        HMM_Vec4 light_directions[max_light_amount];
        HMM_Vec4 light_colors[max_light_amount];
        HMM_Vec4 light_att_params[max_light_amount];
        int light_amount;
        float padding[3];
        HMM_Vec4 ambient_color;
        HMM_Mat4 light_space;
    } lights = {};

    int light_idx = 0;
    lights.light_types_packed[light_idx / 4][light_idx % 4] = 0;
    lights.light_positions[light_idx] = { 0.0f, 0.0f, 0.0f, 0.0f };
    lights.light_directions[light_idx] = { state.directional_light.direction.X, state.directional_light.direction.Y, state.directional_light.direction.Z, 0.0f };
    lights.light_colors[light_idx] = { state.directional_light.color.X, state.directional_light.color.Y, state.directional_light.color.Z, state.directional_light.intensity };
    lights.light_att_params[light_idx] = { 0.0f, 0.0f, 0.0f, 0.0f };
    light_idx++;

    for (const auto& pl : state.point_lights) {
        if (light_idx >= max_light_amount) break;
        lights.light_types_packed[light_idx / 4][light_idx % 4] = 1;
        lights.light_positions[light_idx] = { pl.position.X, pl.position.Y, pl.position.Z, 0.0f };
        lights.light_directions[light_idx] = { 0.0f, 0.0f, 0.0f, 0.0f };
        lights.light_colors[light_idx] = { pl.color.X, pl.color.Y, pl.color.Z, pl.intensity };
        lights.light_att_params[light_idx] = { pl.radius, 0.0f, 0.0f, 0.0f };
        light_idx++;
    }

    for (const auto& sl : state.spot_lights) {
        if (light_idx >= max_light_amount) break;
        lights.light_types_packed[light_idx / 4][light_idx % 4] = 2;
        lights.light_positions[light_idx] = { sl.position.X, sl.position.Y, sl.position.Z, 0.0f };
        lights.light_directions[light_idx] = { sl.direction.X, sl.direction.Y, sl.direction.Z, 0.0f };
        lights.light_colors[light_idx] = { sl.color.X, sl.color.Y, sl.color.Z, sl.intensity };
        lights.light_att_params[light_idx] = { 0.0f, sl.inner_cone_angle, sl.outer_cone_angle, 0.0f };
        light_idx++;
    }

    if (light_idx > 0) {
        HMM_Mat4 light_view;
        HMM_Mat4 light_proj;
        auto [min_bounds, max_bounds] = get_scene_bounds();
        HMM_Vec3 center = HMM_MulV3F(HMM_AddV3(min_bounds, max_bounds), 0.5f);
        HMM_Vec3 light_dir = HMM_NormV3(state.directional_light.direction);
        HMM_Vec3 light_pos = HMM_SubV3(center, HMM_MulV3F(light_dir, shadow_far * 0.5f));
        light_view = HMM_LookAt_RH(light_pos, center, HMM_V3(0.0f, 1.0f, 0.0f));
        light_proj = HMM_Orthographic_RH_NO(-shadow_ortho_size, shadow_ortho_size, -shadow_ortho_size, shadow_ortho_size, shadow_near, shadow_far);
        lights.light_space = HMM_MulM4(light_proj, light_view);
    } else {
        lights.light_space = HMM_M4D(1.0f);
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
            g.normal_view = { .id = SG_INVALID_ID };

            if (mat->has_color_texture && mat->base_color_image.id != SG_INVALID_ID) {
                sg_view_desc diffuse_view_desc = {};
                diffuse_view_desc.texture.image = mat->base_color_image;
                g.diffuse_view = sg_make_view(&diffuse_view_desc);
            }

            if (mat->has_metallic_roughness_texture && mat->metallic_roughness_image.id != SG_INVALID_ID) {
                sg_view_desc specular_view_desc = {};
                specular_view_desc.texture.image = mat->metallic_roughness_image;
                g.specular_view = sg_make_view(&specular_view_desc);
            }

            if (mat->has_normal_texture && mat->normal_image.id != SG_INVALID_ID) {
                sg_view_desc normal_view_desc = {};
                normal_view_desc.texture.image = mat->normal_image;
                g.normal_view = sg_make_view(&normal_view_desc);
            }

            if (mat->has_emissive_texture && mat->emissive_image.id != SG_INVALID_ID) {
                sg_view_desc emissive_view_desc = {};
                emissive_view_desc.texture.image = mat->emissive_image;
                g.emissive_view = sg_make_view(&emissive_view_desc);
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
            if (g.diffuse_view.id != SG_INVALID_ID) {
                state.bind.views[0] = g.diffuse_view;
                state.bind.samplers[0] = mat->base_color_sampler;
            } else {
                state.bind.views[0] = { .id = SG_INVALID_ID };
                state.bind.samplers[0] = { .id = SG_INVALID_ID };
                eprint("invalid diffuse texture view while rendering");
            }

            if (g.specular_view.id != SG_INVALID_ID) {
                state.bind.views[1] = g.specular_view;
                state.bind.samplers[1] = mat->metallic_roughness_sampler;
            } else {
                state.bind.views[1] = { .id = SG_INVALID_ID };
                state.bind.samplers[1] = { .id = SG_INVALID_ID };
                eprint("invalid specular texture view while rendering");
            }

            if (g.normal_view.id != SG_INVALID_ID) {
                state.bind.views[2] = g.normal_view;
                state.bind.samplers[2] = mat->normal_sampler;
            } else {
                state.bind.views[2] = { .id = SG_INVALID_ID };
                state.bind.samplers[2] = { .id = SG_INVALID_ID };
                eprint("invalid normal texture view while rendering");
            }

            if (g.emissive_view.id != SG_INVALID_ID) {
                state.bind.views[3] = g.emissive_view;
                state.bind.samplers[3] = mat->emissive_sampler;
            } else {
                state.bind.views[3] = { .id = SG_INVALID_ID };
                state.bind.samplers[3] = { .id = SG_INVALID_ID };
                eprint("invalid emissive texture view while rendering");
            }

            state.bind.views[4] = shadow_depth_tex_view;
            state.bind.samplers[4] = shadow_sampler;
            state.bind.views[5] = skybox_view;
            state.bind.samplers[5] = skybox_sampler;

            sg_apply_bindings(&state.bind);

            HMM_Mat4 translate_mat = HMM_Translate(obj.position);
            HMM_Mat4 rot_mat = HMM_QToM4(obj.rotation);
            HMM_Mat4 scale_mat = HMM_Scale(obj.scale);
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));

            vs_params.model = model;
            vs_params.opacity = obj.opacity * inst.group_opacity;
            vs_params.enable_shading = obj.enable_shading ? 1 : 0;
            vs_params.light_space = lights.light_space;
            sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

            model_fs_params_t model_fs_params;

            model_fs_params.shininess = 128.0f;
            model_fs_params.camera_pos[0] = state.camera_pos.X;
            model_fs_params.camera_pos[1] = state.camera_pos.Y;
            model_fs_params.camera_pos[2] = state.camera_pos.Z;
            model_fs_params.camera_pos[3] = 1.0f;
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
            if (g.normal_view.id != SG_INVALID_ID) {
                sg_destroy_view(g.normal_view);
            }
            if (g.emissive_view.id != SG_INVALID_ID) {
                sg_destroy_view(g.emissive_view);
            }
        }
    }
    sg_destroy_view(skybox_view);
}

void render_visualizers();

void render_state_surf() {
    if (state.window_surface.pixels.empty() || state.window_surface.pixels[0].empty()) {
        wprint("the window surface is empty");
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

void render_skybox() {
    sg_apply_pipeline(skybox_pipeline);

    if (skybox_vbuf.id == SG_INVALID_ID || skybox_ibuf.id == SG_INVALID_ID) {
        wprint("invalid skybox buffer");
    }
    state.bind.vertex_buffers[0] = skybox_vbuf;
    state.bind.index_buffer = skybox_ibuf;

    sg_view_desc view_desc{};
    view_desc.label = "skybox_view";
    view_desc.texture.image = skybox_img;

    sg_view skybox_view = sg_make_view(&view_desc);

    state.bind.views[0] = skybox_view;
    state.bind.samplers[0] = skybox_sampler;
    sg_apply_bindings(&state.bind);

    skybox_vs_params_t skybox_vs_params;
    skybox_vs_params.projection = vs_params.projection;
    skybox_vs_params.view = vs_params.view;

    sg_apply_uniforms(UB_skybox_vs_params, SG_RANGE(skybox_vs_params));

    sg_draw(0, 36, 1);

    sg_destroy_view(skybox_view);
}

void render_shadow_pass() {
    if (shadow_depth_img.id == SG_INVALID_ID) {
        sg_image_desc shadow_img_desc = {};
        shadow_img_desc.usage.depth_stencil_attachment = true;
        shadow_img_desc.width = shadow_map_size;
        shadow_img_desc.height = shadow_map_size;
        shadow_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
        shadow_img_desc.sample_count = 1;
        shadow_img_desc.label = "shadow-depth-target";
        shadow_depth_img = sg_make_image(&shadow_img_desc);

        sg_view_desc depth_att_view_desc2 = {};
        depth_att_view_desc2.depth_stencil_attachment.image = shadow_depth_img;
        shadow_depth_att_view = sg_make_view(&depth_att_view_desc2);

        sg_view_desc depth_tex_view_desc2 = {};
        depth_tex_view_desc2.texture.image = shadow_depth_img;
        shadow_depth_tex_view = sg_make_view(&depth_tex_view_desc2);
    }

    if (shadow_depth_img.id == SG_INVALID_ID) {
        eprint("could create shadow depth image");
        return;
    }

    auto [min_bounds, max_bounds] = get_scene_bounds();

    HMM_Vec3 extent = HMM_SubV3(max_bounds, min_bounds);
    float max_lateral = std::max(extent.X, extent.Y) * 0.5f;
    shadow_ortho_size = std::max(15.0f, max_lateral * 1.1f);
    shadow_far = std::max(100.0f, extent.Z * 1.1f + 50.0f);

    HMM_Vec3 center = HMM_MulV3F(HMM_AddV3(min_bounds, max_bounds), 0.5f);
    HMM_Vec3 light_dir = HMM_NormV3(state.directional_light.direction);
    HMM_Vec3 light_pos = HMM_SubV3(center, HMM_MulV3F(light_dir, shadow_far * 0.5f));
    HMM_Mat4 light_view = HMM_LookAt_RH(light_pos, center, HMM_V3(0.0f, 1.0f, 0.0f));
    HMM_Mat4 light_proj = HMM_Orthographic_RH_NO(-shadow_ortho_size, shadow_ortho_size, -shadow_ortho_size, shadow_ortho_size, shadow_near, shadow_far);

    HMM_Mat4 light_clip = HMM_MulM4(light_proj, light_view);
    HMM_Vec4 lrow0 = {light_clip.Elements[0][0], light_clip.Elements[1][0], light_clip.Elements[2][0], light_clip.Elements[3][0]};
    HMM_Vec4 lrow1 = {light_clip.Elements[0][1], light_clip.Elements[1][1], light_clip.Elements[2][1], light_clip.Elements[3][1]};
    HMM_Vec4 lrow2 = {light_clip.Elements[0][2], light_clip.Elements[1][2], light_clip.Elements[2][2], light_clip.Elements[3][2]};
    HMM_Vec4 lrow3 = {light_clip.Elements[0][3], light_clip.Elements[1][3], light_clip.Elements[2][3], light_clip.Elements[3][3]};

    HMM_Vec4 left = HMM_AddV4(lrow3, lrow0);
    float len_left = HMM_LenV3(left.XYZ);
    if (len_left > 0.0001f) {
        light_frustum_planes[0].normal = HMM_DivV3F(left.XYZ, len_left);
        light_frustum_planes[0].d = left.W / len_left;
    }

    HMM_Vec4 right = HMM_SubV4(lrow3, lrow0);
    float len_right = HMM_LenV3(right.XYZ);
    if (len_right > 0.0001f) {
        light_frustum_planes[1].normal = HMM_DivV3F(right.XYZ, len_right);
        light_frustum_planes[1].d = right.W / len_right;
    }

    HMM_Vec4 bottom = HMM_AddV4(lrow3, lrow1);
    float len_bottom = HMM_LenV3(bottom.XYZ);
    if (len_bottom > 0.0001f) {
        light_frustum_planes[2].normal = HMM_DivV3F(bottom.XYZ, len_bottom);
        light_frustum_planes[2].d = bottom.W / len_bottom;
    }

    HMM_Vec4 top = HMM_SubV4(lrow3, lrow1);
    float len_top = HMM_LenV3(top.XYZ);
    if (len_top > 0.0001f) {
        light_frustum_planes[3].normal = HMM_DivV3F(top.XYZ, len_top);
        light_frustum_planes[3].d = top.W / len_top;
    }

    HMM_Vec4 near_p = HMM_AddV4(lrow3, lrow2);
    float len_near = HMM_LenV3(near_p.XYZ);
    if (len_near > 0.0001f) {
        light_frustum_planes[4].normal = HMM_DivV3F(near_p.XYZ, len_near);
        light_frustum_planes[4].d = near_p.W / len_near;
    }

    HMM_Vec4 far_p = HMM_SubV4(lrow3, lrow2);
    float len_far = HMM_LenV3(far_p.XYZ);
    if (len_far > 0.0001f) {
        light_frustum_planes[5].normal = HMM_DivV3F(far_p.XYZ, len_far);
        light_frustum_planes[5].d = far_p.W / len_far;
    }

    sg_image_desc shadow_img_desc = {};
    shadow_img_desc.usage.color_attachment = true;
    shadow_img_desc.width = shadow_map_size;
    shadow_img_desc.height = shadow_map_size;
    shadow_img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    shadow_img_desc.sample_count = 1;
    shadow_img_desc.label = "shadow-color-target";
    sg_image shadow_img = sg_make_image(&shadow_img_desc);

    sg_view_desc shadow_att_view_desc = {};
    shadow_att_view_desc.color_attachment.image = shadow_img;
    sg_view shadow_att_view = sg_make_view(&shadow_att_view_desc);

    sg_pass_action shadow_action = {};
    shadow_action.depth.load_action = SG_LOADACTION_CLEAR;
    shadow_action.depth.clear_value = 1.0f;
    shadow_action.depth.store_action = SG_STOREACTION_STORE;
    shadow_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    shadow_action.colors[0].clear_value = {1.0f, 1.0f, 1.0f, 1.0f};
    sg_pass shadow_pass = {};
    shadow_pass.action = shadow_action;
    shadow_pass.attachments.depth_stencil = shadow_depth_att_view;
    shadow_pass.attachments.colors[0] = shadow_att_view;
    shadow_pass.label = "shadow-pass";

    sg_begin_pass(&shadow_pass);

    sg_apply_pipeline(shadow_pip);
    render_shadow_meshes(light_view, light_proj);

    sg_end_pass();

    sg_destroy_view(shadow_att_view);
    sg_destroy_image(shadow_img);
}

void render_offscreen_pass() {
    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

    render_shadow_pass();

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
        cerr << "Could create offscreen image, sorry" << endl;
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

    render_skybox();

    render_meshes();

    if (state.editor_open) render_visualizers();
    _draw_all_billboards(state.camera_pos);

    for (auto& psys : state.particle_systems) {
        psys.draw_particles(particle_pipeline, time_state.dt, vs_params.projection, vs_params.view, state.camera_pos);
    }

    sg_end_pass();
}

void render_bloom_pass() {
    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

    if (bloom_img.id == SG_INVALID_ID) {
        if (bloom_img.id != SG_INVALID_ID) {
            sg_destroy_view(bloom_att_view);
            sg_destroy_view(bloom_depth_att_view);
            sg_destroy_view(bloom_tex_view);
            sg_destroy_view(bloom_depth_tex_view);
            sg_destroy_image(bloom_img);
            sg_destroy_image(bloom_depth_img);
        }

        sg_image_desc bloom_img_desc = {};
        bloom_img_desc.width = w_width;
        bloom_img_desc.height = w_height;
        bloom_img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        bloom_img_desc.sample_count = 1;
        bloom_img_desc.usage.color_attachment = true;
        bloom_img_desc.label = "bloom-render-target";
        bloom_img = sg_make_image(&bloom_img_desc);

        sg_image_desc bloom_depth_img_desc = {};
        bloom_depth_img_desc.width = w_width;
        bloom_depth_img_desc.height = w_height;
        bloom_depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
        bloom_depth_img_desc.sample_count = 1;
        bloom_depth_img_desc.usage.depth_stencil_attachment = true;
        bloom_depth_img_desc.label = "bloom-depth-render-target";
        bloom_depth_img = sg_make_image(&bloom_depth_img_desc);

        sg_view_desc bloom_att_desc = {};
        bloom_att_desc.color_attachment.image = bloom_img;
        bloom_att_view = sg_make_view(&bloom_att_desc);

        sg_view_desc bloom_depth_att_desc = {};
        bloom_depth_att_desc.depth_stencil_attachment.image = bloom_depth_img;
        bloom_depth_att_view = sg_make_view(&bloom_depth_att_desc);

        sg_view_desc bloom_tex_desc = {};
        bloom_tex_desc.texture.image = bloom_img;
        bloom_tex_view = sg_make_view(&bloom_tex_desc);

        sg_view_desc bloom_depth_tex_desc = {};
        bloom_depth_tex_desc.texture.image = bloom_depth_img;
        bloom_depth_tex_view = sg_make_view(&bloom_depth_tex_desc);
    }

    if (bloom_img.id == SG_INVALID_ID) {
        eprint("invalid bloom image");
        return;
    }

    sg_pass_action offscreen_pass_action = {};
    offscreen_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    offscreen_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};
    offscreen_pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    offscreen_pass_action.depth.clear_value = 1.0f;

    sg_pass pass = {};
    pass.action = offscreen_pass_action;
    pass.attachments.colors[0] = bloom_att_view;
    pass.attachments.depth_stencil = bloom_depth_att_view;
    pass.label = "bloom_filter_pass";

    sg_begin_pass(&pass);

    sg_apply_pipeline(bloom_filter_pip);

    sg_bindings bindies;
    bindies.vertex_buffers[0] = {.id = SG_INVALID_ID};
    bindies.views[0] = post_state.rendered_color_tex_view;
    bindies.samplers[0] = post_state.rendered_post_sampler;
    bindies.views[1] = post_state.rendered_depth_tex_view;
    bindies.samplers[1] = post_state.rendered_depth_sampler;
    sg_apply_bindings(&bindies);

    sg_apply_uniforms(UB_bloom_filter_params, SG_RANGE(bloom_params));

    sg_draw(0, 3, 1);

    sg_end_pass();

    blur_image(bloom_img, 1.025f, 10);
}

void render_ssao_pass() {
    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

    HMM_Vec2 ssao_proj{};
    ssao_proj.Y = tanf((state.fov * (HMM_PI32 / 180.0f)) * 0.5f);
    ssao_proj.X = ssao_proj.Y * (static_cast<float>(w_width) / static_cast<float>(w_height));
    ssao_params.proj = ssao_proj;
    ssao_params.screen_size = HMM_Vec2{static_cast<float>(w_width), static_cast<float>(w_height)};
    ssao_params.u_near = camera_near;
    ssao_params.u_far = camera_far;

    if (ssao_image.id == SG_INVALID_ID || ssao_pip.id == SG_INVALID_ID || ssao_att_view.id == SG_INVALID_ID || ssao_tex_view.id == SG_INVALID_ID) {
        eprint("invalid id somewhere in ssao");
        return;
    }

    sg_image_desc depth_img_desc = {};
    depth_img_desc.width = w_width;
    depth_img_desc.height = w_height;
    depth_img_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    depth_img_desc.sample_count = 1;
    depth_img_desc.usage.depth_stencil_attachment = true;
    depth_img_desc.label = "ssao-depth-render-target";
    sg_image depth_img = sg_make_image(&depth_img_desc);

    sg_view_desc depth_att_desc = {};
    depth_att_desc.depth_stencil_attachment.image = depth_img;
    sg_view depth_att_view = sg_make_view(&depth_att_desc);

    if (depth_img.id == SG_INVALID_ID || depth_att_view.id == SG_INVALID_ID) {
        eprint("invalid depth image");
        return;
    }

    sg_pass_action pass_action = {};
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};
    pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    pass_action.depth.clear_value = 1.0f;

    sg_pass pass = {};
    pass.action = pass_action;
    pass.attachments.colors[0] = ssao_att_view;
    pass.attachments.depth_stencil = depth_att_view;
    pass.label = "ssao_pass";

    sg_bindings binds = {};
    binds.vertex_buffers[0] = {.id = SG_INVALID_ID};
    binds.views[0] = post_state.rendered_depth_tex_view;
    binds.samplers[0] = post_state.rendered_depth_sampler;

    sg_begin_pass(&pass);

    sg_apply_pipeline(ssao_pip);
    sg_apply_bindings(&binds);
    sg_apply_uniforms(UB_ssao_params, SG_RANGE(ssao_params));
    sg_draw(0, 3, 1);

    sg_end_pass();

    blur_image(ssao_image, 1.0f, 3);

    sg_destroy_view(depth_att_view);
    sg_destroy_image(depth_img);
}

void render_pp_pass() {
    if (post_state.rendered_color_img.id == SG_INVALID_ID) {
        printf("ERROR: No valid color image from first pass!\n");
        return;
    }

    sg_pass_action swapchain_pass_action = {};
    swapchain_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    swapchain_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

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

    post_state.uniforms.time = stm_sec(stm_now());

    post_state.post_bindings.vertex_buffers[0] = {.id = SG_INVALID_ID};
    post_state.post_bindings.views[0] = post_state.rendered_color_tex_view;
    post_state.post_bindings.samplers[0] = post_state.rendered_post_sampler;
    post_state.post_bindings.views[1] = post_state.rendered_depth_tex_view;
    post_state.post_bindings.samplers[1] = post_state.rendered_depth_sampler;
    post_state.post_bindings.views[2] = bloom_tex_view;
    post_state.post_bindings.samplers[2] = bloom_smp;
    post_state.post_bindings.views[3] = ssao_tex_view;
    post_state.post_bindings.samplers[3] = ssao_smp;

    sg_apply_bindings(&post_state.post_bindings);
    sg_apply_uniforms(0, SG_RANGE(post_state.uniforms));

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
    Object* obj;
    float distance;
};

RaycastResult raycast_from_screen(float mx, float my) {
    RaycastResult result { false, nullptr, std::numeric_limits<float>::max() };

    int width, height;
    SDL_GetWindowSizeInPixels(state.win, &width, &height);

    float x = (2.0f * mx) / width - 1.0f;
    float y = 1.0f - (2.0f * my) / height;

    HMM_Vec4 ray_clip = HMM_V4(x, y, -1.0f, 1.0f);

    HMM_Mat4 inverse_proj = HMM_InvGeneralM4(vs_params.projection);
    HMM_Vec4 ray_eye = HMM_MulM4V4(inverse_proj, ray_clip);
    ray_eye = HMM_V4(ray_eye.X, ray_eye.Y, -1.0f, 0.0f);

    HMM_Mat4 inverse_view = HMM_InvGeneralM4(vs_params.view);
    HMM_Vec4 ray_world = HMM_MulM4V4(inverse_view, ray_eye);

    HMM_Vec3 ray_direction = HMM_NormV3(ray_world.XYZ);
    HMM_Vec3 ray_origin = state.camera_pos;

    std::vector<std::pair<const Object*, float>> candidates;

    for (const auto& vg : state.vis_groups) {
        if (!vg.enabled) continue;

        for (const auto& obj : vg.objects) {
            if (obj.mesh == nullptr || !obj.enable_shading) continue;

            float dist_sq = HMM_LenSqrV3(HMM_SubV3(obj.position, ray_origin));
            candidates.emplace_back(&obj, dist_sq);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });

    float current_min_dist = result.distance;

    for (const auto& pair : candidates) {
        const Object* obj = pair.first;

        HMM_Vec3 half_ext = HMM_MulV3F(obj->bounding_rect, 0.5f);
        half_ext.X *= std::abs(obj->scale.X);
        half_ext.Y *= std::abs(obj->scale.Y);
        half_ext.Z *= std::abs(obj->scale.Z);

        HMM_Vec3 min_bb = HMM_SubV3(obj->position, half_ext);
        HMM_Vec3 max_bb = HMM_AddV3(obj->position, half_ext);

        float tmin = 0.0f;
        float tmax = current_min_dist;

        for (int i = 0; i < 3; ++i) {
            float dir_comp = ray_direction.Elements[i];

            if (std::abs(dir_comp) < 1e-6f) {
                float orig_comp = ray_origin.Elements[i];

                if (orig_comp < min_bb.Elements[i] || orig_comp > max_bb.Elements[i]) {
                    goto next_object;
                }
            } else {
                float ood = 1.0f / dir_comp;

                float t1 = (min_bb.Elements[i] - ray_origin.Elements[i]) * ood;
                float t2 = (max_bb.Elements[i] - ray_origin.Elements[i]) * ood;

                if (t1 > t2) std::swap(t1, t2);

                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);

                if (tmin > tmax) goto next_object;
            }
        }

        if (tmin > 0.0f && tmin < current_min_dist) {
            result.hit = true;
            result.obj = const_cast<Object*>(obj);
            result.distance = tmin;
            current_min_dist = tmin;
        }

    next_object:;
    }

    return result;
}

RaycastResult raycast_point_to_point(HMM_Vec3 start, HMM_Vec3 end) {
    RaycastResult result { false, nullptr, std::numeric_limits<float>::max() };

    HMM_Vec3 ray_direction = HMM_SubV3(end, start);
    float segment_length = HMM_LenV3(ray_direction);

    if (segment_length < 1e-6f) {
        return result;
    }

    ray_direction = HMM_DivV3F(ray_direction, segment_length);

    std::vector<std::pair<const Object*, float>> candidates;

    for (const auto& vg : state.vis_groups) {
        if (!vg.enabled) continue;

        for (const auto& obj : vg.objects) {
            if (obj.mesh == nullptr || !obj.enable_shading) continue;

            float dist_sq = HMM_LenSqrV3(HMM_SubV3(obj.position, start));
            candidates.emplace_back(&obj, dist_sq);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });

    float current_min_dist = result.distance;

    for (const auto& pair : candidates) {
        const Object* obj = pair.first;

        HMM_Vec3 half_ext = HMM_MulV3F(obj->bounding_rect, 0.5f);
        half_ext.X *= std::abs(obj->scale.X);
        half_ext.Y *= std::abs(obj->scale.Y);
        half_ext.Z *= std::abs(obj->scale.Z);

        HMM_Vec3 min_bb = HMM_SubV3(obj->position, half_ext);
        HMM_Vec3 max_bb = HMM_AddV3(obj->position, half_ext);

        float tmin = 0.0f;
        float tmax = std::min(segment_length, current_min_dist);

        for (int i = 0; i < 3; ++i) {
            float dir_comp = ray_direction.Elements[i];

            if (std::abs(dir_comp) < 1e-6f) {
                float orig_comp = start.Elements[i];

                if (orig_comp < min_bb.Elements[i] || orig_comp > max_bb.Elements[i]) {
                    goto next_object;
                }
            } else {
                float ood = 1.0f / dir_comp;

                float t1 = (min_bb.Elements[i] - start.Elements[i]) * ood;
                float t2 = (max_bb.Elements[i] - start.Elements[i]) * ood;

                if (t1 > t2) std::swap(t1, t2);

                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);

                if (tmin > tmax) goto next_object;
            }
        }

        if (tmin >= 0.0f && tmin < current_min_dist) {
            result.hit = true;
            result.obj = const_cast<Object*>(obj);
            result.distance = tmin;
            current_min_dist = tmin;
        }

    next_object:;
    }

    return result;
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

    if (!warn.empty()) wprint("GLTF WARN: " + warn);
    if (!err.empty()) wprint("GLTF ERROR: " + err);
    if (!res) {
        eprint("Failed to load GLB from path " + filename);
        return vector<Object>();
    }

    auto loadTexture = [&](int textureIndex) -> std::pair<uint8_t*, sg_image_desc> {
        iprint("loading texture");
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
                    img_desc.data.mip_levels[0].ptr = texture_data;
                    img_desc.data.mip_levels[0].size = data_size;
                } else {
                    const unsigned char* src = image.image.data();
                    auto [expanded, expanded_size] = expand_to_rgba(src, w, h, channels);
                    flip_tex_vertically_local(expanded, w, h, 4);
                    texture_data = expanded;
                    img_desc.width = w;
                    img_desc.height = h;
                    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                    img_desc.data.mip_levels[0].ptr = texture_data;
                    img_desc.data.mip_levels[0].size = expanded_size;
                }
            } else {
                return {nullptr, {}};
            }
        } else if (!image.uri.empty()) {
            std::string base_dir = filename.substr(0, filename.find_last_of("/\\") + 1);
            std::string full_path = base_dir + image.uri;

            iprint("GLB texture loading from path " + full_path);

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
                img_desc.data.mip_levels[0].ptr = texture_data;
                img_desc.data.mip_levels[0].size = data_size;
                stbi_image_free(pixels);
            }
        } else if (image.bufferView >= 0) {
            const auto& bufferView = model.bufferViews[image.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            iprint("GLB texture loading from file");

            const uint8_t* data = buffer.data.data() + bufferView.byteOffset;
            size_t data_size = bufferView.byteLength;

            int img_width = 0, img_height = 0, num_channels = 0;
            const int desired_channels = 4;
            stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(data_size), &img_width, &img_height, &num_channels, desired_channels);

            if (pixels) {
                img_desc.width = img_width;
                img_desc.height = img_height;
                img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;

                size_t pixel_data_size = (size_t)img_width * (size_t)img_height * desired_channels;
                texture_data = new uint8_t[pixel_data_size];
                memcpy(texture_data, pixels, pixel_data_size);
                flip_tex_vertically_local(texture_data, img_width, img_height, desired_channels);
                img_desc.data.mip_levels[0].ptr = texture_data;
                img_desc.data.mip_levels[0].size = pixel_data_size;

                stbi_image_free(pixels);
            }
        }

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

        Object obj;

        Mesh* base_mesh = new Mesh();
        base_mesh->vertices = base_vertices;
        base_mesh->vertex_count = vcount;
        base_mesh->indices = base_indices;
        base_mesh->index_count = icount;

        Material* mat = new Material();
        if (primitive.material >= 0 && primitive.material < model.materials.size()) {
            const auto& material = model.materials[primitive.material];

            // TODO: load material alpha

            if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                auto [texture_data, img_desc] = loadTexture(material.pbrMetallicRoughness.baseColorTexture.index);

                if (texture_data && img_desc.width > 0) {
                    mat->base_color_image_data = texture_data;
                    mat->base_color_image_desc = img_desc;
                    mat->base_color_image_data_size = img_desc.data.mip_levels[0].size;
                    mat->has_color_texture = true;
                }
            } else {
                Surface default_surface;
                default_surface.clear(16, 16, {static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]), static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]), static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]), 1.0f});
                default_surface.get_sokol_image_data();
                mat->base_color_image_data = new uint8_t[default_surface.sokol_data_u8.size()];;
                memcpy(mat->base_color_image_data, default_surface.sokol_data_u8.data(), default_surface.sokol_data_u8.size());
                mat->base_color_image_data_size = default_surface.sokol_data_u8.size();
                mat->base_color_image_desc.width = default_surface.pixels[0].size();
                mat->base_color_image_desc.height = default_surface.pixels.size();
                mat->base_color_image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                mat->base_color_image_desc.data.mip_levels[0].ptr = mat->base_color_image_data;
                mat->base_color_image_desc.data.mip_levels[0].size = mat->base_color_image_data_size;
                mat->base_color_image = validate_and_make_image(&mat->base_color_image_desc, "base_color");
                mat->has_color_texture = true;
                iprint("no base color image in GLTF, loading default values");
            }

            if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
                auto [u_specular_texture_data, u_specular_img_desc] = loadTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index);

                if (u_specular_texture_data && u_specular_img_desc.width > 0) {
                    mat->metallic_roughness_image_data = u_specular_texture_data;
                    mat->metallic_roughness_image_desc = u_specular_img_desc;
                    mat->metallic_roughness_image_data_size = u_specular_img_desc.data.mip_levels[0].size;
                    mat->has_metallic_roughness_texture = true;
                }
            } else {
                Surface default_surface;
                default_surface.clear(16, 16, {0.0f, static_cast<float>(material.pbrMetallicRoughness.metallicFactor), static_cast<float>(material.pbrMetallicRoughness.roughnessFactor), 1.0f});
                default_surface.get_sokol_image_data();
                mat->metallic_roughness_image_data = new uint8_t[default_surface.sokol_data_u8.size()];
                memcpy(mat->metallic_roughness_image_data, default_surface.sokol_data_u8.data(), default_surface.sokol_data_u8.size());
                mat->metallic_roughness_image_data_size = default_surface.sokol_data_u8.size();
                mat->metallic_roughness_image_desc.width = default_surface.pixels[0].size();
                mat->metallic_roughness_image_desc.height = default_surface.pixels.size();
                mat->metallic_roughness_image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                mat->metallic_roughness_image_desc.data.mip_levels[0].ptr = mat->metallic_roughness_image_data;
                mat->metallic_roughness_image_desc.data.mip_levels[0].size = mat->metallic_roughness_image_data_size;
                mat->metallic_roughness_image = validate_and_make_image(&mat->metallic_roughness_image_desc, "mr_image");
                mat->has_metallic_roughness_texture = true;
                iprint("no metallic roughness image in GLTF, loading default values");
            }

            if (material.normalTexture.index >= 0) {
                auto [u_normal_texture_data, u_normal_img_desc] = loadTexture(material.normalTexture.index);

                if (u_normal_texture_data && u_normal_img_desc.width > 0) {
                    mat->normal_texture_data = u_normal_texture_data;
                    mat->normal_texture_desc = u_normal_img_desc;
                    mat->normal_texture_data_size = u_normal_img_desc.data.mip_levels[0].size;
                    mat->has_normal_texture = true;
                }
            } else {
                Surface default_surface;
                default_surface.clear(16, 16, {0.5f, 0.5f, 1.0f, 1.0f});
                default_surface.get_sokol_image_data();
                mat->normal_texture_data = new uint8_t[default_surface.sokol_data_u8.size()];
                memcpy(mat->normal_texture_data, default_surface.sokol_data_u8.data(), default_surface.sokol_data_u8.size());
                mat->normal_texture_data_size = default_surface.sokol_data_u8.size();
                mat->normal_texture_desc.width = default_surface.pixels[0].size();
                mat->normal_texture_desc.height = default_surface.pixels.size();
                mat->normal_texture_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                mat->normal_texture_desc.data.mip_levels[0].ptr = mat->normal_texture_data;
                mat->normal_texture_desc.data.mip_levels[0].size = mat->normal_texture_data_size;
                mat->normal_image = validate_and_make_image(&mat->normal_texture_desc, "normal_image");
                mat->has_normal_texture = true;
                iprint("no normal texture in GLTF, loading default values");
            }

            if (material.emissiveTexture.index >= 0) {
                auto [u_emissive_texture_data, u_emissive_img_desc] = loadTexture(material.emissiveTexture.index);

                if (u_emissive_texture_data && u_emissive_img_desc.width > 0) {
                    mat->emissive_image_data = u_emissive_texture_data;
                    mat->emissive_image_data_size = u_emissive_img_desc.data.mip_levels[0].size;
                    mat->emissive_image_desc = u_emissive_img_desc;
                    mat->has_emissive_texture = true;
                }
            }  else {
                Surface default_surface;
                default_surface.clear(16, 16, {0.0f, 0.0f, 0.0f, 1.0f});
                default_surface.get_sokol_image_data();
                mat->emissive_image_data = new uint8_t[default_surface.sokol_data_u8.size()];
                memcpy(mat->emissive_image_data, default_surface.sokol_data_u8.data(), default_surface.sokol_data_u8.size());
                mat->emissive_image_data_size = default_surface.sokol_data_u8.size();
                mat->emissive_image_desc.width = default_surface.pixels[0].size();
                mat->emissive_image_desc.height = default_surface.pixels.size();
                mat->emissive_image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
                mat->emissive_image_desc.data.mip_levels[0].ptr = mat->emissive_image_data;
                mat->emissive_image_desc.data.mip_levels[0].size = mat->emissive_image_data_size;
                mat->emissive_image = validate_and_make_image(&mat->emissive_image_desc, "normal_image");
                mat->has_emissive_texture = true;
                iprint("no emissive texture in GLTF, loading default values");
            }
        }

        base_mesh->material = mat;

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

void print_fmod_error(FMOD_RESULT result) {
    if (result != FMOD_OK)
    {
        eprint("FMOD error: " + to_string(result) + "; " + to_string(*FMOD_ErrorString(result)));
        exit(-1);
    }
}

class AudioSource3D {
    public:
        FMOD::Studio::EventInstance* event_instance;
        HMM_Vec3 position;
        FMOD_GUID guid;
        int script_id = -1;

        void initialize(FMOD::Studio::EventDescription* desc, HMM_Vec3 pos) {
            FMOD_RESULT result = desc->createInstance(&event_instance);
            print_fmod_error(result);
            state.audio_sources.push_back(this);
            position = pos;

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
        }

        void remove() {
            state.audio_sources.erase(std::remove(state.audio_sources.begin(), state.audio_sources.end(), this),state.audio_sources.end());

            event_instance->release();
        }
};

class Helper {
public:
    HMM_Vec3 position;
    string name;
    bool operator==(const Helper& other) const { return this == &other; }

    void initialize(const string& name, HMM_Vec3 pos) {
        this->name = name;
        position = pos;

        state.helpers.push_back(this);
    }

    void set_position(HMM_Vec3 pos) {
        position = pos;
    }

    void remove() {
        state.helpers.erase(std::remove(state.helpers.begin(), state.helpers.end(), this),state.helpers.end());
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

void render_visualizers() {
    for (auto& as : state.audio_sources) {
        draw_billboard(&as_visualizer, as->position, 0.25f);
    }
    for (auto& light : state.point_lights) {
        light_visualizer.color.XYZ = light.color;
        draw_billboard(&light_visualizer, light.position, 0.25f);
    }
    for (auto& light : state.spot_lights) {
        light_visualizer.color.XYZ = light.color;
        draw_billboard(&light_visualizer, light.position, 0.25f);
    }
    for (auto& helper : state.helpers) {
        draw_billboard(&hpr_visualizer, helper->position, 0.25f);
    }
}

void clear_scene() {
    for (auto* as : state.audio_sources) {
        as->remove();
    }
    state.audio_sources.clear();

    for (auto& visgroup : state.vis_groups) {
        /*for (auto& obj : visgroup.objects) {
            obj.shape_keys.clear();
        }*/
        visgroup.objects.clear();
    }
    state.vis_groups.clear();

    for (auto* hpr : state.helpers) {
        hpr->remove();
    }
    state.helpers.clear();

    state.point_lights.clear();
    state.spot_lights.clear();
}

void save_scene(const string& path) {
    nlohmann::json j;

    j["visgroups"] = nlohmann::json::array();
    for (auto& visgroup : state.vis_groups) {
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

                mesh_json["shading"] = obj.enable_shading;

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

                    if (material->has_color_texture && material->base_color_image_data && material->base_color_image_data_size > 0) {
                        mat_json["diff_tex"] = nlohmann::json::object();
                        auto& diff_tex = mat_json["diff_tex"];
                        diff_tex["w"] = material->base_color_image_desc.width;
                        diff_tex["h"] = material->base_color_image_desc.height;
                        diff_tex["fmt"] = static_cast<int>(material->base_color_image_desc.pixel_format);

                        std::vector<uint8_t> tex_data(material->base_color_image_data,
                                                     material->base_color_image_data + material->base_color_image_data_size);
                        diff_tex["data"] = tex_data;

                        diff_tex["samp"] = {
                            static_cast<int>(material->base_color_sampler_desc.min_filter),
                            static_cast<int>(material->base_color_sampler_desc.mag_filter),
                            static_cast<int>(material->base_color_sampler_desc.wrap_u),
                            static_cast<int>(material->base_color_sampler_desc.wrap_v)
                        };
                    }

                    if (material->has_metallic_roughness_texture && material->metallic_roughness_image_data && material->metallic_roughness_image_data_size > 0) {
                        mat_json["spec_tex"] = nlohmann::json::object();
                        auto& spec_tex = mat_json["spec_tex"];
                        spec_tex["w"] = material->metallic_roughness_image_desc.width;
                        spec_tex["h"] = material->metallic_roughness_image_desc.height;
                        spec_tex["fmt"] = static_cast<int>(material->metallic_roughness_image_desc.pixel_format);

                        std::vector<uint8_t> tex_data(material->metallic_roughness_image_data,
                                                     material->metallic_roughness_image_data + material->metallic_roughness_image_data_size);
                        spec_tex["data"] = tex_data;

                        spec_tex["samp"] = {
                            static_cast<int>(material->metallic_roughness_sampler_desc.min_filter),
                            static_cast<int>(material->metallic_roughness_sampler_desc.mag_filter),
                            static_cast<int>(material->metallic_roughness_sampler_desc.wrap_u),
                            static_cast<int>(material->metallic_roughness_sampler_desc.wrap_v)
                        };
                    }

                    if (material->has_normal_texture && material->normal_texture_data && material->normal_texture_data_size > 0) {
                        mat_json["norm_tex"] = nlohmann::json::object();
                        auto& norm_tex = mat_json["norm_tex"];
                        norm_tex["w"] = material->normal_texture_desc.width;
                        norm_tex["h"] = material->normal_texture_desc.height;
                        norm_tex["fmt"] = static_cast<int>(material->normal_texture_desc.pixel_format);

                        std::vector<uint8_t> tex_data(material->normal_texture_data,
                                                     material->normal_texture_data + material->normal_texture_data_size);
                        norm_tex["data"] = tex_data;

                        norm_tex["samp"] = {
                            static_cast<int>(material->normal_sampler_desc.min_filter),
                            static_cast<int>(material->normal_sampler_desc.mag_filter),
                            static_cast<int>(material->normal_sampler_desc.wrap_u),
                            static_cast<int>(material->normal_sampler_desc.wrap_v)
                        };
                    }

                    if (material->has_emissive_texture && material->emissive_image_data && material->emissive_image_data_size > 0) {
                        mat_json["emis_tex"] = nlohmann::json::object();
                        auto& emis_tex = mat_json["emis_tex"];
                        emis_tex["w"] = material->emissive_image_desc.width;
                        emis_tex["h"] = material->emissive_image_desc.height;
                        emis_tex["fmt"] = static_cast<int>(material->emissive_image_desc.pixel_format);

                        std::vector<uint8_t> tex_data(material->emissive_image_data,
                                                     material->emissive_image_data + material->emissive_image_data_size);
                        emis_tex["data"] = tex_data;

                        emis_tex["samp"] = {
                            static_cast<int>(material->emissive_sampler_desc.min_filter),
                            static_cast<int>(material->emissive_sampler_desc.mag_filter),
                            static_cast<int>(material->emissive_sampler_desc.wrap_u),
                            static_cast<int>(material->emissive_sampler_desc.wrap_v)
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

    j["dir_light"] = nlohmann::json::array();
    j["dir_light"].push_back({
        {state.directional_light.direction.X, state.directional_light.direction.Y, state.directional_light.direction.Z},
        state.directional_light.intensity,
        {state.directional_light.color.X, state.directional_light.color.Y, state.directional_light.color.Z},
    });

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

    j["pp_exposure"] = post_state.uniforms.exposure;
    j["pp_brightness"] = post_state.uniforms.brightness;
    j["pp_contrast"] = post_state.uniforms.contrast;
    j["pp_saturation"] = post_state.uniforms.saturation;
    j["pp_vignette_strength"] = post_state.uniforms.vignette_strength;
    j["pp_vignette_radius"] = post_state.uniforms.vignette_radius;
    j["pp_color_tint"] = {post_state.uniforms.color_tint.X, post_state.uniforms.color_tint.Y, post_state.uniforms.color_tint.Z};

    std::string json_str = j.dump();

    uLongf compressed_size = compressBound(json_str.length());
    std::vector<Bytef> compressed_data(compressed_size);

    int result = compress(compressed_data.data(), &compressed_size, reinterpret_cast<const Bytef*>(json_str.c_str()), json_str.length());

    if (result == Z_OK) {
        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_size);
            file.close();
            iprint("scene saved to:     " + path);
            iprint("original size:      " + to_string(json_str.length()) + " bytes");
            iprint("compressed size:    " + to_string(compressed_size) + " bytes");
            iprint("compression ratio:  " + to_string(static_cast<float>(compressed_size) / json_str.length() * 100) + "%");
        }
    }
}

void load_scene(const string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        eprint("failed to open scene from path: " + path);
        return;
    }

    std::streamsize compressed_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<Bytef> compressed_data(compressed_size);
    if (!file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_size)) {
        eprint("failed to read compressed data");
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
        eprint("decompression failed with error: " + to_string(result));
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
                            obj.enable_shading = mesh_json["shading"];
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

                            if (mat_json.contains("diff_tex")) {
                                const auto& diff_tex = mat_json["diff_tex"];
                                material->has_color_texture = true;

                                material->base_color_image_desc.width = diff_tex["w"];
                                material->base_color_image_desc.height = diff_tex["h"];
                                material->base_color_image_desc.pixel_format = static_cast<sg_pixel_format>(diff_tex["fmt"]);

                                std::vector<uint8_t> tex_data = diff_tex["data"];
                                material->base_color_image_data_size = tex_data.size();
                                material->base_color_image_data = new uint8_t[material->base_color_image_data_size];
                                std::copy(tex_data.begin(), tex_data.end(), material->base_color_image_data);

                                material->base_color_image_desc.data.mip_levels[0].ptr = material->base_color_image_data;
                                material->base_color_image_desc.data.mip_levels[0].size = material->base_color_image_data_size;

                                auto samp = diff_tex["samp"];
                                material->base_color_sampler_desc.min_filter = static_cast<sg_filter>(samp[0]);
                                material->base_color_sampler_desc.mag_filter = static_cast<sg_filter>(samp[1]);
                                material->base_color_sampler_desc.wrap_u = static_cast<sg_wrap>(samp[2]);
                                material->base_color_sampler_desc.wrap_v = static_cast<sg_wrap>(samp[3]);
                            }

                            if (mat_json.contains("spec_tex")) {
                                const auto& spec_tex = mat_json["spec_tex"];
                                material->has_metallic_roughness_texture = true;

                                material->metallic_roughness_image_desc.width = spec_tex["w"];
                                material->metallic_roughness_image_desc.height = spec_tex["h"];
                                material->metallic_roughness_image_desc.pixel_format = static_cast<sg_pixel_format>(spec_tex["fmt"]);

                                std::vector<uint8_t> tex_data = spec_tex["data"];
                                material->metallic_roughness_image_data_size = tex_data.size();
                                material->metallic_roughness_image_data = new uint8_t[material->metallic_roughness_image_data_size];
                                std::copy(tex_data.begin(), tex_data.end(), material->metallic_roughness_image_data);

                                material->metallic_roughness_image_desc.data.mip_levels[0].ptr = material->metallic_roughness_image_data;
                                material->metallic_roughness_image_desc.data.mip_levels[0].size = material->metallic_roughness_image_data_size;

                                auto samp = spec_tex["samp"];
                                material->metallic_roughness_sampler_desc.min_filter = static_cast<sg_filter>(samp[0]);
                                material->metallic_roughness_sampler_desc.mag_filter = static_cast<sg_filter>(samp[1]);
                                material->metallic_roughness_sampler_desc.wrap_u = static_cast<sg_wrap>(samp[2]);
                                material->metallic_roughness_sampler_desc.wrap_v = static_cast<sg_wrap>(samp[3]);
                            }

                            if (mat_json.contains("norm_tex")) {
                                const auto& norm_tex = mat_json["norm_tex"];
                                material->has_normal_texture = true;

                                material->normal_texture_desc.width = norm_tex["w"];
                                material->normal_texture_desc.height = norm_tex["h"];
                                material->normal_texture_desc.pixel_format = static_cast<sg_pixel_format>(norm_tex["fmt"]);

                                vector<uint8_t> tex_data = norm_tex["data"];
                                material->normal_texture_data_size = tex_data.size();
                                material->normal_texture_data = new uint8_t[material->normal_texture_data_size];
                                std::copy(tex_data.begin(), tex_data.end(), material->normal_texture_data);

                                material->normal_texture_desc.data.mip_levels[0].ptr = material->normal_texture_data;
                                material->normal_texture_desc.data.mip_levels[0].size = material->normal_texture_data_size;

                                auto samp = norm_tex["samp"];
                                material->normal_sampler_desc.min_filter = static_cast<sg_filter>(samp[0]);
                                material->normal_sampler_desc.mag_filter = static_cast<sg_filter>(samp[1]);
                                material->normal_sampler_desc.wrap_u = static_cast<sg_wrap>(samp[2]);
                                material->normal_sampler_desc.wrap_v = static_cast<sg_wrap>(samp[3]);
                            }

                            if (mat_json.contains("emis_tex")) {
                                const auto& emis_tex = mat_json["emis_tex"];
                                material->has_emissive_texture = true;

                                material->emissive_image_desc.width = emis_tex["w"];
                                material->emissive_image_desc.height = emis_tex["h"];
                                material->emissive_image_desc.pixel_format = static_cast<sg_pixel_format>(emis_tex["fmt"]);

                                vector<uint8_t> tex_data = emis_tex["data"];
                                material->emissive_image_data_size = tex_data.size();
                                material->emissive_image_data = new uint8_t[material->emissive_image_data_size];
                                std::copy(tex_data.begin(), tex_data.end(), material->emissive_image_data);

                                material->emissive_image_desc.data.mip_levels[0].ptr = material->emissive_image_data;
                                material->emissive_image_desc.data.mip_levels[0].size = material->emissive_image_data_size;

                                auto samp = emis_tex["samp"];
                                material->emissive_sampler_desc.min_filter = static_cast<sg_filter>(samp[0]);
                                material->emissive_sampler_desc.mag_filter = static_cast<sg_filter>(samp[1]);
                                material->emissive_sampler_desc.wrap_u = static_cast<sg_wrap>(samp[2]);
                                material->emissive_sampler_desc.wrap_v = static_cast<sg_wrap>(samp[3]);
                            }
                        }

                        if (obj_json.contains("shape_keys")) {
                            for (const auto& sk_json : obj_json["shape_keys"]) {
                                Mesh* shape_key_mesh = new Mesh();

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
            state.vis_groups.push_back(std::move(new_visgroup));
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

    if (j.contains("dir_light")) {
        for (const auto& light_data : j["dir_light"]) {
            auto dir = light_data[0];
            state.directional_light.direction = HMM_V3(dir[0], dir[1], dir[2]);
            state.directional_light.intensity = light_data[1];
            auto color = light_data[2];
            state.directional_light.color = HMM_V3(color[0], color[1], color[2]);
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

    if (j.contains("pp_exposure")) post_state.uniforms.exposure = j["pp_exposure"];
    if (j.contains("pp_brightness")) post_state.uniforms.brightness = j["pp_brightness"];
    if (j.contains("pp_contrast")) post_state.uniforms.contrast = j["pp_contrast"];
    if (j.contains("pp_saturation")) post_state.uniforms.saturation = j["pp_saturation"];
    if (j.contains("pp_vignette_strength")) post_state.uniforms.vignette_strength = j["pp_vignette_strength"];
    if (j.contains("pp_vignette_radius")) post_state.uniforms.vignette_radius = j["pp_vignette_radius"];
    if (j.contains("pp_color_tint")) {
        auto tint = j["pp_color_tint"];
        post_state.uniforms.color_tint = HMM_V3(tint[0], tint[1], tint[2]);
    }

    iprint("scene loaded from: " + path);
}

Helper* get_helper_by_name(const string& name) { // DO NOT NAME HELPERS THE SAME NAME
    for (auto& hpr : state.helpers) {
        if (hpr->name == name) {
            return hpr;
        }
    }
    return nullptr;
}

template<typename T>
std::vector<Object*> get_objects_with_component() {
    std::vector<Object*> objects;
    for (auto& vg : state.vis_groups) {
        for (auto& obj : vg.objects) {
            if (obj.has_component<T>()) {
                objects.push_back(&obj);
            }
        }
    }
    return objects;
}

void look_at(HMM_Vec3 position) {
    HMM_Vec3 target = position;
    HMM_Vec3 direction = HMM_NormV3(HMM_SubV3(target, state.camera_pos));
    state.camera_front = direction;
    state.yaw = atan2f(direction.Z, direction.X) * 180.0f / HMM_PI;
    state.pitch = asinf(direction.Y) * 180.0f / HMM_PI;
}

extern void (*init_callback)();
extern void (*frame_callback)();
extern void (*event_callback)(SDL_Event* e);
extern void (*on_dev_command_callback)(const string& command);
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
bool grid_visible = false;

sg_image editor_display_image;
sg_sampler editor_display_sampler;
sg_image editor_specular_display_image;
sg_sampler editor_specular_display_sampler;
ImGuizmo::OPERATION current_gizmo_operation = ImGuizmo::TRANSLATE;

void render_editor() {
    if (state.editor_open) {
        std::vector<sg_view> temp_editor_views;
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::BeginFrame();

        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

        static ImGuizmo::MODE current_gizmo_mode = ImGuizmo::WORLD;

        if (grid_visible) ImGuizmo::DrawGrid(&vs_params.view.Elements[0][0], &vs_params.projection.Elements[0][0], HMM_M4D(1.0f).Elements[0], 100.f);

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

        if (ImGui::RadioButton("TRANSLATE", current_gizmo_operation == ImGuizmo::TRANSLATE)) current_gizmo_operation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("ROTATE", current_gizmo_operation == ImGuizmo::ROTATE)) current_gizmo_operation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("SCALE", current_gizmo_operation == ImGuizmo::SCALE)) current_gizmo_operation = ImGuizmo::SCALE;

        if (ImGui::RadioButton("LOCAL", current_gizmo_mode == ImGuizmo::LOCAL)) current_gizmo_mode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("WORLD", current_gizmo_mode == ImGuizmo::WORLD)) current_gizmo_mode = ImGuizmo::WORLD;

        ImGui::Checkbox("SHOW GRID", &grid_visible);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("VISGROUPS")) {
            ImGui::BeginChild("VISGROUPS", ImVec2(150, 75), true);
            for (int i = 0; i < state.vis_groups.size(); i++) {
                string label = state.vis_groups[i].name;

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
            if (selected_visgroup_index >= 0 && selected_visgroup_index < state.vis_groups.size()) {
                static char visgroup_name_buffer[256];
                static int last_selected_visgroup = -1;
                VisGroup* selected_visgroup = &state.vis_groups[selected_visgroup_index];

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
                        state.vis_groups[0].objects.push_back(obj);
                    }
                    state.vis_groups.erase(state.vis_groups.begin() + selected_visgroup_index);
                    selected_visgroup_index = -1;
                    selected_object_index = -1;
                    selected_mesh_visgroup = -1;
                }
            } else {
                ImGui::Text("NO VISGROUP SELECTED");
            }
            ImGui::EndChild();
            if (ImGui::Button("ADD VISGROUP")) {
                state.vis_groups.emplace_back("New VisGroup", vector<Object>());
            }
        }

        if (ImGui::CollapsingHeader("OBJECT")) {

            ImGui::BeginChild("INDEX MOVEMENT", ImVec2(20, 45), ImGuiChildFlags_None);
            if (ImGui::Button("^")) {
                if (selected_object_index > 0) {
                    Object object_to_move = std::move(state.vis_groups[selected_mesh_visgroup].objects[selected_object_index]);
                    state.vis_groups[selected_mesh_visgroup].objects.erase(state.vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index);
                    state.vis_groups[selected_mesh_visgroup].objects.insert(state.vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index - 1, std::move(object_to_move));
                    selected_object_index--;
                }
            }
            if (ImGui::Button("v")) {
                if (selected_object_index < state.vis_groups[selected_mesh_visgroup].objects.size() - 1) {
                    Object object_to_move = std::move(state.vis_groups[selected_mesh_visgroup].objects[selected_object_index]);
                    state.vis_groups[selected_mesh_visgroup].objects.erase(state.vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index);
                    state.vis_groups[selected_mesh_visgroup].objects.insert(state.vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index + 1, std::move(object_to_move));
                    selected_object_index++;
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("OBJECT", ImVec2(256, 300), true);
            for (int v = 0; v < state.vis_groups.size(); v++) {
                VisGroup visgroup = state.vis_groups[v];
                for (int i = 0; i < visgroup.objects.size(); i++) {
                    Object obj = visgroup.objects[i];
                    Mesh* mesh = obj.mesh;
                    string label = "OBJECT " + to_string(v) + ":" +  to_string(i) + " with VC: " + to_string(mesh->vertex_count);

                    bool is_selected = (selected_object_index == i);
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        selected_object_index = i;
                        selected_mesh_visgroup = v;
                        if (state.vis_groups[v].objects[i].mesh->material->has_color_texture) {
                            editor_display_image = sg_make_image(state.vis_groups[v].objects[i].mesh->material->base_color_image_desc);
                            editor_display_sampler = sg_make_sampler(state.vis_groups[v].objects[i].mesh->material->base_color_sampler_desc);
                        }
                        if (state.vis_groups[v].objects[i].mesh->material->has_metallic_roughness_texture) {
                            editor_specular_display_image = sg_make_image(state.vis_groups[v].objects[i].mesh->material->metallic_roughness_image_desc);
                            editor_specular_display_sampler = sg_make_sampler(state.vis_groups[v].objects[i].mesh->material->metallic_roughness_sampler_desc);
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
                Object* selected_object = &state.vis_groups[selected_mesh_visgroup].objects[selected_object_index];

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

                ImGui::Checkbox("SHADING", &selected_object->enable_shading);

                ImGui::InputInt("SCRIPT ID", &selected_object->script_id);

                ImGui::PopItemWidth();

                if (ImGui::BeginCombo("VISGROUP", state.vis_groups[selected_mesh_visgroup].name.c_str())) {
                    for (int i = 0; i < state.vis_groups.size(); i++) {
                        bool is_selected = (selected_selectable_visgroup_index == i);
                        if (ImGui::Selectable(state.vis_groups[i].name.c_str(), is_selected)) {
                            selected_selectable_visgroup_index = i;
                            if (selected_object_index >= 0 && selected_object_index < state.vis_groups[selected_mesh_visgroup].objects.size()) {
                                Object object_to_move = std::move(state.vis_groups[selected_mesh_visgroup].objects[selected_object_index]);
                                state.vis_groups[selected_mesh_visgroup].objects.erase(state.vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index);
                                state.vis_groups[i].objects.push_back(std::move(object_to_move));
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
                    if (selected_object_index >= 0 && selected_object_index < state.vis_groups[selected_mesh_visgroup].objects.size()) {
                        const Object selected_object = state.vis_groups[selected_mesh_visgroup].objects[selected_object_index];
                        Object new_object;
                        new_object.mesh = selected_object.mesh;

                        new_object.position = selected_object.position;
                        new_object.rotation = selected_object.rotation;
                        new_object.scale = selected_object.scale;
                        new_object.opacity = selected_object.opacity;

                        state.vis_groups[0].objects.push_back(new_object);
                    }
                }
                if (ImGui::Button("DELETE")) {
                    state.vis_groups[selected_mesh_visgroup].objects.erase(state.vis_groups[selected_mesh_visgroup].objects.begin() + selected_object_index);
                    selected_object_index = -1;
                    selected_mesh_visgroup = -1;
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
                    ImGui::Text("BASE COLOR");
                    if (selected_object->mesh->material->has_color_texture) {
                        sg_view_desc editor_view_desc = {};
                        editor_view_desc.texture.image = selected_object->mesh->material->base_color_image;
                        sg_view editor_display_view = sg_make_view(&editor_view_desc);
                        if (editor_display_view.id == SG_INVALID_ID) {
                            ImGui::Text("Failed to create diffuse view!");
                        } else {
                            ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_display_view, selected_object->mesh->material->base_color_sampler);
                            ImGui::Image(imtex_id, ImVec2(128, 128));
                            temp_editor_views.push_back(editor_display_view);
                        }
                    }
                    ImGui::Text("METALLIC ROUGHNESS");
                    if (selected_object->mesh->material->has_metallic_roughness_texture) {
                        sg_view_desc editor_specular_view_desc = {};
                        editor_specular_view_desc.texture.image = selected_object->mesh->material->metallic_roughness_image;
                        sg_view editor_specular_display_view = sg_make_view(&editor_specular_view_desc);
                        if (editor_specular_display_view.id == SG_INVALID_ID) {
                            ImGui::Text("Failed to create specular view!");
                        } else {
                            ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_specular_display_view, selected_object->mesh->material->metallic_roughness_sampler);
                            ImGui::Image(imtex_id, ImVec2(128, 128));
                            temp_editor_views.push_back(editor_specular_display_view);
                        }
                    }
                    ImGui::Text("NORMAL MAP");
                    if (selected_object->mesh->material->has_normal_texture) {
                        sg_view_desc editor_normal_view_desc = {};
                        editor_normal_view_desc.texture.image = selected_object->mesh->material->normal_image;
                        sg_view editor_normal_display_view = sg_make_view(&editor_normal_view_desc);
                        if (editor_normal_display_view.id == SG_INVALID_ID) {
                            ImGui::Text("Failed to create normal view!");
                        } else {
                            ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_normal_display_view, selected_object->mesh->material->normal_sampler);
                            ImGui::Image(imtex_id, ImVec2(128, 128));
                            temp_editor_views.push_back(editor_normal_display_view);
                        }
                    }
                    ImGui::Text("EMISSIVE");
                    if (selected_object->mesh->material->has_emissive_texture) {
                        sg_view_desc editor_emissive_view_desc = {};
                        editor_emissive_view_desc.texture.image = selected_object->mesh->material->emissive_image;
                        sg_view editor_emissive_display_view = sg_make_view(&editor_emissive_view_desc);
                        if (editor_emissive_display_view.id == SG_INVALID_ID) {
                            ImGui::Text("Failed to create emissive view!");
                        } else {
                            ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_emissive_display_view, selected_object->mesh->material->emissive_sampler);
                            ImGui::Image(imtex_id, ImVec2(128, 128));
                            temp_editor_views.push_back(editor_emissive_display_view);
                        }
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
                        state.vis_groups[0].objects.push_back(obj);
                    }
                }
            }
        }

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
                if (ImGui::Button("DUPLICATE")) {
                    state.helpers.emplace_back();
                    state.helpers.back()->name = selected_helper->name;
                    state.helpers.back()->position = selected_helper->position;
                }
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
                ImGui::BeginChild("DIRECTIONAL LIGHT SETTINGS", ImVec2(300, 300), true);
                ImGui::PushItemWidth(200);
                ImGui::SliderFloat3("DIRECTION", &state.directional_light.direction.X, -1.0f, 1.0f, "%.1f");
                ImGui::ColorEdit3("COLOR", &state.directional_light.color.X);
                ImGui::DragFloat("INTENSITY", &state.directional_light.intensity, 0.01f);
                ImGui::PopItemWidth();

                ImGui::Separator();
                ImGui::Text("SHADOW MAP TEXTURE:");
                sg_view_desc shadowmap_view_desc = {};
                shadowmap_view_desc.texture.image = shadow_depth_img;
                sg_view editor_display_view = sg_make_view(&shadowmap_view_desc);
                if (editor_display_view.id == SG_INVALID_ID) {
                    ImGui::Text("I don't know how you did this but there's no shadowmap");
                } else {
                    ImTextureID imtex_id = simgui_imtextureid_with_sampler(editor_display_view, shadow_sampler);
                    ImGui::Image(imtex_id, ImVec2(256, 256));
                    temp_editor_views.push_back(editor_display_view);
                }

                ImGui::EndChild();

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

        if (ImGui::CollapsingHeader("POST PROCESSING")) {
            ImGui::SliderFloat("EXPOSURE", &post_state.uniforms.exposure, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("BRIGHTNESS", &post_state.uniforms.brightness, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("CONTRAST", &post_state.uniforms.contrast, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("SATURATION", &post_state.uniforms.saturation, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("VIGNETTE STRENGTH", &post_state.uniforms.vignette_strength, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("VIGNETTE RADIUS", &post_state.uniforms.vignette_radius, 0.0f, 10.0f, "%.1f");
            ImGui::ColorEdit3("TINT", &post_state.uniforms.color_tint.X);
            ImGui::Separator();
            ImGui::Text("BLOOM TEXTURE:");
            sg_view_desc bloom_preview_desc = {};
            bloom_preview_desc.texture.image = bloom_img;
            sg_view bloom_display_view = sg_make_view(&bloom_preview_desc);
            if (bloom_display_view.id == SG_INVALID_ID) {
                ImGui::Text("I don't know how you did this but there's no bloom filter image");
            } else {
                ImTextureID imtex_id = simgui_imtextureid_with_sampler(bloom_display_view, bloom_smp);
                ImGui::Image(imtex_id, ImVec2(455, 256));
                temp_editor_views.push_back(bloom_display_view);
            }
            ImTextureID imtex_id = simgui_imtextureid_with_sampler(ssao_tex_view, ssao_smp);
            ImGui::Image(imtex_id, ImVec2(455, 256));

            ImGui::Separator();
            ImGui::Text("KUWAHARA FILTER");
            imtex_id = simgui_imtextureid_with_sampler(kw_output_1_tex_view, kw_smp);
            ImGui::Image(imtex_id, ImVec2(455, 256));
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
        ImGui::SliderFloat("GAME SPEED", &time_state.speed_multiplier, 0.0f, 1.0f, "%.3f");

        ImGui::Separator();
        if (ImGui::CollapsingHeader("PROFILER")) {
            ImGui::Text("DT: %f", time_state.dt);
            int vertex_count = 0;
            int index_count = 0;
            int light_count = 0;
            for (auto& vis_group : state.vis_groups) {
                for (auto& object : vis_group.objects) {
                    vertex_count += object.mesh->vertex_count;
                    index_count += object.mesh->index_count;
                }
            }
            light_count++; // directional
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

        if (ImGui::CollapsingHeader("LOGS")) {
            ImGui::BeginChild("LOGS", ImVec2(600, 200), true);
            for (int i = 0; i < MAX_LOGS-1; i++) {
                if (logs[i].text.length() > 0) {
                    if (logs[i].type == LogType::OHNO) {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), logs[i].text.c_str());
                    } else if (logs[i].type == LogType::WARNING) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), logs[i].text.c_str());
                    } else if (logs[i].type == LogType::USER) {
                        ImGui::TextColored(ImVec4(0.0f, 0.25f, 1.0f, 1.0f), logs[i].text.c_str());
                    } else {
                        ImGui::Text(logs[i].text.c_str());
                    }
                }
            }
            ImGui::EndChild();
            static char name_buffer[256];
            if (ImGui::Button("SEND")) {
                _add_log("> " + string(name_buffer), LogType::USER);
                on_dev_command_callback(string(name_buffer));
            }
            ImGui::SameLine();
            ImGui::InputText("COMMAND", name_buffer, sizeof(name_buffer));
        }

        ImGui::End();

        if (selected_object_index != -1 && selected_mesh_visgroup != -1) {
            Object* selected_object = &state.vis_groups[selected_mesh_visgroup].objects[selected_object_index];

            HMM_Mat4 translation = HMM_Translate(selected_object->position);
            HMM_Mat4 rotation_matrix = HMM_QToM4(selected_object->rotation);
            HMM_Mat4 scale_matrix = HMM_Scale(selected_object->scale);
            HMM_Mat4 object_matrix = HMM_MulM4(translation, HMM_MulM4(rotation_matrix, scale_matrix));

            HMM_Mat4 delta_matrix = HMM_M4D(1.0f);

            if (ImGuizmo::Manipulate(&vs_params.view.Elements[0][0], &vs_params.projection.Elements[0][0], current_gizmo_operation, current_gizmo_mode, &object_matrix.Elements[0][0], &delta_matrix.Elements[0][0])) {

                HMM_Vec3 translation_vec, scale_vec, rotation_euler;

                ImGuizmo::DecomposeMatrixToComponents(&object_matrix.Elements[0][0], &translation_vec.X, &rotation_euler.X, &scale_vec.X);

                selected_object->position = translation_vec;
                selected_object->rotation = EulerDegreesToQuat(rotation_euler);
                selected_object->scale = scale_vec;

                if (selected_object_index < mesh_euler_rotations.size()) {
                    mesh_euler_rotations[selected_object_index] = rotation_euler;
                }
            }
        }
        simgui_render();
        for (auto& view : temp_editor_views) {
            sg_destroy_view(view);
        }
    }
}

vector<Object*> get_objects_by_script_id(int id) {
    vector<Object*> objects;
    for (auto& visgroup : state.vis_groups) {
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

bool is_shaking = false;
float shake_remaining = 0.0f;
float shake_strength = 0.0f;

void camera_shake(float duration, float strength) {
    is_shaking = true;
    shake_remaining = duration;
    shake_strength = strength;
}

void _init() {
    VisGroup* default_visgroup = new VisGroup("default", {});
    state.vis_groups.push_back(*default_visgroup);
    stbi_set_flip_vertically_on_load(true);
    stbi_set_flip_vertically_on_load_thread(true);

    state.directional_light.direction = HMM_V3(1.0f, -1.0f, -0.8f);

    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);
    state.window_surface.clear(w_width, w_height);

    // ImGui
    simgui_desc_t imgui_desc = {};
    simgui_setup(imgui_desc);
    ImPlot::CreateContext();

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
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS;
    pip_desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    pip_desc.index_type = SG_INDEXTYPE_UINT32;
    pip_desc.depth.write_enabled = true;
    pip_desc.cull_mode = SG_CULLMODE_FRONT;
    pip_desc.label = "main-pipeline";
    state.pip = sg_make_pipeline(&pip_desc);

    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f };
    state.pass_action.depth.load_action = SG_LOADACTION_CLEAR;
    state.pass_action.depth.clear_value = 1.0f;

    init_blur_filter();
    init_ssao();
    init_shadowmaps();
    init_bloom();
    init_kw();
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

    sg_shader skybox_shader = sg_make_shader(skybox_shader_desc(sg_query_backend()));
    sg_pipeline_desc skybox_pipeline_desc = {};
    skybox_pipeline_desc.shader = skybox_shader;
    skybox_pipeline_desc.layout.attrs[ATTR_skybox_aPos].format = SG_VERTEXFORMAT_FLOAT3;
    skybox_pipeline_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    skybox_pipeline_desc.color_count = 1;
    skybox_pipeline_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    skybox_pipeline_desc.depth.write_enabled = false;
    skybox_pipeline_desc.cull_mode = SG_CULLMODE_NONE;
    skybox_pipeline_desc.index_type = SG_INDEXTYPE_UINT32;
    skybox_pipeline_desc.label = "skybox-pipeline";
    skybox_pipeline = sg_make_pipeline(&skybox_pipeline_desc);

    initialize_skybox_buffers();

    as_visualizer.load_from_file("audiosource.png");
    light_visualizer.load_from_file("lightsource.png");
    hpr_visualizer.load_from_file("hpr.png");
}

uint64_t last_frame_time;
double target_frame_time;

void _frame() {
    Time_BeginFrame(time_state);
    time_state.dt *= time_state.speed_multiplier;
    int w_width, w_height;
    SDL_GetWindowSizeInPixels(state.win, &w_width, &w_height);

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
    for (auto& physics_obj : get_objects_with_component<PhysicsComponent>()) {
        physics_obj->update_components();
    }

    sfetch_dowork();

    state.pass_action.colors[0].clear_value = { state.background_color.X, state.background_color.Y, state.background_color.Z, 1.0f};

    float render_yaw = state.yaw;
    float render_pitch = state.pitch;

    if (is_shaking) {
        shake_remaining -= time_state.dt;
        if (shake_remaining <= 0.0f) {
            is_shaking = false;
        } else {
            float offset_yaw = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * shake_strength;
            float offset_pitch = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * shake_strength;
            render_yaw += offset_yaw;
            render_pitch += offset_pitch;
            if (render_pitch > 89.0f) render_pitch = 89.0f;
            if (render_pitch < -89.0f) render_pitch = -89.0f;
        }
    }

    HMM_Vec3 direction;
    direction.X = cosf(render_yaw * (HMM_PI32 / 180.0f)) * cosf(render_pitch * (HMM_PI32 / 180.0f));
    direction.Y = sinf(render_pitch * (HMM_PI32 / 180.0f));
    direction.Z = sinf(render_yaw * (HMM_PI32 / 180.0f)) * cosf(render_pitch * (HMM_PI32 / 180.0f));
    state.camera_front = HMM_NormV3(direction);

    HMM_Vec3 world_up = HMM_V3(0.0f, 1.0f, 0.0f);
    HMM_Vec3 camera_right = HMM_NormV3(HMM_Cross(state.camera_front, world_up));
    state.camera_up = HMM_NormV3(HMM_Cross(camera_right, state.camera_front));

    float aspect = static_cast<float>(w_width)/static_cast<float>(w_height);
    HMM_Mat4 view = HMM_LookAt_RH(state.camera_pos, HMM_AddV3(state.camera_pos, state.camera_front), state.camera_up);
    HMM_Mat4 projection = HMM_Perspective_RH_NO(state.fov * (HMM_PI32 / 180.0f), aspect, camera_near, camera_far);

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

    for (auto& vg : state.vis_groups) {
        for (auto& obj : vg.objects) {
            obj.update_components();
        }
    }

    render_offscreen_pass();
    render_ssao_pass();
    render_bloom_pass();
    render_kw_pass();
    render_pp_pass();

    // input
    if (state.editor_open && state.rmb) {
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
                    for (int vg_idx = 0; vg_idx < state.vis_groups.size(); ++vg_idx) {
                        auto& visgroup = state.vis_groups[vg_idx];
                        for (int obj_idx = 0; obj_idx < visgroup.objects.size(); ++obj_idx) {
                            if (&visgroup.objects[obj_idx] == result.obj) {
                                selected_object_index = obj_idx;
                                selected_mesh_visgroup = vg_idx;

                                auto* mesh = visgroup.objects[obj_idx].mesh;
                                if (mesh->material->has_color_texture) {
                                    editor_display_image = sg_make_image(&mesh->material->base_color_image_desc);
                                    editor_display_sampler = sg_make_sampler(&mesh->material->base_color_sampler_desc);
                                }
                                if (mesh->material->has_metallic_roughness_texture) {
                                    editor_specular_display_image = sg_make_image(&mesh->material->metallic_roughness_image_desc);
                                    editor_specular_display_sampler = sg_make_sampler(&mesh->material->metallic_roughness_sampler_desc);
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
        if (state.editor_open && !state.rmb && !ImGui::GetIO().WantCaptureKeyboard) {
            if (e->key.key == SDLK_G) {
                current_gizmo_operation = ImGuizmo::OPERATION::TRANSLATE;
            }
            if (e->key.key == SDLK_R) {
                current_gizmo_operation = ImGuizmo::OPERATION::ROTATE;
            }
            if (e->key.key == SDLK_S) {
                current_gizmo_operation = ImGuizmo::OPERATION::SCALE;
            }
            if (e->key.key == SDLK_DELETE) {
                if (selected_object_index != -1) {
                    auto& vg = state.vis_groups[selected_mesh_visgroup];
                    vg.objects.erase(vg.objects.begin() + selected_object_index);
                    selected_object_index = -1;
                    selected_mesh_visgroup = -1;
                }
            }
            if (e->key.key == SDLK_F6) {
                old_state.camera_pos = state.camera_pos;
                old_state.camera_front = state.camera_front;
                old_state.camera_up = state.camera_up;
                old_state.last_time = state.last_time;
                old_state.background_color = state.background_color;
                old_state.lmb = state.lmb;
                old_state.rmb = state.rmb;
                old_state.yaw = state.yaw;
                old_state.pitch = state.pitch;
                old_state.fov = state.fov;
                old_state.inputs = state.inputs;
                old_state.running = state.running;
                old_state.editor_open = state.editor_open;
                old_state.event_descriptions = state.event_descriptions;
                old_state.audio_sources = state.audio_sources;
                old_state.helpers = state.helpers;
                old_state.directional_light = state.directional_light;
                old_state.point_lights = state.point_lights;
                old_state.spot_lights = state.spot_lights;
                old_state.ambient_light = state.ambient_light;
                old_state.window_surface = state.window_surface;
                old_state.vis_groups = state.vis_groups;
            }
            if (e->key.key == SDLK_F7) {
                state.camera_pos = old_state.camera_pos;
                state.camera_front = old_state.camera_front;
                state.camera_up = old_state.camera_up;
                state.last_time = old_state.last_time;
                state.background_color = old_state.background_color;
                state.lmb = old_state.lmb;
                state.rmb = old_state.rmb;
                state.yaw = old_state.yaw;
                state.pitch = old_state.pitch;
                state.fov = old_state.fov;
                state.inputs = old_state.inputs;
                state.running = old_state.running;
                state.editor_open = old_state.editor_open;
                state.event_descriptions = old_state.event_descriptions;
                state.audio_sources = old_state.audio_sources;
                state.helpers = old_state.helpers;
                state.directional_light = old_state.directional_light;
                state.point_lights = old_state.point_lights;
                state.spot_lights = old_state.spot_lights;
                state.ambient_light = old_state.ambient_light;
                state.window_surface = old_state.window_surface;
                state.vis_groups = old_state.vis_groups;
            }
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
            img_desc.data.mip_levels[0].ptr = pixels;
            img_desc.data.mip_levels[0].size = img_width * img_height * 4;

            sg_image new_img = sg_make_image(&img_desc);
            sg_view_desc tex_view_desc = {};
            tex_view_desc.texture.image = new_img;
            state.bind.views[texture_index] = sg_make_view(&tex_view_desc);

            stbi_image_free(pixels);
        }
    } else if (response->failed) {
        state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
        state.pass_action.colors[0].clear_value = { 1.0f, 0.0f, 0.0f, 1.0f };
        eprint("failed to fetch image in fetch_callback");
    }
    texture_index++;
}

const float max_fps = 75.0f;

int main(int argc, char* argv[]) {
    srand (static_cast <unsigned> (time(0)));
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
    desc.buffer_pool_size = 512;
    desc.image_pool_size = 2048;
    desc.sampler_pool_size = 2048;
    desc.shader_pool_size = 64;
    desc.pipeline_pool_size = 64;
    desc.view_pool_size = 2048;
    desc.environment = env;
    desc.logger.func = slog_func;
    sg_setup(&desc);
    stm_setup();
    Time_Init(time_state);
    _init();

    SDL_GL_SetSwapInterval(1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    target_frame_time = 1.0f / max_fps;
    last_frame_time = stm_now();

    bool first_frame = true;
    while (state.running) {
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

    if (shadow_pip.id != SG_INVALID_ID) sg_destroy_pipeline(shadow_pip);
    if (shadow_sampler.id != SG_INVALID_ID) sg_destroy_sampler(shadow_sampler);
    if (shadow_depth_tex_view.id != SG_INVALID_ID) sg_destroy_view(shadow_depth_tex_view);
    if (shadow_depth_att_view.id != SG_INVALID_ID) sg_destroy_view(shadow_depth_att_view);
    if (shadow_depth_img.id != SG_INVALID_ID) sg_destroy_image(shadow_depth_img);
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