#ifndef HEADER_H
#define HEADER_H

extern "C" {
#define __STDC_CONSTANT_MACROS
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
#include "libavformat/avio.h"
}
#include <json/json.h>
#include <evhttp.h>
#include <mylog.h>

#include <vector>
#include <deque>

#define msleep(x) \
    do { \
        struct timespec time_to_wait; \
        time_to_wait.tv_sec = 0; \
        time_to_wait.tv_nsec = 1000 * 1000 * x; \
        nanosleep(&time_to_wait, NULL); \
    } while(0)

#define DEQUE_MAX_SIZE 10000

#define BIT_RATE 40000000
#define RTSP_WIDTH 1900
#define RTSP_HEIGHT 1040
#define REQUEST_SAMPLE_FMT AV_SAMPLE_FMT_NONE
#define BITS_PER_CODED_SAMPLE 24
#define PIX_FMT AV_PIX_FMT_YUV420P
#define TIME_BASE (AVRational){1, 30}
#define GOP_SIZE 10
#define MAX_B_FRAMES 10

typedef struct {
    std::string filename;

    std::deque<AVFrame*> decoder_to_func;
    std::deque<AVFrame*> func_to_encoder;
    std::deque<AVPacket*> encoder_to_pusher;
    std::deque<AVPacket*> pusher_to_storage;
    std::deque<AVPacket*> pusher_to_rtsp;

    pthread_mutex_t mutex_df;
    pthread_mutex_t mutex_fe;
    pthread_mutex_t mutex_ep;
    pthread_mutex_t mutex_ps;
    pthread_mutex_t mutex_pr;

    int is_stop;
} Stream_Context;

typedef struct {
    std::string filename;

    std::deque<AVPacket*> *packet_buffer;
    std::deque<AVFrame*> *frame_buffer;
    pthread_mutex_t *packet_mutex;
    pthread_mutex_t *frame_mutex;
} Buffer_Node;

extern "C" int pickup_packet(AVPacket *pkt, std::deque<AVPacket*> *buffer, pthread_mutex_t *mutex);
extern "C" int pickup_frame(AVFrame **frame, std::deque<AVFrame*> *buffer, pthread_mutex_t *mutex);
extern "C" int insert_packet(AVPacket *pkt, std::deque<AVPacket*> *buffer, pthread_mutex_t *mutex);
extern "C" int insert_frame(AVFrame *frame, std::deque<AVFrame*> *buffer, pthread_mutex_t *mutex);
extern "C" void clear_packets(std::deque<AVPacket*> *buffer, pthread_mutex_t *mutex);
extern "C" void clear_frames(std::deque<AVFrame*> *buffer, pthread_mutex_t *mutex);

extern "C" void* start_decoder(void* arg);
extern "C" void* start_encoder(void* arg);
extern "C" int rtsp_main(int argc, char **argv, AVCodecContext *ctx_src, const char *config_str);

#endif
