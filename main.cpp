//
// Created by gungu on 7/23/25.
//
#include "gungutils.cpp"

AudioSource3D* audio_source = new AudioSource3D();
Helper* campos_helper;


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

    load_scene("maps/physicstest2.gmap");
    for (auto& mesh : vis_groups[0].meshes) {
        auto& holder = state.physics_holders.emplace_back(std::make_unique<PhysicsHolder>());
        holder->assign_mesh(&mesh);
    }
    state.physics_holders[2]->body->setLinearLockAxisFactor({0.0f, 0.0f, 0.0f});
    state.physics_holders[2]->body->setAngularLockAxisFactor({0.0f, 0.0f, 0.0f});
    state.camera_pos = get_helper_by_name("campos")->position;
}

void frame() {
    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    if (!state.editor_open) {
        SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);
    }
}

void event(SDL_Event* e) {
    if (e->type == SDL_EVENT_KEY_DOWN && e->key.repeat == 0) {
        if (e->key.key == SDLK_ESCAPE) {
            state.running = false;
        }
    }
}

void (*init_callback)() = init;
void (*frame_callback)() = frame;
void (*event_callback)(SDL_Event* e) = event;