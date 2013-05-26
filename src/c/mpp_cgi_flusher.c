#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "mpp_flusher.h"
#include "multipart_parser.h"

/**
 * flush to an open C fine descriptor
 * noop if the file is not initialised
 * Uses no APR deps so it can be compiled to a CGI program
 */
int mpp_flush_data(char *buffer, int len, void *ctxi) {
  parse_ctx *ctx = (parse_ctx *)ctxi;
  if (ctx->user_data == 0) {
    return 0;
  }
  int *fle = (int*)ctx->user_data;
  write(*fle, buffer, len);
  return 0;
};

