# orangepi5plus_hdmi_rx
Capturing from hdmi rx input to cpp pipeline without external libraries like ffmpeg or gstreamer.
Since hdmi_rx is not fullly compatible driver with v4l2, this example allows to capture video to C++ pipeline directly from driver. It uses NV24 (YUV420P actually, not NV24 as driver says) pixelformat only, if your source makes NV12 or RGB/BGR do your own realization in camera_capture_frame (camera_hdmi.c)

Compile: g++ main.cpp camera_hdmi.c -o hdmi_capture `pkg-config --cflags --libs opencv4` -lv4l2
Left screen is feeded by orange pi 5 plus: hdmi rx in -> framebuffer -> hdmi tx out. Right screen is original source on hdmi rx in.
![20250820_111933](https://github.com/user-attachments/assets/75c76e57-616c-44e0-9e6a-57a5dad1eaa4)
