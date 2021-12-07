import asyncio

class VisionIpcTrack():
  def __init__(self, vision_stream_type):
    super().__init__()
    global encoder_proc
    encoder_proc = subprocess.Popen(PIPE_ENCODER, bufsize=0, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

  async def recv(self):
    return encoder_proc.stderr.read(100)

if __name__ == "__main__":
    import sys
    from cereal.visionipc.visionipc_pyx import VisionStreamType # pylint: disable=no-name-in-module, import-error
    async def test():
        track = VisionIpcTrack(VisionStreamType.VISION_STREAM_RGB_FRONT)
        while True:
            await track.recv()
        
    # Run event loop
    loop = asyncio.new_event_loop()
    try:
        loop.run_until_complete(test())
    except KeyboardInterrupt:
        sys.exit(0)