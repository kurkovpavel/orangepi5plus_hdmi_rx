#ifndef CAMERA_HDMI_H
#define CAMERA_HDMI_H

#include <stdbool.h>
#include <opencv2/core/core.hpp>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CameraContext CameraContext;

// Create/destroy camera context
CameraContext* camera_create(const char* device);
void camera_destroy(CameraContext* ctx);

// Frame capture functions
bool camera_start(CameraContext* ctx);
bool camera_capture_frame(CameraContext* ctx, cv::Mat& frame);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_HDMI_H