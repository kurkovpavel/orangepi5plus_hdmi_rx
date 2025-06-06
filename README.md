# orangepi5plus_hdmi_rx
Capturing from hdmi rx input to cpp pipeline without external libraries like ffmpeg or gstreamer.
Since hdmi_rx is not fullly compatible driver with v4l2, this example allows to capture video to C++ pipeline directly from driver. It uses NV24 (YUV420P actually, not NV24 as driver says) pixelformat only, if your source makes NV12 or RGB/BGR do your onw realization in camera_capture_frame (camera_hdmi.c)
