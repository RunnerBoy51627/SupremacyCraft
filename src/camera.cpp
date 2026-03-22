#include "camera.h"
#include "utils.h"
#include <math.h>
#include <gccore.h>

Mtx g_viewMatrix;

void Camera_Init(FreeCam* cam) {
    cam->pos     = (guVector){8.0f, 11.6f, 8.0f};
    cam->up      = (guVector){0.0f, 1.0f,  0.0f};
    cam->yaw     = 90.0f;
    cam->pitch   = 0.0f;
    // Compute forward immediately so first frame is correct
    float cosPitch = cosf(cam->pitch * (3.14159265f / 180.0f));
    float cosYaw   = cosf(cam->yaw   * (3.14159265f / 180.0f));
    float sinYaw   = sinf(cam->yaw   * (3.14159265f / 180.0f));
    cam->forward = (guVector){cosYaw * cosPitch, 0.0f, sinYaw * cosPitch};
    // Prime the view matrix so frame 0 has a valid transform
    guVector lookAt;
    guVecAdd(&cam->pos, &cam->forward, &lookAt);
    guLookAt(g_viewMatrix, &cam->pos, &cam->up, &lookAt);
}

// Only updates look direction — position is driven by Player_ApplyToCamera
void Camera_UpdateLook(FreeCam* cam, s8 cStickX, s8 cStickY) {
    if (cStickX > 15 || cStickX < -15) cam->yaw   += (cStickX / 128.0f) * g_config.sensitivity;
    if (cStickY > 15 || cStickY < -15) cam->pitch += (cStickY / 128.0f) * g_config.sensitivity;

    if (cam->pitch >  89.0f) cam->pitch =  89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    float cosPitch = cosf(cam->pitch * (3.14159265f / 180.0f));
    float sinPitch = sinf(cam->pitch * (3.14159265f / 180.0f));
    float cosYaw   = cosf(cam->yaw   * (3.14159265f / 180.0f));
    float sinYaw   = sinf(cam->yaw   * (3.14159265f / 180.0f));

    cam->forward.x = cosYaw * cosPitch;
    cam->forward.y = sinPitch;
    cam->forward.z = sinYaw * cosPitch;
}

// Legacy — kept so old Camera_Update call sites still compile
void Camera_Update(FreeCam* cam, s8 stickX, s8 stickY, s8 cStickX, s8 cStickY) {
    Camera_UpdateLook(cam, cStickX, cStickY);
    // Movement intentionally removed — Player_Update handles it now
}

void Camera_Apply(FreeCam* cam) {
    guVector lookAt;
    guVecAdd(&cam->pos, &cam->forward, &lookAt);
    guLookAt(g_viewMatrix, &cam->pos, &cam->up, &lookAt);
}