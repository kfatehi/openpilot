#!/usr/bin/env python
import asyncio
import json
import logging
import os
import platform
import ssl

from aiohttp import web

from aiortc import RTCPeerConnection, RTCSessionDescription
from aiortc.contrib.media import MediaPlayer, MediaRelay

from vision_ipc_rtc_track import VisionIpcTrack
from cereal.visionipc.visionipc_pyx import VisionStreamType
from aiortc.rtcrtpsender import RTCRtpSender
from aiortc.contrib.media import MediaPlayer,MediaRelay
import subprocess
import os


PIPE_ENCODER = "../loggerd/pipe_encoder"
global encoder_proc

encoder_proc = None


global relay
encoder_proc = subprocess.Popen(PIPE_ENCODER, bufsize=0, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

global cam

def create_local_tracks():
    global relay
    global encoder_proc
    global cam
    cam = MediaPlayer('/proc/'+str(os.getpid())+'/fd/'+str(encoder_proc.stderr.name), transcode=False, options={"video_size": "1928x1208", "framerate": "20", "input_format": "h264"})
    relay = MediaRelay()
    return None, relay.subscribe(cam.video)


async def index(request):
    content = open(os.path.join("index.html"), "r").read()
    return web.Response(content_type="text/html", text=content)


async def javascript(request):
    content = open(os.path.join("client.js"), "r").read()
    return web.Response(content_type="application/javascript", text=content)


async def offer(request):
    params = await request.json()
    offer = RTCSessionDescription(sdp=params["sdp"], type=params["type"])

    pc = RTCPeerConnection()
    pcs.add(pc)

    @pc.on("connectionstatechange")
    async def on_connectionstatechange():
        print("Connection state is %s" % pc.connectionState)
        if pc.connectionState == "failed":
            await pc.close()
            pcs.discard(pc)

    # open media source
    audio, video = create_local_tracks()

    if video:
        pc.addTrack(video)
        # Filter for only for the preferred_codec
        codecs = RTCRtpSender.getCapabilities("video").codecs
        preferences = [codec for codec in codecs if codec.mimeType == "video/H264"]
        transceiver = pc.getTransceivers()[0]
        transceiver.setCodecPreferences(preferences)

    await pc.setRemoteDescription(offer)
    for t in pc.getTransceivers():
        if t.kind == "audio" and audio:
            pc.addTrack(audio)

    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)



    return web.Response(
        content_type="application/json",
        text=json.dumps(
            {"sdp": pc.localDescription.sdp, "type": pc.localDescription.type}
        ),
    )


pcs = set()


async def on_shutdown(app):
    # close peer connections
    coros = [pc.close() for pc in pcs]
    await asyncio.gather(*coros)
    pcs.clear()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    ssl_context = None

    app = web.Application()
    app.on_shutdown.append(on_shutdown)
    app.router.add_get("/", index)
    app.router.add_get("/client.js", javascript)
    app.router.add_post("/offer", offer)
    web.run_app(app, host="192.168.27.102", port="3000", ssl_context=ssl_context)