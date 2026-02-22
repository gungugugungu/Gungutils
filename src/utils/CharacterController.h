//
// Created by gungu on 8/22/25.
//

#ifndef CHARACTERCONTROLLER_H
#define CHARACTERCONTROLLER_H

class CharacterController {
public:
    reactphysics3d::Vector3 position;
    reactphysics3d::Quaternion orientation;
    reactphysics3d::Transform transform;
    reactphysics3d::RigidBody* body = nullptr;
    float speed = 10.0f;
    float jump_height = 3.0f;
    bool on_ground = false;
    bool can_jump = true;
    bool can_move = true;
    bool can_crouch = true;
    bool can_crouch_jump = true;

    void initalize(float collider_width, float collider_height) {
        transform = reactphysics3d::Transform::identity();
        body = world->createRigidBody(transform);
        reactphysics3d::CapsuleShape* capsuleShape = physicsCommon.createCapsuleShape(collider_width, collider_height);
        reactphysics3d::Collider* collider = body->addCollider(capsuleShape, transform);
        collider->getMaterial().setBounciness(0.0f);
        collider->getMaterial().setFrictionCoefficient(1.0f);
        collider->getMaterial().setMassDensity(1.0f);
        body->setType(reactphysics3d::BodyType::DYNAMIC);
        body->enableGravity(true);
        body->setLinearDamping(0.0f);
        body->setAngularDamping(0.0f);
        body->setAngularLockAxisFactor({0.0f,0.0f,0.0f});
        body->setIsAllowedToSleep(false);
    }

    void move_to(reactphysics3d::Vector3 position) {
        transform = reactphysics3d::Transform(position, orientation);
        body->setTransform(transform);
    }

    void update() {
        auto transform = body->getTransform();
        reactphysics3d::CapsuleShape* capsule = dynamic_cast<reactphysics3d::CapsuleShape*>(body->getCollider(0)->getCollisionShape());
        if (capsule) {
            float radius = capsule->getRadius();
            float height = capsule->getHeight();
            float half_height = (height*0.5f)+radius;
            const float ground_check_distance = 0.1f;

            reactphysics3d::Vector3 start = transform.getPosition();
            reactphysics3d::Vector3 end = start - reactphysics3d::Vector3(0.0f, half_height+ground_check_distance, 0.0f);

            struct GroundRaycastCallback : public reactphysics3d::RaycastCallback {
                bool hit = false;
                reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo& raycastinfo) override {
                    hit = true;
                    return 0.0f;
                }
            };

            GroundRaycastCallback callback;
            reactphysics3d::Ray ray(start, end);
            world->raycast(ray, &callback);
            on_ground = callback.hit;
            if (on_ground) {
                auto vel = body->getLinearVelocity();
                if (vel.y < 0.0f) {
                    body->setLinearVelocity(reactphysics3d::Vector3(vel.x, 0.0f, vel.z));
                }
            }
        }
    }

    void move(HMM_Vec3 direction) {
        if (!can_move) return;

        reactphysics3d::Vector3 local_dir(direction.X, 0.0f, direction.Z);
        if (local_dir.lengthSquare() < 0.001f) {
            auto current_vel = body->getLinearVelocity();
            body->setLinearVelocity(reactphysics3d::Vector3(0, current_vel.y, 0));
            return;
        }

        local_dir.normalize();
        reactphysics3d::Vector3 world_dir = orientation * local_dir;

        world_dir *= speed;

        auto current_vel = body->getLinearVelocity();
        reactphysics3d::Vector3 new_vel(world_dir.x, current_vel.y, world_dir.z);
        body->setLinearVelocity(new_vel);
    }

    void jump() {
        if (!can_jump) return;

        reactphysics3d::Vector3 gravity = world->getGravity();
        float g = -gravity.y;
        float jump_velocity = std::sqrt(2.0f*jump_height*g);

        auto current_vel = body->getLinearVelocity();
        reactphysics3d::Vector3 new_vel(current_vel.x, jump_velocity, current_vel.z);
        body->setLinearVelocity(new_vel);
    }
};

#endif //CHARACTERCONTROLLER_H
