//
// Created by gungu on 8/18/25.
//

#ifndef LIGHT_H
#define LIGHT_H

#include <vector>
#include <string>
#include "HandmadeMath/HandmadeMath.h"
using namespace std;

class Light {
public:
    HMM_Vec3 position{};
    float intensity = 20;
    int type;
    HMM_Vec3 color{1,1,1};
};

class DirectionalLight : public Light {
public:
    HMM_Vec3 direction{};
    int type = 0;
};

class PointLight : public Light {
public:
    float radius = 10;
    int type = 1;
};

class SpotLight : public Light {
public:
    float inner_angle = 0.5f;
    float outer_angle = 1.0f;
    float radius = 10;
    int type = 2;
    HMM_Vec3 direction{};
};

#endif //LIGHT_H
