#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include "selfdrive/streamerd/stream_encoder.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>

#include <OMX_Component.h>
#include <OMX_IndexExt.h>
#include <OMX_QCOMExtns.h>
#include <OMX_VideoExt.h>
#include "libyuv.h"

#include "selfdrive/common/swaglog.h"
#include "selfdrive/common/util.h"
#include "selfdrive/loggerd/include/msm_media_info.h"

// Check the OMX error code and assert if an error occurred.
#define OMX_CHECK(_expr)              \
  do {                                \
    assert(OMX_ErrorNone == (_expr)); \
  } while (0)

extern ExitHandler do_exit;

// ***** OMX callback functions *****

void StreamEncoder::wait_for_state(OMX_STATETYPE state_) {
  std::unique_lock lk(this->state_lock);
  while (this->state != state_) {
    this->state_cv.wait(lk);
  }
}

static OMX_CALLBACKTYPE omx_callbacks = {
  .EventHandler = StreamEncoder::event_handler,
  .EmptyBufferDone = StreamEncoder::empty_buffer_done,
  .FillBufferDone = StreamEncoder::fill_buffer_done,
};

OMX_ERRORTYPE StreamEncoder::event_handler(OMX_HANDLETYPE component, OMX_PTR app_data, OMX_EVENTTYPE event,
                                   OMX_U32 data1, OMX_U32 data2, OMX_PTR event_data) {
  StreamEncoder *e = (StreamEncoder*)app_data;
  if (event == OMX_EventCmdComplete) {
    assert(data1 == OMX_CommandStateSet);
    LOG("set state event 0x%x", data2);
    {
      std::unique_lock lk(e->state_lock);
      e->state = (OMX_STATETYPE)data2;
    }
    e->state_cv.notify_all();
  } else if (event == OMX_EventError) {
    LOGE("OMX error 0x%08x", data1);
  } else {
    LOGE("OMX unhandled event %d", event);
    assert(false);
  }

  return OMX_ErrorNone;
}

/** The EmptyBufferDone method is used to return emptied buffers from an
    input port back to the application for reuse.  This is a blocking call 
    so the application should not attempt to refill the buffers during this
    call, but should queue them and refill them in another thread.  There
    is no error return, so the application shall handle any errors generated
    internally.  
    
    The application should return from this call within 5 msec. */
OMX_ERRORTYPE StreamEncoder::empty_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                                   OMX_BUFFERHEADERTYPE *buffer) {
  StreamEncoder *e = (StreamEncoder*)app_data;
  e->free_in.push(buffer);
  return OMX_ErrorNone;
}

/** The FillBufferDone method is used to return filled buffers from an
    output port back to the application for emptying and then reuse.  
    This is a blocking call so the application should not attempt to 
    empty the buffers during this call, but should queue the buffers 
    and empty them in another thread.  There is no error return, so 
    the application shall handle any errors generated internally.  The 
    application shall also update the buffer header to indicate the
    number of bytes placed into the buffer.  

    The application should return from this call within 5 msec. */
OMX_ERRORTYPE StreamEncoder::fill_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                                  OMX_BUFFERHEADERTYPE *buffer) {
  StreamEncoder *e = (StreamEncoder*)app_data;
  e->done_out.push(buffer);
  return OMX_ErrorNone;
}

#define PORT_INDEX_IN 0
#define PORT_INDEX_OUT 1



// ***** encoder functions *****

StreamEncoder::StreamEncoder(int width, int height, int fps, int bitrate) {
  this->width = width;
  this->height = height;
  this->fps = fps;

  auto component = (OMX_STRING)("OMX.qcom.video.encoder.avc");
  int err = OMX_GetHandle(&this->handle, component, this, &omx_callbacks);
  if (err != OMX_ErrorNone) {
    LOGE("error getting codec: %x", err);
  }
  assert(err == OMX_ErrorNone);
  // printf("handle: %p\n", this->handle);

  // setup input port

  OMX_PARAM_PORTDEFINITIONTYPE in_port = {0};
  in_port.nSize = sizeof(in_port);
  in_port.nPortIndex = (OMX_U32) PORT_INDEX_IN;
  OMX_CHECK(OMX_GetParameter(this->handle, OMX_IndexParamPortDefinition, (OMX_PTR) &in_port));

  in_port.format.video.nFrameWidth = this->width;
  in_port.format.video.nFrameHeight = this->height;
  in_port.format.video.nStride = VENUS_Y_STRIDE(COLOR_FMT_NV12, this->width);
  in_port.format.video.nSliceHeight = this->height;
  // in_port.nBufferSize = (this->width * this->height * 3) / 2;
  in_port.nBufferSize = VENUS_BUFFER_SIZE(COLOR_FMT_NV12, this->width, this->height);
  in_port.format.video.xFramerate = (this->fps * 65536);
  in_port.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  // in_port.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
  in_port.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;

  OMX_CHECK(OMX_SetParameter(this->handle, OMX_IndexParamPortDefinition, (OMX_PTR) &in_port));
  OMX_CHECK(OMX_GetParameter(this->handle, OMX_IndexParamPortDefinition, (OMX_PTR) &in_port));
  this->in_buf_headers.resize(in_port.nBufferCountActual);

  // setup output port

  OMX_PARAM_PORTDEFINITIONTYPE out_port = {0};
  out_port.nSize = sizeof(out_port);
  out_port.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  OMX_CHECK(OMX_GetParameter(this->handle, OMX_IndexParamPortDefinition, (OMX_PTR)&out_port));
  out_port.format.video.nFrameWidth = this->width;
  out_port.format.video.nFrameHeight = this->height;
  out_port.format.video.xFramerate = 0;
  out_port.format.video.nBitrate = bitrate;
  out_port.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

  out_port.format.video.eColorFormat = OMX_COLOR_FormatUnused;

  OMX_CHECK(OMX_SetParameter(this->handle, OMX_IndexParamPortDefinition, (OMX_PTR) &out_port));

  OMX_CHECK(OMX_GetParameter(this->handle, OMX_IndexParamPortDefinition, (OMX_PTR) &out_port));
  this->out_buf_headers.resize(out_port.nBufferCountActual);

  OMX_VIDEO_PARAM_BITRATETYPE bitrate_type = {0};
  bitrate_type.nSize = sizeof(bitrate_type);
  bitrate_type.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  OMX_CHECK(OMX_GetParameter(this->handle, OMX_IndexParamVideoBitrate, (OMX_PTR) &bitrate_type));
  bitrate_type.eControlRate = OMX_Video_ControlRateVariable;
  bitrate_type.nTargetBitrate = bitrate;

  OMX_CHECK(OMX_SetParameter(this->handle, OMX_IndexParamVideoBitrate, (OMX_PTR) &bitrate_type));

  // setup h264
  OMX_VIDEO_PARAM_AVCTYPE avc = { 0 };
  avc.nSize = sizeof(avc);
  avc.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  OMX_CHECK(OMX_GetParameter(this->handle, OMX_IndexParamVideoAvc, &avc));

  avc.nBFrames = 0;
  avc.nPFrames = 15;

  avc.eProfile = OMX_VIDEO_AVCProfileHigh;
  avc.eLevel = OMX_VIDEO_AVCLevel31;

  avc.nAllowedPictureTypes |= OMX_VIDEO_PictureTypeB;
  avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

  avc.nRefFrames = 1;
  avc.bUseHadamard = OMX_TRUE;
  avc.bEntropyCodingCABAC = OMX_TRUE;
  avc.bWeightedPPrediction = OMX_TRUE;
  avc.bconstIpred = OMX_TRUE;

  OMX_CHECK(OMX_SetParameter(this->handle, OMX_IndexParamVideoAvc, &avc));

  OMX_CHECK(OMX_SendCommand(this->handle, OMX_CommandStateSet, OMX_StateIdle, NULL));

  for (auto &buf : this->in_buf_headers) {
    OMX_CHECK(OMX_AllocateBuffer(this->handle, &buf, PORT_INDEX_IN, this,
                             in_port.nBufferSize));
  }

  for (auto &buf : this->out_buf_headers) {
    OMX_CHECK(OMX_AllocateBuffer(this->handle, &buf, PORT_INDEX_OUT, this,
                             out_port.nBufferSize));
  }

  wait_for_state(OMX_StateIdle);

  OMX_CHECK(OMX_SendCommand(this->handle, OMX_CommandStateSet, OMX_StateExecuting, NULL));

  wait_for_state(OMX_StateExecuting);

  // give omx all the output buffers
  for (auto &buf : this->out_buf_headers) {
    // printf("fill %p\n", this->out_buf_headers[i]);
    OMX_CHECK(OMX_FillThisBuffer(this->handle, buf));
  }

  // fill the input free queue
  for (auto &buf : this->in_buf_headers) {
    this->free_in.push(buf);
  }
}

void StreamEncoder::handle_out_buf(StreamEncoder *e, OMX_BUFFERHEADERTYPE *out_buf) {
  uint8_t *buf_data = out_buf->pBuffer + out_buf->nOffset;

  if (out_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    if (e->codec_config_len < out_buf->nFilledLen) {
      e->codec_config = (uint8_t *)realloc(e->codec_config, out_buf->nFilledLen);
    }
    e->codec_config_len = out_buf->nFilledLen;
    memcpy(e->codec_config, buf_data, out_buf->nFilledLen);
#ifdef QCOM2
    out_buf->nTimeStamp = 0;
#endif
  }

  // Is this the correct place, then, to turn the H264 into RTP payloads and send them out to subscriber(s) ?
  // If so then the next missing link is something like this: 
  // https://github.com/GStreamer/gst-plugins-good/blob/master/gst/rtp/gstrtph264pay.c

  // give omx back the buffer
#ifdef QCOM2
  if (out_buf->nFlags & OMX_BUFFERFLAG_EOS) {
    out_buf->nTimeStamp = 0;
  }
#endif
  OMX_CHECK(OMX_FillThisBuffer(e->handle, out_buf));
}

int StreamEncoder::encode_frame(const uint8_t *y_ptr, const uint8_t *u_ptr, const uint8_t *v_ptr,
                             int in_width, int in_height, uint64_t ts) {
  int err;

  // this sometimes freezes... put it outside the encoder lock so we can still trigger rotates...
  // THIS IS A REALLY BAD IDEA, but apparently the race has to happen 30 times to trigger this
  //pthread_mutex_unlock(&this->lock);
  OMX_BUFFERHEADERTYPE* in_buf = nullptr;
  while (!this->free_in.try_pop(in_buf, 20)) {
    if (do_exit) {
      return -1;
    }
  }

  //pthread_mutex_lock(&this->lock);

  int ret = this->counter;

  uint8_t *in_buf_ptr = in_buf->pBuffer;

  uint8_t *in_y_ptr = in_buf_ptr;
  int in_y_stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, this->width);
  int in_uv_stride = VENUS_UV_STRIDE(COLOR_FMT_NV12, this->width);
  // uint8_t *in_uv_ptr = in_buf_ptr + (this->width * this->height);
  uint8_t *in_uv_ptr = in_buf_ptr + (in_y_stride * VENUS_Y_SCANLINES(COLOR_FMT_NV12, this->height));

  err = libyuv::I420ToNV12(y_ptr, this->width,
                   u_ptr, this->width/2,
                   v_ptr, this->width/2,
                   in_y_ptr, in_y_stride,
                   in_uv_ptr, in_uv_stride,
                   this->width, this->height);
  assert(err == 0);

  // in_buf->nFilledLen = (this->width*this->height) + (this->width*this->height/2);
  in_buf->nFilledLen = VENUS_BUFFER_SIZE(COLOR_FMT_NV12, this->width, this->height);
  in_buf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
  in_buf->nOffset = 0;
  in_buf->nTimeStamp = ts/1000LL;  // OMX_TICKS, in microseconds
  this->last_t = in_buf->nTimeStamp;

  OMX_CHECK(OMX_EmptyThisBuffer(this->handle, in_buf));

  // pump output
  while (true) {
    OMX_BUFFERHEADERTYPE *out_buf;
    if (!this->done_out.try_pop(out_buf)) {
      break;
    }
    handle_out_buf(this, out_buf);
  }

  this->counter++;

  return ret;
}


StreamEncoder::~StreamEncoder() {
  OMX_CHECK(OMX_SendCommand(this->handle, OMX_CommandStateSet, OMX_StateIdle, NULL));

  wait_for_state(OMX_StateIdle);

  OMX_CHECK(OMX_SendCommand(this->handle, OMX_CommandStateSet, OMX_StateLoaded, NULL));

  for (auto &buf : this->in_buf_headers) {
    OMX_CHECK(OMX_FreeBuffer(this->handle, PORT_INDEX_IN, buf));
  }

  for (auto &buf : this->out_buf_headers) {
    OMX_CHECK(OMX_FreeBuffer(this->handle, PORT_INDEX_OUT, buf));
  }

  wait_for_state(OMX_StateLoaded);

  OMX_CHECK(OMX_FreeHandle(this->handle));

  OMX_BUFFERHEADERTYPE *out_buf;
  while (this->free_in.try_pop(out_buf));
  while (this->done_out.try_pop(out_buf));

  if (this->codec_config) {
    free(this->codec_config);
  }
}
