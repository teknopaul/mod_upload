#include <stdio.h>
#include "mpp_flusher.h"
#include "multipart_parser.h"

/**
 * abstract the flush so unit tests can flush to stdout
 */
int mpp_flush_data(char *buffer, int len, void *ctxi) {
//  parse_ctx *ctx = (parse_ctx *)ctxi;
//  fprintf(stdout, "flush boundary=[%s]", ctx->boundary);
  buffer[len + 1] = 0;
  fprintf(stdout, "flush [%d][%s]\n", len, buffer);
  return 0;
}
