#include <pthread.h>
#include <stdio.h>
#include "header.h"

static std::vector<std::string> streams;
static std::string rtsp_config;
static std::vector<pthread_t> _s_tid;
static std::vector<Stream_Context*> _s_sc;
static std::vector<Buffer_Node*> _s_bn;

void* rtsp_serv(void* arg)
{
    rtsp_main(0, NULL, (AVCodecContext*)arg, rtsp_config.c_str());
    return NULL;
}

int get_rtsp_config(Json::Value config, std::string &rtsp_config)
{
    std::map<std::string, std::string> stream;
    Json::Value::Members::iterator iter;
    Json::Value::Members members;

    if (!config.isMember("RTSPPort") || !config.isMember("RTSPBindAddress") || !config.isMember("Streams")) {
        LOG_ERROR("not found item named 'RTSPPort' or 'RTSPBindAddress' or 'Streams' in rtsp_config");
        return 1;
    } else {
        if (!config["RTSPPort"].isString() || !config["RTSPBindAddress"].isString()) {
            LOG_ERROR("item 'RTSPPort' or 'RTSPBindAddress' is not a string");
            return 1;
        }
        if (!config["Streams"].isObject()) {
            LOG_ERROR("item 'Streams' is not an object");
            return 1;
        }
    }

    rtsp_config = "";

    members = config.getMemberNames();
    for (iter = members.begin(); iter != members.end(); iter++) {
        if (*iter == "Streams") {
            continue;
        } else {
            if (config[*iter].isString()) {
                rtsp_config += *iter + " " + config[*iter].asString() + "\n";
            }
        }
    }
    if (config.isMember("Streams")) {
        if (config["Streams"].isObject()) {
            Json::Value streams = config["Streams"];
            Json::Value::Members streams_mb;
            Json::Value::Members::iterator streams_iter;
            streams_mb = streams.getMemberNames();

            for (streams_iter = streams_mb.begin(); streams_iter != streams_mb.end();
                    streams_iter++) {
                Json::Value stream = streams[*streams_iter];
                if (stream.empty()) {
                    continue;
                }
                if (stream.isObject()) {
                    Json::Value::Members stream_mb;
                    Json::Value::Members::iterator stream_iter;
                    stream_mb = stream.getMemberNames();
                    std::string stream_val = "";
                    for (stream_iter = stream_mb.begin(); stream_iter != stream_mb.end();
                            stream_iter++) {
                        if (stream[*stream_iter].isString()) {
                            stream_val += *stream_iter + " " + stream[*stream_iter].asString() + "\n";
                        }
                    }
                    if (stream_val != "") {
                        rtsp_config += "<Stream " + *streams_iter + ">\n" + stream_val + "</Stream>\n";
                    }
                }
            }
        }
    }
    return 0;
}

int set_env(char *config_file)
{
    unsigned int streams_count = 0;
    char *buf = NULL;
    FILE *fd = NULL;
    int ret = 0;

    Json::Reader reader;
    Json::Value config;
    Json::Value empty;

    struct stat sb;
    if (stat("log", &sb) == -1) {
        if (mkdir("log", 0755) < 0) {
            fprintf(stderr, "[main] mkdir log failed\n");
            return -1;
        }
    } else {
        if ((sb.st_mode & S_IFMT) != S_IFDIR) {
            if (unlink("log") != 0) {
                fprintf(stderr, "[main] ./log need to be a directory, but not, and rm failed\n");
                return -1;
            }
        }
    }

    memset(&sb, 0, sizeof(struct stat));
    if (stat(config_file, &sb) == -1) {
        fprintf(stderr, "cannot stat file %s: %s\n", config_file, strerror(errno));
        return -1;
    } else {
        if ((sb.st_mode & S_IFMT) != S_IFREG) {
            fprintf(stderr, "%s is not a regular file\n", config_file);
            return -1;
        }
    }

    fd = fopen(config_file, "r");
    if (!fd) {
        fprintf(stderr, "open configure file failed: %s\n", strerror(errno));
        return -1;
    }

    buf = (char*)calloc(sb.st_size + 1, 1);
    if (!buf) {
        fprintf(stderr, "calloc failed: %s\n", strerror(errno));
        fclose(fd);
        return -1;
    }

    ret = 0;
    while (!feof(fd)) {
        ret = fread(&buf[ret], 1, sb.st_size - ret, fd);
        if (ferror(fd)) {
            fprintf(stderr, "something error when reading configure file: %s\n", strerror(errno));
            fclose(fd);
            free(buf);
            return -1;
        }
    }

    if (!reader.parse(buf, config)) {
        fprintf(stderr, "the configure file is not in a valid json format\n");
        fclose(fd);
        free(buf);
        return -1;
    }
    fclose(fd);
    free(buf);

    if (!config.isMember("log_file")) {
        fprintf(stderr, "not found config item 'log_file'\n");
        return -1;
    } else {
        if (!config["log_file"].isString()) {
            fprintf(stderr, "config item 'log_file' is not a string\n");
            return -1;
        }
    }
    if (!config.isMember("log_leave")) {
        fprintf(stderr, "not found config item 'log_leave'\n");
        return -1;
    } else {
        if (!config["log_leave"].isInt()) {
            fprintf(stderr, "config item 'log_leave' is not a integer\n");
            return -1;
        }
    }

    if (my_log_init(".", config["log_file"].asString().c_str(),
                (config["log_file"].asString() + ".we").c_str(),
                config["log_leave"].asInt()) < 0) {
        fprintf(stderr, "my_log_init failed: %s\n", strerror(errno));
        unlink(config["log_file"].asString().c_str());
        unlink((config["log_file"].asString() + ".we").c_str());
        //unlink("log");
        return -1;
    }

    if (!freopen("/dev/null", "r", stdin)) {
        LOG_ERROR("failed to redirect STDIN to /dev/null");
        my_log_close();
        return -1;
    }
    if (!freopen((config["log_file"].asString() + ".stdout").c_str(), "w", stdout)) {
        LOG_ERROR("failed to redirect STDIN to %s",
                (config["log_file"].asString() + ".stdout").c_str());
        my_log_close();
        return -1;
    }
    if (!freopen((config["log_file"].asString() + ".stderr").c_str(), "w", stderr)) {
        LOG_ERROR("failed to redirect STDIN to %s",
                (config["log_file"].asString() + ".stderr").c_str());
        my_log_close();
        return -1;
    }

    if (!config.isMember("streams")) {
        LOG_ERROR("there is no item named 'streams' in configure file");
        my_log_close();
        return -1;
    }
    if (!config["streams"].isArray()) {
        LOG_ERROR("the item 'streams' is not a array");
        my_log_close();
        return -1;
    }

    streams_count = config["streams"].size();
    while (streams_count--) {
        Json::Value temp = config["streams"].get(streams_count, empty);
        if (temp.empty()) {
            continue;
        }
        if (temp.isString()) {
            streams.push_back(temp.asString());
        } else {
            LOG_WARN("the %dth item of 'streams' is not a string", streams_count + 1);
        }
    }

    if (streams.size() == 0) {
        LOG_WARN("no stream found yet");
    }

    if (!config.isMember("port")) {
        LOG_ERROR("there is no item named 'port' in configure file");
        my_log_close();
        return -1;
    }
    if (!config["port"].isInt()) {
        LOG_ERROR("the item 'port' is not a integer");
        my_log_close();
        return -1;
    }

    if (!config.isMember("rtsp_config")) {
        LOG_ERROR("there is no item named 'rtsp_config' in configure file");
        my_log_close();
        return -1;
    }
    if (!config["rtsp_config"].isObject()) {
        LOG_ERROR("the item 'port' is not an object");
        my_log_close();
        return -1;
    }
    if (get_rtsp_config(config["rtsp_config"], rtsp_config) != 0) {
        LOG_ERROR("get rtsp config failed");
        my_log_close();
        return -1;
    }

    return 0;
}

int init_rtsp_server(pthread_t *rtsp_thread, int rtsp_width, int rtsp_height,
        AVFrame **p_frame, AVCodecContext **p_ctx)
{
    int ret = 0;

    enum AVCodecID codec_id = AV_CODEC_ID_MPEG4;

    AVCodecContext *ctx = NULL;
    AVCodec *codec = NULL;
    AVFrame *frame = NULL;

    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        LOG_ERROR("canot find encoder: %s", avcodec_get_name(codec_id));
        return -1;
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        LOG_ERROR("could not allocate codec context for %s", avcodec_get_name(codec_id));
        return -1;
    }

    /*
    ctx->bit_rate = 40000000;
    ctx->width = rtsp_width;
    ctx->height = rtsp_height;
    ctx->request_sample_fmt = AV_SAMPLE_FMT_NONE;
    ctx->bits_per_coded_sample = 24;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->time_base = (AVRational){1, 30};
    ctx->gop_size = 10;
    ctx->max_b_frames = 0;
    */
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
        LOG_ERROR("open codec failed: %s\n", av_err2str(ret));
        avcodec_free_context(&ctx);
        av_free(ctx);
        return -1;
    }

    frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("allocating frame failed\n");
        avcodec_free_context(&ctx);
        av_free(ctx);
        return -1;
    }

    frame->format = ctx->pix_fmt;
    frame->height = rtsp_height;
    frame->width = rtsp_width;

    ret = av_image_alloc(frame->data, frame->linesize, frame->width, frame->height, ctx->pix_fmt, 32);
    if (ret < 0) {
        LOG_ERROR("allocating image failed: %s\n", av_err2str(ret));
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
        av_free(ctx);
        return -1;
    }

    ret = pthread_create(rtsp_thread, NULL, rtsp_serv, ctx);
    if (ret != 0) {
        LOG_ERROR("creating rtsp server thread failed");
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
        av_free(ctx);
        return -1;
    }

    *p_frame = frame;
    *p_ctx = ctx;

    return 0;
}

void free_stream_context()
{
    int i, n;

    n = _s_bn.size();
    while (n--) {
        delete _s_bn[n];
    }

    n = _s_sc.size();
    while (n--) {
        i = _s_sc[n]->decoder_to_func.size();
        while (i--) {
            av_frame_free(&_s_sc[n]->decoder_to_func[i]);
            free(_s_sc[n]->decoder_to_func[i]);
        }

        i = _s_sc[n]->func_to_encoder.size();
        while (i--) {
            av_frame_free(&_s_sc[n]->func_to_encoder[i]);
            free(_s_sc[n]->func_to_encoder[i]);
        }

        i = _s_sc[n]->encoder_to_pusher.size();
        while (i--) {
            av_free_packet(_s_sc[n]->encoder_to_pusher[i]);
            free(_s_sc[n]->encoder_to_pusher[i]);
        }

        i = _s_sc[n]->pusher_to_storage.size();
        while (i--) {
            av_free_packet(_s_sc[n]->pusher_to_storage[i]);
            free(_s_sc[n]->pusher_to_storage[i]);
        }

        i = _s_sc[n]->pusher_to_rtsp.size();
        while (i--) {
            av_free_packet(_s_sc[n]->pusher_to_rtsp[i]);
            free(_s_sc[n]->pusher_to_rtsp[i]);
        }

        delete _s_sc[n];
    }

}

int new_stream(Stream_Context *sc)
{
    int ret = 0;

    Buffer_Node *decoder_bn = new Buffer_Node;
    if (!decoder_bn) {
        return -1;
    }

    decoder_bn->filename = sc->filename;
    decoder_bn->packet_buffer = NULL;
    decoder_bn->frame_buffer = &sc->decoder_to_func;
    decoder_bn->packet_mutex = NULL;
    decoder_bn->frame_mutex = &sc->mutex_df;

    pthread_t decoder_thread;
    ret = pthread_create(&decoder_thread, NULL, start_decoder, decoder_bn);
    if (ret != 0) {
        LOG_ERROR("creating decoder thread failed: %s", strerror(ret));
        free(decoder_bn);
        return -1;
    } else {
        _s_bn.push_back(decoder_bn);
    }

    Buffer_Node *encoder_bn = new Buffer_Node;
    if (!encoder_bn) {
        return -1;
    }

    encoder_bn->filename = sc->filename;
    encoder_bn->packet_buffer = &sc->encoder_to_pusher;
    encoder_bn->frame_buffer = &sc->decoder_to_func;
    encoder_bn->packet_mutex = &sc->mutex_ep;
    encoder_bn->frame_mutex = &sc->mutex_df;

    pthread_t encoder_thread;
    ret = pthread_create(&encoder_thread, NULL, start_encoder, encoder_bn);
    if (ret != 0) {
        LOG_ERROR("creating encoder thread failed: %s", strerror(ret));
        free(encoder_bn);
        return -1;
    } else {
        _s_bn.push_back(encoder_bn);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int c = 0;
    char *config_file = NULL;

    while ((c = getopt(argc, argv, "f:")) != EOF) {
        switch (c) {
        case 'f':
            config_file = optarg;
            break;
        case '?':
            fprintf(stderr, "Usage: %s -f config_file\n", argv[0]);
            return 1;
        }
    }

    if (!config_file) {
        fprintf(stderr, "Usage: %s -f config_file\n", argv[0]);
        return 1;
    }

    ret = set_env(config_file);
    if (ret < 0) {
        fprintf(stderr, "set env failed\n");
        return 1;
    }

    av_register_all();
    avcodec_register_all();
    avformat_network_init();

    int rtsp_width = RTSP_WIDTH;
    int rtsp_height = RTSP_HEIGHT;

    AVCodecContext *ctx = NULL;
    AVFrame *frame = NULL;

    pthread_t rtsp_thread;
    ret = init_rtsp_server(&rtsp_thread, rtsp_width, rtsp_height, &frame, &ctx);
    if ((ret < 0)) {
        LOG_ERROR("initialising rtsp server failed");
        return 1;;
    }
    _s_tid.push_back(rtsp_thread);

    int n = streams.size();
    while (n--) {
        Stream_Context *sc = new Stream_Context;
        sc->filename = streams[n];
        _s_sc.push_back(sc);
        new_stream(sc);
    }

    n = _s_tid.size();
    while (n--) {
        pthread_join(_s_tid[n], NULL);
    }

    free_stream_context();

    if (frame) {
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
    }

    if (ctx) {
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
        av_free(ctx);
    }
    
    avformat_network_deinit();

    return 0;
}
