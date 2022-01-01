#include <thread>

#include "cereal/messaging/messaging.h"
#include "cereal/services.h"
#include "cereal/visionipc/visionipc.h"
#include "cereal/visionipc/visionipc_client.h"
#include "selfdrive/camerad/cameras/camera_common.h"
#include "selfdrive/common/swaglog.h"
#include "selfdrive/common/util.h"

#include "selfdrive/streamerd/stream_encoder.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
typedef int SOCKET;
SOCKET sock;
sockaddr_in addr;

constexpr int FPS = 20;
const int BITRATE = 512000;

namespace {
ExitHandler do_exit;

void send_data_to_rtp(uint8_t *data, int len, int framerate) {
  char buffer[50];
  sprintf(buffer, "%d\n", len);
  sendto(sock, buffer, sizeof(buffer), 0, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
}

void encoder_thread() {
  VisionIpcClient vipc_client = VisionIpcClient("camerad", VISION_STREAM_DRIVER, false);
  StreamEncoder *encoder = NULL;

  while (!do_exit) {
    if (!vipc_client.connect(false)) {
      util::sleep_for(1);
      continue;
    }
    printf("connected\n");
    VisionBuf buf_info = vipc_client.buffers[0];

    if (encoder == NULL) {
      encoder = new StreamEncoder(buf_info.width, buf_info.height, FPS, BITRATE, send_data_to_rtp);
    }

    while (!do_exit) {
      // recv frame
      VisionIpcBufExtra extra;
      VisionBuf* buf = vipc_client.recv(&extra);
      if (buf == nullptr) continue;
      // encode and pipe to stderr
      int out_id = encoder->encode_frame(buf->y, buf->u, buf->v, buf->width, buf->height, extra.timestamp_eof);
      if (out_id == -1) {
        printf("out_id not valid\n");
      }
    }
  }
}

} // namespace


int main(int argc, char** argv) {

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  addr.sin_addr.s_addr = inet_addr("192.168.27.119");
  addr.sin_port = htons(5000);
  addr.sin_family = AF_INET;


  Context::create();
  std::thread encoding_thread = std::thread(encoder_thread);
  // while (!do_exit) {
  //   do main loop stuff`2
  // }
  encoding_thread.join();
}