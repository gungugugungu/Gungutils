//
// Created by gungu on 7/23/25.
//
#include "gungutils.cpp"

AudioSource3D* audio_source = new AudioSource3D();
Helper* campos_helper;
std::vector<PhysicsHolder*> physics_holders;

void init() {
    state.background_color = {1.0f, 1.0f, 1.0f};
    SDL_HideCursor();

    FMOD_RESULT result;
    result = state.fmod_system->loadBankFile("fmodproject/Build/Desktop/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &state.bank);
    print_fmod_error(result);
    
    int event_count = 0;
    result = state.bank->getEventCount(&event_count);
    print_fmod_error(result);
    state.event_descriptions.resize(event_count);
    result = state.bank->getEventList(state.event_descriptions.data(), event_count, &event_count);
    print_fmod_error(result);

    load_scene("maps/physicstest.gmap");
    for (auto& mesh : vis_groups[0].meshes) {
        PhysicsHolder cur_mesh_phys_holder;
        cur_mesh_phys_holder.assign_mesh(&mesh);
        physics_holders.push_back(&cur_mesh_phys_holder);
    }
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