//
// Created by gungu on 8/22/25.
//

#ifndef FPSCONTROLLER_H
#define FPSCONTROLLER_H

class FPSController : public CharacterController {
public:
    float mouse_sensitivity = 0.005f;
    float relative_camera_height;

    void update_input(const std::map<SDL_Keycode,
        bool>& inputs,
        float mouse_dx,
        float mouse_dy,
        HMM_Vec3* camera_pos,
        HMM_Vec3* camera_front,
        float* camera_yaw,
        float* camera_pitch) {
        update();

        *camera_yaw -= mouse_dx * mouse_sensitivity;
        *camera_pitch -= mouse_dy * mouse_sensitivity;

        float pitch_limit = HMM_PI32 / 2.0f - 0.01f;
        if (*camera_pitch > pitch_limit) *camera_pitch = pitch_limit;
        if (*camera_pitch < -pitch_limit) *camera_pitch = -pitch_limit;

        *camera_yaw = std::fmod(*camera_yaw, 2.0f * HMM_PI32);
        if (*camera_yaw < 0.0f) *camera_yaw += 2.0f * HMM_PI32;

        holder.orientation = reactphysics3d::Quaternion::fromEulerAngles(0.0f, *camera_yaw, 0.0f);

        HMM_Vec3 direction = {0.0f, 0.0f, 0.0f};
        if (inputs.find(SDLK_W) != inputs.end() && inputs.at(SDLK_S)) direction.Y += 1.0f;
        if (inputs.find(SDLK_S) != inputs.end() && inputs.at(SDLK_S)) direction.Y -= 1.0f;
        if (inputs.find(SDLK_A) != inputs.end() && inputs.at(SDLK_A)) direction.X -= 1.0f;
        if (inputs.find(SDLK_D) != inputs.end() && inputs.at(SDLK_D)) direction.X += 1.0f;

        move(direction);

        if (inputs.find(SDLK_SPACE) != inputs.end() && inputs.at(SDLK_SPACE) && on_ground) {
            jump();
        }

        bool is_crouching = (inputs.find(SDLK_C) != inputs.end() && inputs.at(SDLK_C));
        if (can_crouch && is_crouching) {
            speed = 5.0f;  // TODO: add crouching
        } else {
            speed = 10.0f;
        }
        camera_pos->X = holder.body->getTransform().getPosition().x;
        camera_pos->Y = holder.body->getTransform().getPosition().y + relative_camera_height;
        camera_pos->Z = holder.body->getTransform().getPosition().z;
        // I know it's wrong but I'ma just reuse direction, because I'm not gonna use it after this line anyway
        direction.X = cosf(-*camera_yaw) * cosf(*camera_pitch);
        direction.Y = sinf(*camera_pitch);
        direction.Z = sinf(-*camera_yaw) * cosf(*camera_pitch);
        HMM_Vec3 camera_front_result = HMM_NormV3(direction);
        camera_front->X = camera_front_result.X;
        camera_front->Y = camera_front_result.Y;
        camera_front->Z = camera_front_result.Z;
    }
};

#endif //FPSCONTROLLER_H
