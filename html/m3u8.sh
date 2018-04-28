#!/bin/bash
sudo ffmpeg -y -i videotest.mp4 -vcodec copy -acodec copy -vbsf h264_mp4toannexb abc.ts
sudo ffmpeg -i abc.ts -c copy -map 0 -f segment -segment_list playlist.m3u8 -segment_time 5 abc%03d.ts
sudo cp *.ts ./hls
sudo cp *.m3u8 ./hls
