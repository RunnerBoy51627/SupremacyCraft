#ifndef CAMERA_H
#define CAMERA_H

#include <gccore.h>

typedef struct {
    guVector pos;
    guVector up;
    guVector forward;
    float yaw;
    float pitch;
} FreeCam;

extern Mtx g_viewMatrix;

void Camera_Init(FreeCam* cam);
void Camera_Update(FreeCam* cam, s8 stickX, s8 stickY, s8 cStickX, s8 cStickY);
void Camera_UpdateLook(FreeCam* cam, s8 cStickX, s8 cStickY);
void Camera_Apply(FreeCam* cam);

#endif