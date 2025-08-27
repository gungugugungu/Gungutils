//
// Created by gungu on 8/27/25.
//

#ifndef UIBUTTON_H
#define UIBUTTON_H

class UIButton {
public:
    Surface surface;
    HMM_Vec2 position;
    std::function<void()> on_click_callback;

    void on_click() {
        if (on_click_callback) {
            on_click_callback();
        }
    }

    void update(SDL_Event* event) {
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event->button.button == SDL_BUTTON_LEFT && event->button.x >= position.X && event->button.x <= position.X + surface.pixels[0].size() && event->button.y >= position.Y && event->button.y <= position.Y + surface.pixels.size()) {
                on_click();
            } else {
                cout << "oh no" << endl;
            }
        }
    }

    void draw(Surface* window_surface) {
        window_surface->draw(surface, {position.X, position.Y});
    }
};

#endif //UIBUTTON_H
