#!/bin/sh

rm -f librtsp.a

gcc -g \
    -I../ \
    -I../../thirdparty/output/ffmpeg/include \
    -L../../thirdparty/output/openssl/lib \
    -L../../thirdparty/output/ffmpeg/lib \
    -Wl,--dn -pthread -lssl -lcrypto \
    -lswresample -lavdevice -lavfilter \
    -lswscale -lSDL -lavformat -lavcodec \
    -lswresample -lavutil -lz -Wl,--dy \
    -lrt -lm -ldl -lxcb-shape -lxcb-shm \
    -lxcb-xfixes -lasound -static -c \
    rtsp_server/rtsp_server.c rtsp_server/rtsp_server_config.c ../cmdutils.c

ar rcs librtsp.a cmdutils.o rtsp_server_config.o rtsp_server.o

rm -f cmdutils.o rtsp_server_config.o rtsp_server.o

g++ -g \
    -I../../thirdparty/output/libevent/include \
    -I../../thirdparty/output/libcurl/include \
    -I../../thirdparty/output/jsoncpp/include \
    -I../../thirdparty/output/openssl/include \
    -I../../thirdparty/output/libidn/include \
    -I../../thirdparty/output/ffmpeg/include \
    -I../../thirdparty/output/mylog/include \
    -I../../thirdparty/output/zlib/include \
    -L../../thirdparty/output/libevent/lib \
    -L../../thirdparty/output/libcurl/lib \
    -L../../thirdparty/output/jsoncpp/lib \
    -L../../thirdparty/output/openssl/lib \
    -L../../thirdparty/output/libidn/lib \
    -L../../thirdparty/output/ffmpeg/lib \
    -L../../thirdparty/output/mylog/lib \
    -L../../thirdparty/output/zlib/lib \
    main.cpp decoder.cpp encoder.cpp buffer.cpp librtsp.a \
    -lswresample -lavdevice -lavfilter -ljsoncpp\
    -lswscale -lSDL -lavformat -lavcodec \
    -lswresample -lavutil -lz -lrt -lmylog -Wall \
    -Werror -o run
