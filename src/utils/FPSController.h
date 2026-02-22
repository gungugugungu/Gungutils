//
// Created by gungu on 8/22/25.
//

#ifndef FPSCONTROLLER_H
#define FPSCONTROLLER_H

class FPSController : public CharacterController {
public:
    float mouse_sensitivity = 0.005f;
    float relative_camera_height;

    void update_input(std::map<SDL_Keycode, bool>& inputs,
    float mouse_dx,
    float mouse_dy,
    HMM_Vec3* camera_pos,
    HMM_Vec3* camera_front,
    float* camera_yaw,
    float* camera_pitch) {

        update();

        *camera_yaw += mouse_dx * mouse_sensitivity;
        *camera_pitch -= mouse_dy * mouse_sensitivity;

        if (*camera_pitch > 89.0f) *camera_pitch = 89.0f;
        if (*camera_pitch < -89.0f) *camera_pitch = -89.0f;

        *camera_yaw = std::fmod(*camera_yaw, 360.0f);
        if (*camera_yaw < 0.0f) *camera_yaw += 360.0f;

        float yaw_rad = *camera_yaw * (HMM_PI32 / 180.0f);
        float pitch_rad = *camera_pitch * (HMM_PI32 / 180.0f);

        orientation = reactphysics3d::Quaternion::fromEulerAngles(0.0f, -yaw_rad, 0.0f);

        HMM_Vec3 direction = {0.0f, 0.0f, 0.0f};
        if (inputs[SDLK_W] == true) direction.X += 1.0f;
        if (inputs[SDLK_S] == true) direction.X -= 1.0f;
        if (inputs[SDLK_A] == true) direction.Z -= 1.0f;
        if (inputs[SDLK_D] == true) direction.Z += 1.0f;

        move(direction);

        if (inputs.find(SDLK_SPACE) != inputs.end() && inputs.at(SDLK_SPACE) && on_ground) {
            jump();
        }

        bool is_crouching = (inputs.find(SDLK_C) != inputs.end() && inputs.at(SDLK_C));
        if (can_crouch && is_crouching) {
            speed = 5.0f;
        } else {
            speed = 10.0f;
        }

        camera_pos->X = body->getTransform().getPosition().x;
        camera_pos->Y = body->getTransform().getPosition().y + relative_camera_height;
        camera_pos->Z = body->getTransform().getPosition().z;

        direction.X = cosf(-yaw_rad) * cosf(pitch_rad);
        direction.Y = sinf(pitch_rad);
        direction.Z = sinf(-yaw_rad) * cosf(pitch_rad);
        HMM_Vec3 camera_front_result = HMM_NormV3(direction);
        camera_front->X = camera_front_result.X;
        camera_front->Y = camera_front_result.Y;
        camera_front->Z = camera_front_result.Z;
    }
};

#endif //FPSCONTROLLER_H
