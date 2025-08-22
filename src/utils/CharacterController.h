//
// Created by gungu on 8/22/25.
//

#ifndef CHARACTERCONTROLLER_H
#define CHARACTERCONTROLLER_H

class CharacterController {
public:
    PhysicsHolder holder;
    float speed = 10.0f;
    float jump_height = 10.0f;
    bool on_ground = false;
    bool can_jump = true;
    bool can_move = true;
    bool can_crouch = true;
    bool can_crouch_jump = true;

    void initalize(float collider_width, float collider_height) {
        holder = PhysicsHolder();
        reactphysics3d::CapsuleShape* capsuleShape = physicsCommon.createCapsuleShape(collider_width, collider_height);
        holder.body->addCollider(capsuleShape, holder.transform);
        holder.body->setType(reactphysics3d::BodyType::DYNAMIC);
        holder.body->enableGravity(true);
        holder.body->setLinearDamping(0.0f);
        holder.body->setAngularDamping(0.0f);
        holder.body->setAngularLockAxisFactor({0.0f,0.0f,0.0f});
        holder.body->setIsAllowedToSleep(false);
    }

    void update() {
        auto transform = holder.body->getTransform();
        reactphysics3d::CapsuleShape* capsule = dynamic_cast<reactphysics3d::CapsuleShape*>(holder.body->getCollider(0)->getCollisionShape());
        if (capsule) {
            float radius = capsule->getRadius();
            float height = capsule->getHeight();
            float half_height = (height*0.5f)+radius;
            const float ground_check_distance = 0.1f;

            reactphysics3d::Vector3 start = transform.getPosition();
            reactphysics3d::Vector3 end = start - reactphysics3d::Vector3(0.0f, half_height+ground_check_distance, 0.0f);

            struct GroundRaycastCallback : public reactphysics3d::RaycastCallback {
                bool hit = false;
                reactphysics3d::decimal reactphysics3d::notifyRaycastHit(const reactphysics3d::RaycastInfo& raycastinfo) override {
                    hit = true;
                    return 0.0f;
                }
            };

            GroundRaycastCallback callback;
            reactphysics3d::Ray ray(start, end);
            world->raycast(ray, &callback);
            on_ground = callback.hit;
        }

        holder.update();
    }

    void move(HMM_Vec3 direction) {
        if (!can_move) return;

        reactphysics3d::Vector3 local_dir(direction.X, 0.0f, direction.Z);
        if (local_dir.lengthSquare() < 0.001f) {
            auto current_vel = holder.body->getLinearVelocity();
            holder.body->setLinearVelocity(reactphysics3d::Vector3(0, current_vel.y, 0));
            return;
        }

        local_dir.normalize();
        reactphysics3d::Vector3 world_dir = holder.orientation * local_dir;

        world_dir *= speed;

        auto current_vel = holder.body->getLinearVelocity();
        reactphysics3d::Vector3 new_vel(world_dir.x, current_vel.y, world_dir.z);
        holder.body->setLinearVelocity(new_vel);
    }

    void jump() {
        if (!can_jump) return;

        reactphysics3d::Vector3 gravity = world->getGravity();
        float g = -gravity.y;
        float jump_velocity = std::sqrt(2.0f*jump_height*g);

        auto current_vel = holder.body->getLinearVelocity();
        reactphysics3d::Vector3 new_vel(current_vel.x, jump_velocity, current_vel.z);
        holder.body->setLinearVelocity(new_vel);
    }
};

#endif //CHARACTERCONTROLLER_H
