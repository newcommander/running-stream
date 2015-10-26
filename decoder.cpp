#include <pthread.h>
#include <stdio.h>
#include "header.h"

static std::deque<AVFrame*> *buffer;
static pthread_mutex_t *mutex;

static int open_input_file(AVFormatContext **ifmt_ctx, char *filename, int *stream_index)
{
    int ret = 0;

    AVFormatContext *p_fmt = NULL;

    ret = avformat_open_input(ifmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        LOG_ERROR("avformat_open_input failed: %s (%s)", av_err2str(ret), filename);
        return -1;
    }

    p_fmt = *ifmt_ctx;
    if (!p_fmt) {
        LOG_ERROR("get format context failed (%s)", filename);
        return -1;
    }

    ret = avformat_find_stream_info(p_fmt, NULL);
    if (ret < 0) {
        LOG_ERROR("avformat_find_stream_info failed: %s (%s)", av_err2str(ret), filename);
        avformat_close_input(ifmt_ctx);
        return -1;
    }

    unsigned int i = 0;
    // TODO av_find_best_stream
    for (i = 0; i < p_fmt->nb_streams; i++) {
        AVStream *stream = NULL;
        AVCodecContext *codec_ctx = NULL;
        stream = p_fmt->streams[i];
        codec_ctx = stream->codec;
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            ret = avcodec_open2(codec_ctx, avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                LOG_ERROR("avcodec_open2 failed: %s (%s[stream_index:%d])", av_err2str(ret), filename, i);
                avformat_close_input(ifmt_ctx);
                continue;
            }
            *stream_index = i;
            return 0;
        }
    }
    //av_dump_format(p_fmt, 0, filename, 0);

    LOG_ERROR("Could not found vailed video stream (%s)", filename);
    return -1;
}

static int decoding(char *filename)
{
    int ret = 0;
    int stream_index = -1;

    AVFormatContext *ifmt_ctx = NULL;

    ret = open_input_file(&ifmt_ctx, filename, &stream_index);
    if ((ret < 0) || (stream_index < -1)) {
        return -1;
    }

    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    dec_func = NULL;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    //int64_t tt = 0;
    int got_frame = 0;
    int read_error_counter = 0;
    while (1) {
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            if (read_error_counter++ < 100) {
                msleep(100);
                continue;
            } else {
                break;
            }
        }
        read_error_counter = 0;
        if (pkt.stream_index != stream_index) {
            continue;
        }

        av_packet_rescale_ts(&pkt, ifmt_ctx->streams[stream_index]->time_base,
                ifmt_ctx->streams[stream_index]->codec->time_base);

        dec_func = avcodec_decode_video2;

        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            LOG_ERROR("av_frame_alloc failed (%s)", filename);
            avcodec_close(ifmt_ctx->streams[stream_index]->codec);
            avformat_close_input(&ifmt_ctx);
            return -1;
        }

        do {
            got_frame = 0;
            ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame, &got_frame, &pkt);
            if (ret < 0) {
                LOG_WARN("decode packet failed: %s (%s[stream_index:%d])", av_err2str(ret), filename, stream_index);
                av_free_packet(&pkt);
                continue;
            }
            pkt.data += ret;
            pkt.size -= ret;
        } while (pkt.size > 0);
        if (got_frame) {
            /*
            if (frame->pts == AV_NOPTS_VALUE) {
                frame->pts = tt++;
            } else {
                frame->pts = av_frame_get_best_effort_timestamp(frame);
            }
            */

            while (insert_frame(frame, buffer, mutex) == -EAGAIN) {
                msleep(100);
            }
        }
        av_free_packet(&pkt);
    }

    avcodec_close(ifmt_ctx->streams[stream_index]->codec);
    avformat_close_input(&ifmt_ctx);

    return 0;
}

extern "C"
void* start_decoder(void* arg)
{
    if (!arg) {
        return NULL;
    }

    Buffer_Node *bn = (Buffer_Node*)arg;
    buffer = bn->frame_buffer;
    mutex = bn->frame_mutex;
    if (!buffer || !mutex) {
        LOG_INFO("Invalied parameter");
        return NULL;
    }

    pthread_t tid = pthread_self();

    LOG_INFO("decoder started (tid: %ld)", tid);

    decoding((char*)bn->filename.c_str());

    LOG_INFO("decoder exited (tid: %ld)", tid);

    return NULL;
}
