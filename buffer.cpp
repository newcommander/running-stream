#include <pthread.h>
#include <stdio.h>
#include "header.h"

extern "C"
int pickup_packet(AVPacket *pkt, std::deque<AVPacket*> *buffer, pthread_mutex_t *mutex)
{
    if (!pkt || !buffer || !mutex) {
        return -EINVAL;
    }

    AVPacket *tmp = NULL;

    int total;

    pthread_mutex_lock(mutex);

    total = buffer->size();
    if (total <= 0) {
        pthread_mutex_unlock(mutex);
        return -EAGAIN;
    }
    tmp = buffer->front();
    buffer->pop_front();

    pthread_mutex_unlock(mutex);

    av_copy_packet(pkt, tmp);
    av_free_packet(tmp);
    free(tmp);

    LOG_INFO("pickup packet success, total %d", total - 1);

    return 0;
}

extern "C"
int insert_packet(AVPacket *pkt, std::deque<AVPacket*> *buffer, pthread_mutex_t *mutex)
{
    if (!pkt || !buffer || !mutex) {
        return -EINVAL;
    }

    int total;
    AVPacket *tmp = NULL;

    pthread_mutex_lock(mutex);

    total = buffer->size();
    if (total >= DEQUE_MAX_SIZE) {
        ////////////////////////////////
        pthread_mutex_unlock(mutex);
        LOG_WARN("packet buffer full, drop newest one, total: %d", total);
        av_free_packet(pkt);
        free(pkt);
        return -EAGAIN;
        ////////////////////////////////
        tmp = buffer->front();
        buffer->pop_front();
    }
    buffer->push_back(pkt);

    pthread_mutex_unlock(mutex);

    if (tmp) {
        av_free_packet(tmp);
        free(tmp);
    }

    LOG_INFO("insert packet success, total %d", total + 1);

    return 0;
}

extern "C"
void clear_packets(std::deque<AVPacket*> *buffer, pthread_mutex_t *mutex)
{
    AVPacket *pkt;

    if (!buffer || !mutex) {
        return;
    }

    pthread_mutex_lock(mutex);

    while (!buffer->empty()) {
        pkt = buffer->front();
        buffer->pop_front();
        av_free_packet(pkt);
        free(pkt);
    }
    buffer->clear();

    pthread_mutex_unlock(mutex);

    LOG_INFO("cleared packets buffer");
}

extern "C"
int pickup_frame(AVFrame **frame, std::deque<AVFrame*> *buffer, pthread_mutex_t *mutex)
{
    if (!frame || !buffer || !mutex) {
        return -EINVAL;
    }

    int total;

    pthread_mutex_lock(mutex);

    total = buffer->size();
    if (total <= 0) {
        pthread_mutex_unlock(mutex);
        return -EAGAIN;
    }
    *frame = buffer->front();
    buffer->pop_front();

    pthread_mutex_unlock(mutex);

    LOG_INFO("pickup frame success, total %d", total - 1);

    return 0;
}

extern "C"
int insert_frame(AVFrame *frame, std::deque<AVFrame*> *buffer, pthread_mutex_t *mutex)
{
    if (!frame || !buffer || !mutex) {
        return -EINVAL;
    }

    int total;
    AVFrame *tmp = NULL;

    pthread_mutex_lock(mutex);

    total = buffer->size();
    if (total >= DEQUE_MAX_SIZE) {
        ////////////////////////////////
        pthread_mutex_unlock(mutex);
        LOG_WARN("frame buffer full, drop newest one, total: %d", total);
        av_frame_free(&frame);
        free(frame);
        return -EAGAIN;
        ////////////////////////////////
        tmp = buffer->front();
        buffer->pop_front();
    }
    buffer->push_back(frame);

    pthread_mutex_unlock(mutex);

    if (tmp) {
        av_frame_free(&tmp);
        free(tmp);
    }

    LOG_INFO("insert frame success, total %d", total + 1);

    return 0;
}

extern "C"
void clear_frames(std::deque<AVFrame*> *buffer, pthread_mutex_t *mutex)
{
    AVFrame *frame;

    if (!buffer || !mutex) {
        return;
    }

    pthread_mutex_lock(mutex);

    while (!buffer->empty()) {
        frame = buffer->front();
        buffer->pop_front();
        av_frame_free(&frame);
        free(frame);
    }
    buffer->clear();

    pthread_mutex_unlock(mutex);

    LOG_INFO("cleared frames buffer");
}
