#!/bin/bash
PID=$(pidof "UploadFile.cgi")

if ($PID); then
 	echo "UploadFile.cgi not exit"
 	rm /home/tbs/TBSNative/TBSUploadFile/cgi/UploadFile.cgi
	gcc UploadFile.c -I ../include/ -L /usr/local/lib -o UploadFile.cgi -lfcgi -std=c99
	mv UploadFile.cgi ../cgi
	/home/tbs/TBSNative/nginx/sbin/spawn-fcgi -a 168.160.111.26 -p 9012 -f /home/tbs/TBSNative/TBSUploadFile/cgi/UploadFile.cgi
    exit
else 
	echo UploadFile.cgi pid = $PID
	echo kill $PID
    kill $PID
fi




