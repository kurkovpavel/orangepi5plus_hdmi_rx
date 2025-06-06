#include "camera_hdmi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>

#define VIDEO_MAX_PLANES 8

struct CameraContext {
    int fd;
    struct v4l2_format fmt;
    struct v4l2_dv_timings timings;
    void* buffers[VIDEO_MAX_PLANES];
    struct v4l2_buffer buf;
    bool streaming;
};

CameraContext* camera_create(const char* device) {
    CameraContext* ctx = (CameraContext*)calloc(1, sizeof(CameraContext));
    if (!ctx) return NULL;

    // 1. Open device
    ctx->fd = open(device, O_RDWR);
    if (ctx->fd < 0) {
        perror("Failed to open device");
        free(ctx);
        return NULL;
    }

    // 2. Query capabilities
    struct v4l2_capability cap;
    if (ioctl(ctx->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP failed");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    printf("Driver: %s\n", cap.driver);
    printf("Card: %s\n", cap.card);
    printf("Bus info: %s\n", cap.bus_info);
    printf("Capabilities: 0x%x\n", cap.capabilities);

    // 3. Get current format
    memset(&ctx->fmt, 0, sizeof(ctx->fmt));
    ctx->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    
    if (ioctl(ctx->fd, VIDIOC_G_FMT, &ctx->fmt) < 0) {
        perror("Failed to get current format");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    printf("Current format: %.4s (%dx%d)\n", 
           (char*)&ctx->fmt.fmt.pix_mp.pixelformat,
           ctx->fmt.fmt.pix_mp.width,
           ctx->fmt.fmt.pix_mp.height);
    std::cout << "Pixel format: " << std::hex << ctx->fmt.fmt.pix_mp.pixelformat << std::dec << std::endl;
    std::cout << "Field: " << ctx->fmt.fmt.pix_mp.field << std::endl;
    std::cout << "Colorspace: " << ctx->fmt.fmt.pix_mp.colorspace << std::endl;
    
    // 4. Get DV timings (critical for HDMI)
    memset(&ctx->timings, 0, sizeof(ctx->timings));
    if (ioctl(ctx->fd, VIDIOC_G_DV_TIMINGS, &ctx->timings) < 0) {
        perror("Failed to get timings");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    // 5. Set format (important for some drivers)
    if (ioctl(ctx->fd, VIDIOC_S_FMT, &ctx->fmt) < 0) {
        perror("Failed to set format");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    // 6. Request buffers
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(ctx->fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Failed to request buffers");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    // 7. Map the buffer
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    memset(&ctx->buf, 0, sizeof(ctx->buf));
    ctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ctx->buf.memory = V4L2_MEMORY_MMAP;
    ctx->buf.index = 0;
    ctx->buf.length = ctx->fmt.fmt.pix_mp.num_planes;
    ctx->buf.m.planes = planes;

    if (ioctl(ctx->fd, VIDIOC_QUERYBUF, &ctx->buf) < 0) {
        perror("Failed to query buffer");
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    for (int i = 0; i < ctx->fmt.fmt.pix_mp.num_planes; i++) {
        ctx->buffers[i] = mmap(NULL, ctx->buf.m.planes[i].length, 
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             ctx->fd, ctx->buf.m.planes[i].m.mem_offset);
        
        if (ctx->buffers[i] == MAP_FAILED) {
            perror("Failed to mmap buffer");
            for (int j = 0; j < i; j++)
                munmap(ctx->buffers[j], ctx->buf.m.planes[j].length);
            close(ctx->fd);
            free(ctx);
            return NULL;
        }
    }

    // 8. Queue the buffer (critical step)
    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer bufq = {0};
        struct v4l2_plane planesq[VIDEO_MAX_PLANES] = {0};
        
        bufq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        bufq.memory = V4L2_MEMORY_MMAP;
        bufq.index = i;
        bufq.length = ctx->fmt.fmt.pix_mp.num_planes;
        bufq.m.planes = planesq;

        // Set bytesused for each plane
        for (int j = 0; j < ctx->fmt.fmt.pix_mp.num_planes; j++) {
            bufq.m.planes[j].bytesused = ctx->fmt.fmt.pix_mp.plane_fmt[j].sizeimage;
        }

        if (ioctl(ctx->fd, VIDIOC_QBUF, &bufq) < 0) {
            perror("Failed to queue buffer");
            for (int j = 0; j < ctx->fmt.fmt.pix_mp.num_planes; j++)
                munmap(ctx->buffers[j], ctx->buf.m.planes[j].length);
            close(ctx->fd);
            free(ctx);
            return NULL;
        }
    }

    return ctx;
}

bool camera_start(CameraContext* ctx) {
    if (!ctx) return false;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(ctx->fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Failed to start streaming");
        return false;
    }

    ctx->streaming = true;
    return true;
}

bool camera_capture_frame(CameraContext* ctx, cv::Mat& frame) {
    if (!ctx || !ctx->streaming) return false;

    // Reset buffer info
    memset(&ctx->buf, 0, sizeof(ctx->buf));
    ctx->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ctx->buf.memory = V4L2_MEMORY_MMAP;
    ctx->buf.index = 0;
    ctx->buf.length = ctx->fmt.fmt.pix_mp.num_planes;
    ctx->buf.m.planes = (struct v4l2_plane*)malloc(sizeof(struct v4l2_plane) * ctx->fmt.fmt.pix_mp.num_planes);

    // Dequeue buffer
    if (ioctl(ctx->fd, VIDIOC_DQBUF, &ctx->buf) < 0) {
        perror("Failed to dequeue buffer");
        free(ctx->buf.m.planes);
        return false;
    }

    // Process frame
    switch (ctx->fmt.fmt.pix_mp.pixelformat) {
        case V4L2_PIX_FMT_NV24: {
            if (!ctx->buffers[0]) {
                std::cerr << "Invalid buffer pointer!" << std::endl;
                free(ctx->buf.m.planes);
                return false;
            }

            int width = ctx->fmt.fmt.pix_mp.width;
            int height = ctx->fmt.fmt.pix_mp.height;
            uchar* yuv_data = static_cast<uchar*>(ctx->buffers[0]);

            // Convert to BGR
            cv::Mat yuv(height + height / 2, width, CV_8UC1, yuv_data);
            cv::cvtColor(yuv, frame, cv::COLOR_YUV2BGR_I420);
            break;
        }
        default:
            fprintf(stderr, "Unsupported pixel format: 0x%x\n", ctx->fmt.fmt.pix_mp.pixelformat);
            free(ctx->buf.m.planes);
            return false;
    }

    // Requeue buffer
    if (ioctl(ctx->fd, VIDIOC_QBUF, &ctx->buf) < 0) {
        perror("Failed to requeue buffer");
    }

    return true;
}

void camera_destroy(CameraContext* ctx) {
    if (!ctx) return;

    if (ctx->streaming) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        ioctl(ctx->fd, VIDIOC_STREAMOFF, &type);
    }

    for (int i = 0; i < ctx->fmt.fmt.pix_mp.num_planes; i++) {
        if (ctx->buffers[i] != MAP_FAILED) {
            munmap(ctx->buffers[i], ctx->buf.m.planes[i].length);
        }
    }

    close(ctx->fd);
    free(ctx);
}