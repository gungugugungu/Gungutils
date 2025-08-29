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
stbtt_fontinfo font;

void init() {
    state.background_color = {1.0f, 1.0f, 1.0f};
    //SDL_HideCursor();

    jeff_goldblum.load_from_file("jeff goldblum.png");  // jeff goldblum 👍
    load_font(&font, "font.ttf");

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
}

void frame() {
    // input
    int w_width, w_height;
    SDL_GetWindowSize(state.win, &w_width, &w_height);
    //SDL_WarpMouseInWindow(state.win, w_width/2, w_height/2);
    state.window_surface.clear(w_width, w_height);
    if (show_jeff) {
        state.window_surface.draw(jeff_goldblum, {32.0f, 0.0f});
        state.window_surface.draw_text(&font, "Jeff Goldblum", {400.0f, 128.0f}, 0.25f, {0.0f, 1.0f, 0.0f, 1.0f});
    }
    /*yrot += 45.0f*time_state.dt;
    vis_groups[0].objects[0].rotation = EulerDegreesToQuat(HMM_Vec3{90.0f, yrot, 0.0f});*/
}

void event(SDL_Event* e) {
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        mouse_movement_x = e->motion.xrel;
        mouse_movement_y = e->motion.yrel;
    }

    if (e->type == SDL_EVENT_KEY_DOWN) {
        if (e->key.key == SDLK_H) {
            state.particle_systems.emplace_back(&jeff_goldblum, 3, HMM_Vec3{0.0f, 0.0f, 0.0f}, HMM_Vec3{0.0f, 0.5f, 0.0f}, 0.5f, 2.0f);
        }
    }
}

void (*init_callback)() = init;
void (*frame_callback)() = frame;
void (*event_callback)(SDL_Event* e) = event;