#include <pthread.h>
#include <stdio.h>
#include "header.h"

static std::deque<AVPacket*> *packet_buffer;
static std::deque<AVFrame*> *frame_buffer;
static pthread_mutex_t *packet_mutex;
static pthread_mutex_t *frame_mutex;

static enum AVCodecID codec_id = AV_CODEC_ID_MPEG4;

static int encoding(char *filename)
{
    int ret = 0;

    AVCodecContext *ctx = NULL;
    AVCodec *codec = NULL;
    AVFrame *frame = NULL;

    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        LOG_ERROR("canot find encoder: %s (%s)", avcodec_get_name(codec_id), filename);
        return -1;
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        LOG_ERROR("avcodec_alloc_context3 failed for %s (%s)", avcodec_get_name(codec_id), filename);
        return -1;
    }

    ctx->bit_rate = BIT_RATE;
    ctx->width = RTSP_WIDTH;
    ctx->height = RTSP_HEIGHT;
    ctx->request_sample_fmt = REQUEST_SAMPLE_FMT;
    ctx->bits_per_coded_sample = BITS_PER_CODED_SAMPLE;
    ctx->pix_fmt = PIX_FMT;
    ctx->time_base = TIME_BASE;
    ctx->gop_size = GOP_SIZE;
    ctx->max_b_frames = MAX_B_FRAMES;

    ret = avcodec_open2(ctx, codec, NULL);  // not thread safe
    if (ret <  0) {
        LOG_ERROR("open codec failed: %s (%s)", av_err2str(ret), filename);
        avcodec_free_context(&ctx);
        av_free(ctx);
        return -1;
    }

    int got_output = 0;

    while (1) {
        ret = pickup_frame(&frame, frame_buffer, frame_mutex);
        if (ret == -EAGAIN) {
            msleep(100);
            continue;
        } else if (ret == -EINVAL) {
            LOG_ERROR("pickup frame failed (%s)", filename);
            avcodec_free_context(&ctx);
            av_free(ctx);
            return -1;
        }

        AVPacket *pkt = (AVPacket*)malloc(sizeof(AVPacket));
        if (!pkt) {
            LOG_ERROR("allocting AVPacket failed (%s)", filename);
            avcodec_free_context(&ctx);
            av_free(ctx);
            return -1;
        }
        av_init_packet(pkt);
        pkt->data = NULL;
        pkt->size = 0;

        ret = avcodec_encode_video2(ctx, pkt, frame, &got_output);
        av_frame_free(&frame);
        if (ret < 0) {
            LOG_WARN("avcodec_encode_video2 failed: %s (%s)", av_err2str(ret), filename);
            continue;
        }

        if (!got_output) {
            continue;
        }

        ret = insert_packet(pkt, packet_buffer, packet_mutex);
        if (ret == -EAGAIN) {
            msleep(100);
            continue;
        } else if (ret == -EINVAL) {
            LOG_ERROR("insert packet failed (%s)", filename);
            avcodec_free_context(&ctx);
            av_free(ctx);
            return -1;
        }
    }

    return 0;
}

extern "C"
int pickup_pkt(AVPacket *pkt, int *stop)
{
    return pickup_packet(pkt, packet_buffer, packet_mutex);
}

extern "C"
void stop_and_clear()
{
}

extern "C"
void* start_encoder(void* arg)
{
    if (!arg) {
        return NULL;
    }

    Buffer_Node *bn = (Buffer_Node*)arg;

    packet_buffer = bn->packet_buffer;
    frame_buffer = bn->frame_buffer;
    packet_mutex = bn->packet_mutex;
    frame_mutex = bn->frame_mutex;

    if (!packet_buffer || !packet_mutex || !frame_buffer || !frame_mutex) {
        LOG_INFO("Invalied parameter");
        return NULL;
    }

    pthread_t tid = pthread_self();

    LOG_INFO("encoder started (tid: %ld)", tid);

    encoding((char*)bn->filename.c_str());

    LOG_INFO("encoder exited (tid: %ld)", tid);

    return NULL;
}
