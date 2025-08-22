//
// Created by gungu on 7/23/25.
//
#include "gungutils.cpp"

AudioSource3D* audio_source = new AudioSource3D();
FPSController player_controller;

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

    load_scene("maps/fpscontroller.gmap");
    for (auto& obj : vis_groups[0].objects) {
        PhysicsHolder* holder = new PhysicsHolder();
        holder->assign_mesh(&obj);
        holder->body->setType(reactphysics3d::BodyType::KINEMATIC);
    }
    player_controller.initalize(0.5f, 1.5f);
}

void frame() {
    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    if (!state.editor_open) {
        SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);
    }
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);
    player_controller.update_input(state.inputs, mouse_dx, mouse_dy, &state.camera_pos, &state.camera_front, &state.yaw, &state.pitch);
}

void event(SDL_Event* e) {}

void (*init_callback)() = init;
void (*frame_callback)() = frame;
void (*event_callback)(SDL_Event* e) = event;