#include "multipart_parser.h"
#include <stdio.h>
#include <string.h>

/**
 * TEST broken content, too much boundary data
 */
int main() {
 
  char too_big_boundary[UNKNOWN_BUFFER_SIZE + 2]; // +1 to overflow + 1 for trailing 0 so we can strcat
  memset(too_big_boundary, 'b', UNKNOWN_BUFFER_SIZE);
  too_big_boundary[UNKNOWN_BUFFER_SIZE] = '\n';
  too_big_boundary[UNKNOWN_BUFFER_SIZE + 1] = 0;
fprintf (stdout,"boundary size %d \n", sizeof(too_big_boundary) );
fprintf (stdout,"boundary len %d \n", strlen(too_big_boundary) );

  char * test_data_00 = too_big_boundary;
  char * test_data_01 = " Content-disposition : form-data; name=\"hidden\"\n";
  char * test_data_02 = "\n";
  char * test_data_03 = "data\n";
  char * test_data_04 = too_big_boundary;

 fprintf (stdout,"\nRunning unit test, too much boundary data \n");

  char test_data[8096];
  test_data[0] = 0;
  strcat(test_data, test_data_00);
  strcat(test_data, test_data_01);
  strcat(test_data, test_data_02);
  strcat(test_data, test_data_03);
  strcat(test_data, test_data_04);

  parse_ctx sctx;
  parse_ctx *ctx = &sctx;
  memset( ctx, 0, sizeof(parse_ctx) );	
  ctx->boundary = too_big_boundary;
  ctx->boundary_len = strlen(ctx->boundary);
 // fprintf (stdout,"setup test data %s \n", test_data);
fprintf(stdout, "meta=%d data=%d unknown=%d\n", META_BUFFER_SIZE, DATA_BUFFER_SIZE, UNKNOWN_BUFFER_SIZE); 
  int i;
  int ret;
  for (i = 0 ; i < strlen(test_data) ; i++) {
	ret = mpp_process_char(ctx, test_data[i]);
    if (ret < 0 ) {
	  fprintf (stdout,"\nfailed [%d]\n", ret );
      return -1;
    }
  }
  if (ret == 0) {
	mpp_finish(ctx);
  }
  fprintf (stdout,"\nFinished file_name [%s] [%s]\n", ctx->file_name, ret == 0 ? "OK" : "FAIL" );
  return ret;
};
