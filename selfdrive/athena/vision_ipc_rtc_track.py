from av import VideoFrame
from cereal.visionipc.visionipc_pyx import VisionIpcClient # pylint: disable=no-name-in-module, import-error
from aiortc import VideoStreamTrack
import asyncio
import numpy as np

class VisionIpcTrack(VideoStreamTrack):
  def __init__(self, vision_stream_type):
    super().__init__()
    self.vipc_client = VisionIpcClient("camerad", vision_stream_type, True)

  async def recv(self):
    pts, time_base = await self.next_timestamp()

    # Connect if not connected
    while not self.vipc_client.is_connected():
      self.vipc_client.connect(True)
      print("vision ipc connected")

    raw_frame = None
    while raw_frame is None or not raw_frame.any():
      raw_frame = self.vipc_client.recv()

    raw_frame = np.frombuffer(raw_frame, dtype=np.uint8).reshape((self.vipc_client.height, self.vipc_client.width, 3))
    frame = VideoFrame.from_ndarray(raw_frame, "bgr24")
    frame.pts = pts
    frame.time_base = time_base
    return frame

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