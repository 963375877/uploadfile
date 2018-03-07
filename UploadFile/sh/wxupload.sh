#!/bin/bash

src="/home/tbs/TBSNative/TBSUploadFile/src"
cgi="/home/tbs/TBSNative/TBSUploadFile/cgi"
inc="/home/tbs/TBSNative/TBSUploadFile/include/"
obj="/home/tbs/TBSNative/TBSUploadFile/obj/"


rm ${obj}/wxqsi.o
rm ${obj}/wxupload.o
rm ${obj}/wxcJSON_cgi.o

rm ${cgi}/wxupload.cgi

g++ -c -I ${inc} -DTBSVIDEODEAL_CGI -std=c++11 -o ${obj}/wxqsi.o ${src}/qsi.cpp
g++ -c -I ${inc} -DTBSVIDEODEAL_CGI -std=c++11 -o ${obj}/wxupload.o ${src}/wxupload.c
g++ -c -I ${inc} -DTBSVIDEODEAL_CGI -std=c++11 -o ${obj}/wxcJSON_cgi.o ${src}/cJSON.c

g++ -o ${cgi}/wxupload.cgi ${obj}/wxqsi.o ${obj}/wxupload.o ${obj}/wxcJSON_cgi.o -lfcgi

PID=$(pidof "wxupload.cgi")

if (${PID}); then
 	echo "wxupload.cgi not exit"
	/home/tbs/TBSNative/nginx/sbin/spawn-fcgi -a 168.160.111.26 -p 9014 -f /home/tbs/TBSNative/TBSUploadFile/cgi/wxupload.cgi
    exit
else 
	echo wxupload.cgi pid = ${PID}
	echo kill ${PID}
    kill -9 $PID
fi








