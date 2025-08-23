//
// Created by gungu on 8/23/25.
//

#ifndef LIGHT_H
#define LIGHT_H

class Light {
public:
    float intensity = 1.0f;
    int type; // 0: directional, 1: point, 2: spot
    HMM_Vec3 color{1.0f, 1.0f, 1.0f};
};

class DirectionalLight : public Light {
public:
    int type = 0;
    HMM_Vec3 direction{0.0f, 0.0f, 0.0f};
};

class PointLight : public Light {
public:
    int type = 1;
    HMM_Vec3 position{0.0f, 0.0f, 0.0f};
    float radius = 100.0f;
};

class SpotLight : public Light {
public:
    int type = 2;
    HMM_Vec3 position{0.0f, 0.0f, 0.0f};
    HMM_Vec3 direction{0.0f, 0.0f, 0.0f};
    float inner_cone_angle = 0.0f;
    float outer_cone_angle = 0.0f;
};

#endif //LIGHT_H
