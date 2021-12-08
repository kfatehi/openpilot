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


---

PR 559 makes us be able to bring back pipe encoder, but Robbe predicted
this solution was not going to work too well, as we see it introduces a
huge amount of latency.

Instead, we would like for AIORTC's h264 encoder to be replaced with OMX.
This should be possible, and PR 562 is somewhat along those lines. Anyway
We need to hack inside so let's see if we can install a local checkout:

python -m pip install git+file:///data/media/developer/pip/checkouts/aiortc#egg=aiortc

unfortunately that does not work

but actually there is an even simpler way (Which also does not work):

cd into our checkout
pip -e .

the failure has to do with our environment but like it says here
https://github.com/pypa/pip/issues/8438#issuecomment-674427517
if we set our target there instead of in an env var it should work:

comma@tici:/data/openpilot$ export TMPDIR=/data/media/developer/pip/tmp
comma@tici:/data/openpilot$ export PIP_CACHE_DIR=/data/media/developer/pip/cache
comma@tici:/data/openpilot$ cd /data/media/developer/pip/checkouts/aiortc/
comma@tici:/data/media/developer/pip/checkouts/aiortc$ pip install --target /data/openpilot/pyextra -e .

"e" means editable so this creates a '.egg-link' file in the target dir
which points to where the source checkout lives

unfortunately this is still failing to yeild an editable scenario.
regardless...

it is interesting that PR 562 exists despite the existence of PR 488
https://github.com/aiortc/aiortc/pull/488/files
which implements OMX for Pi.

562 is not so much about encoding using the OMX within aiortc but rather
turning off transcode/re-encode similar to PR 559 because the Pi is
already shipping an encoded stream in the author's example -- it's just
a less intrusive implementation of the same end.

Meanwhile, on the tici, we have VisionIPC which gives us a raw YUV
and we have a separate OMX encoder which is probably different from
the Pi one implemented in 488, but the existince of 488 should serve
as a pretty good example of how to get the tici's encoder working
efficiently within aiortc

this gets close but still no import luck (even when putting a __init__.py in the root)

pip install --upgrade --target /data/openpilot/pyextra -e /data/media/developer/pip/checkouts/aiortc 

but maybe just maybe... this does something:

export TMPDIR=/data/media/developer/pip/tmp
export PIP_CACHE_DIR=/data/media/developer/pip/cache
python setup.py develop --install-dir /data/openpilot/pyextra

now i can import and edit aiortc!