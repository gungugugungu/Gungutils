//
// Created by gungu on 8/15/25.
//

#ifndef GUNGUTILS_H
#define GUNGUTILS_H

#endif //GUNGUTILS_H

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
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/animation.h"
// shaders
#include "ozz/animation/runtime/skeleton.h"
#include "shaders/mainshader.glsl.h"

extern void (*init_callback)();
extern void (*frame_callback)();
extern void (*event_callback)(SDL_Event* e);

struct AppState;
AppState state;
HMM_Vec3 QuatToEulerDegrees(const HMM_Quat& q);
HMM_Quat EulerDegreesToQuat(const HMM_Vec3& euler);
class Mesh;
class VisGroup;
std::vector<VisGroup> vis_groups;
std::vector<Mesh> visualizer_meshes;
void Mesh_To_Buffers(Mesh& mesh, bool dontadd = false);
void render_meshes_streaming();
void render_meshes_batched_streaming(size_t batch_size = 10);
struct RaycastResult;
RaycastResult raycast_from_screen(float screen_x, float screen_y);
std::vector<Mesh> load_gltf(const std::string& path);
bool load_obj(const std::string& filename, float** out_vertices, uint32_t* out_vertex_count, uint32_t** out_indices,uint32_t* out_index_count);
void print_fmod_error(FMOD_RESULT result);
class AudioSource3D;
class Helper;
void make_audiosource_by_index(int index);
void clear_scene();
void save_scene(const std::string& path);
void load_scene(const std::string& path);
Helper* get_helper_by_name(const std::string& name);
ImGuiKey ImGui_ImplSDL3_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode);