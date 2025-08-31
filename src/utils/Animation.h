//
// Created by gungu on 8/31/25.
//

#ifndef ANIMATION_H
#define ANIMATION_H

struct AnimationFrame {
    int shape_key_index;
    float time;
};

struct Animation {
    std::vector<AnimationFrame> frames;
};

class AnimationPlayer {
public:
    Animation* animation = nullptr;
    Object* obj = nullptr;
    int current_frame = 0;
    float current_time = 0.0f;
    bool playing = false;
    bool loop = false;

    void update(float dt) {
        if (playing) {
            current_time += dt;
            if (current_time >= animation->frames[current_frame].time) {
                current_time = 0;
                if (current_frame < animation->frames.size() - 1) {
                    current_frame++;
                    obj->select_shape_key(animation->frames[current_frame].shape_key_index);
                } else if (loop) {
                    current_frame = 0;
                    obj->select_shape_key(animation->frames[current_frame].shape_key_index);
                } else {
                    playing = false;
                }
            }
        }
    }
};

#endif //ANIMATION_H
