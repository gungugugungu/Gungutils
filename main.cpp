//
// Created by gungu on 7/23/25.
//
#include <memory>
#include "gungutils.cpp"

AudioSource3D* audio_source = new AudioSource3D();
//FPSController player_controller;

float mouse_movement_x = 0.0f;
float mouse_movement_y = 0.0f;
float yrot = 0.0f;

DirectionalLight light{};

void init() {
    state.background_color = {1.0f, 1.0f, 1.0f};
    SDL_HideCursor();

    world->setGravity({0.0f, -9.81f, 0.0f});

    FMOD_RESULT result;
    result = state.fmod_system->loadBankFile("fmodproject/Build/Desktop/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &state.bank);
    print_fmod_error(result);

    int event_count = 0;
    result = state.bank->getEventCount(&event_count);
    print_fmod_error(result);
    state.event_descriptions.resize(event_count);
    result = state.bank->getEventList(state.event_descriptions.data(), event_count, &event_count);
    print_fmod_error(result);

    load_scene("maps/boxes.gmap");
    light.direction = {1.0f, -1.0f, -1.0f};
    light.color = {1.0f, 1.0f, 1.0f};
    light.intensity = 1.0f;
    state.directional_lights.push_back(light);
}

void frame() {
    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    if (!state.editor_open) {
        SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);
    }
    /*yrot += 45.0f*time_state.dt;
    vis_groups[0].objects[0].rotation = EulerDegreesToQuat(HMM_Vec3{90.0f, yrot, 0.0f});*/
}

void event(SDL_Event* e) {
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        mouse_movement_x = e->motion.xrel;
        mouse_movement_y = e->motion.yrel;
    }
}

void (*init_callback)() = init;
void (*frame_callback)() = frame;
void (*event_callback)(SDL_Event* e) = event;