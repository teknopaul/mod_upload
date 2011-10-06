// I don't really know why this is needed but it seems to make APR includes work on my 32bit linux
#ifndef off64_t
# define off64_t unsigned long long
#endif

#include <httpd.h>
#include <apr_strings.h>
#include <util_filter.h>
#include <http_config.h>
#include <http_log.h>


#include "mpp_flusher.h"
#include "multipart_parser.h"
/**
 * flush to a open file
 * noop if the file is not initialised
 */
int mpp_flush_data(char *buffer, int len, void *ctxi) {
  apr_size_t inout = len;
  parse_ctx *ctx = (parse_ctx *)ctxi;
  if (ctx->user_data == 0) {
    return 0;
  }
  apr_file_t *fle = (apr_file_t *)ctx->user_data;
  apr_file_write(fle, buffer, &inout);
  return inout;
};

