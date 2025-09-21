//
// Created by gungu on 9/21/25.
//

#ifndef INPUT_H
#define INPUT_H

enum InputType {
    KEYBOARD,
    MOUSE,
    GAMEPAD_BUTTON,
    GAMEPAD_AXIS,
};

struct InputEvent {
    InputType type;
    SDL_Keycode key;
    int mouse_button;
    int axis;
    int gamepad_button;
    std::string name;
};

#endif //INPUT_H
