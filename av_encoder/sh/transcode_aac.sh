#! /bin/sh
gcc transcode_aac.c -g -o transcode_aac.out \
-I /usr/local/include -L /usr/local/lib \
-lSDLmain -lSDL -lavformat -lavcodec -lavutil -lavfilter -lswscale -lswresample
