#!/bin/bash

src="/home/tbs/TBSNative/TBSUploadFile/src"
cgi="/home/tbs/TBSNative/TBSUploadFile/cgi"
inc="/home/tbs/TBSNative/TBSUploadFile/include/"
obj="/home/tbs/TBSNative/TBSUploadFile/obj/"


rm ${obj}/qsi.o 
rm ${obj}/upload.o
rm ${obj}/cJSON_cgi.o

rm ${cgi}/upload.cgi

g++ -c -I ${inc} -DTBSVIDEODEAL_CGI -std=c++11 -o ${obj}/qsi.o ${src}/qsi.cpp
g++ -c -I ${inc} -DTBSVIDEODEAL_CGI -std=c++11 -o ${obj}/upload.o ${src}/upload.c
g++ -c -I ${inc} -DTBSVIDEODEAL_CGI -std=c++11 -o ${obj}/cJSON_cgi.o ${src}/cJSON.c

g++ -o ${cgi}/upload.cgi -g ${obj}/qsi.o ${obj}/upload.o ${obj}/cJSON_cgi.o -lfcgi -lpthread -lmysqlclient

PID=$(pidof "upload.cgi")

if (${PID}); then
 	echo "upload.cgi not exit"
	/home/tbs/TBSNative/nginx/sbin/spawn-fcgi -a 168.160.111.26 -p 9013 -f /home/tbs/TBSNative/TBSUploadFile/cgi/upload.cgi
    exit
else 
	echo upload.cgi pid = ${PID}
	echo kill ${PID}
    kill -9 $PID
fi








