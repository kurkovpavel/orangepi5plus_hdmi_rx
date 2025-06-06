#include "camera_hdmi.h"
#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    // 1. Create camera context
    CameraContext* camera = camera_create("/dev/video0");
    if (!camera) {
        std::cerr << "Failed to initialize camera" << std::endl;
        return 1;
    }

    // 2. Start capturing
    if (!camera_start(camera)) {
        std::cerr << "Failed to start camera" << std::endl;
        camera_destroy(camera);
        return 1;
    }

    // 3. Setup video writer
    cv::VideoWriter videoWriter("output.avi", 
                              cv::VideoWriter::fourcc('M','J','P','G'), 
                              60.0, 
                              cv::Size(1920, 1080), // Adjust to your resolution
                              true);
    if (!videoWriter.isOpened()) {
        std::cerr << "Failed to open VideoWriter" << std::endl;
        camera_destroy(camera);
        return 1;
    }

    // 4. Capture loop
    while (true) {
        cv::Mat frame;
        if (!camera_capture_frame(camera, frame)) {
            std::cerr << "Failed to capture frame" << std::endl;
            break;
        }

        // Write to video
        videoWriter.write(frame);

        // Display frame (optional)
        //cv::imshow("Frame", frame);
        if (cv::waitKey(1) == 'q') break;
    }

    // 5. Cleanup
    videoWriter.release();
    camera_destroy(camera);
    return 0;
}