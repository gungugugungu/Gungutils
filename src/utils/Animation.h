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

#endif //ANIMATION_H
