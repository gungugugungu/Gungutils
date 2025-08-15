//
// Created by gungu on 7/23/25.
//
#include "gungutils.cpp"

AudioSource3D* audio_source = new AudioSource3D();
Helper* campos_helper;

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

    /*load_scene("maps/world.gmap");
    campos_helper = get_helper_by_name("campos");*/
}

void frame() {
    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    if (!state.editor_open) {
        SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);
    }
    /*state.camera_pos = campos_helper->position;
    HMM_Vec3 target = HMM_V3(0.0f, 1.0f, 0.0f);
    HMM_Vec3 direction = HMM_NormV3(HMM_SubV3(target, state.camera_pos));
    state.camera_front = direction;
    state.yaw = atan2f(direction.Z, direction.X) * 180.0f / HMM_PI;
    state.pitch = asinf(direction.Y) * 180.0f / HMM_PI;
    state.camera_up = HMM_V3(0.0f, 1.0f, 0.0f);*/
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