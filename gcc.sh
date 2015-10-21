#!/bin/bash -e

cd `dirname $0`
cd src/c

rm multipart_parser.o mpp_debug_flusher.o 2>/dev/null ||:

FEDORA_LIBS="-I/usr/include/httpd -I/usr/include/apr-1"
DEBIAN_LIBS="-I/usr/include/apache2 -I/usr/include/apr-1.0"
APACHE_LIBS=$DEBIAN_LIBS

gcc -Wall -shared -fPIC -c multipart_parser.c
gcc -Wall -shared -fPIC -c mpp_debug_flusher.c
gcc -Wall -shared -fPIC $APACHE_LIBS -c mpp_apr_file_flusher.c

gcc multipart_parser_test.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test
./multipart_parser_test
gcc multipart_parser_test1.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test1
./multipart_parser_test1
gcc multipart_parser_test2.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test2
./multipart_parser_test2
gcc multipart_parser_test3.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test3
./multipart_parser_test3
gcc multipart_parser_test4.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test4
./multipart_parser_test4
gcc multipart_parser_test5.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test5
./multipart_parser_test5
gcc multipart_parser_test6.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test6
./multipart_parser_test6
gcc multipart_parser_test_negative2.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test_negative2
if ./multipart_parser_test_negative2 ; then exit 1 ; fi
gcc multipart_parser_test_negative3.c multipart_parser.o mpp_debug_flusher.o -o multipart_parser_test_negative3
if ./multipart_parser_test_negative3 ; then exit 1 ; fi


rm mod_upload.so 2> /dev/null ||:

gcc -Wall -shared -fPIC -DSHARED_MODULE $APACHE_LIBS mod_upload.c mpp_apr_file_flusher.o multipart_parser.o -o mod_upload.so

if [ -f mod_upload.so ] ; then
  echo created mod_upload.so
  mv mod_upload.so ../..
fi

gcc -Wall -shared -fPIC $APACHE_LIBS -c mod_jsonindex.c
ld -Bshareable -o mod_jsonindex.so mod_jsonindex.o

if [ -f mod_jsonindex.so ] ; then
  echo created mod_jsonindex.so
  mv mod_jsonindex.so ../..
fi

rm *.o

cd ../..

echo done.

