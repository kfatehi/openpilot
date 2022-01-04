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

int FPS = 20;
int BITRATE = 5120000;
char *target = NULL;
int port = 50000;

// typedef struct{
//   uint8_t *buf;
//   uint8_t len;
// } nalu;

// SafeQueue<nalu *> nalUnitsFromEncoder;

namespace {
ExitHandler do_exit;




#define TYPE_H264               96
#define SSRC_NUM                10
#define BUF_SIZE                1500
#define RTP_PAYLOAD_MAX_SIZE    1400
/* RTP HEADER */
typedef struct{
    /* 1 byte */
    uint8_t csrc_len:    4;    
    uint8_t extension:    1;    
    uint8_t padding:    1;    
    uint8_t version:    2;    
    /* 2 byte */
    uint8_t payload_type:    7;    
    uint8_t marker:        1;    
    /* 3-4 */
    uint16_t seq_no;        
    /* 5-8 */
    uint32_t timestamp;        
    /* 9-12 */
    uint32_t ssrc;            
}__attribute__ ((packed)) rtp_header;

typedef struct {
    uint8_t type:        5;    
    uint8_t nri:        2;   
    uint8_t f:        1;   
}__attribute__ ((packed)) nalu_header;

typedef struct {
    uint8_t type: 5;
    uint8_t nri: 2;
    uint8_t f: 1;
} __attribute__ ((packed)) fu_indicator;

typedef struct {
    uint8_t type: 5;
    uint8_t r: 1;
    uint8_t e: 1;
    uint8_t s: 1;
} __attribute__ ((packed)) fu_header;


// Write data to socket
void send_data_client(uint8_t *send_buf, size_t len_sendbuf)
{
  sendto(sock, send_buf, len_sendbuf, 0, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
}

void send_data_to_rtp(uint8_t *data, int len) {
  // this happens on encoder thread
  // on the network thread we will actually be draining it
  // and then calling the real function
  // nalu *thisnalu;
  // thisnalu->buf = data;
  // thisnalu->len = len;
  // nalUnitsFromEncoder.push(thisnalu);

    static uint8_t sendbuf[BUF_SIZE];
    static uint32_t ts_current = 0;
    static uint16_t seq_num = 0;
    static uint16_t pack_num, last_pack_size, current_pack;
    uint8_t *nalu_playload;
    /* RTP HEADER */
    rtp_header *rtp_hdr;
    /* NALU HEADER */
    nalu_header *nalu_hdr;

    fu_indicator *fu_ind;

    fu_header *fu_hdr;

    ts_current += (90000 / FPS);
    memset(sendbuf, 0, sizeof(sendbuf));

    rtp_hdr = (rtp_header*)&sendbuf[0];
    rtp_hdr->version = 2;
    rtp_hdr->marker = 0;
    rtp_hdr->csrc_len = 0;
    rtp_hdr->extension = 0;
    rtp_hdr->padding = 0;
    rtp_hdr->ssrc = htonl(SSRC_NUM);
    rtp_hdr->payload_type = TYPE_H264;
    rtp_hdr->timestamp = htonl(ts_current);

    if (len <= RTP_PAYLOAD_MAX_SIZE) {
        rtp_hdr->marker = 1;
        rtp_hdr->seq_no = htons(++seq_num);
        nalu_hdr = (nalu_header*)&sendbuf[12];

        nalu_hdr->type = data[0] & 0x1f;
        nalu_hdr->f = data[0] & 0x80;
        nalu_hdr->nri = data[0] & 0x60 >> 5;
        nalu_playload = (uint8_t*)&sendbuf[13];

        memcpy(nalu_playload, data + 1, len-1);

        send_data_client(sendbuf, len + 13);
    } else {
        pack_num = (len % RTP_PAYLOAD_MAX_SIZE) ? (len / RTP_PAYLOAD_MAX_SIZE + 1) : (len / RTP_PAYLOAD_MAX_SIZE);
        /* data size in last packege */
        last_pack_size = (len % RTP_PAYLOAD_MAX_SIZE) ? (len % RTP_PAYLOAD_MAX_SIZE) : (RTP_PAYLOAD_MAX_SIZE);
        current_pack = 0;

        fu_ind = (fu_indicator *)&sendbuf[12];
        fu_ind->f = data[0] & 0x80;
        fu_ind->nri = (data[0] & 0x60) >> 5;

        fu_ind->type = 28;
        fu_hdr = (fu_header *)&sendbuf[13];
        fu_hdr->type = data[0] & 0x1f;

        while (current_pack < pack_num) {

            rtp_hdr->seq_no = htons(++seq_num);
            /* first packet */
            if(current_pack == 0) {

                fu_hdr->s = 1, fu_hdr->e = 0, fu_hdr->r = 0;
                rtp_hdr->marker = 0;

                nalu_playload = (uint8_t*)&sendbuf[14];
                memset(nalu_playload, 0, RTP_PAYLOAD_MAX_SIZE);
                memcpy(nalu_playload, data + 1, RTP_PAYLOAD_MAX_SIZE);

                send_data_client(sendbuf, RTP_PAYLOAD_MAX_SIZE + 14);
            } else if(current_pack < pack_num - 1){
                fu_hdr->s = 0, fu_hdr->e = 0, fu_hdr->r = 0;
                rtp_hdr->marker = 0;
                nalu_playload = (uint8_t*)&sendbuf[14];
                memset(nalu_playload, 0, RTP_PAYLOAD_MAX_SIZE);
                memcpy(nalu_playload, data + (current_pack * RTP_PAYLOAD_MAX_SIZE) + 1, RTP_PAYLOAD_MAX_SIZE);

                send_data_client(sendbuf, RTP_PAYLOAD_MAX_SIZE + 14);
            /* last packet */
            } else {
                rtp_hdr->marker = 1;
                nalu_playload = (uint8_t*)&sendbuf[14];
                fu_hdr->s = 0, fu_hdr->e = 1, fu_hdr->r = 0;
                memset(nalu_playload, 0, RTP_PAYLOAD_MAX_SIZE);
                memcpy(nalu_playload, data + (current_pack * RTP_PAYLOAD_MAX_SIZE) + 1, last_pack_size - 1);

                send_data_client(sendbuf, last_pack_size - 1 + 14);
            }
            current_pack += 1;
        }
    }
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
        printf("out_id not valid\n");;
      }
    }
  }
}


// void network_thread() {
//   sock = socket(AF_INET, SOCK_DGRAM, 0);
//   addr.sin_addr.s_addr = inet_addr(target);
//   addr.sin_port = htons(port);
//   addr.sin_family = AF_INET;

//   printf("will ship frames to %s\n", target);
//   nalu *n;
//   while (!do_exit) {
//     if (nalUnitsFromEncoder.try_pop(n)) {
//       handle_data_sent_to_rtp(n->buf, n->len);
//       printf("did a thing?\n");
//     }
//   }
// }

} // namespace


int main(int argc, char** argv) {
  int c;
  int index;
  opterr = 0;

  while ((c = getopt (argc, argv, "f:t:p:b:")) != -1)
    switch (c)
      {
      case 't':
        target = optarg;
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'f':
        FPS = atoi(optarg);
        break;
      case 'b':
        BITRATE = atoi(optarg);
        break;
      case '?':
        if (optopt == 'f' || optopt == 't' || optopt == 'p' || optopt == 'b')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
      }

  if (target == nullptr) {
    target = (char *) "127.0.0.1";
  }

  printf ("fps = %d, bitrate = %d, port = %d, target = %s\n",
          FPS, BITRATE, port, target);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  addr.sin_addr.s_addr = inet_addr(target);
  addr.sin_port = htons(port);
  addr.sin_family = AF_INET;

  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);

  Context::create();
  std::thread encoding_thread = std::thread(encoder_thread);
  // std::thread networking_thread = std::thread(network_thread);
  while (!do_exit) {
    sleep(1);
  }
  encoding_thread.join();
  // networking_thread.join();
}