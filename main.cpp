//
// Created by gungu on 7/23/25.
//
#include "gungutils.cpp"

float mouse_movement_x = 0.0f;
float mouse_movement_y = 0.0f;
float yrot = 0.0f;
Surface jeff_goldblum;
Surface jeff_button;
bool show_jeff = false;

AnimationPlayer anim_player;
Animation animation;

void init() {
    state.background_color = {1.0f, 1.0f, 1.0f};
    //SDL_HideCursor();

    jeff_goldblum.load_from_file("jeff goldblum.png");  // jeff goldblum 👍

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

    load_scene("maps/cottage.gmap");
}

void frame() {
    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);

    state.window_surface.clear(w_width, w_height);
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