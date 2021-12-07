https://community.octoprint.org/t/low-latency-h264-streaming-support-w-webrtc/36996/8

https://github.com/aiortc/aiortc/pull/559/files
https://github.com/aiortc/aiortc/pull/562


the first one seems fine enough let's try it.

python webcam.py --no-transcode --preferred-codec=video/H264 --video-options='{"video_size": "1920x1080", "framerate": "30", "input_format": "h264"}'


python -m pip install git+https://github.com/rprata/aiortc.git@fc653c330b956cb75a11fc4adf7507a0c6e9eefc