#!/bin/bash

cd `dirname $0`
cd src/c


gcc -Wall -shared -fPIC -c multipart_parser.c
gcc -Wall -shared -fPIC -c mpp_cgi_flusher.c

gcc mpp_cgi_parser.c multipart_parser.o mpp_cgi_flusher.o -o ../../cgiupload

cd ../..