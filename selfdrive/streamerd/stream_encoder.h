#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

#include <OMX_Component.h>
extern "C" {
#include <libavformat/avformat.h>
}

#include "selfdrive/common/queue.h"

// StreamEncoder, lossey codec using hardware hevc
class StreamEncoder {
public:
  StreamEncoder(int width, int height, int fps, int bitrate, void (*send_data_to_rtp)(uint8_t *data, int len, int framerate));
  ~StreamEncoder();
  int encode_frame(const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr,
                   int in_width, int in_height, uint64_t ts);

  // OMX callbacks
  static OMX_ERRORTYPE event_handler(OMX_HANDLETYPE component, OMX_PTR app_data, OMX_EVENTTYPE event,
                                     OMX_U32 data1, OMX_U32 data2, OMX_PTR event_data);
  static OMX_ERRORTYPE empty_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                         OMX_BUFFERHEADERTYPE *buffer);
  static OMX_ERRORTYPE fill_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                        OMX_BUFFERHEADERTYPE *buffer);

private:
  void wait_for_state(OMX_STATETYPE state);
  static void handle_out_buf(StreamEncoder *e, OMX_BUFFERHEADERTYPE *out_buf);

  void (*send_data_to_rtp)(uint8_t *data, int len, int framerate);
  int width, height, fps;
  int counter = 0;

  size_t codec_config_len;
  uint8_t *codec_config = NULL;

  std::mutex state_lock;
  std::condition_variable state_cv;
  OMX_STATETYPE state = OMX_StateLoaded;

  OMX_HANDLETYPE handle;

  std::vector<OMX_BUFFERHEADERTYPE *> in_buf_headers;
  std::vector<OMX_BUFFERHEADERTYPE *> out_buf_headers;

  uint64_t last_t;

  SafeQueue<OMX_BUFFERHEADERTYPE *> free_in;
  SafeQueue<OMX_BUFFERHEADERTYPE *> done_out;
};
