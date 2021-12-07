https://community.octoprint.org/t/low-latency-h264-streaming-support-w-webrtc/36996/8

https://github.com/aiortc/aiortc/pull/559/files
https://github.com/aiortc/aiortc/pull/562


the first one seems fine enough let's try it.
    const int full_width_tici = 1928;
    const int full_height_tici = 1208;
python webcam.py --no-transcode --preferred-codec=video/H264 --video-options='
{"video_size": "1928x1208", "framerate": "20", "input_format": "h264"}
'


python -m pip install git+https://github.com/rprata/aiortc.git@fc653c330b956cb75a11fc4adf7507a0c6e9eefc