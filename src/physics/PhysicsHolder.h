//
// Created by gungu on 8/15/25.
//

#ifndef PHYSICSHOLDER_H
#define PHYSICSHOLDER_H
#include <reactphysics3d/reactphysics3d.h>
using namespace reactphysics3d;

inline PhysicsCommon physicsCommon;
inline PhysicsWorld* world = physicsCommon.createPhysicsWorld();

class PhysicsHolder {
public:
    Mesh assigned_mesh;
    Vector3 position;
    Quaternion orientation;
    Transform transform;
    RigidBody* body = world->createRigidBody(transform);
};

#endif //PHYSICSHOLDER_H
