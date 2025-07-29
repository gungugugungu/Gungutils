//
// Created by gungu on 7/23/25.
//
#include "gungutils.hpp"

AudioSource3D* audio_source = new AudioSource3D();

void init() {
    state.background_color = {1.0f, 1.0f, 1.0f};
    SDL_HideCursor();
    /*float* verts = nullptr;
    uint32_t vertex_count = 0;
    uint32_t* indices = nullptr;
    uint32_t index_count = 0;*/

    FMOD_RESULT result;
    result = state.fmod_system->loadBankFile("fmodproject/Build/Desktop/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &state.bank); // Use state.bank
    print_fmod_error(result);

    int event_count = 0;
    result = state.bank->getEventCount(&event_count);
    print_fmod_error(result);

    state.event_descriptions.resize(event_count);
    result = state.bank->getEventList(state.event_descriptions.data(), event_count, &event_count);
    std::cout << "Created event desc, count: " << event_count << std::endl;
    print_fmod_error(result);

    HMM_Vec3 pos = {0.0f, 0.0f, 0.0f};
    if (!state.event_descriptions.empty()) {
        audio_source->initalize(state.event_descriptions[0], pos);
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