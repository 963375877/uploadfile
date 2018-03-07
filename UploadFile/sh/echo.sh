#!/bin/bash

src="/home/tbs/TBSNative/TBSUploadFile/src"
cgi="/home/tbs/TBSNative/TBSUploadFile/cgi"
inc="/home/tbs/TBSNative/TBSUploadFile/include/"
obj="/home/tbs/TBSNative/TBSUploadFile/obj/"


gcc ${src}/echo.c -I ${inc} -L /usr/local/lib -o ${cgi}/echo.cgi -lfcgi
/home/tbs/TBSNative/nginx/sbin/spawn-fcgi -a 168.160.111.26 -p 9011 -f ${cgi}/echo.cgi

