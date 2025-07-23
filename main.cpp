//
// Created by gungu on 7/23/25.
//
#include "gungutils.hpp"

int frames = 0;

void init() {
    state.background_color = {0.2f, 0.3f, 0.5f};
    /*float* verts = nullptr;
    uint32_t vertex_count = 0;
    uint32_t* indices = nullptr;
    uint32_t index_count = 0;*/

    vector<Mesh> loaded_meshes = load_gltf("test4.glb");
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
}

void frame() {
    frames++;
    state.background_color.X = sin(frames * 0.01f) * 0.5f + 0.5f;

    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    if (!SDL_CursorVisible()) {SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);}
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

void event(SDL_Event* e) {
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        state.mouse_btn = true;
    } else if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        state.mouse_btn = false;
    } else if (e->type == SDL_EVENT_KEY_DOWN && e->key.repeat == 0) {
        state.inputs[e->key.key] = true;
        if (e->key.key == SDLK_ESCAPE) {
            state.running = false;
        }

        if (e->key.key == SDLK_SPACE) {
            if (SDL_CursorVisible()) {
                SDL_HideCursor();
            } else {
                SDL_ShowCursor();
            }
        }

    } else if (e->type == SDL_EVENT_KEY_UP && e->key.repeat == 0) {
        state.inputs[e->key.key] = false;
    }  else if (e->type == SDL_EVENT_MOUSE_MOTION && state.mouse_btn) {
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
    }
    else if (e->type == SDL_EVENT_MOUSE_WHEEL) {
        if (state.fov >= 1.0f && state.fov <= 45.0f) {
            state.fov -= e->wheel.y;
        }
        if (state.fov <= 60.0f)
            state.fov = 60.0f;
        else if (state.fov >= 95.0f)
            state.fov = 95.0f;
    }
}

void (*init_callback)() = init;
void (*frame_callback)() = frame;
void (*event_callback)(SDL_Event* e) = event;