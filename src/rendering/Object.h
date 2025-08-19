//
// Created by gungu on 8/19/25.
//

#ifndef OBJECT_H
#define OBJECT_H

class Object {
public:
    HMM_Vec3 position{0,0,0};
    HMM_Quat rotation{0,0,0,1};
    HMM_Vec3 scale{1,1,1};
    float opacity = 1.0f;

    Mesh* mesh;
};

#endif //OBJECT_H
